#ifndef __SCX_OSSIM_H
#define __SCX_OSSIM_H

/* BPF environment already has types from vmlinux.h */
#ifndef __BPF__
#include <linux/types.h>
#else
#include "vmlinux.h"
#endif

/* Event types for the registration queue */
enum ossim_event_type {
  OSSIM_EVENT_VCPU_REGISTER = 1,
  OSSIM_EVENT_VCPU_UNREGISTER = 2,
};

/* Event structure for vCPU registration/unregistration */
struct ossim_vcpu_event {
  __u32 event_type; /* enum ossim_event_type */
  pid_t tid;        /* Thread ID of the vCPU */
  __u32 vm_id;      /* Virtual machine identifier */
  __u32 vcpu_id;    /* vCPU index within the VM */
};

/* Metadata stored for each registered vCPU */
struct ossim_bpf_vcpu_metadata {
  pid_t tid;
  __u32 vm_id;
  __u32 vcpu_id;
  __u64 timestamp; /* Registration timestamp */
};

/* Maximum number of pending events in the queue */
#define OSSIM_MAX_PENDING_EVENTS 1024

/* Maximum number of registered vCPUs */
#define OSSIM_MAX_VCPUS 4096

#endif /* __SCX_OSSIM_H */
