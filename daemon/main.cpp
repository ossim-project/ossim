#include <memory>
#include <string>
#include <thread>

#include "ossim_rpc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

extern "C" {
#include <assert.h>
#include <bpf/bpf.h>
#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <scx/common.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ossim/config.h"
#include "scx_ossim/interface.h"
#include "scx_ossim/scx_ossim.bpf.skel.h"
}

const char help_fmt[] =
    "A sched_ext scheduler with gRPC interface.\n"
    "\n"
    "See the top-level comment in .bpf.c for more details.\n"
    "\n"
    "Usage: %s [-f] [-v] [-h]\n"
    "\n"
    "  -f            Use FIFO scheduling instead of weighted vtime scheduling\n"
    "  -v            Print libbpf debug messages\n"
    "  -h            Display this help and exit\n";

static bool verbose = false;
static volatile int exit_req = 0;
static struct scx_ossim *global_skel = nullptr;

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
  __u64 *cnts[4];
  cnts[0] = (__u64 *)calloc(nr_cpus, sizeof(__u64));
  cnts[1] = (__u64 *)calloc(nr_cpus, sizeof(__u64));
  cnts[2] = (__u64 *)calloc(nr_cpus, sizeof(__u64));
  cnts[3] = (__u64 *)calloc(nr_cpus, sizeof(__u64));
  __u32 idx;

  memset(stats, 0, sizeof(stats[0]) * 4);

  for (idx = 0; idx < 4; idx++) {
    int ret, cpu;

    ret = bpf_map_lookup_elem(bpf_map__fd(skel->maps.stats), &idx, cnts[idx]);
    if (ret < 0)
      continue;
    for (cpu = 0; cpu < nr_cpus; cpu++)
      stats[idx] += cnts[idx][cpu];
  }

  free(cnts[0]);
  free(cnts[1]);
  free(cnts[2]);
  free(cnts[3]);
}

/* Print registered vCPUs */
static void print_registered_vcpus(struct scx_ossim *skel) {
  pid_t key, next_key;
  struct ossim_bpf_vcpu_metadata metadata;
  int count = 0;
  int map_fd = bpf_map__fd(skel->maps.vcpu_registry);

  printf("Registered vCPUs:\n");

  /* Iterate through all keys in the hash map */
  key = 0;
  while (bpf_map_get_next_key(map_fd, &key, &next_key) == 0) {
    if (bpf_map_lookup_elem(map_fd, &next_key, &metadata) == 0) {
      printf("  [%d] tid=%d vm_id=%u vcpu_id=%u timestamp=%llu\n", count,
             metadata.tid, metadata.vm_id, metadata.vcpu_id,
             metadata.timestamp);
      count++;
    }
    key = next_key;
  }

  if (count == 0) {
    printf("  (none)\n");
  }
}

/* Register a vCPU by pushing an event to the queue */
static int register_vcpu(struct scx_ossim *skel, pid_t tid, __u32 vm_id,
                         __u32 vcpu_id) {
  struct ossim_event event = {
      .event_type = OSSIM_EVENT_VCPU_REGISTER,
      .vcpu_reg =
          {
              .tid = tid,
              .vm_id = vm_id,
              .vcpu_id = vcpu_id,
          },
  };

  int ret = bpf_map_update_elem(bpf_map__fd(skel->maps.event_queue), NULL,
                                &event, BPF_ANY);
  if (ret < 0) {
    fprintf(stderr, "Failed to push vCPU registration event: %s\n",
            strerror(errno));
    return ret;
  }

  if (verbose) {
    printf("Registered vCPU: tid=%d vm_id=%u vcpu_id=%u\n", tid, vm_id,
           vcpu_id);
  }

  return 0;
}

