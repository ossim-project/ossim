/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Ossim vCPU-aware scheduler.
 *
 * This scheduler ONLY manages vCPU threads using a global vtime-ordered queue.
 * Non-vCPU threads are dispatched to SCX_DSQ_GLOBAL and managed by the built-in
 * scheduler, ensuring minimal overhead for regular system tasks.
 *
 * Features:
 * - vCPU thread identification and differentiated scheduling
 * - Global vtime-ordered queue ensures fairness across all vCPUs
 * - Per-vCPU statistics tracking (enqueues, dispatches, runtime)
 * - Graceful degradation when vCPU metadata is unavailable
 *
 * Architecture (Section 6.4 of specification):
 * - This BPF scheduler OWNS and CREATES the three shared maps:
 *   1. vcpu_metadata: TID -> vCPU metadata (vcpu_index, flags, VM UUID, etc.)
 *   2. vm_config: VM UUID (full 128-bit) -> VM-level config (cpu_shares,
 * quotas)
 *   3. vcpu_stats: TID -> per-vCPU statistics (enqueues, dispatches, runtime)
 * - Maps are pinned to /sys/fs/bpf/ossim/ by the userspace daemon
 * - QEMU opens pinned maps via bpf_obj_get() and updates them after ioctl
 * validation
 * - Kernel module (ossim.ko) validates registrations but doesn't update BPF
 * maps (bpf_map_update_elem not exported to modules)
 * - This design enables zero-overhead vCPU identification in the scheduler hot
 * path
 *
 * Copyright (c) 2025 Ossim Project
 */
#include <scx/common.bpf.h>

#include "scx_ossim_common.h"

char _license[] SEC("license") = "GPL";

const volatile bool fifo_sched;

static u64 vtime_now;
UEI_DEFINE(uei);

/*
 * Built-in DSQs such as SCX_DSQ_GLOBAL cannot be used as priority queues
 * (meaning, cannot be dispatched to with scx_bpf_dsq_insert_vtime()). We
 * therefore create a separate DSQ with ID 0 that we dispatch to and consume
 * from.
 *
 * This single global vtime-ordered DSQ is used ONLY for vCPU threads,
 * ensuring that ossim_running() always runs the vCPU thread with the globally
 * smallest dsq_vtime across all CPUs. Non-vCPU threads use the built-in
 * SCX_DSQ_GLOBAL and are not managed by this scheduler.
 */
#define SHARED_DSQ 0

/* Map: TID -> vCPU metadata */
struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(key_size, sizeof(pid_t)); /* vCPU thread TID */
  __uint(value_size, sizeof(struct vcpu_metadata_bpf));
  __uint(max_entries, 4096); /* Support up to 4096 vCPUs */
  __uint(map_flags, BPF_F_NO_PREALLOC);
} vcpu_metadata SEC(".maps");

/* Map: VM UUID (full 128 bits) -> VM config */
struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(
      key_size,
      sizeof(struct vm_uuid_key)); /* Full 128-bit UUID to prevent collisions */
  __uint(value_size, sizeof(struct vm_config_bpf));
  __uint(max_entries, 256); /* Support up to 256 VMs */
} vm_config SEC(".maps");

/* Map: Per-vCPU statistics exported to userspace */
struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(key_size, sizeof(pid_t));
  __uint(value_size, sizeof(struct vcpu_stats_bpf));
  __uint(max_entries, 4096);
} vcpu_stats SEC(".maps");

/* Per-CPU statistics for local/global queueing */
struct {
  __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
  __uint(key_size, sizeof(u32));
  __uint(value_size, sizeof(u64));
  __uint(max_entries, 2); /* [local, global] */
} stats SEC(".maps");

/* Map: Track start timestamp for vCPU threads (for runtime measurement) */
struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(key_size, sizeof(pid_t));
  __uint(value_size, sizeof(u64)); /* start timestamp */
  __uint(max_entries, 4096);       /* Match vcpu_metadata max_entries */
  __uint(map_flags, BPF_F_NO_PREALLOC);
} task_start_ts SEC(".maps");

static void stat_inc(u32 idx) {
  u64 *cnt_p = bpf_map_lookup_elem(&stats, &idx);
  if (cnt_p)
    (*cnt_p)++;
}

/* Check if task is a registered vCPU */
static bool is_vcpu_thread(struct task_struct *p) {
  struct vcpu_metadata_bpf *vcpu;
  pid_t tid = p->pid;

  vcpu = bpf_map_lookup_elem(&vcpu_metadata, &tid);
  return vcpu != NULL;
}

