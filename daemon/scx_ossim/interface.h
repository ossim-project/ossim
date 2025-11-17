/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Common data structures shared between BPF and userspace
 *
 * This header defines structures used by both the BPF scheduler
 * (scx_ossim.bpf.c) and the userspace daemon (scx_ossim.c).
 *
 * Copyright (c) 2025 Ossim Project
 */
#ifndef __SCX_OSSIM_H
#define __SCX_OSSIM_H

/* BPF environment already has types from vmlinux.h */
#ifndef __BPF__
#include <linux/types.h>
#else
#include "vmlinux.h"
#endif

#endif /* __SCX_OSSIM_H */
