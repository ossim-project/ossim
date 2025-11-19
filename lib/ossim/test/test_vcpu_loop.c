/* Simple test program that registers as a vCPU and runs a busy loop */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "ossim/ossim_ctl.h"

int main(int argc, char **argv) {
  struct ossim_ctl *ctl;
  struct ossim_ctl_vcpu_registration vcpu;
  int ret;
  pid_t tid;

  /* Get our thread ID (not process ID) */
  tid = syscall(SYS_gettid);
  printf("Thread ID: %d\n", tid);

  /* Connect to scheduler */
  ctl = ossim_ctl_connect(NULL);
  if (!ctl) {
    fprintf(stderr, "Failed to connect: %s\n", strerror(errno));
    return 1;
  }

  /* Register this thread as a vCPU */
  vcpu.vcpu_tid = tid;
  vcpu.vm_id = 0;
  vcpu.vcpu_id = 0;

  ret = ossim_ctl_register_vcpu(ctl, &vcpu);
  if (ret != OSSIM_OK) {
    fprintf(stderr, "Failed to register vCPU: %s\n", ossim_strerror(ret));
    ossim_ctl_disconnect(ctl);
    return 1;
  }

  printf("Registered as vCPU (tid=%d, vm_id=%u, vcpu_id=%u)\n", vcpu.vcpu_tid,
         vcpu.vm_id, vcpu.vcpu_id);

  /* Disconnect - registration persists */
  ossim_ctl_disconnect(ctl);

  /* Run a busy loop for 30 seconds to generate scheduling events */
  printf("Running busy loop for 30 seconds...\n");
  printf("Watch the daemon output to see vcpu counter increment!\n");

  time_t start = time(NULL);
  volatile unsigned long counter = 0;

  while (time(NULL) - start < 30) {
    counter++;
    /* Yield occasionally to actually trigger rescheduling */
    if (counter % 1000000 == 0) {
      usleep(1);
    }
  }

  printf("Done! Counter reached: %lu\n", counter);

  /* Unregister before exiting */
  ctl = ossim_ctl_connect(NULL);
  if (ctl) {
    ret = ossim_ctl_unregister_vcpu(ctl, tid);
    if (ret == OSSIM_OK) {
      printf("Unregistered vCPU\n");
    }
    ossim_ctl_disconnect(ctl);
  }

  return 0;
}
