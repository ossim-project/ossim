#include <scx/common.bpf.h>

#include <bpf/bpf_helpers.h>

#include "interface.h"
#include "vmlinux.h"

char _license[] SEC("license") = "GPL";

#define MAX_SCHED_GRP_SIZE 64

UEI_DEFINE(uei);

/*
 * We maintain two global DSQs:
 * - SYSTEM_DSQ: For system threads (non-vCPU tasks)
 */
#define SYSTEM_DSQ 1

enum sched_node_status {
  /* TODO: Currently there can be race condition where a thread is not enquable
   * but in this state. This will be fixed after separating global events and
   * per-vCPU events. Also See `process_events()`.
   */
  SCHED_NODE_STATUS_ENQUABLE = 1,
  SCHED_NODE_STATUS_ENQUEUED,
  SCHED_NODE_STATUS_NONENQUABLE,
};

struct sched_node {
  pid_t tid;
  int status;
};

struct sched_grp {
  struct bpf_spin_lock lock;
  struct sched_node nodes[MAX_SCHED_GRP_SIZE];
  size_t cnt;
};

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

struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(max_entries, SCX_OSSIM_MAX_VCPUS);
  __type(key, pid_t);
  __type(value, u64);
} simt_tbl SEC(".maps");

/* Global scheduling group (single-element array to hold spinlock) */
struct {
  __uint(type, BPF_MAP_TYPE_ARRAY);
  __uint(max_entries, 1);
  __type(key, u32);
  __type(value, struct sched_grp);
} global_sched_grp SEC(".maps");

/* vtime for system thread scheduling (see scx_simple) */
static u64 system_vtime_now;

/* Simulated time epoch */
volatile u64 simt_epoch;

/* This stores a lower bound of the simulated time of all vCPUs  */
volatile u64 global_simt;

struct scx_ossim_sync_scope global_sync_scope;

static void stat_inc(u32 idx) {
  u64 *cnt_p = bpf_map_lookup_elem(&stats, &idx);
  if (cnt_p)
    (*cnt_p)++;
}

/*
 * Simulated time (simt) helper functions
 */
static void simt_tbl_update(pid_t tid, u64 new_simt) {
  bpf_map_update_elem(&simt_tbl, &tid, &new_simt, BPF_ANY);
}

static u64 simt_tbl_get(pid_t tid, u64 default_simt) {
  u64 *current_simt = bpf_map_lookup_elem(&simt_tbl, &tid);
  if (current_simt) {
    return *current_simt;
  }
  return default_simt;
}

static void simt_tbl_remove(pid_t tid) { bpf_map_delete_elem(&simt_tbl, &tid); }

/*
 * vCPU pool helper functions
 */
static bool sched_grp_add(struct sched_grp *grp, pid_t tid) {
  size_t cnt;
  bool ret;

  bpf_spin_lock(&grp->lock);

  cnt = grp->cnt;
  if (cnt >= MAX_SCHED_GRP_SIZE) {
    ret = false;
  } else {
    grp->nodes[cnt].tid = tid;
    grp->nodes[cnt].status = SCHED_NODE_STATUS_ENQUABLE;
    grp->cnt++;
    ret = true;
  }

  bpf_spin_unlock(&grp->lock);
  return ret;
}

static bool sched_grp_remove(struct sched_grp *grp, pid_t tid) {
  bool ret = false;

  bpf_spin_lock(&grp->lock);

  for (u32 i = 0; i < grp->cnt && i < MAX_SCHED_GRP_SIZE; i++) {
    if (grp->nodes[i].tid == tid) {
      for (u32 j = i; j < grp->cnt - 1 && j < MAX_SCHED_GRP_SIZE; j++) {
        grp->nodes[j] = grp->nodes[j + 1];
      }
      grp->cnt--;
      ret = true;
      break;
    }
  }

  bpf_spin_unlock(&grp->lock);
  return ret;
}

