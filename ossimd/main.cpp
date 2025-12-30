#include <memory>
#include <string>
#include <thread>

#include "ossim_rpc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

extern "C" {
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ossim/config.h"
#include <linux/ossim.h>
}

const char help_fmt[] =
    "Ossim daemon.\n"
    "\n"
    "Usage: %s [-v] [-d] [-h]\n"
    "\n"
    "  -v            Print verbose debug messages\n"
    "  -d            Disable synchronized scheduling for vCPU threads\n"
    "  -h            Display this help and exit\n";

static bool verbose = false;
static bool sync_enabled = true;
static volatile int exit_req = 0;

static void sigint_handler(int simple) { exit_req = 1; }

/*
 * OssimKernelInterface - Wrapper class for /dev/ossim ioctl operations
 */
class OssimKernelInterface {
private:
  int fd_;
  bool valid_;

public:
  OssimKernelInterface() : fd_(-1), valid_(false) {}

  ~OssimKernelInterface() {
    if (fd_ >= 0) {
      close(fd_);
    }
  }

  bool open() {
    fd_ = ::open(OSSIM_DEVICE_PATH, O_RDWR);
    if (fd_ < 0) {
      fprintf(stderr, "Failed to open %s: %s\n", OSSIM_DEVICE_PATH,
              strerror(errno));
      return false;
    }
    valid_ = true;
    return true;
  }

  bool is_valid() const { return valid_; }
  int fd() const { return fd_; }

  /* vCPU registration */
  int register_vcpu(pid_t tid, uint32_t vm_id, uint32_t vcpu_id) {
    struct ossim_vcpu_params params = {
        .tid = tid,
        .vm_id = vm_id,
        .vcpu_id = vcpu_id,
    };

    int ret = ioctl(fd_, OSSIM_REGISTER_VCPU, &params);
    if (ret < 0) {
      fprintf(stderr, "Failed to register vCPU (tid=%d): %s\n", tid,
              strerror(errno));
      return -errno;
    }

    if (verbose) {
      printf("Registered vCPU: tid=%d vm_id=%u vcpu_id=%u\n", tid, vm_id,
             vcpu_id);
    }

    return 0;
  }

  /* vCPU unregistration */
  int unregister_vcpu(pid_t tid) {
    __s32 tid_param = tid;

    int ret = ioctl(fd_, OSSIM_UNREGISTER_VCPU, &tid_param);
    if (ret < 0) {
      fprintf(stderr, "Failed to unregister vCPU (tid=%d): %s\n", tid,
              strerror(errno));
      return -errno;
    }

    if (verbose) {
      printf("Unregistered vCPU: tid=%d\n", tid);
    }

    return 0;
  }

  /* Query vCPU information */
  int query_vcpu(pid_t tid, struct ossim_vcpu_info *info) {
    memset(info, 0, sizeof(*info));
    info->tid = tid;

    int ret = ioctl(fd_, OSSIM_QUERY_VCPU, info);
    if (ret < 0) {
      return -errno;
    }

    return 0;
  }

  /* Add thread IDs to a synchronization scope */
  int add_sscope(const struct ossim_sscope &sscope, const int32_t *tids,
                 uint32_t count) {
    struct ossim_sscope_params params;
    memset(&params, 0, sizeof(params));

    if (count > OSSIM_MAX_SSCOPE_SIZE) {
      return -EINVAL;
    }

    params.sscope = sscope;
    params.count = count;
    for (uint32_t i = 0; i < count; i++) {
      params.tids[i] = tids[i];
    }

    int ret = ioctl(fd_, OSSIM_ADD_SSCOPE, &params);
    if (ret < 0) {
      return -errno;
    }

    return 0;
  }

  /* Remove thread IDs from a synchronization scope */
  int remove_sscope(const struct ossim_sscope &sscope, const int32_t *tids,
                    uint32_t count) {
    struct ossim_sscope_params params;
    memset(&params, 0, sizeof(params));

    if (count > OSSIM_MAX_SSCOPE_SIZE) {
      return -EINVAL;
    }

    params.sscope = sscope;
    params.count = count;
    for (uint32_t i = 0; i < count; i++) {
      params.tids[i] = tids[i];
    }

    int ret = ioctl(fd_, OSSIM_REMOVE_SSCOPE, &params);
    if (ret < 0) {
      return -errno;
    }

    return 0;
  }

