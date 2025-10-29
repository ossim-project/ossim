/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/sched/task.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "ossim_vcpu.h"
#include "ossim_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ossim Project");
MODULE_DESCRIPTION("Ossim Kernel Module");
MODULE_VERSION("0.2");

/* Hash table for vCPU registry (key: TID) */
#define VCPU_HASH_BITS 10
static DEFINE_HASHTABLE(vcpu_registry, VCPU_HASH_BITS);
static DEFINE_SPINLOCK(vcpu_registry_lock);

/* BPF map file descriptors - exported to BPF programs */
static int vcpu_metadata_map_fd = -1;
static int vm_config_map_fd = -1;
static int vcpu_stats_map_fd = -1;

/* Statistics */
static atomic64_t total_registrations = ATOMIC64_INIT(0);
static atomic64_t total_unregistrations = ATOMIC64_INIT(0);
static atomic64_t active_vcpus = ATOMIC64_INIT(0);

/* Procfs entries */
static struct proc_dir_entry *ossim_proc_dir;
static struct proc_dir_entry *ossim_proc_vcpus;
static struct proc_dir_entry *ossim_proc_stats;

/* Find vCPU info by TID (must hold RCU read lock) */
static struct ossim_vcpu_info *find_vcpu_by_tid(pid_t tid)
{
	struct ossim_vcpu_info *vcpu;

	hash_for_each_possible_rcu(vcpu_registry, vcpu, hlist, tid) {
		if (vcpu->vcpu_tid == tid)
			return vcpu;
	}
	return NULL;
}

