/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Ossim vCPU-aware scheduler - userspace daemon
 *
 * This daemon manages BPF maps for vCPU metadata and provides
 * statistics monitoring for VM-aware scheduling decisions.
 *
 * Copyright (c) 2025 Ossim Project
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <scx/common.h>
#include "scx_ossim.bpf.skel.h"

#define BPF_PIN_PATH "/sys/fs/bpf/ossim"

const char help_fmt[] =
"Ossim vCPU-aware sched_ext scheduler.\n"
"\n"
"This scheduler identifies vCPU threads from QEMU/KVM and applies\n"
"VM-specific scheduling policies. vCPU metadata is shared via pinned\n"
"BPF maps at /sys/fs/bpf/ossim/.\n"
"\n"
"Usage: %s [-f] [-v]\n"
"\n"
"  -f            Use FIFO scheduling instead of weighted vtime scheduling\n"
"  -v            Print libbpf debug messages\n"
"  -h            Display this help and exit\n";

static bool verbose;
static volatile int exit_req;

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (level == LIBBPF_DEBUG && !verbose)
		return 0;
	return vfprintf(stderr, format, args);
}

static void sigint_handler(int sig)
{
	exit_req = 1;
}

/* Create BPF filesystem directory for pinned maps */
static int create_bpf_pin_dir(void)
{
	int ret;

	ret = mkdir(BPF_PIN_PATH, 0755);
	if (ret < 0 && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %s\n", BPF_PIN_PATH, strerror(errno));
		return -1;
	}

	return 0;
}

/* Pin BPF maps for sharing with QEMU
 *
 * ARCHITECTURE NOTE (from Section 6.4 of specification):
 * The BPF scheduler OWNS and CREATES these maps. QEMU and the kernel module
 * are consumers that open the pinned maps via bpf_obj_get() to update them.
 *
 * This design is necessary because bpf_map_update_elem() is not exported to
 * loadable kernel modules. QEMU updates the maps directly after successful
 * ioctl validation by ossim.ko.
 */
static int pin_ossim_maps(struct scx_ossim *skel)
{
	int ret;

	ret = bpf_map__pin(skel->maps.vcpu_metadata, BPF_PIN_PATH "/vcpu_metadata");
	if (ret < 0) {
		fprintf(stderr, "Failed to pin vcpu_metadata map: %s\n", strerror(-ret));
		return ret;
	}

	ret = bpf_map__pin(skel->maps.vm_config, BPF_PIN_PATH "/vm_config");
	if (ret < 0) {
		fprintf(stderr, "Failed to pin vm_config map: %s\n", strerror(-ret));
		bpf_map__unpin(skel->maps.vcpu_metadata, BPF_PIN_PATH "/vcpu_metadata");
		return ret;
	}

	ret = bpf_map__pin(skel->maps.vcpu_stats, BPF_PIN_PATH "/vcpu_stats");
	if (ret < 0) {
		fprintf(stderr, "Failed to pin vcpu_stats map: %s\n", strerror(-ret));
		bpf_map__unpin(skel->maps.vcpu_metadata, BPF_PIN_PATH "/vcpu_metadata");
		bpf_map__unpin(skel->maps.vm_config, BPF_PIN_PATH "/vm_config");
		return ret;
	}

	fprintf(stderr, "Successfully pinned BPF maps to %s/\n", BPF_PIN_PATH);
	return 0;
}

/* Unpin BPF maps on exit */
static void unpin_ossim_maps(struct scx_ossim *skel)
{
	bpf_map__unpin(skel->maps.vcpu_metadata, BPF_PIN_PATH "/vcpu_metadata");
	bpf_map__unpin(skel->maps.vm_config, BPF_PIN_PATH "/vm_config");
	bpf_map__unpin(skel->maps.vcpu_stats, BPF_PIN_PATH "/vcpu_stats");
}

static void read_stats(struct scx_ossim *skel, __u64 *stats)
{
	int nr_cpus = libbpf_num_possible_cpus();
	assert(nr_cpus > 0);
	__u64 cnts[2][nr_cpus];
	__u32 idx;

	memset(stats, 0, sizeof(stats[0]) * 2);

	for (idx = 0; idx < 2; idx++) {
		int ret, cpu;

		ret = bpf_map_lookup_elem(bpf_map__fd(skel->maps.stats),
					  &idx, cnts[idx]);
		if (ret < 0)
			continue;
		for (cpu = 0; cpu < nr_cpus; cpu++)
			stats[idx] += cnts[idx][cpu];
	}
}

