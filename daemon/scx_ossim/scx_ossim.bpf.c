#include <scx/common.bpf.h>

#include "interface.h"

char _license[] SEC("license") = "GPL";

const volatile bool fifo_sched;

static u64 vtime_now;
UEI_DEFINE(uei);

/*
 * We maintain two global DSQs:
 * - VCPU_DSQ: For registered vCPU threads
 * - SYSTEM_DSQ: For system threads (non-vCPU tasks)
 */
#define VCPU_DSQ 0
#define SYSTEM_DSQ 1

/*
 * Statistics counters:
 * [0]: local enqueues
 * [1]: global enqueues
 * [2]: vCPU enqueues
 * [3]: system enqueues
 */
struct {
  __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
  __uint(key_size, sizeof(u32));
  __uint(value_size, sizeof(u64));
  __uint(max_entries, 4); /* [local, global, vcpu, system] */
} stats SEC(".maps");

/* Unified FIFO queue for all pending events */
struct {
  __uint(type, BPF_MAP_TYPE_QUEUE);
  __uint(max_entries, SCX_OSSIM_MAX_PENDING_EVENTS);
  __type(value, struct scx_ossim_event);
} event_queue SEC(".maps");

/* Hash map storing registered vCPUs (key: TID, value: metadata) */
struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(max_entries, SCX_OSSIM_MAX_VCPUS);
  __type(key, pid_t);
  __type(value, struct scx_ossim_vcpu_metadata);
} vcpu_registry SEC(".maps");

/* Global coordination list (single entry, key = 0) */
struct {
  __uint(type, BPF_MAP_TYPE_ARRAY);
  __uint(max_entries, 1);
  __type(key, __u32);
  __type(value, struct scx_ossim_coord_list);
} global_coord_list SEC(".maps");

static void stat_inc(u32 idx) {
  u64 *cnt_p = bpf_map_lookup_elem(&stats, &idx);
  if (cnt_p)
    (*cnt_p)++;
}

/* Coordination list helper functions
 *
 * TODO: Consider whether we need to protect each coordination list
 * with a spinlock.
 */
static void coord_list_add(struct scx_ossim_coord_list *list, pid_t tid) {
  if (!list || list->count >= SCX_OSSIM_MAX_COORD_VCPUS)
    return;

  /* Check if already in list */
  __u32 count = list->count;
  if (count > SCX_OSSIM_MAX_COORD_VCPUS)
    count = SCX_OSSIM_MAX_COORD_VCPUS;

  for (__u32 j = 0; j < count; j++) {
    if (j >= SCX_OSSIM_MAX_COORD_VCPUS)
      break;
    if (list->tids[j] == tid)
      return; /* Already exists */
  }

  /* Add to list */
  if (list->count < SCX_OSSIM_MAX_COORD_VCPUS) {
    list->tids[list->count] = tid;
    list->count++;
  }
}

static void coord_list_remove(struct scx_ossim_coord_list *list, pid_t tid) {
  if (!list || list->count > SCX_OSSIM_MAX_COORD_VCPUS)
    return;

  __u32 count = list->count;
  if (count > SCX_OSSIM_MAX_COORD_VCPUS)
    count = SCX_OSSIM_MAX_COORD_VCPUS;

  for (__u32 j = 0; j < count; j++) {
    if (j >= SCX_OSSIM_MAX_COORD_VCPUS)
      break;
    if (list->tids[j] == tid) {
      /* Shift remaining elements */
      __u32 shift_count = count - 1;
      if (shift_count > SCX_OSSIM_MAX_COORD_VCPUS - 1)
        shift_count = SCX_OSSIM_MAX_COORD_VCPUS - 1;
      for (__u32 k = j; k < shift_count; k++) {
        if (k >= SCX_OSSIM_MAX_COORD_VCPUS - 1 ||
            k + 1 >= SCX_OSSIM_MAX_COORD_VCPUS)
          break;
        list->tids[k] = list->tids[k + 1];
      }
      list->count--;
      break;
    }
  }
}

