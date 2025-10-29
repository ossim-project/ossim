/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _OSSIM_VCPU_H
#define _OSSIM_VCPU_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/uuid.h>
#else
/* Userspace includes */
#include <stdint.h>
#include <sys/types.h>
#include <linux/types.h>  /* For __u8, __s32, __u32, __s64, __u64 */
/* UUID type for userspace - compatible with uuid_t layout */
typedef unsigned char uuid_t[16];
#endif

/* Protocol version for compatibility checks */
#define OSSIM_VCPU_API_VERSION 1

/* Maximum length for VM name string */
#define OSSIM_VM_NAME_MAX 64

/* vCPU thread flags */
#define OSSIM_VCPU_FLAG_IOTHREAD  (1 << 0)  /* IO thread, not vCPU */
#define OSSIM_VCPU_FLAG_REALTIME  (1 << 1)  /* Requires RT scheduling */
#define OSSIM_VCPU_FLAG_PINNED    (1 << 2)  /* CPU affinity enforced */

/**
 * struct ossim_vcpu_registration - QEMU -> kernel vCPU registration
 * @api_version: Must be OSSIM_VCPU_API_VERSION
 * @vm_uuid: Stable VM identifier (from QEMU's MachineState)
 * @vm_name: Human-readable VM name (e.g., "bigdata-controller")
 * @qemu_pid: QEMU process PID (for validation and cleanup)
 * @vcpu_tid: vCPU thread ID (gettid() from kvm_vcpu_thread_fn)
 * @vcpu_index: vCPU index within VM (cpu->cpu_index from QEMU)
 * @kvm_fd: KVM vCPU file descriptor (cpu->kvm_fd)
 *          Purpose: Identifies the KVM vCPU device this thread manages.
 *          - Allows kernel to verify this is a real KVM vCPU (not IO thread)
 *          - Value -1 indicates IO thread or TCG mode (not KVM accelerated)
 *          - Future: Could enable direct KVM stats queries or coordination
 *          Current use: Validation and metadata only
 * @flags: Bitmask of OSSIM_VCPU_FLAG_*
 * @priority_hint: Scheduling priority hint (0 = default, higher = more important)
 * @weight_hint: Scheduling weight hint for proportional share (100 = default)
 * @reserved: Reserved for future use, must be zero
 */
struct ossim_vcpu_registration {
	__u32 api_version;
	uuid_t vm_uuid;
	char vm_name[OSSIM_VM_NAME_MAX];
	pid_t qemu_pid;
	pid_t vcpu_tid;
	__u32 vcpu_index;
	__s32 kvm_fd;
	__u32 flags;
	__s32 priority_hint;
	__u32 weight_hint;
	__u32 reserved[8];
} __attribute__((packed));

#ifdef __KERNEL__
/**
 * struct ossim_vcpu_info - Kernel-internal vCPU metadata
 * @vm_uuid: Copy of vm_uuid from registration
 * @vm_name: Copy of vm_name from registration
 * @qemu_pid: QEMU process PID
 * @vcpu_tid: vCPU thread TID (hash table key)
 * @vcpu_index: vCPU index within VM
 * @kvm_fd: KVM vCPU file descriptor
 * @flags: vCPU flags
 * @priority_hint: Scheduling priority
 * @weight_hint: Scheduling weight
 * @registration_time: ktime_get() at registration
 * @last_update: ktime_get() at last metadata update
 * @stats_enqueues: Number of times enqueued (updated by BPF scheduler)
 * @stats_dispatches: Number of times dispatched
 * @hlist: Hash table linkage
 * @rcu: RCU callback head for safe deletion
 */
struct ossim_vcpu_info {
	uuid_t vm_uuid;
	char vm_name[OSSIM_VM_NAME_MAX];
	pid_t qemu_pid;
	pid_t vcpu_tid;
	__u32 vcpu_index;
	__s32 kvm_fd;
	__u32 flags;
	__s32 priority_hint;
	__u32 weight_hint;
	u64 registration_time;
	u64 last_update;
	/* Statistics - updated by BPF scheduler via shared memory */
	u64 stats_enqueues;
	u64 stats_dispatches;
	struct hlist_node hlist;
	struct rcu_head rcu;
};
#endif /* __KERNEL__ */

/**
 * struct ossim_vm_config - VM-level configuration
 * @vm_uuid: VM identifier
 * @vm_name: VM name
 * @num_vcpus: Total number of vCPUs in this VM
 * @cpu_shares: Proportional CPU share (default 1024)
 * @cpu_quota_us: CPU quota in microseconds per period (-1 = unlimited)
 * @cpu_period_us: CPU quota enforcement period (default 100000 = 100ms)
 * @isolation_level: Isolation policy (0=none, 1=soft, 2=hard)
 */
struct ossim_vm_config {
	uuid_t vm_uuid;
	char vm_name[OSSIM_VM_NAME_MAX];
	__u32 num_vcpus;
	__u32 cpu_shares;
	__s64 cpu_quota_us;
	__u64 cpu_period_us;
	__u32 isolation_level;
	__u32 reserved[7];
} __attribute__((packed));

/**
 * struct vcpu_metadata_bpf - BPF-side view of vCPU metadata
 * Simplified from ossim_vcpu_info for BPF verifier compatibility
 * Used by both kernel module and userspace for BPF map updates
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

#endif /* _OSSIM_VCPU_H */
