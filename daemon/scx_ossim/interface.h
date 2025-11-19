#ifndef __SCX_OSSIM_H
#define __SCX_OSSIM_H

/* BPF environment already has types from vmlinux.h */
#ifndef __BPF__
#include <linux/types.h>
#else
#include "vmlinux.h"
#endif

/* Maximum number of pending events in the queue */
#define OSSIM_MAX_PENDING_EVENTS 1024

/* Maximum number of registered vCPUs */
#define OSSIM_MAX_VCPUS 4096

/* Event types for the registration queue */
enum ossim_event_type {
  OSSIM_EVENT_VCPU_REGISTER = 1,
  OSSIM_EVENT_VCPU_UNREGISTER = 2,
  OSSIM_EVENT_COORD_ADD = 3,           /* Add vCPU to coordination list */
  OSSIM_EVENT_COORD_REMOVE = 4,        /* Remove vCPU from coordination list */
  OSSIM_EVENT_COORD_CLEAR = 5,         /* Clear coordination list */
  OSSIM_EVENT_GLOBAL_COORD_ADD = 6,    /* Add TID to global coordination list */
  OSSIM_EVENT_GLOBAL_COORD_REMOVE = 7, /* Remove TID from global list */
  OSSIM_EVENT_GLOBAL_COORD_CLEAR = 8,  /* Clear global coordination list */
};

/* vCPU registration event data */
struct ossim_vcpu_reg_event {
  pid_t tid;
  __u32 vm_id;
  __u32 vcpu_id;
};

/* vCPU unregistration event data */
struct ossim_vcpu_unreg_event {
  pid_t tid;
};

/* Per-vCPU coordination event data */
struct ossim_coord_op_event {
  pid_t vcpu_tid;
  pid_t related_tid; /* For add/remove ops */
};

/* Global coordination event data */
struct ossim_global_coord_event {
  pid_t tid;
};

/* Unified event structure with union for type-specific data */
struct ossim_event {
  __u32 event_type; /* enum ossim_event_type */
  union {
    struct ossim_vcpu_reg_event vcpu_reg;
    struct ossim_vcpu_unreg_event vcpu_unreg;
    struct ossim_coord_op_event coord_op;
    struct ossim_global_coord_event global_coord;
  };
};

/* Maximum vCPUs that a single vCPU can coordinate with */
#define OSSIM_MAX_COORD_VCPUS 8

/* Coordination list structure (used for both per-vCPU and global lists) */
struct ossim_coord_list {
  __u32 count;                       /* Number of TIDs in the list */
  pid_t tids[OSSIM_MAX_COORD_VCPUS]; /* Array of TIDs */
};

/* Metadata stored for each registered vCPU */
struct ossim_bpf_vcpu_metadata {
  pid_t tid;
  __u32 vm_id;
  __u32 vcpu_id;
  __u64 timestamp;                    /* Registration timestamp */
  struct ossim_coord_list coord_list; /* Coordination list for this vCPU */
};

#endif /* __SCX_OSSIM_H */
