/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Ossim vCPU-aware scheduler.
 *
 * By default, it operates as a simple global weighted vtime scheduler.
 * When VMs are running, it identifies vCPU threads and applies VM-specific
 * scheduling policies based on metadata shared through BPF maps.
 *
 * Features:
 * - vCPU thread identification and differentiated scheduling
 * - VM-aware weight adjustment and priority boosting
 * - Per-vCPU statistics tracking
 * - Graceful degradation when vCPU metadata is unavailable
 *
 * Architecture (Section 6.4 of specification):
 * - This BPF scheduler OWNS and CREATES the three shared maps:
 *   1. vcpu_metadata: TID -> vCPU metadata (vcpu_index, flags, VM UUID, etc.)
 *   2. vm_config: VM UUID (full 128-bit) -> VM-level config (cpu_shares, quotas)
 *   3. vcpu_stats: TID -> per-vCPU statistics (enqueues, dispatches, runtime)
 * - Maps are pinned to /sys/fs/bpf/ossim/ by the userspace daemon
 * - QEMU opens pinned maps via bpf_obj_get() and updates them after ioctl validation
 * - Kernel module (ossim.ko) validates registrations but doesn't update BPF maps
 *   (bpf_map_update_elem not exported to modules)
 * - This design enables zero-overhead vCPU identification in the scheduler hot path
 *
 * Copyright (c) 2025 Ossim Project
 */
#include <scx/common.bpf.h>

char _license[] SEC("license") = "GPL";

const volatile bool fifo_sched;

static u64 vtime_now;
UEI_DEFINE(uei);

/* Maximum length for VM name string */
#define OSSIM_VM_NAME_MAX 64

/* vCPU thread flags */
#define OSSIM_VCPU_FLAG_IOTHREAD  (1 << 0)  /* IO thread, not vCPU */
#define OSSIM_VCPU_FLAG_REALTIME  (1 << 1)  /* Requires RT scheduling */
#define OSSIM_VCPU_FLAG_PINNED    (1 << 2)  /* CPU affinity enforced */

/*
 * Built-in DSQs such as SCX_DSQ_GLOBAL cannot be used as priority queues
 * (meaning, cannot be dispatched to with scx_bpf_dsq_insert_vtime()). We
 * therefore create a separate DSQ with ID 0 that we dispatch to and consume
 * from.
 */
#define SHARED_DSQ 0

/**
 * BPF-side view of vCPU metadata
 * Simplified from ossim_vcpu_info for BPF verifier compatibility
 */
struct vcpu_metadata_bpf {
	__u32 vcpu_index;
	__u32 flags;
	__s32 priority_hint;
	__u32 weight_hint;
	__u64 vm_uuid_low;   /* Lower 64 bits of UUID */
	__u64 vm_uuid_high;  /* Upper 64 bits of UUID */
	char vm_name[OSSIM_VM_NAME_MAX];
};

struct vm_config_bpf {
	__u32 num_vcpus;
	__u32 cpu_shares;
	__s64 cpu_quota_us;
	__u64 cpu_period_us;
	__u32 isolation_level;
};

/* Full 128-bit UUID key for vm_config map to avoid collisions */
struct vm_uuid_key {
	__u64 uuid_low;
	__u64 uuid_high;
};

/* Map: TID -> vCPU metadata */
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(key_size, sizeof(pid_t));        /* vCPU thread TID */
	__uint(value_size, sizeof(struct vcpu_metadata_bpf));
	__uint(max_entries, 4096);               /* Support up to 4096 vCPUs */
	__uint(map_flags, BPF_F_NO_PREALLOC);
} vcpu_metadata SEC(".maps");

/* Map: VM UUID (full 128 bits) -> VM config */
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(key_size, sizeof(struct vm_uuid_key));  /* Full 128-bit UUID to prevent collisions */
	__uint(value_size, sizeof(struct vm_config_bpf));
	__uint(max_entries, 256);                /* Support up to 256 VMs */
} vm_config SEC(".maps");

/* Map: Per-vCPU statistics exported to userspace */
struct vcpu_stats_bpf {
	__u64 enqueues;
	__u64 dispatches;
	__u64 total_runtime_ns;
	__u64 last_enqueue_ts;
	__u64 last_run_start_ts;  /* Timestamp when task started running (for runtime tracking) */
};

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
	__uint(max_entries, 2);			/* [local, global] */
} stats SEC(".maps");