static u64 sched_grp_find_min_simt_enquable_nolock(struct sched_grp *grp,
                                                   struct sched_node **node) {
  u64 min_simt = SCX_OSSIM_SIMT_MAX;
  struct sched_node *min_node = NULL;
  pid_t tid;
  u64 simt;

  for (u32 i = 0; i < grp->cnt && i < MAX_SCHED_GRP_SIZE; i++) {
    if (grp->nodes[i].status != SCHED_NODE_STATUS_ENQUABLE)
      continue;
    tid = grp->nodes[i].tid;
    simt = simt_tbl_get(tid, SCX_OSSIM_SIMT_MAX);
    if (simt < min_simt) {
      min_simt = simt;
      min_node = &grp->nodes[i];
    }
  }
  *node = min_node;
  return min_simt;
}

/* Synchronization scope helper functions
 *
 * TODO: Consider whether we need to protect each synchronization scope
 * with a spinlock.
 */
static void sync_scope_add(struct scx_ossim_sync_scope *scope, pid_t tid) {
  if (!scope || scope->count >= SCX_OSSIM_MAX_COORD_VCPUS)
    return;

  /* Check if already in list */
  u32 count = scope->count;
  if (count > SCX_OSSIM_MAX_COORD_VCPUS)
    count = SCX_OSSIM_MAX_COORD_VCPUS;

  for (u32 j = 0; j < count; j++) {
    if (j >= SCX_OSSIM_MAX_COORD_VCPUS)
      break;
    if (scope->tids[j] == tid)
      return; /* Already exists */
  }

  /* Add to list */
  if (scope->count < SCX_OSSIM_MAX_COORD_VCPUS) {
    scope->tids[scope->count] = tid;
    scope->count++;
  }
}

static void sync_scope_remove(struct scx_ossim_sync_scope *domain, pid_t tid) {
  if (!domain || domain->count > SCX_OSSIM_MAX_COORD_VCPUS)
    return;

  u32 count = domain->count;
  if (count > SCX_OSSIM_MAX_COORD_VCPUS)
    count = SCX_OSSIM_MAX_COORD_VCPUS;

  for (u32 j = 0; j < count; j++) {
    if (j >= SCX_OSSIM_MAX_COORD_VCPUS)
      break;
    if (domain->tids[j] == tid) {
      /* Shift remaining elements */
      u32 shift_count = count - 1;
      if (shift_count > SCX_OSSIM_MAX_COORD_VCPUS - 1)
        shift_count = SCX_OSSIM_MAX_COORD_VCPUS - 1;
      for (u32 k = j; k < shift_count; k++) {
        if (k >= SCX_OSSIM_MAX_COORD_VCPUS - 1 ||
            k + 1 >= SCX_OSSIM_MAX_COORD_VCPUS)
          break;
        domain->tids[k] = domain->tids[k + 1];
      }
      domain->count--;
      break;
    }
  }
}

static u64 sync_scope_get_min_simt(struct scx_ossim_sync_scope *scope,
                                   u64 default_t) {
  u64 min_simt = SCX_OSSIM_SIMT_MAX;
  if (scope) {
    for (u32 i = 0; i < scope->count && i < SCX_OSSIM_MAX_COORD_VCPUS; i++) {
      u64 simt = simt_tbl_get(scope->tids[i], SCX_OSSIM_SIMT_MAX);
      min_simt = simt < min_simt ? simt : min_simt;
    }
  }
  return min_simt == SCX_OSSIM_SIMT_MAX ? default_t : min_simt;
}

/* TODO: We may want to protect this with a spinlock */
static u64 global_sync_scope_get_min_simt_update(void) {
  u64 current_min = sync_scope_get_min_simt(&global_sync_scope, 0);

  if (global_simt < current_min) {
    global_simt = current_min;
  }

  return global_simt;
}

static void unregister_vcpu(pid_t tid) {

  /* [tmp] Remove from the global scheduling group */
  u32 grp_key = 0;
  struct sched_grp *grp = bpf_map_lookup_elem(&global_sched_grp, &grp_key);
  if (grp) {
    sched_grp_remove(grp, tid);
  }

  /* Remove from the vCPU registry */
  bpf_map_delete_elem(&vcpu_registry, &tid);

  /* Remove from the simulated time map */
  simt_tbl_remove(tid);

  /* Remove from the global sync scope */
  sync_scope_remove(&global_sync_scope, tid);

  /*
   * Note: We don't clean up per-vCPU coordination lists here because
   * iterating through all registered vCPUs would be too expensive in BPF.
   * Per-vCPU coordination list entries pointing to this TID will naturally
   * fail when trying to coordinate with a non-existent vCPU.
   */
}