/* vCPU statistics structure matching BPF-side definition */
struct vcpu_stats_bpf {
	__u64 enqueues;
	__u64 dispatches;
	__u64 total_runtime_ns;
	__u64 last_enqueue_ts;
};

/* Print vCPU statistics */
static void print_vcpu_stats(struct scx_ossim *skel)
{
	int vcpu_stats_fd = bpf_map__fd(skel->maps.vcpu_stats);
	pid_t tid = 0, next_tid;
	struct vcpu_stats_bpf stats;
	int count = 0;

	printf("\n=== vCPU Statistics ===\n");
	printf("%-8s %-12s %-12s %-16s\n", "TID", "ENQUEUES", "DISPATCHES", "RUNTIME (ms)");

	while (bpf_map_get_next_key(vcpu_stats_fd, &tid, &next_tid) == 0) {
		if (bpf_map_lookup_elem(vcpu_stats_fd, &next_tid, &stats) == 0) {
			printf("%-8d %-12llu %-12llu %-16.2f\n",
			       next_tid,
			       (unsigned long long)stats.enqueues,
			       (unsigned long long)stats.dispatches,
			       stats.total_runtime_ns / 1000000.0);
			count++;
		}
		tid = next_tid;
	}

	if (count == 0) {
		printf("(no vCPU threads currently registered)\n");
	}
}

int main(int argc, char **argv)
{
	struct scx_ossim *skel;
	struct bpf_link *link;
	__u32 opt;
	__u64 ecode;
	int iteration = 0;

	libbpf_set_print(libbpf_print_fn);
	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	/* Create BPF pin directory */
	if (create_bpf_pin_dir() < 0) {
		return 1;
	}

restart:
	/* Open the BPF skeleton */
	skel = scx_ossim__open();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	/* Reset getopt for restart case */
	optind = 1;

	while ((opt = getopt(argc, argv, "fvh")) != -1) {
		switch (opt) {
		case 'f':
			skel->rodata->fifo_sched = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			fprintf(stderr, help_fmt, basename(argv[0]));
			return opt != 'h';
		}
	}

	/* Load the BPF program */
	if (scx_ossim__load(skel)) {
		fprintf(stderr, "Failed to load BPF skeleton\n");
		scx_ossim__destroy(skel);
		return 1;
	}

	/* Pin maps for QEMU access */
	if (pin_ossim_maps(skel) < 0) {
		fprintf(stderr, "Failed to pin BPF maps\n");
		scx_ossim__destroy(skel);
		return 1;
	}

	/* Attach the struct_ops scheduler */
	link = bpf_map__attach_struct_ops(skel->maps.ossim_ops);
	if (!link) {
		fprintf(stderr, "Failed to attach struct_ops\n");
		unpin_ossim_maps(skel);
		scx_ossim__destroy(skel);
		return 1;
	}

	fprintf(stderr, "scx_ossim scheduler started successfully\n");
	fprintf(stderr, "vCPU-aware scheduling enabled - BPF maps pinned at %s/\n", BPF_PIN_PATH);
	fprintf(stderr, "QEMU can now register vCPU threads via these maps\n");

	while (!exit_req && !UEI_EXITED(skel, uei)) {
		__u64 stats[2];

		read_stats(skel, stats);
		printf("local=%llu global=%llu",
		       (unsigned long long)stats[0],
		       (unsigned long long)stats[1]);
		fflush(stdout);

		/* Print vCPU statistics every 5 seconds */
		if (++iteration % 5 == 0) {
			print_vcpu_stats(skel);
		} else {
			printf("\n");
		}

		sleep(1);
	}

	fprintf(stderr, "\nDetaching scheduler...\n");
	unpin_ossim_maps(skel);
	bpf_link__destroy(link);
	ecode = UEI_REPORT(skel, uei);
	scx_ossim__destroy(skel);

	if (UEI_ECODE_RESTART(ecode))
		goto restart;
	return 0;
}