  /* Reset (clear all thread IDs from) a synchronization scope */
  int reset_sscope(const struct ossim_sscope &sscope) {
    int ret = ioctl(fd_, OSSIM_RESET_SSCOPE, &sscope);
    if (ret < 0) {
      return -errno;
    }

    return 0;
  }

  /* Get thread IDs in a synchronization scope */
  int get_sscope(const struct ossim_sscope &sscope,
                 struct ossim_sscope_params *params) {
    memset(params, 0, sizeof(*params));
    params->sscope = sscope;

    int ret = ioctl(fd_, OSSIM_GET_SSCOPE, params);
    if (ret < 0) {
      return -errno;
    }

    return 0;
  }

  /* Convenience: Add to per-vCPU local scope */
  int add_vcpu_local_sscope(pid_t vcpu_tid, const int32_t *tids, uint32_t count) {
    struct ossim_sscope sscope = {OSSIM_SSCOPE_TYPE_VCPU_LOCAL, vcpu_tid};
    return add_sscope(sscope, tids, count);
  }

  /* Convenience: Remove from per-vCPU local scope */
  int remove_vcpu_local_sscope(pid_t vcpu_tid, const int32_t *tids,
                               uint32_t count) {
    struct ossim_sscope sscope = {OSSIM_SSCOPE_TYPE_VCPU_LOCAL, vcpu_tid};
    return remove_sscope(sscope, tids, count);
  }

  /* Convenience: Reset per-vCPU local scope */
  int reset_vcpu_local_sscope(pid_t vcpu_tid) {
    struct ossim_sscope sscope = {OSSIM_SSCOPE_TYPE_VCPU_LOCAL, vcpu_tid};
    return reset_sscope(sscope);
  }

  /* Convenience: Add to custom scope */
  int add_custom_sscope(int32_t scope_id, const int32_t *tids, uint32_t count) {
    struct ossim_sscope sscope = {OSSIM_SSCOPE_TYPE_CUSTOM, scope_id};
    return add_sscope(sscope, tids, count);
  }

  /* Convenience: Remove from custom scope */
  int remove_custom_sscope(int32_t scope_id, const int32_t *tids, uint32_t count) {
    struct ossim_sscope sscope = {OSSIM_SSCOPE_TYPE_CUSTOM, scope_id};
    return remove_sscope(sscope, tids, count);
  }

  /* Convenience: Reset custom scope */
  int reset_custom_sscope(int32_t scope_id) {
    struct ossim_sscope sscope = {OSSIM_SSCOPE_TYPE_CUSTOM, scope_id};
    return reset_sscope(sscope);
  }

  /* Convenience: Get custom scope */
  int get_custom_sscope(int32_t scope_id, struct ossim_sscope_params *params) {
    struct ossim_sscope sscope = {OSSIM_SSCOPE_TYPE_CUSTOM, scope_id};
    return get_sscope(sscope, params);
  }

  /* Get scheduler statistics */
  int get_stats(struct ossim_stats *stats) {
    memset(stats, 0, sizeof(*stats));

    int ret = ioctl(fd_, OSSIM_GET_STATS, stats);
    if (ret < 0) {
      return -errno;
    }

    return 0;
  }

  /* Enable/disable synchronized scheduling */
  int set_sync_enabled(bool enabled) {
    __s32 enabled_param = enabled ? 1 : 0;

    int ret = ioctl(fd_, OSSIM_SET_SYNC_ENABLED, &enabled_param);
    if (ret < 0) {
      return -errno;
    }

    return 0;
  }

  /* Request kernel module shutdown */
  int shutdown() {
    int ret = ioctl(fd_, OSSIM_SHUTDOWN);
    if (ret < 0) {
      return -errno;
    }

    return 0;
  }
};