/* Process pending events from the unified queue */
static void process_events(void) {
  struct scx_ossim_event event;
  int ret;

  /* Process up to 32 events per call to avoid hogging CPU */
  for (int i = 0; i < 32; i++) {
    ret = bpf_map_pop_elem(&event_queue, &event);
    if (ret != 0) {
      /* Queue is empty */
      break;
    }

    if (event.event_type == SCX_OSSIM_EVENT_VCPU_REGISTER) {
      /* Register vCPU */
      struct scx_ossim_vcpu_metadata metadata = {
          .tid = event.vcpu_reg.tid,
          .vm_id = event.vcpu_reg.vm_id,
          .vcpu_id = event.vcpu_reg.vcpu_id,
          .timestamp = bpf_ktime_get_ns(),
          .coord_list = {.count = 0},
      };
      bpf_map_update_elem(&vcpu_registry, &event.vcpu_reg.tid, &metadata,
                          BPF_ANY);

      /* Now, we add all vCPUs to the global coordination list */
      __u32 key = 0;
      struct scx_ossim_coord_list *global_list;
      global_list = bpf_map_lookup_elem(&global_coord_list, &key);
      if (global_list) {
        coord_list_add(global_list, event.vcpu_reg.tid);
      }
    } else if (event.event_type == SCX_OSSIM_EVENT_VCPU_UNREGISTER) {
      /* Unregister vCPU */
      bpf_map_delete_elem(&vcpu_registry, &event.vcpu_unreg.tid);

      /* Remove from global coordination list */
      __u32 key = 0;
      struct scx_ossim_coord_list *global_list;
      global_list = bpf_map_lookup_elem(&global_coord_list, &key);
      if (global_list) {
        coord_list_remove(global_list, event.vcpu_unreg.tid);
      }

    } else if (event.event_type == SCX_OSSIM_EVENT_COORD_ADD) {
      /* Add vCPU to another vCPU's coordination list */
      struct scx_ossim_vcpu_metadata *metadata;
      metadata = bpf_map_lookup_elem(&vcpu_registry, &event.coord_op.vcpu_tid);
      if (metadata) {
        coord_list_add(&metadata->coord_list, event.coord_op.related_tid);
      }

    } else if (event.event_type == SCX_OSSIM_EVENT_COORD_REMOVE) {
      /* Remove vCPU from coordination list */
      struct scx_ossim_vcpu_metadata *metadata;
      metadata = bpf_map_lookup_elem(&vcpu_registry, &event.coord_op.vcpu_tid);
      if (metadata) {
        coord_list_remove(&metadata->coord_list, event.coord_op.related_tid);
      }

    } else if (event.event_type == SCX_OSSIM_EVENT_COORD_CLEAR) {
      /* Clear coordination list */
      struct scx_ossim_vcpu_metadata *metadata;
      metadata = bpf_map_lookup_elem(&vcpu_registry, &event.coord_op.vcpu_tid);
      if (metadata) {
        metadata->coord_list.count = 0;
      }

    } else if (event.event_type == SCX_OSSIM_EVENT_GLOBAL_COORD_ADD) {
      /* Add TID to global coordination list */
      __u32 key = 0;
      struct scx_ossim_coord_list *global_list;
      global_list = bpf_map_lookup_elem(&global_coord_list, &key);
      if (global_list) {
        coord_list_add(global_list, event.global_coord.tid);
      }

    } else if (event.event_type == SCX_OSSIM_EVENT_GLOBAL_COORD_REMOVE) {
      /* Remove TID from global coordination list */
      __u32 key = 0;
      struct scx_ossim_coord_list *global_list;
      global_list = bpf_map_lookup_elem(&global_coord_list, &key);
      if (global_list) {
        coord_list_remove(global_list, event.global_coord.tid);
      }

    } else if (event.event_type == SCX_OSSIM_EVENT_GLOBAL_COORD_CLEAR) {
      /* Clear global coordination list */
      __u32 key = 0;
      struct scx_ossim_coord_list *global_list;
      global_list = bpf_map_lookup_elem(&global_coord_list, &key);
      if (global_list) {
        global_list->count = 0;
      }
    }
  }
}

s32 BPF_STRUCT_OPS(ossim_select_cpu, struct task_struct *p, s32 prev_cpu,
                   u64 wake_flags) {
  bool is_idle = false;
  s32 cpu;
  pid_t tid;
  void *metadata;

  cpu = scx_bpf_select_cpu_dfl(p, prev_cpu, wake_flags, &is_idle);
  if (is_idle) {
    stat_inc(0); /* count local queueing */

    /* Check if this is a vCPU task */
    tid = p->pid;
    metadata = bpf_map_lookup_elem(&vcpu_registry, &tid);

    if (metadata != NULL) {
      /* vCPU task - route to vCPU DSQ instead of local */
      scx_bpf_dsq_insert(p, VCPU_DSQ, SCX_SLICE_DFL, 0);
      stat_inc(2); /* count vCPU enqueues */
    } else {
      /* System task - can use local DSQ */
      scx_bpf_dsq_insert(p, SCX_DSQ_LOCAL, SCX_SLICE_DFL, 0);
    }
  }

  return cpu;
}