/* Unregister a vCPU by pushing an event to the queue */
static int unregister_vcpu(struct scx_ossim *skel, pid_t tid) {
  struct ossim_event event = {
      .event_type = OSSIM_EVENT_VCPU_UNREGISTER,
      .vcpu_unreg =
          {
              .tid = tid,
          },
  };

  int ret = bpf_map_update_elem(bpf_map__fd(skel->maps.event_queue), NULL,
                                &event, BPF_ANY);
  if (ret < 0) {
    fprintf(stderr, "Failed to push vCPU unregistration event: %s\n",
            strerror(errno));
    return ret;
  }

  if (verbose) {
    printf("Unregistered vCPU: tid=%d\n", tid);
  }

  return 0;
}

/* Query vCPU registration status */
static int query_vcpu(struct scx_ossim *skel, pid_t tid,
                      struct ossim_bpf_vcpu_metadata *metadata) {
  int ret = bpf_map_lookup_elem(bpf_map__fd(skel->maps.vcpu_registry), &tid,
                                metadata);
  return ret;
}

// gRPC Service Implementation
class OssimSchedulerServiceImpl final : public ossim::OssimScheduler::Service {
private:
  struct scx_ossim *skel_;

public:
  explicit OssimSchedulerServiceImpl(struct scx_ossim *skel) : skel_(skel) {}

  grpc::Status GetStats(grpc::ServerContext *context,
                        const ossim::GetStatsRequest *request,
                        ossim::Stats *response) override {
    __u64 stats[4];
    read_stats(skel_, stats);
    response->set_local_enqueues(stats[0]);
    response->set_global_enqueues(stats[1]);
    response->set_vcpu_enqueues(stats[2]);
    response->set_system_enqueues(stats[3]);
    return grpc::Status::OK;
  }

  grpc::Status RegisterVcpu(grpc::ServerContext *context,
                            const ossim::RegisterVcpuRequest *request,
                            ossim::RegisterVcpuResponse *response) override {
    int ret = register_vcpu(skel_, request->tid(), request->vm_id(),
                            request->vcpu_id());
    if (ret == 0) {
      response->set_success(true);
      response->set_message("vCPU registered successfully");
    } else {
      response->set_success(false);
      response->set_message("Failed to register vCPU: " +
                            std::string(strerror(-ret)));
    }
    return grpc::Status::OK;
  }

  grpc::Status
  UnregisterVcpu(grpc::ServerContext *context,
                 const ossim::UnregisterVcpuRequest *request,
                 ossim::UnregisterVcpuResponse *response) override {
    int ret = unregister_vcpu(skel_, request->tid());
    if (ret == 0) {
      response->set_success(true);
      response->set_message("vCPU unregistered successfully");
    } else {
      response->set_success(false);
      response->set_message("Failed to unregister vCPU: " +
                            std::string(strerror(-ret)));
    }
    return grpc::Status::OK;
  }

  grpc::Status QueryVcpu(grpc::ServerContext *context,
                         const ossim::QueryVcpuRequest *request,
                         ossim::QueryVcpuResponse *response) override {
    struct ossim_bpf_vcpu_metadata metadata;
    int ret = query_vcpu(skel_, request->tid(), &metadata);
    if (ret == 0) {
      response->set_success(true);
      response->set_message("vCPU found");
      auto *vcpu_meta = response->mutable_metadata();
      vcpu_meta->set_tid(metadata.tid);
      vcpu_meta->set_vm_id(metadata.vm_id);
      vcpu_meta->set_vcpu_id(metadata.vcpu_id);
      vcpu_meta->set_timestamp(metadata.timestamp);
      vcpu_meta->set_coord_count(metadata.coord_list.count);
      for (__u32 i = 0; i < metadata.coord_list.count; i++) {
        vcpu_meta->add_coord_vcpus(metadata.coord_list.tids[i]);
      }
    } else {
      response->set_success(false);
      response->set_message("vCPU not found");
    }
    return grpc::Status::OK;
  }

