#ifndef __SCX_OSSIM_H
#define __SCX_OSSIM_H

/* BPF environment already has types from vmlinux.h */
#ifndef __BPF__
#include <linux/types.h>
#else
#include "vmlinux.h"
#endif

enum scx_ossim_stat_type {
  SCX_OSSIM_STAT_LOCAL_ENQUEUE = 0,
  SCX_OSSIM_STAT_GLOBAL_ENQUEUE,
  SCX_OSSIM_STAT_SYSTEM_ENQUEUE,
  SCX_OSSIM_STAT_VCPU_ENQUEUE,
  SCX_OSSIM_STAT_VCPU_NOENQUABLE,
  SCX_OSSIM_STAT_VCPU_ENQUABLE_TOO_EARLY,
  SCX_OSSIM_NUM_STAT,
};

/* In scx_ossim, `simt` serves as the abbreviation for `simulated time` */
#define SCX_OSSIM_SIMT_MAX ((u64)(-1))

/* Maximum number of pending events in the queue */
#define SCX_OSSIM_MAX_PENDING_EVENTS 1024

/* Maximum number of registered vCPUs */
#define SCX_OSSIM_MAX_VCPUS 64
#define SCX_OSSIM_LOG2_MAX_VCPUS 6

/* Event types for the registration queue */
enum scx_ossim_event_type {
  SCX_OSSIM_EVENT_VCPU_REGISTER = 1,
  SCX_OSSIM_EVENT_VCPU_UNREGISTER = 2,
  SCX_OSSIM_EVENT_COORD_ADD = 3,    /* Add vCPU to coordination list */
  SCX_OSSIM_EVENT_COORD_REMOVE = 4, /* Remove vCPU from coordination list */
  SCX_OSSIM_EVENT_COORD_CLEAR = 5,  /* Clear coordination list */
  SCX_OSSIM_EVENT_GLOBAL_COORD_ADD =
      6, /* Add TID to global coordination list */
  SCX_OSSIM_EVENT_GLOBAL_COORD_REMOVE = 7, /* Remove TID from global list */
  SCX_OSSIM_EVENT_GLOBAL_COORD_CLEAR = 8,  /* Clear global coordination list */
};

/* vCPU registration event data */
struct scx_ossim_vcpu_reg_event {
  pid_t tid;
  __u32 vm_id;
  __u32 vcpu_id;
};

/* vCPU unregistration event data */
struct scx_ossim_vcpu_unreg_event {
  pid_t tid;
};

/* Per-vCPU coordination event data */
struct scx_ossim_coord_op_event {
  pid_t vcpu_tid;
  pid_t related_tid; /* For add/remove ops */
};

/* Global coordination event data */
struct scx_ossim_global_coord_event {
  pid_t tid;
};

/* Unified event structure with union for type-specific data */
struct scx_ossim_event {
  u32 event_type; /* enum ossim_event_type */
  union {
    struct scx_ossim_vcpu_reg_event vcpu_reg;
    struct scx_ossim_vcpu_unreg_event vcpu_unreg;
    struct scx_ossim_coord_op_event coord_op;
    struct scx_ossim_global_coord_event global_coord;
  };
};

/* Maximum vCPUs that a single vCPU can coordinate with */
#define SCX_OSSIM_MAX_SYNC_SCOPE_SIZE 64

/* Coordination domain structure (used for both per-vCPU and global lists) */
struct scx_ossim_sync_scope {
  u32 count;                                 /* Number of TIDs in the list */
  pid_t tids[SCX_OSSIM_MAX_SYNC_SCOPE_SIZE]; /* Array of TIDs */
};

/* Metadata stored for each registered vCPU */
struct scx_ossim_vcpu_metadata {
  pid_t tid;
  u32 vm_id;
  u32 vcpu_id;
  u64 simt;
  bool blocked;
  struct scx_ossim_sync_scope sync_scope;
};

#endif /* __SCX_OSSIM_H */
