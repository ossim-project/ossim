#ifndef OSSIM_TYPES_H
#define OSSIM_TYPES_H

#include <stdint.h>
#include <unistd.h>

/* Command types for RPC protocol (for future use) */
enum ossim_ctl_cmd {
  OSSIM_CTL_QUERY_STATS = 1,
  OSSIM_CTL_REGISTER_VCPU,
};

/* vCPU registration structure */
struct ossim_ctl_vcpu_registration {
  pid_t vcpu_tid;
  uint32_t vm_id;
  uint32_t vcpu_id;
} __attribute__((packed));

/* Maximum vCPUs that a single vCPU can coordinate with */
#define OSSIM_MAX_SYNC_SCOPE_SIZE 32

/* Synchronization scope for managing vCPU relationships */
struct ossim_sync_scope {
  uint32_t count;                        /* Number of TIDs in the list */
  pid_t tids[OSSIM_MAX_SYNC_SCOPE_SIZE]; /* Array of vCPU TIDs */
};

/* vCPU metadata returned from query */
struct ossim_vcpu_metadata {
  pid_t tid;
  uint32_t vm_id;
  uint32_t vcpu_id;
  uint64_t simt;                      /* Current simulated time */
  struct ossim_sync_scope sync_scope; /* Synchronization scope for this vCPU */
};

/* Scheduler statistics */
struct ossim_stats {
  uint64_t local_enqueues;  /* Local enqueue count */
  uint64_t global_enqueues; /* Global enqueue count */
  uint64_t vcpu_enqueues;   /* vCPU enqueue count */
  uint64_t system_enqueues; /* System (non-vCPU) enqueue count */
};

/* Error codes */
enum ossim_error {
  OSSIM_OK = 0,
  OSSIM_ERR_CONNECT = -1, /* Connection failed */
  OSSIM_ERR_WRITE = -2,   /* Write failed */
  OSSIM_ERR_READ = -3,    /* Read failed */
  OSSIM_ERR_PARSE = -4,   /* Failed to parse response */
  OSSIM_ERR_INVALID = -5, /* Invalid parameter */
  OSSIM_ERR_UNKNOWN = -6, /* Unknown error */
};

#endif /* OSSIM_TYPES_H */
