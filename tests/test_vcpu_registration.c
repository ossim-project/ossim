/* Test vCPU registration with ossim.ko and BPF map updates */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <uuid/uuid.h>

#define DEVICE_PATH "/dev/ossim"
#define OSSIM_VM_NAME_MAX 64

/* From ossim_vcpu.h */
#define OSSIM_VCPU_API_VERSION 1
#define OSSIM_IOCTL_MAGIC 'O'

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

#define OSSIM_IOCTL_REGISTER_VCPU \
	_IOW(OSSIM_IOCTL_MAGIC, 1, struct ossim_vcpu_registration)
#define OSSIM_IOCTL_UNREGISTER_VCPU \
	_IOW(OSSIM_IOCTL_MAGIC, 2, pid_t)

/* BPF map structures */
struct vcpu_metadata_bpf {
	__u32 vcpu_index;
	__u32 flags;
	__s32 priority_hint;
	__u32 weight_hint;
	__u64 vm_uuid_low;
	__u64 vm_uuid_high;
	char vm_name[OSSIM_VM_NAME_MAX];
};

int main(int argc, char **argv)
{
	int ossim_fd, vcpu_meta_fd, ret;
	struct ossim_vcpu_registration reg;
	struct vcpu_metadata_bpf bpf_meta;
	pid_t tid;

	printf("=== Ossim vCPU Registration Test ===\n\n");

	/* Step 1: Open ossim device */
	printf("1. Opening /dev/ossim...\n");
	ossim_fd = open(DEVICE_PATH, O_RDWR);
	if (ossim_fd < 0) {
		perror("Failed to open /dev/ossim");
		return EXIT_FAILURE;
	}
	printf("   ✓ Device opened successfully\n\n");

	/* Step 2: Prepare registration data */
	printf("2. Preparing vCPU registration data...\n");
	memset(&reg, 0, sizeof(reg));
	reg.api_version = OSSIM_VCPU_API_VERSION;
	uuid_generate(reg.vm_uuid);
	snprintf(reg.vm_name, sizeof(reg.vm_name), "test-vm");
	reg.qemu_pid = getpid();
	reg.vcpu_tid = gettid();
	reg.vcpu_index = 0;
	reg.kvm_fd = -1;  /* No actual KVM FD in test */
	reg.flags = 0;
	reg.priority_hint = 0;
	reg.weight_hint = 100;

	char uuid_str[37];
	uuid_unparse(reg.vm_uuid, uuid_str);
	printf("   VM Name: %s\n", reg.vm_name);
	printf("   VM UUID: %s\n", uuid_str);
	printf("   QEMU PID: %d\n", reg.qemu_pid);
	printf("   vCPU TID: %d\n", reg.vcpu_tid);
	printf("   vCPU Index: %u\n", reg.vcpu_index);
	printf("   ✓ Registration data prepared\n\n");

	/* Step 3: Register with kernel module */
	printf("3. Registering vCPU with kernel module via ioctl...\n");
	ret = ioctl(ossim_fd, OSSIM_IOCTL_REGISTER_VCPU, &reg);
	if (ret < 0) {
		perror("   ✗ ioctl REGISTER_VCPU failed");
		close(ossim_fd);
		return EXIT_FAILURE;
	}
	printf("   ✓ Kernel module registration successful\n\n");

	/* Step 4: Update BPF map (simulating QEMU behavior) */
	printf("4. Opening BPF map at /sys/fs/bpf/ossim/vcpu_metadata...\n");
	vcpu_meta_fd = bpf_obj_get("/sys/fs/bpf/ossim/vcpu_metadata");
	if (vcpu_meta_fd < 0) {
		printf("   ✗ Failed to open BPF map: %s\n", strerror(errno));
		printf("   Note: Scheduler may not be running\n");
		goto cleanup_ioctl;
	}
	printf("   ✓ BPF map opened (fd=%d)\n\n", vcpu_meta_fd);

	printf("5. Updating BPF map with vCPU metadata...\n");
	memset(&bpf_meta, 0, sizeof(bpf_meta));
	bpf_meta.vcpu_index = reg.vcpu_index;
	bpf_meta.flags = reg.flags;
	bpf_meta.priority_hint = reg.priority_hint;
	bpf_meta.weight_hint = reg.weight_hint;
	memcpy(&bpf_meta.vm_uuid_low, &reg.vm_uuid[0], 8);
	memcpy(&bpf_meta.vm_uuid_high, &reg.vm_uuid[8], 8);
	strncpy(bpf_meta.vm_name, reg.vm_name, sizeof(bpf_meta.vm_name));

	tid = reg.vcpu_tid;
	ret = bpf_map_update_elem(vcpu_meta_fd, &tid, &bpf_meta, BPF_ANY);
	if (ret < 0) {
		printf("   ✗ Failed to update BPF map: %s\n", strerror(errno));
		close(vcpu_meta_fd);
		goto cleanup_ioctl;
	}
	printf("   ✓ BPF map updated successfully\n\n");

	/* Step 5: Wait and let scheduler process some tasks */
	printf("6. Waiting 3 seconds for scheduler to see vCPU...\n");
	sleep(3);
	printf("   ✓ Check scheduler output for vCPU statistics\n\n");

	/* Step 6: Cleanup */
	printf("7. Cleaning up...\n");
	ret = bpf_map_delete_elem(vcpu_meta_fd, &tid);
	if (ret < 0) {
		printf("   ⚠ Failed to delete from BPF map: %s\n", strerror(errno));
	} else {
		printf("   ✓ Removed from BPF map\n");
	}
	close(vcpu_meta_fd);

cleanup_ioctl:
	ret = ioctl(ossim_fd, OSSIM_IOCTL_UNREGISTER_VCPU, &tid);
	if (ret < 0) {
		printf("   ⚠ ioctl UNREGISTER_VCPU failed: %s\n", strerror(errno));
	} else {
		printf("   ✓ Unregistered from kernel module\n");
	}
	close(ossim_fd);

	printf("\n=== Test Complete ===\n");
	printf("Check:\n");
	printf("  - sudo cat /proc/ossim/stats\n");
	printf("  - Scheduler output for vCPU statistics\n");
	printf("  - sudo dmesg | grep ossim\n");

	return EXIT_SUCCESS;
}
