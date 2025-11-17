/* SPDX-License-Identifier: GPL-2.0 */
/*
 * A simple scheduler.
 *
 * By default, it operates as a simple global weighted vtime scheduler and can
 * be switched to FIFO scheduling. It also demonstrates the following niceties.
 *
 * - Statistics tracking how many tasks are queued to local and global dsq's.
 * - Termination notification for userspace.
 *
 * While very simple, this scheduler should work reasonably well on CPUs with a
 * uniform L3 cache topology. While preemption is not implemented, the fact that
 * the scheduling queue is shared across all CPUs means that whatever is at the
 * front of the queue is likely to be executed fairly quickly given enough
 * number of CPUs. The FIFO scheduling mode may be beneficial to some workloads
 * but comes with the usual problems with FIFO scheduling where saturating
 * threads can easily drown out interactive ones.
 *
 * Copyright (c) 2022 Meta Platforms, Inc. and affiliates.
 * Copyright (c) 2022 Tejun Heo <tj@kernel.org>
 * Copyright (c) 2022 David Vernet <dvernet@meta.com>
 */
#include <scx/common.bpf.h>

#include "interface.h"

char _license[] SEC("license") = "GPL";

const volatile bool fifo_sched;

static u64 vtime_now;
UEI_DEFINE(uei);

/*
 * Built-in DSQs such as SCX_DSQ_GLOBAL cannot be used as priority queues
 * (meaning, cannot be dispatched to with scx_bpf_dsq_insert_vtime()). We
 * therefore create a separate DSQ with ID 0 that we dispatch to and consume
 * from. If scx_simple only supported global FIFO scheduling, then we could just
 * use SCX_DSQ_GLOBAL.
 */
#define SHARED_DSQ 0

struct {
  __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
  __uint(key_size, sizeof(u32));
  __uint(value_size, sizeof(u64));
  __uint(max_entries, 2); /* [local, global] */
} stats SEC(".maps");

/* FIFO queue for pending vCPU registration events */
struct {
  __uint(type, BPF_MAP_TYPE_QUEUE);
  __uint(max_entries, OSSIM_MAX_PENDING_EVENTS);
  __type(value, struct ossim_vcpu_event);
} vcpu_event_queue SEC(".maps");

/* Hash map storing registered vCPUs (key: TID, value: metadata) */
struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(max_entries, OSSIM_MAX_VCPUS);
  __type(key, pid_t);
  __type(value, struct ossim_bpf_vcpu_metadata);
} vcpu_registry SEC(".maps");

static void stat_inc(u32 idx) {
  u64 *cnt_p = bpf_map_lookup_elem(&stats, &idx);
  if (cnt_p)
    (*cnt_p)++;
}

/* Process pending vCPU registration events from the queue */
static void process_vcpu_events(void) {
  struct ossim_vcpu_event event;
  int ret;

/* Process up to 32 events per call to avoid hogging CPU */
#pragma unroll
  for (int i = 0; i < 32; i++) {
    ret = bpf_map_pop_elem(&vcpu_event_queue, &event);
    if (ret != 0) {
      /* Queue is empty */
      break;
    }

    if (event.event_type == OSSIM_EVENT_VCPU_REGISTER) {
      struct ossim_bpf_vcpu_metadata metadata = {
          .tid = event.tid,
          .vm_id = event.vm_id,
          .vcpu_id = event.vcpu_id,
          .timestamp = bpf_ktime_get_ns(),
      };

      /* Register the vCPU in the registry */
      bpf_map_update_elem(&vcpu_registry, &event.tid, &metadata, BPF_ANY);

    } else if (event.event_type == OSSIM_EVENT_VCPU_UNREGISTER) {
      /* Unregister the vCPU from the registry */
      bpf_map_delete_elem(&vcpu_registry, &event.tid);
    }
  }
}

s32 BPF_STRUCT_OPS(ossim_select_cpu, struct task_struct *p, s32 prev_cpu,
                   u64 wake_flags) {
  bool is_idle = false;
  s32 cpu;

  cpu = scx_bpf_select_cpu_dfl(p, prev_cpu, wake_flags, &is_idle);
  if (is_idle) {
    stat_inc(0); /* count local queueing */
    scx_bpf_dsq_insert(p, SCX_DSQ_LOCAL, SCX_SLICE_DFL, 0);
  }

  return cpu;
}

void BPF_STRUCT_OPS(ossim_enqueue, struct task_struct *p, u64 enq_flags) {
  stat_inc(1); /* count global queueing */

  if (fifo_sched) {
    scx_bpf_dsq_insert(p, SHARED_DSQ, SCX_SLICE_DFL, enq_flags);
  } else {
    u64 vtime = p->scx.dsq_vtime;

    /*
     * Limit the amount of budget that an idling task can accumulate
     * to one slice.
     */
    if (time_before(vtime, vtime_now - SCX_SLICE_DFL))
      vtime = vtime_now - SCX_SLICE_DFL;

    scx_bpf_dsq_insert_vtime(p, SHARED_DSQ, SCX_SLICE_DFL, vtime, enq_flags);
  }
}

void BPF_STRUCT_OPS(ossim_dispatch, s32 cpu, struct task_struct *prev) {
  /* Process pending vCPU registration events */
  process_vcpu_events();

  scx_bpf_dsq_move_to_local(SHARED_DSQ);
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
               .enable = (void *)ossim_enable, .init = (void *)ossim_init,
               .exit = (void *)ossim_exit, .name = "ossim");