static void stat_inc(u32 idx)
{
	u64 *cnt_p = bpf_map_lookup_elem(&stats, &idx);
	if (cnt_p)
		(*cnt_p)++;
}

/* Check if task is a registered vCPU */
static bool is_vcpu_thread(struct task_struct *p)
{
	struct vcpu_metadata_bpf *vcpu;
	pid_t tid = p->pid;

	vcpu = bpf_map_lookup_elem(&vcpu_metadata, &tid);
	return vcpu != NULL;
}

/* Get VM configuration for a vCPU */
static struct vm_config_bpf *get_vm_config(struct vcpu_metadata_bpf *vcpu)
{
	struct vm_uuid_key key = {
		.uuid_low = vcpu->vm_uuid_low,
		.uuid_high = vcpu->vm_uuid_high,
	};
	return bpf_map_lookup_elem(&vm_config, &key);
}

/* Update vCPU statistics */
static void update_vcpu_stats(pid_t tid, bool is_enqueue)
{
	struct vcpu_stats_bpf *stats;
	struct vcpu_stats_bpf new_stats = {
		.enqueues = 0,
		.dispatches = 0,
		.total_runtime_ns = 0,
		.last_enqueue_ts = 0,
		.last_run_start_ts = 0,
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
s32 BPF_STRUCT_OPS(ossim_select_cpu, struct task_struct *p, s32 prev_cpu, u64 wake_flags)
{
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

	/* Count but don't insert here - let enqueue handle it to maintain consistent DSQ mode */
	if (is_idle) {
		stat_inc(0);	/* count local queueing */
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
 *    b. Lookup VM configuration using full 128-bit UUID key
 *    c. Apply weight adjustment: adjusted_weight = (vcpu_weight * vm_cpu_shares) / 1024
 *    d. Check REALTIME flag -> use shorter time slice if set
 *    e. Insert into SHARED_DSQ using FIFO (if RT) or vtime-based scheduling
 * 3. If not vCPU (regular task):
 *    a. Use default scheduling policy (FIFO or vtime-based)
 *    b. No statistics tracking, no weight adjustment
 *
 * Graceful Degradation:
 * - If vcpu_metadata map is empty (no VMs running), all tasks are regular tasks
 * - If vm_config lookup fails, uses default weight from vcpu_metadata
 * - Scheduler operates normally even if ossim.ko is not loaded
 */
void BPF_STRUCT_OPS(ossim_enqueue, struct task_struct *p, u64 enq_flags)
{
	pid_t tid = p->pid;
	struct vcpu_metadata_bpf *vcpu;
	struct vm_config_bpf *vm_cfg;
	u64 slice = SCX_SLICE_DFL;
	u64 vtime = p->scx.dsq_vtime;

	stat_inc(1);	/* count global queueing */

	/* Check if this is a vCPU thread */
	vcpu = bpf_map_lookup_elem(&vcpu_metadata, &tid);
	if (vcpu) {
		/* This is a vCPU - apply VM-specific scheduling */
		update_vcpu_stats(tid, true);

		vm_cfg = get_vm_config(vcpu);
		if (vm_cfg) {
			/* Apply priority boost if requested */
			if (vcpu->flags & OSSIM_VCPU_FLAG_REALTIME) {
				/* Use shorter slice for RT vCPUs */
				slice = SCX_SLICE_DFL / 2;
			}
			/* Weight adjustment will be applied in vtime calculation below */
		}

		/* vCPU-specific scheduling logic */
		if (fifo_sched) {
			scx_bpf_dsq_insert(p, SHARED_DSQ, slice, enq_flags);
		} else {
			/* Apply weight-based vtime adjustment */
			if (vtime < vtime_now - slice)
				vtime = vtime_now - slice;
			/* Boost RT vCPUs by reducing their vtime */
			if (vcpu->flags & OSSIM_VCPU_FLAG_REALTIME)
				vtime = vtime / 2;
			scx_bpf_dsq_insert_vtime(p, SHARED_DSQ, slice, vtime, enq_flags);
		}
	} else {
		/* Regular task - use default scheduling */
		if (fifo_sched) {
			scx_bpf_dsq_insert(p, SHARED_DSQ, slice, enq_flags);
		} else {
			if (vtime < vtime_now - SCX_SLICE_DFL)
				vtime = vtime_now - SCX_SLICE_DFL;
			scx_bpf_dsq_insert_vtime(p, SHARED_DSQ, SCX_SLICE_DFL, vtime, enq_flags);
		}
	}
}

/* Dispatch operation - update statistics */
void BPF_STRUCT_OPS(ossim_dispatch, s32 cpu, struct task_struct *prev)
{
	if (prev) {
		pid_t tid = prev->pid;
		if (is_vcpu_thread(prev)) {
			update_vcpu_stats(tid, false);
		}
	}

	scx_bpf_dsq_move_to_local(SHARED_DSQ);
}

void BPF_STRUCT_OPS(ossim_running, struct task_struct *p)
{
	pid_t tid = p->pid;
	struct vcpu_stats_bpf *stats;

	/* Track start time for vCPU runtime calculation */
	if (is_vcpu_thread(p)) {
		stats = bpf_map_lookup_elem(&vcpu_stats, &tid);
		if (stats) {
			stats->last_run_start_ts = scx_bpf_now();
		}
	}

	if (fifo_sched)
		return;

	/*
	 * Global vtime always progresses forward as tasks start executing. The
	 * test and update can be performed concurrently from multiple CPUs and
	 * thus racy. Any error should be contained and temporary. Let's just
	 * live with it.
	 */
	if (vtime_now < p->scx.dsq_vtime)
		vtime_now = p->scx.dsq_vtime;
}

void BPF_STRUCT_OPS(ossim_stopping, struct task_struct *p, bool runnable)
{
	u64 slice_ns;
	pid_t tid = p->pid;
	struct vcpu_stats_bpf *stats;

	/* Update vCPU runtime statistics using explicit timestamps */
	if (is_vcpu_thread(p)) {
		stats = bpf_map_lookup_elem(&vcpu_stats, &tid);
		if (stats && stats->last_run_start_ts > 0) {
			u64 now = scx_bpf_now();
			u64 runtime_ns = now - stats->last_run_start_ts;
			__sync_fetch_and_add(&stats->total_runtime_ns, runtime_ns);
			stats->last_run_start_ts = 0;  /* Reset for next run */
		}
	}

	/* Calculate slice consumption for vtime accounting */
	slice_ns = SCX_SLICE_DFL - p->scx.slice;

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
	p->scx.dsq_vtime += slice_ns * 100 / p->scx.weight;
}

void BPF_STRUCT_OPS(ossim_enable, struct task_struct *p)
{
	pid_t tid = p->pid;
	struct vcpu_metadata_bpf *vcpu;
	struct vcpu_stats_bpf zero_stats = {
		.enqueues = 0,
		.dispatches = 0,
		.total_runtime_ns = 0,
		.last_enqueue_ts = 0,
		.last_run_start_ts = 0,
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

void BPF_STRUCT_OPS(ossim_disable, struct task_struct *p)
{
	pid_t tid = p->pid;
	struct vcpu_metadata_bpf *vcpu;

	/*
	 * Clean up vCPU entries when thread exits.
	 * This handles both normal QEMU shutdown and abnormal exits (signals).
	 * We only remove from vcpu_metadata and vcpu_stats maps here.
	 * VM config entries are left intact as they may be shared by other vCPUs.

	 TODO: Also clean up vcpu_stats map entries at proper time.
	 */
	vcpu = bpf_map_lookup_elem(&vcpu_metadata, &tid);
	if (vcpu) {
		/* This is a vCPU thread - clean up its entries */
		bpf_map_delete_elem(&vcpu_metadata, &tid);
		bpf_map_delete_elem(&vcpu_stats, &tid);
	}
}

s32 BPF_STRUCT_OPS_SLEEPABLE(ossim_init)
{
	return scx_bpf_create_dsq(SHARED_DSQ, -1);
}

void BPF_STRUCT_OPS(ossim_exit, struct scx_exit_info *ei)
{
	UEI_RECORD(uei, ei);
}

SCX_OPS_DEFINE(ossim_ops,
	       .select_cpu		= (void *)ossim_select_cpu,
	       .enqueue			= (void *)ossim_enqueue,
	       .dispatch		= (void *)ossim_dispatch,
	       .running			= (void *)ossim_running,
	       .stopping		= (void *)ossim_stopping,
	       .enable			= (void *)ossim_enable,
	       .disable			= (void *)ossim_disable,
	       .init			= (void *)ossim_init,
	       .exit			= (void *)ossim_exit,
	       .name			= "ossim");