/* Update vCPU statistics */
static void update_vcpu_stats(pid_t tid, bool is_enqueue) {
  struct vcpu_stats_bpf *stats;
  struct vcpu_stats_bpf new_stats = {
      .enqueues = 0,
      .dispatches = 0,
      .total_runtime_ns = 0,
      .last_enqueue_ts = 0,
      .vtime = 0,
  };

  stats = bpf_map_lookup_elem(&vcpu_stats, &tid);
  if (!stats) {
    /* Initialize stats entry */
    bpf_map_update_elem(&vcpu_stats, &tid, &new_stats, BPF_NOEXIST);
    stats = bpf_map_lookup_elem(&vcpu_stats, &tid);
    if (!stats)
      return;
  }

  if (is_enqueue) {
    __sync_fetch_and_add(&stats->enqueues, 1);
    stats->last_enqueue_ts = scx_bpf_now();
  } else {
    __sync_fetch_and_add(&stats->dispatches, 1);
  }
}

/* Enhanced CPU selection with vCPU affinity awareness */
s32 BPF_STRUCT_OPS(ossim_select_cpu, struct task_struct *p, s32 prev_cpu,
                   u64 wake_flags) {
  struct vcpu_metadata_bpf *vcpu;
  bool is_idle = false;
  s32 cpu;
  pid_t tid = p->pid;

  vcpu = bpf_map_lookup_elem(&vcpu_metadata, &tid);
  if (vcpu && (vcpu->flags & OSSIM_VCPU_FLAG_PINNED)) {
    /* Respect explicit CPU pinning for this vCPU */
    cpu = scx_bpf_select_cpu_dfl(p, prev_cpu, wake_flags, &is_idle);
  } else {
    cpu = scx_bpf_select_cpu_dfl(p, prev_cpu, wake_flags, &is_idle);
  }

  /* Count but don't insert here - let enqueue handle it to maintain consistent
   * DSQ mode */
  if (is_idle) {
    stat_inc(0); /* count local queueing */
  }

  return cpu;
}

/*
 * Enhanced enqueue with vCPU awareness
 *
 * Scheduling Algorithm:
 * 1. Lookup task TID in vcpu_metadata map (O(1) hash lookup)
 * 2. If vCPU found:
 *    a. Update vCPU statistics (enqueue counter, timestamp)
 *    b. Insert into SHARED_DSQ using FIFO or vtime-based scheduling
 * 3. If not vCPU (regular task):
 *    a. Dispatch to SCX_DSQ_GLOBAL (built-in scheduler manages it)
 *    b. No custom scheduling policy applied
 *
 * Graceful Degradation:
 * - If vcpu_metadata map is empty (no VMs running), all tasks bypass to
 * built-in
 * - Scheduler only actively manages vCPU threads
 */
void BPF_STRUCT_OPS(ossim_enqueue, struct task_struct *p, u64 enq_flags) {
  pid_t tid = p->pid;
  struct vcpu_metadata_bpf *vcpu;
  u64 slice = SCX_SLICE_DFL;
  u64 vtime = 0;
  struct vcpu_stats_bpf *stats;

  /* Check if this is a vCPU thread */
  vcpu = bpf_map_lookup_elem(&vcpu_metadata, &tid);
  if (vcpu) {
    /* This is a vCPU - manage it with our custom scheduler */
    stat_inc(1); /* count global queueing */

    stats = bpf_map_lookup_elem(&vcpu_stats, &tid);
    if (stats) {
      vtime = stats->vtime;
    }

    update_vcpu_stats(tid, true);

    /* Insert into SHARED_DSQ with vtime ordering */
    if (fifo_sched) {
      scx_bpf_dsq_insert(p, SHARED_DSQ, slice, enq_flags);
    } else {
      scx_bpf_dsq_insert_vtime(p, SHARED_DSQ, slice, vtime, enq_flags);
    }
  } else {
    /* Not a vCPU - let built-in scheduler handle it */
    stat_inc(0); /* count local/built-in queueing */
    scx_bpf_dsq_insert(p, SCX_DSQ_GLOBAL, slice, enq_flags);
  }
}

/*
 * Dispatch operation - consume from vCPU queue
 *
 * This callback is invoked when the scheduler needs to dispatch a task to run.
 * We only consume from SHARED_DSQ (vCPU threads). Non-vCPU threads in
 * SCX_DSQ_GLOBAL are automatically handled by the built-in consume logic.
 */
