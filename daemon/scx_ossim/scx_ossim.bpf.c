#include <scx/common.bpf.h>

#include <bpf/bpf_helpers.h>

#include "interface.h"
#include "vmlinux.h"

char _license[] SEC("license") = "GPL";

#define MAX_SCHED_GRP_SIZE 32

UEI_DEFINE(uei);

/*
 * We maintain two global DSQs:
 * - SYSTEM_DSQ: For system threads (non-vCPU tasks)
 */
#define SYSTEM_DSQ 1

enum sched_node_status {
  /* TODO: Currently there can be race condition where a thread is not enquable
   * but in this state (when a vCPU is registered when blocked). This will be
   * fixed after separating global events and per-vCPU events. See
   * `process_events()` and TODO.md.
   */
  SCHED_NODE_STATUS_ENQUABLE = 1,
  SCHED_NODE_STATUS_ENQUEUED,
  SCHED_NODE_STATUS_NONENQUABLE,
};

struct sched_node {
  pid_t tid;
  u64 simt;
  int status;
};

struct sched_grp {
  struct bpf_spin_lock lock;
  struct sched_node nodes[MAX_SCHED_GRP_SIZE];
  size_t cnt;
};

/*
 * Statistics counters:
 *
 * See `enum scx_ossim_stat_type`
 */
