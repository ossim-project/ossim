/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Meta Platforms, Inc. and affiliates.
 * Copyright (c) 2022 Tejun Heo <tj@kernel.org>
 * Copyright (c) 2022 David Vernet <dvernet@meta.com>
 */
#include <assert.h>
#include <bpf/bpf.h>
#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <scx/common.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "scx_ossim/scx_ossim.bpf.skel.h"
#include "scx_ossim/interface.h"

#define SOCKET_PATH "/tmp/scx_ossim.sock"
#define MAX_CLIENTS 10

const char help_fmt[] =
    "A simple sched_ext scheduler.\n"
    "\n"
    "See the top-level comment in .bpf.c for more details.\n"
    "\n"
    "Usage: %s [-f] [-v] [-s]\n"
    "\n"
    "  -f            Use FIFO scheduling instead of weighted vtime scheduling\n"
    "  -v            Print libbpf debug messages\n"
    "  -h            Display this help and exit\n";

/* Unix socket server context */
typedef struct {
  int server_fd;
  pthread_t thread;
  struct scx_ossim *skel;
  volatile int running;
} SocketServerContext;

static bool verbose;
static volatile int exit_req;
static SocketServerContext socket_ctx;

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
                           va_list args) {
  if (level == LIBBPF_DEBUG && !verbose)
    return 0;
  return vfprintf(stderr, format, args);
}

static void sigint_handler(int simple) { exit_req = 1; }

static void read_stats(struct scx_ossim *skel, __u64 *stats) {
  int nr_cpus = libbpf_num_possible_cpus();
  assert(nr_cpus > 0);
  __u64 cnts[2][nr_cpus];
  __u32 idx;

  memset(stats, 0, sizeof(stats[0]) * 2);

  for (idx = 0; idx < 2; idx++) {
    int ret, cpu;

    ret = bpf_map_lookup_elem(bpf_map__fd(skel->maps.stats), &idx, cnts[idx]);
    if (ret < 0)
      continue;
    for (cpu = 0; cpu < nr_cpus; cpu++)
      stats[idx] += cnts[idx][cpu];
  }
}

/* Handle client request */
static void handle_client(int client_fd, struct scx_ossim *skel) {
  char buffer[256];
  ssize_t bytes_read;
  __u64 stats[2];
  char response[256];

  bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
  if (bytes_read <= 0) {
    return;
  }

  buffer[bytes_read] = '\0';

  /* Remove trailing newline */
  if (bytes_read > 0 && buffer[bytes_read - 1] == '\n') {
    buffer[bytes_read - 1] = '\0';
  }

  if (verbose) {
    printf("Received command: '%s'\n", buffer);
  }

  /* Process commands */
  if (strcmp(buffer, "stats") == 0) {
    read_stats(skel, stats);
    snprintf(response, sizeof(response), "local=%llu global=%llu\n", stats[0],
             stats[1]);
  } else if (strcmp(buffer, "local") == 0) {
    read_stats(skel, stats);
    snprintf(response, sizeof(response), "%llu\n", stats[0]);
  } else if (strcmp(buffer, "global") == 0) {
    read_stats(skel, stats);
    snprintf(response, sizeof(response), "%llu\n", stats[1]);
  } else if (strcmp(buffer, "shutdown") == 0) {
    snprintf(response, sizeof(response), "OK\n");
    exit_req = 1;
  } else if (strcmp(buffer, "help") == 0) {
    snprintf(response, sizeof(response),
             "Available commands:\n"
             "  stats    - Get both local and global enqueue counts\n"
             "  local    - Get local enqueue count\n"
             "  global   - Get global enqueue count\n"
             "  shutdown - Shutdown the scheduler\n"
             "  help     - Show this help message\n");
  } else {
    snprintf(response, sizeof(response),
             "ERROR: Unknown command. Type 'help' for available commands.\n");
  }

  /* Send response back to client */
  write(client_fd, response, strlen(response));
}

