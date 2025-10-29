/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _OSSIM_IOCTL_H
#define _OSSIM_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#include "ossim_vcpu.h"

#define OSSIM_IOCTL_MAGIC 'O'

/**
 * OSSIM_IOCTL_REGISTER_VCPU - Register a vCPU thread
 * @arg: struct ossim_vcpu_registration *
 *
 * Registers the calling thread (or specified TID) as a KVM vCPU thread.
 * Must be called from QEMU's kvm_vcpu_thread_fn() after qemu_get_thread_id().
 *
 * Returns: 0 on success, -errno on failure
 *   -EINVAL: Invalid API version or parameters
 *   -EEXIST: TID already registered
 *   -ENOMEM: Out of memory
 *   -EPERM: Caller does not match qemu_pid
 */
#define OSSIM_IOCTL_REGISTER_VCPU \
	_IOW(OSSIM_IOCTL_MAGIC, 1, struct ossim_vcpu_registration)

/**
 * OSSIM_IOCTL_UNREGISTER_VCPU - Unregister a vCPU thread
 * @arg: pid_t * (TID to unregister)
 *
 * Removes vCPU registration. Called from kvm_destroy_vcpu() or QEMU exit.
 * Automatically removes corresponding BPF map entries.
 *
 * Returns: 0 on success, -errno on failure
 *   -ENOENT: TID not registered
 *   -EPERM: Caller does not match original qemu_pid
 */
#define OSSIM_IOCTL_UNREGISTER_VCPU \
	_IOW(OSSIM_IOCTL_MAGIC, 2, pid_t)

/**
 * OSSIM_IOCTL_UPDATE_VCPU - Update vCPU metadata (priority, weight, etc.)
 * @arg: struct ossim_vcpu_registration *
 *
 * Allows dynamic updates to scheduling hints without re-registration.
 * Only updates priority_hint, weight_hint, and flags fields.
 *
 * Returns: 0 on success, -errno on failure
 */
#define OSSIM_IOCTL_UPDATE_VCPU \
	_IOW(OSSIM_IOCTL_MAGIC, 3, struct ossim_vcpu_registration)

/**
 * OSSIM_IOCTL_GET_VCPU_INFO - Query vCPU metadata
 * @arg: struct ossim_vcpu_query *
 *
 * Allows querying registered vCPU information (primarily for debugging).
 */
struct ossim_vcpu_query {
	pid_t vcpu_tid;				/* Input: TID to query */
	struct ossim_vcpu_registration info;	/* Output: Current info */
};

#define OSSIM_IOCTL_GET_VCPU_INFO \
	_IOWR(OSSIM_IOCTL_MAGIC, 4, struct ossim_vcpu_query)

/**
 * OSSIM_IOCTL_SET_VM_CONFIG - Set VM-level configuration
 * @arg: struct ossim_vm_config *
 *
 * Configures VM-wide scheduling parameters (shares, quotas, isolation).
 */
#define OSSIM_IOCTL_SET_VM_CONFIG \
	_IOW(OSSIM_IOCTL_MAGIC, 5, struct ossim_vm_config)

/**
 * OSSIM_IOCTL_LIST_VCPUS - List all registered vCPUs
 * @arg: struct ossim_vcpu_list *
 *
 * Returns array of registered vCPU TIDs for monitoring/debugging.
 */
struct ossim_vcpu_list {
	__u32 count;		/* Input: max entries, Output: actual count */
	pid_t tids[0];		/* Variable-length array of TIDs */
};

#define OSSIM_IOCTL_LIST_VCPUS \
	_IOWR(OSSIM_IOCTL_MAGIC, 6, struct ossim_vcpu_list)

#endif /* _OSSIM_IOCTL_H */