void BPF_STRUCT_OPS(ossim_enqueue, struct task_struct *p, u64 enq_flags) {
  pid_t tid;
  u64 dsq_id;
  void *metadata;

  stat_inc(1); /* count global queueing */

  /* Get the thread ID from task_struct */
  tid = p->pid;

  /* Check if this task is a registered vCPU */
  metadata = bpf_map_lookup_elem(&vcpu_registry, &tid);

  if (metadata != NULL) {
    dsq_id = VCPU_DSQ;
    stat_inc(2); /* vCPU enqueue */
  } else {
    dsq_id = SYSTEM_DSQ;
    stat_inc(3); /* system enqueu */
  }

  if (fifo_sched) {
    scx_bpf_dsq_insert(p, dsq_id, SCX_SLICE_DFL, enq_flags);
  } else {
    u64 vtime = p->scx.dsq_vtime;

    /*
     * Limit the amount of budget that an idling task can accumulate
     * to one slice.
     */
    if (time_before(vtime, vtime_now - SCX_SLICE_DFL))
      vtime = vtime_now - SCX_SLICE_DFL;

    scx_bpf_dsq_insert_vtime(p, dsq_id, SCX_SLICE_DFL, vtime, enq_flags);
  }
}

void BPF_STRUCT_OPS(ossim_dispatch, s32 cpu, struct task_struct *prev) {
  /* Process pending events (registration and coordination) */
  process_events();

  /* Dispatch from vCPU DSQ first (higher priority) */
  scx_bpf_dsq_move_to_local(VCPU_DSQ);

  /* Then dispatch from system DSQ */
  scx_bpf_dsq_move_to_local(SYSTEM_DSQ);
}

void BPF_STRUCT_OPS(ossim_running, struct task_struct *p) {
  if (fifo_sched)
    return;

  /*
   * Global vtime always progresses forward as tasks start executing. The
   * test and update can be performed concurrently from multiple CPUs and
   * thus racy. Any error should be contained and temporary. Let's just
   * live with it.
   */
  if (time_before(vtime_now, p->scx.dsq_vtime))
    vtime_now = p->scx.dsq_vtime;
}

void BPF_STRUCT_OPS(ossim_stopping, struct task_struct *p, bool runnable) {
  if (fifo_sched)
    return;

  /*
   * Scale the execution time by the inverse of the weight and charge.
   *
   * Note that the default yield implementation yields by setting
   * @p->scx.slice to zero and the following would treat the yielding task
   * as if it has consumed all its slice. If this penalizes yielding tasks
   * too much, determine the execution time by taking explicit timestamps
   * instead of depending on @p->scx.slice.
   */
  p->scx.dsq_vtime += (SCX_SLICE_DFL - p->scx.slice) * 100 / p->scx.weight;
}

void BPF_STRUCT_OPS(ossim_enable, struct task_struct *p) {
  p->scx.dsq_vtime = vtime_now;
}

void BPF_STRUCT_OPS(ossim_disable, struct task_struct *p) {
  pid_t tid = p->pid;
  struct scx_ossim_vcpu_metadata *metadata;

  /* Check if this task is a registered vCPU */
  metadata = bpf_map_lookup_elem(&vcpu_registry, &tid);
  if (!metadata)
    return; /* Not a vCPU, nothing to clean up */

  /* Remove from the global coordination list */
  __u32 key = 0;
  struct scx_ossim_coord_list *global_list;
  global_list = bpf_map_lookup_elem(&global_coord_list, &key);
  if (global_list) {
    coord_list_remove(global_list, tid);
  }

  /*
   * Note: We don't clean up per-vCPU coordination lists here because
   * iterating through all registered vCPUs would be too expensive in BPF.
   * Per-vCPU coordination list entries pointing to this TID will naturally
   * fail when trying to coordinate with a non-existent vCPU.
   */
}

s32 BPF_STRUCT_OPS_SLEEPABLE(ossim_init) {
  s32 ret;
  __u32 key = 0;
  struct scx_ossim_coord_list empty_list = {.count = 0};

  ret = scx_bpf_create_dsq(VCPU_DSQ, -1);
  if (ret)
    return ret;

  ret = scx_bpf_create_dsq(SYSTEM_DSQ, -1);
  if (ret)
    return ret;

  /* Initialize global coordination list to empty */
  bpf_map_update_elem(&global_coord_list, &key, &empty_list, BPF_ANY);

  return 0;
}

void BPF_STRUCT_OPS(ossim_exit, struct scx_exit_info *ei) {
  UEI_RECORD(uei, ei);
}

SCX_OPS_DEFINE(ossim_ops, .select_cpu = (void *)ossim_select_cpu,
               .enqueue = (void *)ossim_enqueue,
               .dispatch = (void *)ossim_dispatch,
               .running = (void *)ossim_running,
               .stopping = (void *)ossim_stopping,
               .enable = (void *)ossim_enable, .disable = (void *)ossim_disable,
               .init = (void *)ossim_init, .exit = (void *)ossim_exit,
               .name = "ossim");