  grpc::Status
  AddCoordination(grpc::ServerContext *context,
                  const ossim::AddCoordinationRequest *request,
                  ossim::AddCoordinationResponse *response) override {
    struct ossim_event event = {
        .event_type = OSSIM_EVENT_COORD_ADD,
        .coord_op =
            {
                .vcpu_tid = request->vcpu_tid(),
                .related_tid = request->related_tid(),
            },
    };

    int ret = bpf_map_update_elem(bpf_map__fd(skel_->maps.event_queue), NULL,
                                  &event, BPF_ANY);
    if (ret < 0) {
      response->set_success(false);
      response->set_message("Failed to push coordination event: " +
                            std::string(strerror(errno)));
      return grpc::Status::OK;
    }

    response->set_success(true);
    response->set_message("Coordination add event queued");
    return grpc::Status::OK;
  }

  grpc::Status
  RemoveCoordination(grpc::ServerContext *context,
                     const ossim::RemoveCoordinationRequest *request,
                     ossim::RemoveCoordinationResponse *response) override {
    struct ossim_event event = {
        .event_type = OSSIM_EVENT_COORD_REMOVE,
        .coord_op =
            {
                .vcpu_tid = request->vcpu_tid(),
                .related_tid = request->related_tid(),
            },
    };

    int ret = bpf_map_update_elem(bpf_map__fd(skel_->maps.event_queue), NULL,
                                  &event, BPF_ANY);
    if (ret < 0) {
      response->set_success(false);
      response->set_message("Failed to push coordination event: " +
                            std::string(strerror(errno)));
      return grpc::Status::OK;
    }

    response->set_success(true);
    response->set_message("Coordination remove event queued");
    return grpc::Status::OK;
  }

  grpc::Status
  SetCoordinationList(grpc::ServerContext *context,
                      const ossim::SetCoordinationListRequest *request,
                      ossim::SetCoordinationListResponse *response) override {
    pid_t vcpu_tid = request->vcpu_tid();
    int queue_fd = bpf_map__fd(skel_->maps.event_queue);

    // Check if the new coordination list is too large
    if (request->related_tids_size() > OSSIM_MAX_COORD_VCPUS) {
      response->set_success(false);
      response->set_message("Coordination list exceeds maximum size");
      return grpc::Status::OK;
    }

    // First, push a CLEAR event
    struct ossim_event clear_event = {
        .event_type = OSSIM_EVENT_COORD_CLEAR,
        .coord_op =
            {
                .vcpu_tid = vcpu_tid,
                .related_tid = 0,
            },
    };
    int ret = bpf_map_update_elem(queue_fd, NULL, &clear_event, BPF_ANY);
    if (ret < 0) {
      response->set_success(false);
      response->set_message("Failed to push clear event: " +
                            std::string(strerror(errno)));
      return grpc::Status::OK;
    }

    // Then, push ADD events for each TID in the new list
    for (int i = 0; i < request->related_tids_size(); i++) {
      struct ossim_event add_event = {
          .event_type = OSSIM_EVENT_COORD_ADD,
          .coord_op =
              {
                  .vcpu_tid = vcpu_tid,
                  .related_tid = request->related_tids(i),
              },
      };
      ret = bpf_map_update_elem(queue_fd, NULL, &add_event, BPF_ANY);
      if (ret < 0) {
        response->set_success(false);
        response->set_message("Failed to push add event: " +
                              std::string(strerror(errno)));
        return grpc::Status::OK;
      }
    }

    response->set_success(true);
    response->set_message("Coordination list update events queued");
    return grpc::Status::OK;
  }