/* Process pending events from the unified queue */

// TODO: Let's separate global events and per-vCPU events
//   - For global events, we process it in the daemon threas's context.
//   - For per-vCPU events, we process it in the vCPU's context.
static void process_events(void) {
  struct scx_ossim_event event;
  int ret;

  /* Process up to 32 events per call to avoid hogging CPU */
  for (u32 i = 0; i < 32; i++) {
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
          .sync_scope = {.count = 0},
      };

      /* Initialze simulated time with the global simulated time */
      simt_tbl_update(event.vcpu_reg.tid, global_simt);
      /* [tmp] add all vCPUs to the global synchronization scope */
      sync_scope_add(&global_sync_scope, event.vcpu_reg.tid);
      /* Register vCPU in the vCPU registry */
      bpf_map_update_elem(&vcpu_registry, &event.vcpu_reg.tid, &metadata,
                          BPF_ANY);
      /* [tmp] add all vCPUs to the global scheduling group */
      u32 grp_key = 0;
      struct sched_grp *grp = bpf_map_lookup_elem(&global_sched_grp, &grp_key);
      if (grp)
        sched_grp_add(grp, event.vcpu_reg.tid);
    } else if (event.event_type == SCX_OSSIM_EVENT_VCPU_UNREGISTER) {
      unregister_vcpu(event.vcpu_unreg.tid);
    } else if (event.event_type == SCX_OSSIM_EVENT_COORD_ADD) {
      /* Add vCPU to another vCPU's coordination list */
      struct scx_ossim_vcpu_metadata *metadata;
      metadata = bpf_map_lookup_elem(&vcpu_registry, &event.coord_op.vcpu_tid);
      if (metadata) {
        sync_scope_add(&metadata->sync_scope, event.coord_op.related_tid);
      }

    } else if (event.event_type == SCX_OSSIM_EVENT_COORD_REMOVE) {
      /* Remove vCPU from coordination list */
      struct scx_ossim_vcpu_metadata *metadata;
      metadata = bpf_map_lookup_elem(&vcpu_registry, &event.coord_op.vcpu_tid);
      if (metadata) {
        sync_scope_remove(&metadata->sync_scope, event.coord_op.related_tid);
      }

    } else if (event.event_type == SCX_OSSIM_EVENT_COORD_CLEAR) {
      /* Clear coordination list */
      struct scx_ossim_vcpu_metadata *metadata;
      metadata = bpf_map_lookup_elem(&vcpu_registry, &event.coord_op.vcpu_tid);
      if (metadata) {
        metadata->sync_scope.count = 0;
      }

    } else if (event.event_type == SCX_OSSIM_EVENT_GLOBAL_COORD_ADD) {
      /* Add TID to global coordination list */
      u32 key = 0;
      struct scx_ossim_sync_scope *global_list;
      global_list = bpf_map_lookup_elem(&global_sync_scope, &key);
      if (global_list) {
        sync_scope_add(global_list, event.global_coord.tid);
      }

    } else if (event.event_type == SCX_OSSIM_EVENT_GLOBAL_COORD_REMOVE) {
      /* Remove TID from global coordination list */
      u32 key = 0;
      struct scx_ossim_sync_scope *global_list;
      global_list = bpf_map_lookup_elem(&global_sync_scope, &key);
      if (global_list) {
        sync_scope_remove(global_list, event.global_coord.tid);
      }

    } else if (event.event_type == SCX_OSSIM_EVENT_GLOBAL_COORD_CLEAR) {
      /* Clear global coordination list */
      u32 key = 0;
      struct scx_ossim_sync_scope *global_list;
      global_list = bpf_map_lookup_elem(&global_sync_scope, &key);
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

    if (metadata == NULL) {
      /* System task - can use local DSQ */
      scx_bpf_dsq_insert(p, SCX_DSQ_LOCAL, SCX_SLICE_DFL, 0);
      stat_inc(3); /* count system enqueues */
    }
    /* Let's always enqueue vCPU tasks in ossim_enqueue */
  }

  return cpu;
}