/* Validate registration request */
static int validate_registration(const struct ossim_vcpu_registration *reg)
{
	if (reg->api_version != OSSIM_VCPU_API_VERSION) {
		pr_err("[ossim] Invalid API version: %u (expected %u)\n",
		       reg->api_version, OSSIM_VCPU_API_VERSION);
		return -EINVAL;
	}

	if (reg->qemu_pid != current->tgid) {
		pr_err("[ossim] PID mismatch: registration claims %d, caller is %d\n",
		       reg->qemu_pid, current->tgid);
		return -EPERM;
	}

	if (reg->vcpu_tid <= 0) {
		pr_err("[ossim] Invalid TID: %d\n", reg->vcpu_tid);
		return -EINVAL;
	}

	if (reg->vm_name[OSSIM_VM_NAME_MAX - 1] != '\0') {
		pr_err("[ossim] VM name not null-terminated\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * Export vCPU metadata to BPF map
 *
 * NOTE: This is a placeholder implementation. According to Section 6.4 of the
 * specification, the kernel module cannot directly update BPF maps because
 * bpf_map_update_elem() is not exported to loadable kernel modules.
 *
 * The actual BPF map updates must happen from userspace:
 * 1. QEMU calls ioctl(OSSIM_IOCTL_REGISTER_VCPU) - validated here
 * 2. Kernel module maintains authoritative hash table
 * 3. QEMU userspace updates BPF maps directly after successful ioctl
 * 4. BPF scheduler reads from maps (fast, read-only access)
 *
 * This function is kept for future phases where we might add netlink
 * notifications or other coordination mechanisms.
 */
static int export_to_bpf_map(struct ossim_vcpu_info *vcpu)
{
	pr_debug("[ossim] Would export vCPU %d (tid=%d) to BPF map\n",
		 vcpu->vcpu_index, vcpu->vcpu_tid);

	/*
	 * Userspace (QEMU) must call bpf_obj_get("/sys/fs/bpf/ossim/vcpu_metadata")
	 * and then bpf_map_update_elem() with the vCPU metadata.
	 */

	return 0;
}

/*
 * Remove vCPU metadata from BPF map
 *
 * NOTE: This is a placeholder - see export_to_bpf_map() comment above.
 * Userspace must handle BPF map deletions.
 */
static int remove_from_bpf_map(pid_t tid)
{
	pr_debug("[ossim] Would remove TID %d from BPF map\n", tid);
	return 0;
}

/* RCU callback for freeing vcpu_info */
static void free_vcpu_info_rcu(struct rcu_head *head)
{
	struct ossim_vcpu_info *vcpu = container_of(head, struct ossim_vcpu_info, rcu);
	kfree(vcpu);
}

/* Register vCPU - ioctl handler */
static int do_register_vcpu(struct ossim_vcpu_registration __user *arg)
{
	struct ossim_vcpu_registration reg;
	struct ossim_vcpu_info *vcpu, *existing;
	int ret;

	if (copy_from_user(&reg, arg, sizeof(reg)))
		return -EFAULT;

	ret = validate_registration(&reg);
	if (ret)
		return ret;

	/* Allocate new vCPU info */
	vcpu = kzalloc(sizeof(*vcpu), GFP_KERNEL);
	if (!vcpu)
		return -ENOMEM;

	/* Populate vcpu_info from registration */
	memcpy(&vcpu->vm_uuid, &reg.vm_uuid, sizeof(uuid_t));
	strncpy(vcpu->vm_name, reg.vm_name, sizeof(vcpu->vm_name));
	vcpu->qemu_pid = reg.qemu_pid;
	vcpu->vcpu_tid = reg.vcpu_tid;
	vcpu->vcpu_index = reg.vcpu_index;
	vcpu->kvm_fd = reg.kvm_fd;
	vcpu->flags = reg.flags;
	vcpu->priority_hint = reg.priority_hint;
	vcpu->weight_hint = reg.weight_hint;
	vcpu->registration_time = ktime_get();
	vcpu->last_update = vcpu->registration_time;
	vcpu->stats_enqueues = 0;
	vcpu->stats_dispatches = 0;

	/* Insert into hash table */
	spin_lock(&vcpu_registry_lock);
	existing = find_vcpu_by_tid(reg.vcpu_tid);
	if (existing) {
		spin_unlock(&vcpu_registry_lock);
		kfree(vcpu);
		pr_warn("[ossim] vCPU tid=%d already registered\n", reg.vcpu_tid);
		return -EEXIST;
	}
	hash_add_rcu(vcpu_registry, &vcpu->hlist, vcpu->vcpu_tid);
	spin_unlock(&vcpu_registry_lock);

	/* Export to BPF map (placeholder - see function comment) */
	ret = export_to_bpf_map(vcpu);
	if (ret) {
		/* Rollback registration */
		spin_lock(&vcpu_registry_lock);
		hash_del_rcu(&vcpu->hlist);
		spin_unlock(&vcpu_registry_lock);
		synchronize_rcu();
		kfree(vcpu);
		return ret;
	}

	atomic64_inc(&total_registrations);
	atomic64_inc(&active_vcpus);

	pr_info("[ossim] Registered vCPU: VM=%s, index=%u, tid=%d, qemu_pid=%d\n",
		vcpu->vm_name, vcpu->vcpu_index, vcpu->vcpu_tid, vcpu->qemu_pid);

	return 0;
}

/* Unregister vCPU - ioctl handler */
static int do_unregister_vcpu(pid_t __user *arg)
{
	pid_t tid;
	struct ossim_vcpu_info *vcpu;

	if (copy_from_user(&tid, arg, sizeof(tid)))
		return -EFAULT;

	spin_lock(&vcpu_registry_lock);
	vcpu = find_vcpu_by_tid(tid);
	if (!vcpu) {
		spin_unlock(&vcpu_registry_lock);
		pr_warn("[ossim] Unregister failed: tid=%d not found\n", tid);
		return -ENOENT;
	}

	/* Permission check: only original QEMU process can unregister */
	if (vcpu->qemu_pid != current->tgid) {
		spin_unlock(&vcpu_registry_lock);
		pr_err("[ossim] Unregister permission denied: tid=%d registered by pid=%d, caller is pid=%d\n",
		       tid, vcpu->qemu_pid, current->tgid);
		return -EPERM;
	}

	/* Remove from hash table */
	hash_del_rcu(&vcpu->hlist);
	spin_unlock(&vcpu_registry_lock);

	/* Remove from BPF map (placeholder - userspace must do this) */
	remove_from_bpf_map(tid);

	/* Free via RCU to ensure no concurrent readers */
	call_rcu(&vcpu->rcu, free_vcpu_info_rcu);

	atomic64_inc(&total_unregistrations);
	atomic64_dec(&active_vcpus);

	pr_info("[ossim] Unregistered vCPU: tid=%d\n", tid);

	return 0;
}

/*
 * Update vCPU metadata - ioctl handler
 *
 * CRITICAL: Uses spinlock (not RCU read lock) to prevent torn reads.
 * See Section 3.1 of specification for details.
 */
static int do_update_vcpu(struct ossim_vcpu_registration __user *arg)
{
	struct ossim_vcpu_registration reg;
	struct ossim_vcpu_info *vcpu;
	int ret = 0;

	if (copy_from_user(&reg, arg, sizeof(reg)))
		return -EFAULT;

	spin_lock(&vcpu_registry_lock);
	vcpu = find_vcpu_by_tid(reg.vcpu_tid);
	if (!vcpu) {
		spin_unlock(&vcpu_registry_lock);
		return -ENOENT;
	}

	/* Permission check */
	if (vcpu->qemu_pid != current->tgid) {
		spin_unlock(&vcpu_registry_lock);
		return -EPERM;
	}

	/* Update mutable fields - now protected by spinlock to prevent torn reads */
	vcpu->priority_hint = reg.priority_hint;
	vcpu->weight_hint = reg.weight_hint;
	vcpu->flags = reg.flags;
	vcpu->last_update = ktime_get();

	spin_unlock(&vcpu_registry_lock);

	/* Re-export to BPF map (outside lock to avoid holding spinlock during syscall) */
	ret = export_to_bpf_map(vcpu);

	pr_debug("[ossim] Updated vCPU tid=%d\n", reg.vcpu_tid);

	return ret;
}

/* IOCTL dispatcher */
static long ossim_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case OSSIM_IOCTL_REGISTER_VCPU:
		return do_register_vcpu((struct ossim_vcpu_registration __user *)arg);

	case OSSIM_IOCTL_UNREGISTER_VCPU:
		return do_unregister_vcpu((pid_t __user *)arg);

	case OSSIM_IOCTL_UPDATE_VCPU:
		return do_update_vcpu((struct ossim_vcpu_registration __user *)arg);

	case OSSIM_IOCTL_GET_VCPU_INFO:
		/* TODO: Implement query functionality */
		return -ENOSYS;

	case OSSIM_IOCTL_SET_VM_CONFIG:
		/* TODO: Implement VM config */
		return -ENOSYS;

	case OSSIM_IOCTL_LIST_VCPUS:
		/* TODO: Implement listing */
		return -ENOSYS;

	default:
		pr_warn("[ossim] Unknown ioctl command: %u\n", cmd);
		return -EINVAL;
	}
}

/*
 * File release handler - cleanup orphaned vCPUs
 *
 * CRITICAL: Two-pass approach to avoid sleeping-while-atomic bug.
 * See Section 13.2 of specification for details.
 *
 * Pass 1: Collect TIDs under lock (cannot sleep)
 * Pass 2: Cleanup BPF maps outside lock (can sleep)
 */
static int ossim_release(struct inode *inode, struct file *file)
{
	struct ossim_vcpu_info *vcpu;
	struct hlist_node *tmp;
	pid_t qemu_pid = current->tgid;
	int bkt;
	int count = 0;
	pid_t *tids_to_cleanup = NULL;
	int cleanup_idx = 0;
	int i;

	pr_debug("[ossim] Release called by pid=%d\n", qemu_pid);

	/* First pass: count vCPUs to cleanup */
	spin_lock(&vcpu_registry_lock);
	hash_for_each_safe(vcpu_registry, bkt, tmp, vcpu, hlist) {
		if (vcpu->qemu_pid == qemu_pid)
			count++;
	}

	if (count == 0) {
		spin_unlock(&vcpu_registry_lock);
		return 0;
	}

	tids_to_cleanup = kmalloc(count * sizeof(pid_t), GFP_ATOMIC);
	if (!tids_to_cleanup) {
		spin_unlock(&vcpu_registry_lock);
		pr_err("[ossim] Failed to allocate cleanup array for %d vCPUs\n", count);
		return -ENOMEM;
	}

	/* Second pass: remove from hash and collect TIDs */
	hash_for_each_safe(vcpu_registry, bkt, tmp, vcpu, hlist) {
		if (vcpu->qemu_pid == qemu_pid) {
			tids_to_cleanup[cleanup_idx++] = vcpu->vcpu_tid;
			hash_del_rcu(&vcpu->hlist);
			call_rcu(&vcpu->rcu, free_vcpu_info_rcu);
		}
	}
	spin_unlock(&vcpu_registry_lock);

	/* Now cleanup BPF maps without holding lock (safe to sleep here) */
	for (i = 0; i < count; i++)
		remove_from_bpf_map(tids_to_cleanup[i]);

	kfree(tids_to_cleanup);

	pr_warn("[ossim] Cleaned up %d orphaned vCPUs from pid=%d\n",
		count, qemu_pid);
	atomic64_sub(count, &active_vcpus);
	atomic64_add(count, &total_unregistrations);

	return 0;
}

static const struct file_operations ossim_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ossim_ioctl,
	.release = ossim_release,
};