  grpc::Status GetGlobalCoordinationList(
      grpc::ServerContext *context,
      const ossim::GetGlobalCoordinationListRequest *request,
      ossim::GetGlobalCoordinationListResponse *response) override {
    struct ossim_coord_list coord_list;
    __u32 key = 0;
    int map_fd = bpf_map__fd(skel_->maps.global_coord_list);

    // Look up the global coordination list
    int ret = bpf_map_lookup_elem(map_fd, &key, &coord_list);
    if (ret != 0) {
      response->set_success(false);
      response->set_message("Failed to get global coordination list: " +
                            std::string(strerror(errno)));
      return grpc::Status::OK;
    }

    response->set_success(true);
    response->set_message("Global coordination list retrieved");
    for (__u32 i = 0; i < coord_list.count; i++) {
      response->add_tids(coord_list.tids[i]);
    }

    return grpc::Status::OK;
  }

  grpc::Status AddGlobalCoordination(
      grpc::ServerContext *context,
      const ossim::AddGlobalCoordinationRequest *request,
      ossim::AddGlobalCoordinationResponse *response) override {
    struct ossim_event event = {
        .event_type = OSSIM_EVENT_GLOBAL_COORD_ADD,
        .global_coord =
            {
                .tid = request->tid(),
            },
    };

    int ret = bpf_map_update_elem(bpf_map__fd(skel_->maps.event_queue), NULL,
                                  &event, BPF_ANY);
    if (ret < 0) {
      response->set_success(false);
      response->set_message("Failed to push global coordination event: " +
                            std::string(strerror(errno)));
      return grpc::Status::OK;
    }

    response->set_success(true);
    response->set_message("Global coordination add event queued");
    return grpc::Status::OK;
  }

  grpc::Status RemoveGlobalCoordination(
      grpc::ServerContext *context,
      const ossim::RemoveGlobalCoordinationRequest *request,
      ossim::RemoveGlobalCoordinationResponse *response) override {
    struct ossim_event event = {
        .event_type = OSSIM_EVENT_GLOBAL_COORD_REMOVE,
        .global_coord =
            {
                .tid = request->tid(),
            },
    };

    int ret = bpf_map_update_elem(bpf_map__fd(skel_->maps.event_queue), NULL,
                                  &event, BPF_ANY);
    if (ret < 0) {
      response->set_success(false);
      response->set_message("Failed to push global coordination event: " +
                            std::string(strerror(errno)));
      return grpc::Status::OK;
    }

    response->set_success(true);
    response->set_message("Global coordination remove event queued");
    return grpc::Status::OK;
  }

  grpc::Status SetGlobalCoordinationList(
      grpc::ServerContext *context,
      const ossim::SetGlobalCoordinationListRequest *request,
      ossim::SetGlobalCoordinationListResponse *response) override {
    int queue_fd = bpf_map__fd(skel_->maps.event_queue);

    // Check if the list is too large
    if (request->tids_size() > OSSIM_MAX_COORD_VCPUS) {
      response->set_success(false);
      response->set_message("Global coordination list exceeds maximum size");
      return grpc::Status::OK;
    }

    // First, push a CLEAR event
    struct ossim_event clear_event = {
        .event_type = OSSIM_EVENT_GLOBAL_COORD_CLEAR,
        .global_coord =
            {
                .tid = 0,
            },
    };
    int ret = bpf_map_update_elem(queue_fd, NULL, &clear_event, BPF_ANY);
    if (ret < 0) {
      response->set_success(false);
      response->set_message("Failed to push clear event: " +
                            std::string(strerror(errno)));
      return grpc::Status::OK;
    }

    // Then, push ADD events for each TID in the new list
    for (int i = 0; i < request->tids_size(); i++) {
      struct ossim_event add_event = {
          .event_type = OSSIM_EVENT_GLOBAL_COORD_ADD,
          .global_coord =
              {
                  .tid = request->tids(i),
              },
      };
      ret = bpf_map_update_elem(queue_fd, NULL, &add_event, BPF_ANY);
      if (ret < 0) {
        response->set_success(false);
        response->set_message("Failed to push add event: " +
                              std::string(strerror(errno)));
        return grpc::Status::OK;
      }
    }

    response->set_success(true);
    response->set_message("Global coordination list update events queued");
    return grpc::Status::OK;
  }