void BPF_STRUCT_OPS(ossim_enqueue, struct task_struct *p, u64 enq_flags) {
  pid_t tid;
  void *metadata;

  stat_inc(1); /* count global queueing */

  /* Get the thread ID from task_struct */
  tid = p->pid;

  /* Check if this task is a registered vCPU */
  metadata = bpf_map_lookup_elem(&vcpu_registry, &tid);

  if (metadata == NULL) {
    stat_inc(3); /* system enqueu */

    u64 vtime = p->scx.dsq_vtime;

    /*
     * Limit the amount of budget that an idling task can accumulate
     * to one slice.
     */
    if (time_before(vtime, system_vtime_now - SCX_SLICE_DFL))
      vtime = system_vtime_now - SCX_SLICE_DFL;

    scx_bpf_dsq_insert_vtime(p, SYSTEM_DSQ, SCX_SLICE_DFL, vtime, enq_flags);
  }
}

void BPF_STRUCT_OPS(ossim_dispatch, s32 cpu, struct task_struct *prev) {
  /* Process pending events (registration and coordination) */
  process_events();

  /* Dispatch vCPUs from the global scheduling group */
  {
    pid_t tid = 0;
    u64 slice = 0;
    struct sched_node *sched_node;
    u32 grp_key = 0;
    struct sched_grp *grp = bpf_map_lookup_elem(&global_sched_grp, &grp_key);

    if (grp) {
      bpf_spin_lock(&grp->lock);

      u64 simt = sched_grp_find_min_simt_enquable_nolock(grp, &sched_node);
      if (sched_node != NULL) {
        u64 limit = sync_scope_get_min_simt(&global_sync_scope, 0) + simt_epoch;
        if (simt <= limit) {
          tid = sched_node->tid;
          sched_node->status = SCHED_NODE_STATUS_ENQUEUED;
          slice = limit - simt;
        }
      }

      bpf_spin_unlock(&grp->lock);
    }

    if (tid != 0) {
      struct task_struct *p = bpf_task_from_pid(tid);
      if (p != NULL) {
        stat_inc(2); /* count vCPU enqueues */
        scx_bpf_dsq_insert(p, SCX_DSQ_LOCAL_ON | cpu, slice, 0);
      }
      bpf_task_release(p);
    }
  }

  /* Then dispatch from system DSQ */
  scx_bpf_dsq_move_to_local(SYSTEM_DSQ);
}

/* TODO: simt bookkeeping */
void BPF_STRUCT_OPS(ossim_running, struct task_struct *p) {
  /*
   * Global vtime always progresses forward as tasks start executing. The
   * test and update can be performed concurrently from multiple CPUs and
   * thus racy. Any error should be contained and temporary. Let's just
   * live with it.
   */
  if (time_before(system_vtime_now, p->scx.dsq_vtime))
    system_vtime_now = p->scx.dsq_vtime;
}

/* TODO: simt bookkeeping */
void BPF_STRUCT_OPS(ossim_stopping, struct task_struct *p, bool runnable) {

  pid_t tid = p->pid;
  struct scx_ossim_vcpu_metadata *metadata =
      bpf_map_lookup_elem(&vcpu_registry, &tid);

  if (metadata) {
    /* Update simulated time */
  }
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
  p->scx.dsq_vtime = system_vtime_now;
}

void BPF_STRUCT_OPS(ossim_disable, struct task_struct *p) {
  pid_t tid = p->pid;
  struct scx_ossim_vcpu_metadata *metadata;

  /* Check if this task is a registered vCPU */
  metadata = bpf_map_lookup_elem(&vcpu_registry, &tid);
  if (metadata)
    unregister_vcpu(tid);
}

s32 BPF_STRUCT_OPS_SLEEPABLE(ossim_init) {
  s32 ret;

  ret = scx_bpf_create_dsq(SYSTEM_DSQ, -1);
  if (ret)
    return ret;

  /* BPF array maps and BSS variables are auto-zeroed on init */
  /* global_simt, simt_epoch, global_sync_scope start at 0 */
  /* global_sched_grp map entry 0 is also auto-zeroed */

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