static struct miscdevice ossim_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ossim",
	.fops = &ossim_fops,
	.mode = 0666,
};

/* /proc/ossim/vcpus - list all registered vCPUs */
static int ossim_proc_vcpus_show(struct seq_file *m, void *v)
{
	struct ossim_vcpu_info *vcpu;
	int bkt;

	seq_printf(m, "%-8s %-8s %-20s %-6s %-6s %-8s %-8s\n",
		   "TID", "QEMU_PID", "VM_NAME", "INDEX", "FLAGS",
		   "ENQUEUES", "DISPATCHES");

	rcu_read_lock();
	hash_for_each_rcu(vcpu_registry, bkt, vcpu, hlist) {
		seq_printf(m, "%-8d %-8d %-20s %-6u 0x%-4x %-8llu %-8llu\n",
			   vcpu->vcpu_tid, vcpu->qemu_pid, vcpu->vm_name,
			   vcpu->vcpu_index, vcpu->flags,
			   vcpu->stats_enqueues, vcpu->stats_dispatches);
	}
	rcu_read_unlock();

	return 0;
}

/* /proc/ossim/stats - global statistics */
static int ossim_proc_stats_show(struct seq_file *m, void *v)
{
	seq_printf(m, "total_registrations: %lld\n", atomic64_read(&total_registrations));
	seq_printf(m, "total_unregistrations: %lld\n", atomic64_read(&total_unregistrations));
	seq_printf(m, "active_vcpus: %lld\n", atomic64_read(&active_vcpus));
	return 0;
}