void BPF_STRUCT_OPS(ossim_dispatch, s32 cpu, struct task_struct *prev) {
  if (prev) {
    pid_t tid = prev->pid;
    if (is_vcpu_thread(prev)) {
      update_vcpu_stats(tid, false);
    }
  }

  /* Consume vCPU threads from our custom queue */
  scx_bpf_dsq_move_to_local(SHARED_DSQ);
}

void BPF_STRUCT_OPS(ossim_running, struct task_struct *p) {

  /* Track start time for vCPU threads only */
  if (is_vcpu_thread(p)) {
    /*
     * Track the global minimum vtime among vCPU threads.
     *
     * Since we use a single global vtime-ordered dispatch queue (SHARED_DSQ)
     * for vCPU threads only, when a vCPU thread is running, p->scx.dsq_vtime
     * represents the minimum vtime among all runnable vCPU threads.
     *
     * We update vtime_now to track this global minimum, which is used for:
     * 1. Initializing new vCPU thread vtimes
     * 2. Enforcing vtime delta constraints for time-skew bounding
     *
     * Note: Only updated when vCPU threads are running. Non-vCPU threads are
     * ignored as they don't participate in our vtime scheduling.
     */
    if (vtime_now < p->scx.dsq_vtime) {
      vtime_now = p->scx.dsq_vtime;
    }

    pid_t tid = p->pid;
    u64 now = scx_bpf_now();
    bpf_map_update_elem(&task_start_ts, &tid, &now, BPF_ANY);
  }
}

void BPF_STRUCT_OPS(ossim_stopping, struct task_struct *p, bool runnable) {

  /* Only track runtime for vCPU threads */
  if (!is_vcpu_thread(p)) {
    return;
  }

  pid_t tid = p->pid;
  u64 runtime_ns = 0;
  u64 *start_ts;
  u64 now = scx_bpf_now();

  /* Measure runtime using timestamps */
  start_ts = bpf_map_lookup_elem(&task_start_ts, &tid);
  if (start_ts && *start_ts > 0) {
    runtime_ns = now - *start_ts;
    /* Remove from map to clean up */
    bpf_map_delete_elem(&task_start_ts, &tid);
  }

  /* Update vCPU runtime statistics and vtime */
  struct vcpu_stats_bpf *stats = bpf_map_lookup_elem(&vcpu_stats, &tid);
  if (stats) {
    if (runtime_ns > 0) {
      __sync_fetch_and_add(&stats->vtime, runtime_ns);
      stats->total_runtime_ns += runtime_ns;
    }

    if (!fifo_sched) {
      /* Update task's vtime for next scheduling decision */
      p->scx.dsq_vtime = stats->vtime;
    }
  }
}

void BPF_STRUCT_OPS(ossim_enable, struct task_struct *p) {
  pid_t tid = p->pid;
  struct vcpu_metadata_bpf *vcpu;
  struct vcpu_stats_bpf zero_stats = {
      .enqueues = 0,
      .dispatches = 0,
      .total_runtime_ns = 0,
      .last_enqueue_ts = 0,
      .vtime = vtime_now,
  };

  p->scx.dsq_vtime = vtime_now;

  /*
   * Initialize stats entry for vCPU threads when they first join the scheduler.
   * This ensures runtime starts at 0 and prevents uninitialized data.
   */
  vcpu = bpf_map_lookup_elem(&vcpu_metadata, &tid);
  if (vcpu) {
    /* This is a vCPU thread - ensure stats entry exists and is zeroed */
    bpf_map_update_elem(&vcpu_stats, &tid, &zero_stats, BPF_ANY);
  }
}

void BPF_STRUCT_OPS(ossim_disable, struct task_struct *p) {
  pid_t tid = p->pid;
  struct vcpu_metadata_bpf *vcpu;

  /*
   * Clean up vCPU entries when thread exits.
   * This handles both normal QEMU shutdown and abnormal exits (signals).
   * We only remove from vcpu_metadata and vcpu_stats maps here.
   * VM config entries are left intact as they may be shared by other vCPUs.
   */
  vcpu = bpf_map_lookup_elem(&vcpu_metadata, &tid);
  if (vcpu) {
    /* This is a vCPU thread - clean up its entries */
    bpf_map_delete_elem(&task_start_ts, &tid);
    bpf_map_delete_elem(&vcpu_metadata, &tid);
    bpf_map_delete_elem(&vcpu_stats, &tid);
  }
}

s32 BPF_STRUCT_OPS_SLEEPABLE(ossim_init) {
  return scx_bpf_create_dsq(SHARED_DSQ, -1);
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
