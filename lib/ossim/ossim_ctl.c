/*
 * OSSIM Control Library - RPC Client Implementation
 *
 * This library provides a simple interface to communicate with the
 * scx_ossim scheduler daemon via UNIX domain sockets.
 *
 * Copyright (c) 2025 Ossim Project
 */

#include "ossim/ossim_ctl.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/* Internal connection structure */
struct ossim_ctl {
  int fd;                /* Socket file descriptor */
  char socket_path[108]; /* Socket path (sun_path max length) */
};

/**
 * ossim_ctl_connect - Connect to the scx_ossim scheduler daemon
 */
struct ossim_ctl *ossim_ctl_connect(const char *socket_path) {
  struct ossim_ctl *ctl;
  struct sockaddr_un addr;
  int ret;

  /* Allocate connection handle */
  ctl = calloc(1, sizeof(*ctl));
  if (!ctl) {
    return NULL;
  }

  /* Use default path if not specified */
  if (!socket_path) {
    socket_path = SCX_OSSIM_SOCKET_PATH;
  }

  /* Validate socket path length */
  if (strlen(socket_path) >= sizeof(addr.sun_path)) {
    free(ctl);
    errno = ENAMETOOLONG;
    return NULL;
  }

  /* Create UNIX domain socket */
  ctl->fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (ctl->fd < 0) {
    free(ctl);
    return NULL;
  }

  /* Connect to server */
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

  ret = connect(ctl->fd, (struct sockaddr *)&addr, sizeof(addr));
  if (ret < 0) {
    close(ctl->fd);
    free(ctl);
    return NULL;
  }

  /* Save socket path for debugging */
  strncpy(ctl->socket_path, socket_path, sizeof(ctl->socket_path) - 1);

  return ctl;
}

/**
 * ossim_ctl_disconnect - Disconnect from the scheduler daemon
 */
void ossim_ctl_disconnect(struct ossim_ctl *ctl) {
  if (!ctl) {
    return;
  }

  if (ctl->fd >= 0) {
    close(ctl->fd);
  }

  free(ctl);
}

/**
 * ossim_ctl_send_command - Send a raw command to the scheduler
 */
int ossim_ctl_send_command(struct ossim_ctl *ctl, const char *command,
                           char *response, size_t response_size) {
  char buffer[OSSIM_RPC_BUFFER_SIZE];
  ssize_t bytes_written, bytes_read;
  size_t command_len;

  if (!ctl || !command) {
    return OSSIM_ERR_INVALID;
  }

  /* Prepare command (add newline if not present) */
  command_len = strlen(command);
  if (command_len >= sizeof(buffer) - 2) {
    return OSSIM_ERR_INVALID;
  }

  strncpy(buffer, command, sizeof(buffer) - 2);
  if (buffer[command_len - 1] != '\n') {
    buffer[command_len] = '\n';
    buffer[command_len + 1] = '\0';
    command_len++;
  }

  /* Send command */
  bytes_written = write(ctl->fd, buffer, command_len);
  if (bytes_written < 0) {
    return OSSIM_ERR_WRITE;
  }

  /* Read response if requested */
  if (response && response_size > 0) {
    memset(response, 0, response_size);
    bytes_read = read(ctl->fd, response, response_size - 1);
    if (bytes_read < 0) {
      return OSSIM_ERR_READ;
    }
    response[bytes_read] = '\0';

    /* Remove trailing newline */
    if (bytes_read > 0 && response[bytes_read - 1] == '\n') {
      response[bytes_read - 1] = '\0';
    }
  }

  return OSSIM_OK;
}

/**
 * ossim_ctl_get_stats - Get both local and global enqueue statistics
 */
int ossim_ctl_get_stats(struct ossim_ctl *ctl, struct ossim_stats *stats) {
  char response[OSSIM_RPC_BUFFER_SIZE];
  int ret;

  if (!ctl || !stats) {
    return OSSIM_ERR_INVALID;
  }

  ret = ossim_ctl_send_command(ctl, "stats", response, sizeof(response));
  if (ret != OSSIM_OK) {
    return ret;
  }

  /* Parse response: "local=<num> global=<num>" */
  ret = sscanf(response, "local=%lu global=%lu", &stats->local_enqueues,
               &stats->global_enqueues);
  if (ret != 2) {
    return OSSIM_ERR_PARSE;
  }

  return OSSIM_OK;
}

/**
 * ossim_ctl_get_local_enqueues - Get local enqueue count
 */
int ossim_ctl_get_local_enqueues(struct ossim_ctl *ctl, uint64_t *count) {
  char response[OSSIM_RPC_BUFFER_SIZE];
  int ret;

  if (!ctl || !count) {
    return OSSIM_ERR_INVALID;
  }

  ret = ossim_ctl_send_command(ctl, "local", response, sizeof(response));
  if (ret != OSSIM_OK) {
    return ret;
  }

  /* Parse response: "<num>" */
  ret = sscanf(response, "%lu", count);
  if (ret != 1) {
    return OSSIM_ERR_PARSE;
  }

  return OSSIM_OK;
}

/**
 * ossim_ctl_get_global_enqueues - Get global enqueue count
 */