static int ossim_proc_vcpus_open(struct inode *inode, struct file *file)
{
	return single_open(file, ossim_proc_vcpus_show, NULL);
}

static int ossim_proc_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ossim_proc_stats_show, NULL);
}

static const struct proc_ops ossim_proc_vcpus_ops = {
	.proc_open = ossim_proc_vcpus_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops ossim_proc_stats_ops = {
	.proc_open = ossim_proc_stats_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init ossim_init(void)
{
	int ret;

	pr_info("[ossim] Initializing Ossim kernel module\n");

	hash_init(vcpu_registry);

	ret = misc_register(&ossim_miscdev);
	if (ret < 0) {
		pr_err("[ossim] Failed to register misc device: %d\n", ret);
		return ret;
	}

	/* Create /proc/ossim directory */
	ossim_proc_dir = proc_mkdir("ossim", NULL);
	if (!ossim_proc_dir) {
		pr_err("[ossim] Failed to create /proc/ossim directory\n");
		misc_deregister(&ossim_miscdev);
		return -ENOMEM;
	}

	/* Create /proc/ossim/vcpus */
	ossim_proc_vcpus = proc_create("vcpus", 0444, ossim_proc_dir,
				       &ossim_proc_vcpus_ops);
	if (!ossim_proc_vcpus) {
		pr_err("[ossim] Failed to create /proc/ossim/vcpus\n");
		proc_remove(ossim_proc_dir);
		misc_deregister(&ossim_miscdev);
		return -ENOMEM;
	}

	/* Create /proc/ossim/stats */
	ossim_proc_stats = proc_create("stats", 0444, ossim_proc_dir,
				       &ossim_proc_stats_ops);
	if (!ossim_proc_stats) {
		pr_err("[ossim] Failed to create /proc/ossim/stats\n");
		proc_remove(ossim_proc_vcpus);
		proc_remove(ossim_proc_dir);
		misc_deregister(&ossim_miscdev);
		return -ENOMEM;
	}

	pr_info("[ossim] Module loaded! Device: /dev/ossim\n");
	pr_info("[ossim] Procfs: /proc/ossim/vcpus, /proc/ossim/stats\n");
	return 0;
}

static void __exit ossim_exit(void)
{
	struct ossim_vcpu_info *vcpu;
	struct hlist_node *tmp;
	int bkt;
	int count = 0;

	/* Remove procfs entries */
	if (ossim_proc_stats)
		proc_remove(ossim_proc_stats);
	if (ossim_proc_vcpus)
		proc_remove(ossim_proc_vcpus);
	if (ossim_proc_dir)
		proc_remove(ossim_proc_dir);

	misc_deregister(&ossim_miscdev);

	/* Clean up all registered vCPUs */
	spin_lock(&vcpu_registry_lock);
	hash_for_each_safe(vcpu_registry, bkt, tmp, vcpu, hlist) {
		hash_del_rcu(&vcpu->hlist);
		remove_from_bpf_map(vcpu->vcpu_tid);
		call_rcu(&vcpu->rcu, free_vcpu_info_rcu);
		count++;
	}
	spin_unlock(&vcpu_registry_lock);

	synchronize_rcu();

	pr_info("[ossim] Module unloaded! Cleaned up %d vCPUs\n", count);
	pr_info("[ossim] Lifetime stats: registrations=%lld, unregistrations=%lld\n",
		atomic64_read(&total_registrations),
		atomic64_read(&total_unregistrations));
}

module_init(ossim_init);
module_exit(ossim_exit);