struct {
  __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
  __uint(key_size, sizeof(u32));
  __uint(value_size, sizeof(u64));
  __uint(max_entries, SCX_OSSIM_NUM_STAT);
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
 * vCPU pool helper functions
 */
static bool sched_grp_add(struct sched_grp *grp, pid_t tid, u64 simt) {
  size_t cnt;
  bool ret;

  bpf_spin_lock(&grp->lock);

  cnt = grp->cnt;
  if (cnt >= MAX_SCHED_GRP_SIZE) {
    ret = false;
  } else {
    grp->nodes[cnt].tid = tid;
    grp->nodes[cnt].simt = simt;
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
      for (u32 j = i; j < grp->cnt - 1 && j < MAX_SCHED_GRP_SIZE - 1; j++) {
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
  u64 simt;
  int status;

  for (u32 i = 0; i < grp->cnt && i < MAX_SCHED_GRP_SIZE; i++) {
    status = grp->nodes[i].status;
    if (status == SCHED_NODE_STATUS_NONENQUABLE)
      continue;
    simt = grp->nodes[i].simt;
    if (simt < min_simt) {
      min_simt = simt;
      if (status == SCHED_NODE_STATUS_ENQUABLE) {
        min_node = &grp->nodes[i];
      }
    }
  }
  *node = min_node;
  return min_simt;
}

static struct sched_node *sched_grp_find_nolock(struct sched_grp *grp,
                                                pid_t tid) {
  for (u32 i = 0; i < grp->cnt && i < MAX_SCHED_GRP_SIZE; i++) {
    if (grp->nodes[i].tid == tid)
      return &grp->nodes[i];
  }
  return NULL;
}

static struct sched_grp *get_global_sched_grp(void) {
  u32 key = 0;
  return bpf_map_lookup_elem(&global_sched_grp, &key);
}

/* Synchronization scope helper functions
 *
 * TODO: Consider protecting (global) synchronization scope with RCU. See
 * TODO.md
 */
static void sync_scope_add(struct scx_ossim_sync_scope *scope, pid_t tid) {
  if (!scope || scope->count >= SCX_OSSIM_MAX_SYNC_SCOPE_SIZE)
    return;

  /* Check if already in list */
  u32 count = scope->count;
  if (count > SCX_OSSIM_MAX_SYNC_SCOPE_SIZE)
    count = SCX_OSSIM_MAX_SYNC_SCOPE_SIZE;

  for (u32 j = 0; j < count; j++) {
    if (j >= SCX_OSSIM_MAX_SYNC_SCOPE_SIZE)
      break;
    if (scope->tids[j] == tid)
      return; /* Already exists */
  }

  /* Add to list */
  if (scope->count < SCX_OSSIM_MAX_SYNC_SCOPE_SIZE) {
    scope->tids[scope->count] = tid;
    scope->count++;
  }
}

static void sync_scope_remove(struct scx_ossim_sync_scope *domain, pid_t tid) {
  if (!domain || domain->count > SCX_OSSIM_MAX_SYNC_SCOPE_SIZE)
    return;

  u32 count = domain->count;
  if (count > SCX_OSSIM_MAX_SYNC_SCOPE_SIZE)
    count = SCX_OSSIM_MAX_SYNC_SCOPE_SIZE;

  for (u32 j = 0; j < count; j++) {
    if (j >= SCX_OSSIM_MAX_SYNC_SCOPE_SIZE)
      break;
    if (domain->tids[j] == tid) {
      /* Shift remaining elements */
      u32 shift_count = count - 1;
      if (shift_count > SCX_OSSIM_MAX_SYNC_SCOPE_SIZE - 1)
        shift_count = SCX_OSSIM_MAX_SYNC_SCOPE_SIZE - 1;
      for (u32 k = j; k < shift_count; k++) {
        if (k >= SCX_OSSIM_MAX_SYNC_SCOPE_SIZE - 1 ||
            k + 1 >= SCX_OSSIM_MAX_SYNC_SCOPE_SIZE)
          break;
        domain->tids[k] = domain->tids[k + 1];
      }
      domain->count--;
      break;
    }
  }
}

static u64
sync_scope_get_min_simt_nonblocked(struct scx_ossim_sync_scope *scope,
                                   u64 default_t) {
  u64 min_simt = SCX_OSSIM_SIMT_MAX;
  if (scope) {
    for (u32 i = 0; i < scope->count && i < SCX_OSSIM_MAX_SYNC_SCOPE_SIZE;
         i++) {
      struct scx_ossim_vcpu_metadata *metadata =
          bpf_map_lookup_elem(&vcpu_registry, &scope->tids[i]);
      if (metadata) {
        if (!metadata->blocked) {
          min_simt = metadata->simt < min_simt ? metadata->simt : min_simt;
        }
      }
    }
  }
  return min_simt == SCX_OSSIM_SIMT_MAX ? default_t : min_simt;
}

static u64 global_sync_scope_get_min_simt_update(void) {
  u64 current_min =
      sync_scope_get_min_simt_nonblocked(&global_sync_scope, global_simt);

  if (global_simt < current_min) {
    global_simt = current_min;
  }

  return global_simt;
}

static void unregister_vcpu(pid_t tid) {

  /* [tmp] Remove from the global scheduling group */
  struct sched_grp *grp = get_global_sched_grp();
  if (grp) {
    sched_grp_remove(grp, tid);
  }

  /* Remove from the vCPU registry */
  bpf_map_delete_elem(&vcpu_registry, &tid);

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
// TODO: Let's separate global events and per-vCPU events. See TODO.md
static void process_events(void) {
  struct scx_ossim_event event;
  int ret;

  /* Process up to 8 events per call to avoid hogging CPU */
  for (u32 i = 0; i < 8; i++) {
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
          .simt = global_simt,
          .sync_scope = {.count = 0},
      };

      /* [tmp] add all vCPUs to the global synchronization scope */
      sync_scope_add(&global_sync_scope, event.vcpu_reg.tid);
      /* Register vCPU in the vCPU registry */
      bpf_map_update_elem(&vcpu_registry, &event.vcpu_reg.tid, &metadata,
                          BPF_ANY);
      /* [tmp] add all vCPUs to the global scheduling group */
      struct sched_grp *grp = get_global_sched_grp();
      if (grp)
        sched_grp_add(grp, event.vcpu_reg.tid, metadata.simt);
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
      sync_scope_add(&global_sync_scope, event.global_coord.tid);

    } else if (event.event_type == SCX_OSSIM_EVENT_GLOBAL_COORD_REMOVE) {
      /* Remove TID from global coordination list */
      sync_scope_remove(&global_sync_scope, event.global_coord.tid);

    } else if (event.event_type == SCX_OSSIM_EVENT_GLOBAL_COORD_CLEAR) {
      /* Clear global coordination list */
      global_sync_scope.count = 0;
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
    /* Check if this is a vCPU task */
    tid = p->pid;
    metadata = bpf_map_lookup_elem(&vcpu_registry, &tid);

    if (metadata == NULL) {
      /* System task - can use local DSQ */
      scx_bpf_dsq_insert(p, SCX_DSQ_LOCAL, SCX_SLICE_DFL, 0);
      stat_inc(SCX_OSSIM_STAT_LOCAL_ENQUEUE);
      stat_inc(SCX_OSSIM_STAT_SYSTEM_ENQUEUE); /* count system enqueues */
    }
    /* Let's always enqueue vCPU tasks in ossim_enqueue */
  }

  return cpu;
}

void BPF_STRUCT_OPS(ossim_enqueue, struct task_struct *p, u64 enq_flags) {
  /* Process pending events (registration and coordination) */
  process_events();

  pid_t tid;
  struct scx_ossim_vcpu_metadata *metadata;

  /* Get the thread ID from task_struct */
  tid = p->pid;

  /* Check if this task is a registered vCPU */
  metadata = bpf_map_lookup_elem(&vcpu_registry, &tid);

  if (metadata != NULL) {
    if (metadata->blocked) {
      metadata->simt =
          global_simt > metadata->simt ? global_simt : metadata->simt;
      metadata->blocked = false;
    }
    struct sched_grp *grp = get_global_sched_grp();
    if (grp) {
      bpf_spin_lock(&grp->lock);

      struct sched_node *node = sched_grp_find_nolock(grp, tid);
      if (node) {
        node->status = SCHED_NODE_STATUS_ENQUABLE;
      }
      bpf_spin_unlock(&grp->lock);
    }
  } else {

    u64 vtime = p->scx.dsq_vtime;

    /*
     * Limit the amount of budget that an idling task can accumulate
     * to one slice.
     */
    if (time_before(vtime, system_vtime_now - SCX_SLICE_DFL))
      vtime = system_vtime_now - SCX_SLICE_DFL;

    scx_bpf_dsq_insert_vtime(p, SYSTEM_DSQ, SCX_SLICE_DFL, vtime, enq_flags);
    stat_inc(SCX_OSSIM_STAT_SYSTEM_ENQUEUE); /* system enqueu */
  }
}

void BPF_STRUCT_OPS(ossim_dispatch, s32 cpu, struct task_struct *prev) {
  /* Dispatch vCPUs from the global scheduling group */
  {
    pid_t tid = 0;
    u64 slice = 0;
    struct sched_node *sched_node;
    struct sched_grp *grp = get_global_sched_grp();

    if (grp) {

      u64 limit = global_sync_scope_get_min_simt_update() + simt_epoch;

      bpf_spin_lock(&grp->lock);

      u64 simt = sched_grp_find_min_simt_enquable_nolock(grp, &sched_node);
      if (sched_node != NULL) {
        if (simt <= limit) {
          tid = sched_node->tid;
          sched_node->status = SCHED_NODE_STATUS_ENQUEUED;
          slice = limit - simt;
        }
      }

      bpf_spin_unlock(&grp->lock);

      if (sched_node == NULL) {
        stat_inc(SCX_OSSIM_STAT_VCPU_NOENQUABLE);
      } else if (tid == 0) {
        stat_inc(SCX_OSSIM_STAT_VCPU_ENQUABLE_TOO_EARLY);
      }
    }

    if (tid != 0) {
      struct task_struct *p = bpf_task_from_pid(tid);
      if (p != NULL) {
        stat_inc(2); /* count vCPU enqueues */
        scx_bpf_dsq_insert(p, SCX_DSQ_LOCAL_ON | cpu, slice, 0);
        bpf_task_release(p);
      } else {
        /* Failed to get task - reset status so it can be retried */
        if (grp) {
          bpf_spin_lock(&grp->lock);
          struct sched_node *node = sched_grp_find_nolock(grp, tid);
          if (node) {
            node->status = SCHED_NODE_STATUS_NONENQUABLE;
          }
          bpf_spin_unlock(&grp->lock);
        }
      }
    }
  }

  /* Then dispatch from system DSQ */
  scx_bpf_dsq_move_to_local(SYSTEM_DSQ);
}

void BPF_STRUCT_OPS(ossim_running, struct task_struct *p) {

  pid_t tid = p->pid;
  struct scx_ossim_vcpu_metadata *metadata =
      bpf_map_lookup_elem(&vcpu_registry, &tid);

  if (metadata) {
    p->scx.dsq_vtime = bpf_ktime_get_ns(); /* Use this to track wall time */
  } else {
    /*
     * Global vtime always progresses forward as tasks start executing. The
     * test and update can be performed concurrently from multiple CPUs and
     * thus racy. Any error should be contained and temporary. Let's just
     * live with it.
     */
    if (time_before(system_vtime_now, p->scx.dsq_vtime))
      system_vtime_now = p->scx.dsq_vtime;
  }
}

void BPF_STRUCT_OPS(ossim_stopping, struct task_struct *p, bool runnable) {

  pid_t tid = p->pid;
  struct scx_ossim_vcpu_metadata *metadata =
      bpf_map_lookup_elem(&vcpu_registry, &tid);

  if (metadata) {
    /* Update simulated time */
    u64 simt_delta = bpf_ktime_get_ns() - p->scx.dsq_vtime;
    metadata->simt += simt_delta;
    metadata->blocked = !runnable;

    struct sched_grp *grp = get_global_sched_grp();
    if (grp) {
      bpf_spin_lock(&grp->lock);

      struct sched_node *node = sched_grp_find_nolock(grp, tid);
      if (node) {
        node->simt = metadata->simt;
        node->status = runnable ? SCHED_NODE_STATUS_ENQUABLE
                                : SCHED_NODE_STATUS_NONENQUABLE;
      }

      bpf_spin_unlock(&grp->lock);
    }
  } else {
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
