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
} __attribute__((packed));

/* Scheduler statistics */
struct ossim_stats {
  uint64_t local_enqueues;  /* Local enqueue count */
  uint64_t global_enqueues; /* Global enqueue count */
};

/* Error codes */
enum ossim_error {
  OSSIM_OK = 0,
  OSSIM_ERR_CONNECT = -1,    /* Connection failed */
  OSSIM_ERR_WRITE = -2,      /* Write failed */
  OSSIM_ERR_READ = -3,       /* Read failed */
  OSSIM_ERR_PARSE = -4,      /* Failed to parse response */
  OSSIM_ERR_INVALID = -5,    /* Invalid parameter */
  OSSIM_ERR_UNKNOWN = -6,    /* Unknown error */
};

#endif /* OSSIM_TYPES_H */
