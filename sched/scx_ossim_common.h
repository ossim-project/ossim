/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Common data structures shared between BPF and userspace
 *
 * This header defines structures used by both the BPF scheduler
 * (scx_ossim.bpf.c) and the userspace daemon (scx_ossim.c).
 *
 * Copyright (c) 2025 Ossim Project
 */
#ifndef __SCX_OSSIM_COMMON_H
#define __SCX_OSSIM_COMMON_H

/* BPF environment already has types from vmlinux.h */
#ifndef __BPF__
#include <linux/types.h>
#endif

/* Maximum length for VM name string */
#define OSSIM_VM_NAME_MAX 64

/* vCPU thread flags */
#define OSSIM_VCPU_FLAG_IOTHREAD (1 << 0) /* IO thread, not vCPU */
#define OSSIM_VCPU_FLAG_REALTIME (1 << 1) /* Requires RT scheduling */
#define OSSIM_VCPU_FLAG_PINNED (1 << 2)   /* CPU affinity enforced */

/* vtime delta threshold: threads with vtime > vtime_now + delta won't be
 * scheduled */
#define VTIME_DELTA_NS (1000ULL) /* 1us (as currently configured) */

/**
 * BPF-side view of vCPU metadata
 * Simplified from ossim_vcpu_info for BPF verifier compatibility
 */
struct vcpu_metadata_bpf {
  __u32 vcpu_index;
  __u32 flags;
  __u64 vm_uuid_low;  /* Lower 64 bits of UUID */
  __u64 vm_uuid_high; /* Upper 64 bits of UUID */
  char vm_name[OSSIM_VM_NAME_MAX];
};

struct vm_config_bpf {
  __u32 num_vcpus;
  __s64 cpu_quota_us;
  __u64 cpu_period_us;
  __u32 isolation_level;
};

/* Full 128-bit UUID key for vm_config map to avoid collisions */
struct vm_uuid_key {
  __u64 uuid_low;
  __u64 uuid_high;
};

/* Per-vCPU statistics exported to userspace */
struct vcpu_stats_bpf {
  __u64 enqueues;
  __u64 dispatches;
  __u64 total_runtime_ns;
  __u64 last_enqueue_ts;
  __u64 vtime;
};

#endif /* __SCX_OSSIM_COMMON_H */