/* Socket server thread */
static void *socket_server_thread(void *arg) {
  SocketServerContext *ctx = (SocketServerContext *)arg;
  struct sockaddr_un client_addr;
  socklen_t client_len;
  int client_fd;
  fd_set read_fds;
  struct timeval timeout;

  printf("Unix socket server listening on %s\n", SOCKET_PATH);

  while (ctx->running && !exit_req) {
    FD_ZERO(&read_fds);
    FD_SET(ctx->server_fd, &read_fds);

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int ret = select(ctx->server_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret < 0) {
      if (errno == EINTR)
        continue;
      perror("select");
      break;
    }

    if (ret == 0)
      continue; /* Timeout */

    if (FD_ISSET(ctx->server_fd, &read_fds)) {
      client_len = sizeof(client_addr);
      client_fd =
          accept(ctx->server_fd, (struct sockaddr *)&client_addr, &client_len);

      if (client_fd < 0) {
        if (errno == EINTR)
          continue;
        perror("accept");
        continue;
      }

      handle_client(client_fd, ctx->skel);
      close(client_fd);
    }
  }

  printf("Unix socket server stopped\n");
  return NULL;
}

/* Initialize Unix socket server */
static int init_socket_server(SocketServerContext *ctx,
                              struct scx_ossim *skel) {
  struct sockaddr_un addr;

  ctx->skel = skel;
  ctx->running = 1;

  /* Remove existing socket file */
  unlink(SOCKET_PATH);

  /* Create stream socket */
  ctx->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (ctx->server_fd < 0) {
    perror("socket");
    return -1;
  }

  /* Bind socket */
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

  if (bind(ctx->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(ctx->server_fd);
    return -1;
  }

  /* Set socket permissions to allow non-root access */
  if (chmod(SOCKET_PATH, 0666) < 0) {
    perror("chmod");
    close(ctx->server_fd);
    unlink(SOCKET_PATH);
    return -1;
  }

  /* Listen for connections */
  if (listen(ctx->server_fd, MAX_CLIENTS) < 0) {
    perror("listen");
    close(ctx->server_fd);
    unlink(SOCKET_PATH);
    return -1;
  }

  /* Create server thread */
  if (pthread_create(&ctx->thread, NULL, socket_server_thread, ctx) != 0) {
    perror("pthread_create");
    close(ctx->server_fd);
    unlink(SOCKET_PATH);
    return -1;
  }

  return 0;
}

/* Cleanup Unix socket server */
static void cleanup_socket_server(SocketServerContext *ctx) {
  if (ctx->running) {
    ctx->running = 0;
    pthread_join(ctx->thread, NULL);
  }

  if (ctx->server_fd >= 0) {
    close(ctx->server_fd);
    unlink(SOCKET_PATH);
  }
}

int main(int argc, char **argv) {
  struct scx_ossim *skel;
  struct bpf_link *link;
  __u32 opt;
  __u64 ecode;

  libbpf_set_print(libbpf_print_fn);
  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigint_handler);
restart:
  skel = SCX_OPS_OPEN(ossim_ops, scx_ossim);

  while ((opt = getopt(argc, argv, "fvsh")) != -1) {
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

  SCX_OPS_LOAD(skel, ossim_ops, scx_ossim, uei);
  link = SCX_OPS_ATTACH(skel, ossim_ops, scx_ossim);

  /* Initialize socket server if enabled */
  memset(&socket_ctx, 0, sizeof(socket_ctx));
  socket_ctx.server_fd = -1;

  if (init_socket_server(&socket_ctx, skel) < 0) {
    fprintf(stderr, "Failed to initialize socket server\n");
    exit(1);
  }

  /* Main stats display loop */
  while (!exit_req && !UEI_EXITED(skel, uei)) {
    __u64 stats[2];

    read_stats(skel, stats);
    printf("local=%llu global=%llu\n", stats[0], stats[1]);
    fflush(stdout);
    sleep(1);
  }

  /* Cleanup */
  cleanup_socket_server(&socket_ctx);

  bpf_link__destroy(link);
  ecode = UEI_REPORT(skel, uei);
  scx_ossim__destroy(skel);

  if (UEI_ECODE_RESTART(ecode))
    goto restart;
  return 0;
}
