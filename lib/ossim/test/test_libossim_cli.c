#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ossim/ossim_ctl.h"

/* Send a command to the server and print the response */
static int send_command(const char *command) {
  struct ossim_ctl *ctl;
  int ret;

  /* Connect to scheduler */
  ctl = ossim_ctl_connect(NULL);
  if (!ctl) {
    fprintf(stderr, "Failed to connect: %s\n", strerror(errno));
    fprintf(stderr, "Make sure scx_ossim is running\n");
    return -1;
  }

  /* Handle common commands with typed API */
  if (strcmp(command, "stats") == 0) {
    struct ossim_stats stats;
    ret = ossim_ctl_get_stats(ctl, &stats);
    if (ret == OSSIM_OK) {
      printf("local=%lu global=%lu vcpu=%lu system=%lu\n", stats.local_enqueues,
             stats.global_enqueues, stats.vcpu_enqueues, stats.system_enqueues);
    } else {
      fprintf(stderr, "Failed to get stats: %s\n", ossim_strerror(ret));
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strcmp(command, "local") == 0) {
    uint64_t count;
    ret = ossim_ctl_get_local_enqueues(ctl, &count);
    if (ret == OSSIM_OK) {
      printf("%lu\n", count);
    } else {
      fprintf(stderr, "Failed to get local count: %s\n", ossim_strerror(ret));
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strcmp(command, "global") == 0) {
    uint64_t count;
    ret = ossim_ctl_get_global_enqueues(ctl, &count);
    if (ret == OSSIM_OK) {
      printf("%lu\n", count);
    } else {
      fprintf(stderr, "Failed to get global count: %s\n", ossim_strerror(ret));
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strcmp(command, "shutdown") == 0) {
    ret = ossim_ctl_shutdown(ctl);
    if (ret == OSSIM_OK) {
      printf("OK\n");
    } else {
      fprintf(stderr, "Failed to shutdown: %s\n", ossim_strerror(ret));
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strcmp(command, "register_vcpu") == 0) {
    /* Parse: register_vcpu <tid> <vm_id> <vcpu_id> */
    struct ossim_ctl_vcpu_registration vcpu;
    vcpu.vcpu_tid = getpid();
    vcpu.vm_id = 0;
    vcpu.vcpu_id = 0;
    ret = ossim_ctl_register_vcpu(ctl, &vcpu);
    if (ret == OSSIM_OK) {
      printf("Registered vCPU: tid=%d vm_id=%u vcpu_id=%u\n", vcpu.vcpu_tid,
             vcpu.vm_id, vcpu.vcpu_id);
    } else {
      fprintf(stderr, "Failed to register vCPU: %s\n", ossim_strerror(ret));
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strcmp(command, "unregister_vcpu") == 0) {
    /* Parse: unregister_vcpu <tid> */
    pid_t tid = getpid();
    ret = ossim_ctl_unregister_vcpu(ctl, tid);
    if (ret == OSSIM_OK) {
      printf("Unregistered vCPU: tid=%d\n", tid);
    } else {
      fprintf(stderr, "Failed to unregister vCPU: %s\n", ossim_strerror(ret));
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strncmp(command, "query_vcpu ", 11) == 0) {
    /* Parse: query_vcpu <tid> */
    pid_t tid;
    if (sscanf(command + 11, "%d", &tid) == 1) {
      struct ossim_vcpu_metadata metadata;
      ret = ossim_ctl_query_vcpu(ctl, tid, &metadata);
      if (ret == OSSIM_OK) {
        printf("vCPU metadata: tid=%d vm_id=%u vcpu_id=%u simt=%lu "
               "coord_count=%u\n",
               metadata.tid, metadata.vm_id, metadata.vcpu_id, metadata.simt,
               metadata.sync_scope.count);
        if (metadata.sync_scope.count > 0) {
          printf("Coordination list:");
          for (uint32_t i = 0; i < metadata.sync_scope.count; i++) {
            printf(" %d", metadata.sync_scope.tids[i]);
          }
          printf("\n");
        }
      } else {
        fprintf(stderr, "Failed to query vCPU: %s\n", ossim_strerror(ret));
        ossim_ctl_disconnect(ctl);
        return -1;
      }
    } else {
      fprintf(stderr, "Invalid format. Use: query_vcpu <tid>\n");
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strncmp(command, "add_coord ", 10) == 0) {
    /* Parse: add_coord <vcpu_tid> <related_tid> */
    pid_t vcpu_tid, related_tid;
    if (sscanf(command + 10, "%d %d", &vcpu_tid, &related_tid) == 2) {
      ret = ossim_ctl_add_coordination(ctl, vcpu_tid, related_tid);
      if (ret == OSSIM_OK) {
        printf("Added vCPU %d to coordination list of vCPU %d\n", related_tid,
               vcpu_tid);
      } else {
        fprintf(stderr, "Failed to add coordination: %s\n",
                ossim_strerror(ret));
        ossim_ctl_disconnect(ctl);
        return -1;
      }
    } else {
      fprintf(stderr,
              "Invalid format. Use: add_coord <vcpu_tid> <related_tid>\n");
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strncmp(command, "remove_coord ", 13) == 0) {
    /* Parse: remove_coord <vcpu_tid> <related_tid> */
    pid_t vcpu_tid, related_tid;
    if (sscanf(command + 13, "%d %d", &vcpu_tid, &related_tid) == 2) {
      ret = ossim_ctl_remove_coordination(ctl, vcpu_tid, related_tid);
      if (ret == OSSIM_OK) {
        printf("Removed vCPU %d from coordination list of vCPU %d\n",
               related_tid, vcpu_tid);
      } else {
        fprintf(stderr, "Failed to remove coordination: %s\n",
                ossim_strerror(ret));
        ossim_ctl_disconnect(ctl);
        return -1;
      }
    } else {
      fprintf(stderr,
              "Invalid format. Use: remove_coord <vcpu_tid> <related_tid>\n");
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strncmp(command, "set_coord_list ", 15) == 0) {
    /* Parse: set_coord_list <vcpu_tid> <tid1> <tid2> ... */
    pid_t vcpu_tid;
    struct ossim_sync_scope sync_scope = {.count = 0};
    const char *arg_start = command + 15;
    char *endptr;

    /* Parse vcpu_tid */
    vcpu_tid = strtol(arg_start, &endptr, 10);
    if (endptr == arg_start) {
      fprintf(
          stderr,
          "Invalid format. Use: set_coord_list <vcpu_tid> <tid1> <tid2> ...\n");
      ossim_ctl_disconnect(ctl);
      return -1;
    }

    /* Parse remaining TIDs */
    arg_start = endptr;
    while (*arg_start && sync_scope.count < OSSIM_MAX_SYNC_SCOPE_SIZE) {
      pid_t tid = strtol(arg_start, &endptr, 10);
      if (endptr == arg_start)
        break;
      sync_scope.tids[sync_scope.count++] = tid;
      arg_start = endptr;
    }

    ret = ossim_ctl_set_coordination_list(ctl, vcpu_tid, &sync_scope);
    if (ret == OSSIM_OK) {
      printf("Set coordination list for vCPU %d with %u entries\n", vcpu_tid,
             sync_scope.count);
    } else {
      fprintf(stderr, "Failed to set coordination list: %s\n",
              ossim_strerror(ret));
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strcmp(command, "get_global_coord") == 0) {
    struct ossim_sync_scope sync_scope;
    ret = ossim_ctl_get_global_coordination_list(ctl, &sync_scope);
    if (ret == OSSIM_OK) {
      printf("Global coordination list (count=%u):", sync_scope.count);
      for (uint32_t i = 0; i < sync_scope.count; i++) {
        printf(" %d", sync_scope.tids[i]);
      }
      printf("\n");
    } else {
      fprintf(stderr, "Failed to get global coordination list: %s\n",
              ossim_strerror(ret));
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strncmp(command, "add_global_coord ", 17) == 0) {
    /* Parse: add_global_coord <tid> */
    pid_t tid;
    if (sscanf(command + 17, "%d", &tid) == 1) {
      ret = ossim_ctl_add_global_coordination(ctl, tid);
      if (ret == OSSIM_OK) {
        printf("Added TID %d to global coordination list\n", tid);
      } else {
        fprintf(stderr, "Failed to add global coordination: %s\n",
                ossim_strerror(ret));
        ossim_ctl_disconnect(ctl);
        return -1;
      }
    } else {
      fprintf(stderr, "Invalid format. Use: add_global_coord <tid>\n");
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strncmp(command, "remove_global_coord ", 20) == 0) {
    /* Parse: remove_global_coord <tid> */
    pid_t tid;
    if (sscanf(command + 20, "%d", &tid) == 1) {
      ret = ossim_ctl_remove_global_coordination(ctl, tid);
      if (ret == OSSIM_OK) {
        printf("Removed TID %d from global coordination list\n", tid);
      } else {
        fprintf(stderr, "Failed to remove global coordination: %s\n",
                ossim_strerror(ret));
        ossim_ctl_disconnect(ctl);
        return -1;
      }
    } else {
      fprintf(stderr, "Invalid format. Use: remove_global_coord <tid>\n");
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strncmp(command, "set_global_coord_list ", 22) == 0) {
    /* Parse: set_global_coord_list <tid1> <tid2> ... */
    struct ossim_sync_scope sync_scope = {.count = 0};
    const char *arg_start = command + 22;
    char *endptr;

    /* Parse TIDs */
    while (*arg_start && sync_scope.count < OSSIM_MAX_SYNC_SCOPE_SIZE) {
      pid_t tid = strtol(arg_start, &endptr, 10);
      if (endptr == arg_start)
        break;
      sync_scope.tids[sync_scope.count++] = tid;
      arg_start = endptr;
    }

    ret = ossim_ctl_set_global_coordination_list(ctl, &sync_scope);
    if (ret == OSSIM_OK) {
      printf("Set global coordination list with %u entries\n",
             sync_scope.count);
    } else {
      fprintf(stderr, "Failed to set global coordination list: %s\n",
              ossim_strerror(ret));
      ossim_ctl_disconnect(ctl);
      return -1;
    }
  } else if (strcmp(command, "help") == 0) {
    printf("Available commands:\n");
    printf("  stats                              Get both local and global "
           "enqueue counts\n");
    printf("  local                              Get local enqueue count\n");
    printf("  global                             Get global enqueue count\n");
    printf("  register_vcpu <tid> <vm_id> <vcpu_id>  Register a vCPU\n");
    printf("  unregister_vcpu <tid>              Unregister a vCPU\n");
    printf("  query_vcpu <tid>                   Query vCPU registration "
           "status\n");
    printf(
        "  add_coord <vcpu_tid> <related_tid> Add vCPU to coordination list\n");
    printf("  remove_coord <vcpu_tid> <related_tid>  Remove vCPU from "
           "coordination "
           "list\n");
    printf("  set_coord_list <vcpu_tid> <tid1> <tid2> ...  Set entire "
           "coordination list\n");
    printf(
        "  get_global_coord                   Get global coordination list\n");
    printf(
        "  add_global_coord <tid>             Add TID to global coordination "
        "list\n");
    printf("  remove_global_coord <tid>          Remove TID from global "
           "coordination list\n");
    printf("  set_global_coord_list <tid1> <tid2> ...  Set entire global "
           "coordination list\n");
    printf("  shutdown                           Shutdown the scheduler\n");
    printf("  help                               Show this help message\n");
  } else {
    fprintf(stderr, "Unknown command: %s\n", command);
    fprintf(stderr, "Type 'help' for available commands\n");
    ossim_ctl_disconnect(ctl);
    return -1;
  }

  ossim_ctl_disconnect(ctl);
  return 0;
}

/* Interactive mode */
static void interactive_mode(void) {
  char command[256];

  printf("scx_ossim socket client - Interactive mode\n");
  printf("Type 'help' for available commands, 'quit' to exit\n\n");

  while (1) {
    printf("> ");
    fflush(stdout);

    if (fgets(command, sizeof(command), stdin) == NULL) {
      break;
    }

    /* Remove trailing newline */
    size_t len = strlen(command);
    if (len > 0 && command[len - 1] == '\n') {
      command[len - 1] = '\0';
    }

    /* Check for exit commands */
    if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
      break;
    }

    /* Skip empty commands */
    if (strlen(command) == 0) {
      continue;
    }

    /* Send command */
    if (send_command(command) < 0) {
      fprintf(stderr, "Failed to communicate with server\n");
      break;
    }
  }

  printf("Goodbye!\n");
}

/* Monitor mode - continuously query stats */
static void monitor_mode(int interval) {
  printf("Monitoring stats every %d second(s). Press Ctrl+C to exit.\n\n",
         interval);

  while (1) {
    printf("=== %ld ===\n", (long)time(NULL));
    if (send_command("stats") < 0) {
      fprintf(stderr, "Failed to get stats\n");
      break;
    }
    printf("\n");
    sleep(interval);
  }
}

static void print_usage(const char *prog_name) {
  printf("Usage: %s [OPTIONS] [COMMAND]\n", prog_name);
  printf("\nOptions:\n");
  printf("  -i              Interactive mode (default if no command given)\n");
  printf(
      "  -m INTERVAL     Monitor mode - query stats every INTERVAL seconds\n");
  printf("  -h              Show this help\n");
  printf("\nCommands (for one-shot mode):\n");
  printf("  stats                              Get both local and global "
         "enqueue counts\n");
  printf("  local                              Get local enqueue count\n");
  printf("  global                             Get global enqueue count\n");
  printf("  register_vcpu <tid> <vm_id> <vcpu_id>  Register a vCPU\n");
  printf("  unregister_vcpu <tid>              Unregister a vCPU\n");
  printf(
      "  query_vcpu <tid>                   Query vCPU registration status\n");
  printf(
      "  add_coord <vcpu_tid> <related_tid> Add vCPU to coordination list\n");
  printf(
      "  remove_coord <vcpu_tid> <related_tid>  Remove vCPU from coordination "
      "list\n");
  printf("  set_coord_list <vcpu_tid> <tid1> <tid2> ...  Set entire "
         "coordination list\n");
  printf("  get_global_coord                   Get global coordination list\n");
  printf("  add_global_coord <tid>             Add TID to global coordination "
         "list\n");
  printf("  remove_global_coord <tid>          Remove TID from global "
         "coordination list\n");
  printf("  set_global_coord_list <tid1> <tid2> ...  Set entire global "
         "coordination list\n");
  printf("  shutdown                           Shutdown the scheduler\n");
  printf(
      "  help                               Show available server commands\n");
  printf("\nExamples:\n");
  printf("  %s stats                           # Query stats once and exit\n",
         prog_name);
  printf("  %s register_vcpu 1234 0 0          # Register vCPU with tid=1234, "
         "vm_id=0, vcpu_id=0\n",
         prog_name);
  printf("  %s query_vcpu 1234                 # Query vCPU with tid=1234\n",
         prog_name);
  printf("  %s add_coord 1234 5678             # Add vCPU 5678 to coordination "
         "list of vCPU 1234\n",
         prog_name);
  printf("  %s set_coord_list 1234 5678 9012   # Set coordination list of vCPU "
         "1234 to [5678, 9012]\n",
         prog_name);
  printf(
      "  %s get_global_coord                # Get global coordination list\n",
      prog_name);
  printf("  %s add_global_coord 1234           # Add TID 1234 to global "
         "coordination list\n",
         prog_name);
  printf("  %s set_global_coord_list 1234 5678 # Set global coordination list "
         "to [1234, 5678]\n",
         prog_name);
  printf(
      "  %s unregister_vcpu 1234            # Unregister vCPU with tid=1234\n",
      prog_name);
  printf("  %s -i                              # Interactive mode\n",
         prog_name);
  printf(
      "  %s -m 2                            # Monitor stats every 2 seconds\n",
      prog_name);
  printf("  %s shutdown                        # Shutdown the scheduler\n",
         prog_name);
}

int main(int argc, char **argv) {
  int opt;
  int monitor_interval = 0;

  /* Parse options */
  while ((opt = getopt(argc, argv, "im:h")) != -1) {
    switch (opt) {
    case 'i':
      /* Interactive mode - this is the default, so just continue */
      break;
    case 'm':
      monitor_interval = atoi(optarg);
      if (monitor_interval <= 0) {
        fprintf(stderr, "Invalid interval: %s\n", optarg);
        return 1;
      }
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  /* Monitor mode */
  if (monitor_interval > 0) {
    monitor_mode(monitor_interval);
    return 0;
  }

  /* One-shot command mode */
  if (optind < argc) {
    /* Concatenate remaining arguments as command */
    char command[256] = "";
    for (int i = optind; i < argc; i++) {
      if (i > optind)
        strcat(command, " ");
      strncat(command, argv[i], sizeof(command) - strlen(command) - 1);
    }

    return send_command(command) == 0 ? 0 : 1;
  }

  /* Default to interactive mode */
  interactive_mode();
  return 0;
}