int ossim_ctl_get_global_enqueues(struct ossim_ctl *ctl, uint64_t *count) {
  char response[OSSIM_RPC_BUFFER_SIZE];
  int ret;

  if (!ctl || !count) {
    return OSSIM_ERR_INVALID;
  }

  ret = ossim_ctl_send_command(ctl, "global", response, sizeof(response));
  if (ret != OSSIM_OK) {
    return ret;
  }

  /* Parse response: "<num>" */
  ret = sscanf(response, "%lu", count);
  if (ret != 1) {
    return OSSIM_ERR_PARSE;
  }

  return OSSIM_OK;
}

/**
 * ossim_ctl_shutdown - Request scheduler to shutdown
 */
int ossim_ctl_shutdown(struct ossim_ctl *ctl) {
  char response[OSSIM_RPC_BUFFER_SIZE];
  int ret;

  if (!ctl) {
    return OSSIM_ERR_INVALID;
  }

  ret = ossim_ctl_send_command(ctl, "shutdown", response, sizeof(response));
  if (ret != OSSIM_OK) {
    return ret;
  }

  /* Check for "OK" response */
  if (strncmp(response, "OK", 2) != 0) {
    return OSSIM_ERR_UNKNOWN;
  }

  return OSSIM_OK;
}

/**
 * ossim_ctl_register_vcpu - Register a vCPU thread
 */
int ossim_ctl_register_vcpu(struct ossim_ctl *ctl,
                            struct ossim_ctl_vcpu_registration *vcpu) {
  char command[OSSIM_RPC_BUFFER_SIZE];
  char response[OSSIM_RPC_BUFFER_SIZE];
  int ret;

  if (!ctl || !vcpu) {
    return OSSIM_ERR_INVALID;
  }

  /* Format command: register_vcpu <tid> <vm_id> <vcpu_id> */
  snprintf(command, sizeof(command), "register_vcpu %d %u %u", vcpu->vcpu_tid,
           vcpu->vm_id, vcpu->vcpu_id);

  ret = ossim_ctl_send_command(ctl, command, response, sizeof(response));
  if (ret != OSSIM_OK) {
    return ret;
  }

  /* Check for "OK:" response */
  if (strncmp(response, "OK:", 3) != 0) {
    return OSSIM_ERR_UNKNOWN;
  }

  return OSSIM_OK;
}

/**
 * ossim_ctl_unregister_vcpu - Unregister a vCPU thread
 */
int ossim_ctl_unregister_vcpu(struct ossim_ctl *ctl, pid_t tid) {
  char command[OSSIM_RPC_BUFFER_SIZE];
  char response[OSSIM_RPC_BUFFER_SIZE];
  int ret;

  if (!ctl) {
    return OSSIM_ERR_INVALID;
  }

  /* Format command: unregister_vcpu <tid> */
  snprintf(command, sizeof(command), "unregister_vcpu %d", tid);

  ret = ossim_ctl_send_command(ctl, command, response, sizeof(response));
  if (ret != OSSIM_OK) {
    return ret;
  }

  /* Check for "OK:" response */
  if (strncmp(response, "OK:", 3) != 0) {
    return OSSIM_ERR_UNKNOWN;
  }

  return OSSIM_OK;
}

/**
 * ossim_ctl_query_vcpu - Query vCPU registration status
 */
int ossim_ctl_query_vcpu(struct ossim_ctl *ctl, pid_t tid,
                         struct ossim_vcpu_metadata *metadata) {
  char command[OSSIM_RPC_BUFFER_SIZE];
  char response[OSSIM_RPC_BUFFER_SIZE];
  int ret;

  if (!ctl || !metadata) {
    return OSSIM_ERR_INVALID;
  }

  /* Format command: query_vcpu <tid> */
  snprintf(command, sizeof(command), "query_vcpu %d", tid);

  ret = ossim_ctl_send_command(ctl, command, response, sizeof(response));
  if (ret != OSSIM_OK) {
    return ret;
  }

  /* Parse response: "OK: tid=<tid> vm_id=<vm_id> vcpu_id=<vcpu_id>
   * timestamp=<timestamp>" */
  ret = sscanf(response, "OK: tid=%d vm_id=%u vcpu_id=%u timestamp=%lu",
               &metadata->tid, &metadata->vm_id, &metadata->vcpu_id,
               &metadata->timestamp);
  if (ret != 4) {
    return OSSIM_ERR_PARSE;
  }

  return OSSIM_OK;
}

/**
 * ossim_strerror - Get error message for error code
 */
const char *ossim_strerror(int error) {
  switch (error) {
  case OSSIM_OK:
    return "Success";
  case OSSIM_ERR_CONNECT:
    return "Connection failed";
  case OSSIM_ERR_WRITE:
    return "Write failed";
  case OSSIM_ERR_READ:
    return "Read failed";
  case OSSIM_ERR_PARSE:
    return "Failed to parse response";
  case OSSIM_ERR_INVALID:
    return "Invalid parameter";
  case OSSIM_ERR_UNKNOWN:
    return "Unknown error";
  default:
    return "Invalid error code";
  }
}