/* Global kernel interface pointer */
static OssimKernelInterface *global_kernel_interface = nullptr;

// gRPC Service Implementation
class OssimSchedulerServiceImpl final : public ossim::OssimScheduler::Service {
private:
  OssimKernelInterface *kernel_;

public:
  explicit OssimSchedulerServiceImpl(OssimKernelInterface *kernel)
      : kernel_(kernel) {}

  grpc::Status GetStats(grpc::ServerContext *context,
                        const ossim::GetStatsRequest *request,
                        ossim::Stats *response) override {
    struct ossim_stats stats;
    int ret = kernel_->get_stats(&stats);
    if (ret < 0) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "Failed to get stats: " + std::string(strerror(-ret)));
    }

    /* Map kernel stats to gRPC response */
    response->set_local_enqueues(0); /* Not tracked separately in new interface */
    response->set_global_enqueues(0); /* Not tracked separately in new interface */
    response->set_vcpu_enqueues(stats.vcpu_enqueues);
    response->set_system_enqueues(stats.system_enqueues);
    return grpc::Status::OK;
  }

  grpc::Status RegisterVcpu(grpc::ServerContext *context,
                            const ossim::RegisterVcpuRequest *request,
                            ossim::RegisterVcpuResponse *response) override {
    int ret = kernel_->register_vcpu(request->tid(), request->vm_id(),
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
    int ret = kernel_->unregister_vcpu(request->tid());
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
    struct ossim_vcpu_info info;
    int ret = kernel_->query_vcpu(request->tid(), &info);
    if (ret == 0) {
      response->set_success(true);
      response->set_message("vCPU found");
      auto *vcpu_meta = response->mutable_metadata();
      vcpu_meta->set_tid(info.tid);
      vcpu_meta->set_vm_id(info.vm_id);
      vcpu_meta->set_vcpu_id(info.vcpu_id);
      vcpu_meta->set_simt(info.simt);
      vcpu_meta->set_coord_count(info.coord_count);
      for (uint32_t i = 0; i < info.coord_count; i++) {
        vcpu_meta->add_coord_vcpus(info.coord_tids[i]);
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
    int32_t tid = request->related_tid();
    int ret = kernel_->add_vcpu_local_sscope(request->vcpu_tid(), &tid, 1);
    if (ret == 0) {
      response->set_success(true);
      response->set_message("Coordination added successfully");
    } else {
      response->set_success(false);
      response->set_message("Failed to add coordination: " +
                            std::string(strerror(-ret)));
    }
    return grpc::Status::OK;
  }

  grpc::Status
  RemoveCoordination(grpc::ServerContext *context,
                     const ossim::RemoveCoordinationRequest *request,
                     ossim::RemoveCoordinationResponse *response) override {
    int32_t tid = request->related_tid();
    int ret = kernel_->remove_vcpu_local_sscope(request->vcpu_tid(), &tid, 1);
    if (ret == 0) {
      response->set_success(true);
      response->set_message("Coordination removed successfully");
    } else {
      response->set_success(false);
      response->set_message("Failed to remove coordination: " +
                            std::string(strerror(-ret)));
    }
    return grpc::Status::OK;
  }

  grpc::Status
  SetCoordinationList(grpc::ServerContext *context,
                      const ossim::SetCoordinationListRequest *request,
                      ossim::SetCoordinationListResponse *response) override {
    pid_t vcpu_tid = request->vcpu_tid();

    /* Check if the new coordination list is too large */
    if (request->related_tids_size() > OSSIM_MAX_SSCOPE_SIZE) {
      response->set_success(false);
      response->set_message("Coordination list exceeds maximum size");
      return grpc::Status::OK;
    }

    /* Reset existing list first */
    int ret = kernel_->reset_vcpu_local_sscope(vcpu_tid);
    if (ret < 0) {
      response->set_success(false);
      response->set_message("Failed to reset coordination list: " +
                            std::string(strerror(-ret)));
      return grpc::Status::OK;
    }

    /* Add new entries if any */
    if (request->related_tids_size() > 0) {
      std::vector<int32_t> tids(request->related_tids().begin(),
                                request->related_tids().end());
      ret = kernel_->add_vcpu_local_sscope(vcpu_tid, tids.data(), tids.size());
      if (ret < 0) {
        response->set_success(false);
        response->set_message("Failed to add coordination list: " +
                              std::string(strerror(-ret)));
        return grpc::Status::OK;
      }
    }

    response->set_success(true);
    response->set_message("Coordination list updated successfully");
    return grpc::Status::OK;
  }

  grpc::Status GetGlobalCoordinationList(
      grpc::ServerContext *context,
      const ossim::GetGlobalCoordinationListRequest *request,
      ossim::GetGlobalCoordinationListResponse *response) override {
    struct ossim_sscope_params params;
    int ret = kernel_->get_custom_sscope(OSSIM_SSCOPE_GLOBAL_ID, &params);
    if (ret == 0) {
      response->set_success(true);
      response->set_message("Global coordination list retrieved");
      for (uint32_t i = 0; i < params.count; i++) {
        response->add_tids(params.tids[i]);
      }
    } else {
      response->set_success(false);
      response->set_message("Failed to get global coordination list: " +
                            std::string(strerror(-ret)));
    }
    return grpc::Status::OK;
  }

  grpc::Status AddGlobalCoordination(
      grpc::ServerContext *context,
      const ossim::AddGlobalCoordinationRequest *request,
      ossim::AddGlobalCoordinationResponse *response) override {
    int32_t tid = request->tid();
    int ret = kernel_->add_custom_sscope(OSSIM_SSCOPE_GLOBAL_ID, &tid, 1);
    if (ret == 0) {
      response->set_success(true);
      response->set_message("Global coordination added successfully");
    } else {
      response->set_success(false);
      response->set_message("Failed to add global coordination: " +
                            std::string(strerror(-ret)));
    }
    return grpc::Status::OK;
  }

  grpc::Status RemoveGlobalCoordination(
      grpc::ServerContext *context,
      const ossim::RemoveGlobalCoordinationRequest *request,
      ossim::RemoveGlobalCoordinationResponse *response) override {
    int32_t tid = request->tid();
    int ret = kernel_->remove_custom_sscope(OSSIM_SSCOPE_GLOBAL_ID, &tid, 1);
    if (ret == 0) {
      response->set_success(true);
      response->set_message("Global coordination removed successfully");
    } else {
      response->set_success(false);
      response->set_message("Failed to remove global coordination: " +
                            std::string(strerror(-ret)));
    }
    return grpc::Status::OK;
  }

  grpc::Status SetGlobalCoordinationList(
      grpc::ServerContext *context,
      const ossim::SetGlobalCoordinationListRequest *request,
      ossim::SetGlobalCoordinationListResponse *response) override {
    /* Check if the list is too large */
    if (request->tids_size() > OSSIM_MAX_SSCOPE_SIZE) {
      response->set_success(false);
      response->set_message("Global coordination list exceeds maximum size");
      return grpc::Status::OK;
    }

    /* Reset existing list first */
    int ret = kernel_->reset_custom_sscope(OSSIM_SSCOPE_GLOBAL_ID);
    if (ret < 0) {
      response->set_success(false);
      response->set_message("Failed to reset global coordination list: " +
                            std::string(strerror(-ret)));
      return grpc::Status::OK;
    }

    /* Add new entries if any */
    if (request->tids_size() > 0) {
      std::vector<int32_t> tids(request->tids().begin(), request->tids().end());
      ret = kernel_->add_custom_sscope(OSSIM_SSCOPE_GLOBAL_ID, tids.data(), tids.size());
      if (ret < 0) {
        response->set_success(false);
        response->set_message("Failed to add global coordination list: " +
                              std::string(strerror(-ret)));
        return grpc::Status::OK;
      }
    }

    response->set_success(true);
    response->set_message("Global coordination list updated successfully");
    return grpc::Status::OK;
  }

  grpc::Status
  SetSyncEnabled(grpc::ServerContext *context,
                 const ossim::SetSyncEnabledRequest *request,
                 ossim::SetSyncEnabledResponse *response) override {
    int ret = kernel_->set_sync_enabled(request->enabled());
    if (ret == 0) {
      response->set_success(true);
      response->set_message(request->enabled()
                                ? "Synchronized scheduling enabled"
                                : "Synchronized scheduling disabled");
    } else {
      response->set_success(false);
      response->set_message("Failed to set sync enabled: " +
                            std::string(strerror(-ret)));
    }
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

/* gRPC server thread */
void run_grpc_server(OssimKernelInterface *kernel) {
  std::string server_address =
      std::string("unix://") + OSSIMD_DEFAULT_SOCKET_PATH;
  OssimSchedulerServiceImpl service(kernel);

  /* Create directory for socket if it doesn't exist (mkdir -p equivalent) */
  std::string socket_path(OSSIMD_DEFAULT_SOCKET_PATH);
  size_t last_slash = socket_path.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string dir_path = socket_path.substr(0, last_slash);

    /* Create parent directories recursively */
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
    /* Create final directory (only root can write to it for security) */
    if (mkdir(dir_path.c_str(), 0755) != 0 && errno != EEXIST) {
      fprintf(stderr, "Failed to create directory %s: %s\n", dir_path.c_str(),
              strerror(errno));
      return;
    }
    /* Ensure directory has correct permissions even if it already existed */
    chmod(dir_path.c_str(), 0755);
  }

  /* Remove existing Unix socket if it exists */
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

  /* Set socket file permissions to allow all users to connect */
  if (chmod(OSSIMD_DEFAULT_SOCKET_PATH, 0666) != 0) {
    fprintf(stderr, "Warning: Failed to set socket permissions: %s\n",
            strerror(errno));
  }

  printf("gRPC server listening on %s\n", server_address.c_str());

  /* Wait for shutdown request */
  while (!exit_req) {
    sleep(1);
  }

  server->Shutdown();
  printf("gRPC server stopped\n");
}

int main(int argc, char **argv) {
  OssimKernelInterface kernel;
  int opt;

  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigint_handler);

  while ((opt = getopt(argc, argv, "vdh")) != -1) {
    switch (opt) {
    case 'v':
      verbose = true;
      break;
    case 'd':
      sync_enabled = false;
      break;
    case 'h':
      fprintf(stderr, help_fmt, basename(argv[0]));
      return 0;
    default:
      fprintf(stderr, help_fmt, basename(argv[0]));
      return opt != 'h';
    }
  }

  /* Open the kernel device */
  if (!kernel.open()) {
    fprintf(stderr, "Failed to open kernel device. Is the ossim module loaded?\n");
    return 1;
  }

  printf("Connected to %s\n", OSSIM_DEVICE_PATH);

  /* Set initial sync enabled state */
  int ret = kernel.set_sync_enabled(sync_enabled);
  if (ret < 0) {
    fprintf(stderr, "Warning: Failed to set sync enabled state: %s\n",
            strerror(-ret));
  }

  /* Store global kernel interface reference */
  global_kernel_interface = &kernel;

  /* Start gRPC server in a separate thread */
  std::thread grpc_thread(run_grpc_server, &kernel);

  /* Main stats display loop */
  while (!exit_req) {
    struct ossim_stats stats;

    ret = kernel.get_stats(&stats);
    if (ret < 0) {
      fprintf(stderr, "Failed to get stats: %s\n", strerror(-ret));
    } else {
      printf("[global_simt=%lu vcpu_count=%u sync_enabled:%d]\n",
             (unsigned long)stats.global_simt, stats.vcpu_count, sync_enabled);
      printf("vcpu=%llu system=%llu\n",
             (unsigned long long)stats.vcpu_enqueues,
             (unsigned long long)stats.system_enqueues);
    }

    printf("\n");
    fflush(stdout);
    sleep(1);
  }

  /* Wait for gRPC server to finish */
  grpc_thread.join();

  printf("ossimd shutdown complete\n");
  return 0;
}