  grpc::Status Shutdown(grpc::ServerContext *context,
                        const ossim::ShutdownRequest *request,
                        ossim::ShutdownResponse *response) override {
    response->set_success(true);
    response->set_message("Shutdown initiated");
    exit_req = 1;
    return grpc::Status::OK;
  }
};

// gRPC server thread
void run_grpc_server(struct scx_ossim *skel) {
  std::string server_address =
      std::string("unix://") + OSSIMD_DEFAULT_SOCKET_PATH;
  OssimSchedulerServiceImpl service(skel);

  // Create directory for socket if it doesn't exist (mkdir -p equivalent)
  std::string socket_path(OSSIMD_DEFAULT_SOCKET_PATH);
  size_t last_slash = socket_path.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string dir_path = socket_path.substr(0, last_slash);

    // Create parent directories recursively
    std::string current_path;
    for (size_t i = 0; i < dir_path.length(); i++) {
      if (dir_path[i] == '/' && i > 0) {
        current_path = dir_path.substr(0, i);
        if (mkdir(current_path.c_str(), 0755) != 0 && errno != EEXIST) {
          fprintf(stderr, "Failed to create directory %s: %s\n",
                  current_path.c_str(), strerror(errno));
          return;
        }
      }
    }
    // Create final directory (only root can write to it for security)
    if (mkdir(dir_path.c_str(), 0755) != 0 && errno != EEXIST) {
      fprintf(stderr, "Failed to create directory %s: %s\n", dir_path.c_str(),
              strerror(errno));
      return;
    }
    // Ensure directory has correct permissions even if it already existed
    chmod(dir_path.c_str(), 0755);
  }

  // Remove existing Unix socket if it exists
  unlink(OSSIMD_DEFAULT_SOCKET_PATH);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  if (!server) {
    fprintf(stderr, "Failed to start gRPC server on %s\n",
            server_address.c_str());
    return;
  }

  // Set socket file permissions to allow all users to connect
  if (chmod(OSSIMD_DEFAULT_SOCKET_PATH, 0666) != 0) {
    fprintf(stderr, "Warning: Failed to set socket permissions: %s\n",
            strerror(errno));
  }

  printf("gRPC server listening on %s\n", server_address.c_str());

  // Wait for shutdown request
  while (!exit_req) {
    sleep(1);
  }

  server->Shutdown();
  printf("gRPC server stopped\n");
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

  while ((opt = getopt(argc, argv, "fvh")) != -1) {
    switch (opt) {
    case 'f':
      skel->rodata->fifo_sched = true;
      break;
    case 'v':
      verbose = true;
      break;
    case 'h':
      fprintf(stderr, help_fmt, basename(argv[0]));
      return 0;
    default:
      fprintf(stderr, help_fmt, basename(argv[0]));
      return opt != 'h';
    }
  }

  SCX_OPS_LOAD(skel, ossim_ops, scx_ossim, uei);
  link = SCX_OPS_ATTACH(skel, ossim_ops, scx_ossim);

  /* Store global skel reference for signal handlers */
  global_skel = skel;

  /* Start gRPC server in a separate thread */
  std::thread grpc_thread(run_grpc_server, skel);

  /* Main stats display loop */
  while (!exit_req && !UEI_EXITED(skel, uei)) {
    __u64 stats[4];

    read_stats(skel, stats);
    printf("local=%llu global=%llu vcpu=%llu system=%llu\n", stats[0], stats[1],
           stats[2], stats[3]);
    print_registered_vcpus(skel);
    printf("\n");
    fflush(stdout);
    sleep(1);
  }

  /* Wait for gRPC server to finish */
  grpc_thread.join();

  bpf_link__destroy(link);
  ecode = UEI_REPORT(skel, uei);
  scx_ossim__destroy(skel);

  if (UEI_ECODE_RESTART(ecode))
    goto restart;
  return 0;
}
