#include <memory>
#include <string>

#include "ossim_rpc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

extern "C" {
#include "ossim/ossim_ctl.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
}

struct ossim_ctl {
  std::unique_ptr<ossim::OssimScheduler::Stub> stub;
  std::string server_address;
};

extern "C" {

/**
 * ossim_ctl_connect - Connect to the scx_ossim scheduler daemon
 */
struct ossim_ctl *ossim_ctl_connect(const char *socket_path) {
  struct ossim_ctl *ctl;

  try {
    /* Allocate connection handle */
    ctl = new ossim_ctl();

    /* Use default path if not specified */
    if (!socket_path) {
      socket_path = OSSIMD_DEFAULT_SOCKET_PATH;
    }

    /* Create gRPC channel */
    std::string address = std::string("unix://") + socket_path;
    auto channel =
        grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    ctl->stub = ossim::OssimScheduler::NewStub(channel);
    ctl->server_address = address;

    return ctl;
  } catch (...) {
    errno = ECONNREFUSED;
    return nullptr;
  }
}

/**
 * ossim_ctl_disconnect - Disconnect from the scheduler daemon
 */
void ossim_ctl_disconnect(struct ossim_ctl *ctl) {
  if (!ctl) {
    return;
  }

  delete ctl;
}

/**
 * ossim_ctl_get_stats - Get both local and global enqueue statistics
 */
int ossim_ctl_get_stats(struct ossim_ctl *ctl, struct ossim_stats *stats) {
  if (!ctl || !stats) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::GetStatsRequest request;
    ossim::Stats response;

    grpc::Status status = ctl->stub->GetStats(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_READ;
    }

    stats->local_enqueues = response.local_enqueues();
    stats->global_enqueues = response.global_enqueues();
    stats->vcpu_enqueues = response.vcpu_enqueues();
    stats->system_enqueues = response.system_enqueues();

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_get_local_enqueues - Get local enqueue count
 */
int ossim_ctl_get_local_enqueues(struct ossim_ctl *ctl, uint64_t *count) {
  struct ossim_stats stats;
  int ret = ossim_ctl_get_stats(ctl, &stats);
  if (ret == OSSIM_OK) {
    *count = stats.local_enqueues;
  }
  return ret;
}

/**
 * ossim_ctl_get_global_enqueues - Get global enqueue count
 */
int ossim_ctl_get_global_enqueues(struct ossim_ctl *ctl, uint64_t *count) {
  struct ossim_stats stats;
  int ret = ossim_ctl_get_stats(ctl, &stats);
  if (ret == OSSIM_OK) {
    *count = stats.global_enqueues;
  }
  return ret;
}

/**
 * ossim_ctl_shutdown - Request scheduler to shutdown
 */
int ossim_ctl_shutdown(struct ossim_ctl *ctl) {
  if (!ctl) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::ShutdownRequest request;
    ossim::ShutdownResponse response;

    grpc::Status status = ctl->stub->Shutdown(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      return OSSIM_ERR_UNKNOWN;
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_register_vcpu - Register a vCPU thread
 */
int ossim_ctl_register_vcpu(struct ossim_ctl *ctl,
                            struct ossim_ctl_vcpu_registration *vcpu) {
  if (!ctl || !vcpu) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::RegisterVcpuRequest request;
    ossim::RegisterVcpuResponse response;

    request.set_tid(vcpu->vcpu_tid);
    request.set_vm_id(vcpu->vm_id);
    request.set_vcpu_id(vcpu->vcpu_id);

    grpc::Status status = ctl->stub->RegisterVcpu(&context, request, &response);
    if (!status.ok()) {
      fprintf(stderr, "gRPC error: %s (code: %d)\n",
              status.error_message().c_str(), status.error_code());
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      return OSSIM_ERR_UNKNOWN;
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_unregister_vcpu - Unregister a vCPU thread
 */
int ossim_ctl_unregister_vcpu(struct ossim_ctl *ctl, pid_t tid) {
  if (!ctl) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::UnregisterVcpuRequest request;
    ossim::UnregisterVcpuResponse response;

    request.set_tid(tid);

    grpc::Status status =
        ctl->stub->UnregisterVcpu(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      return OSSIM_ERR_UNKNOWN;
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_query_vcpu - Query vCPU registration status
 */
int ossim_ctl_query_vcpu(struct ossim_ctl *ctl, pid_t tid,
                         struct ossim_vcpu_metadata *metadata) {
  if (!ctl || !metadata) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::QueryVcpuRequest request;
    ossim::QueryVcpuResponse response;

    request.set_tid(tid);

    grpc::Status status = ctl->stub->QueryVcpu(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      return OSSIM_ERR_UNKNOWN;
    }

    const auto &vcpu_meta = response.metadata();
    metadata->tid = vcpu_meta.tid();
    metadata->vm_id = vcpu_meta.vm_id();
    metadata->vcpu_id = vcpu_meta.vcpu_id();
    metadata->simt = vcpu_meta.simt();
    metadata->sync_scope.count = vcpu_meta.coord_count();
    for (uint32_t i = 0;
         i < vcpu_meta.coord_count() && i < OSSIM_MAX_SYNC_SCOPE_SIZE; i++) {
      metadata->sync_scope.tids[i] = vcpu_meta.coord_vcpus(i);
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_add_coordination - Add a vCPU to another vCPU's coordination list
 */
int ossim_ctl_add_coordination(struct ossim_ctl *ctl, pid_t vcpu_tid,
                               pid_t related_tid) {
  if (!ctl) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::AddCoordinationRequest request;
    ossim::AddCoordinationResponse response;

    request.set_vcpu_tid(vcpu_tid);
    request.set_related_tid(related_tid);

    grpc::Status status =
        ctl->stub->AddCoordination(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      fprintf(stderr, "Add coordination failed: %s\n",
              response.message().c_str());
      return OSSIM_ERR_UNKNOWN;
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_remove_coordination - Remove a vCPU from coordination list
 */
int ossim_ctl_remove_coordination(struct ossim_ctl *ctl, pid_t vcpu_tid,
                                  pid_t related_tid) {
  if (!ctl) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::RemoveCoordinationRequest request;
    ossim::RemoveCoordinationResponse response;

    request.set_vcpu_tid(vcpu_tid);
    request.set_related_tid(related_tid);

    grpc::Status status =
        ctl->stub->RemoveCoordination(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      fprintf(stderr, "Remove coordination failed: %s\n",
              response.message().c_str());
      return OSSIM_ERR_UNKNOWN;
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_set_coordination_list - Set entire coordination list for a vCPU
 */
int ossim_ctl_set_coordination_list(struct ossim_ctl *ctl, pid_t vcpu_tid,
                                    struct ossim_sync_scope *sync_scope) {
  if (!ctl || !sync_scope) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::SetCoordinationListRequest request;
    ossim::SetCoordinationListResponse response;

    request.set_vcpu_tid(vcpu_tid);
    for (uint32_t i = 0; i < sync_scope->count; i++) {
      request.add_related_tids(sync_scope->tids[i]);
    }

    grpc::Status status =
        ctl->stub->SetCoordinationList(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      fprintf(stderr, "Set coordination list failed: %s\n",
              response.message().c_str());
      return OSSIM_ERR_UNKNOWN;
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_get_global_coordination_list - Get the global coordination list
 */
int ossim_ctl_get_global_coordination_list(
    struct ossim_ctl *ctl, struct ossim_sync_scope *sync_scope) {
  if (!ctl || !sync_scope) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::GetGlobalCoordinationListRequest request;
    ossim::GetGlobalCoordinationListResponse response;

    grpc::Status status =
        ctl->stub->GetGlobalCoordinationList(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      fprintf(stderr, "Get global coordination list failed: %s\n",
              response.message().c_str());
      return OSSIM_ERR_UNKNOWN;
    }

    sync_scope->count = response.tids_size();
    for (int i = 0; i < response.tids_size() && i < OSSIM_MAX_SYNC_SCOPE_SIZE;
         i++) {
      sync_scope->tids[i] = response.tids(i);
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_add_global_coordination - Add a TID to the global coordination list
 */
int ossim_ctl_add_global_coordination(struct ossim_ctl *ctl, pid_t tid) {
  if (!ctl) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::AddGlobalCoordinationRequest request;
    ossim::AddGlobalCoordinationResponse response;

    request.set_tid(tid);

    grpc::Status status =
        ctl->stub->AddGlobalCoordination(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      fprintf(stderr, "Add global coordination failed: %s\n",
              response.message().c_str());
      return OSSIM_ERR_UNKNOWN;
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_remove_global_coordination - Remove a TID from global coordination
 * list
 */
int ossim_ctl_remove_global_coordination(struct ossim_ctl *ctl, pid_t tid) {
  if (!ctl) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::RemoveGlobalCoordinationRequest request;
    ossim::RemoveGlobalCoordinationResponse response;

    request.set_tid(tid);

    grpc::Status status =
        ctl->stub->RemoveGlobalCoordination(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      fprintf(stderr, "Remove global coordination failed: %s\n",
              response.message().c_str());
      return OSSIM_ERR_UNKNOWN;
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_set_global_coordination_list - Set entire global coordination list
 */
int ossim_ctl_set_global_coordination_list(
    struct ossim_ctl *ctl, struct ossim_sync_scope *sync_scope) {
  if (!ctl || !sync_scope) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::SetGlobalCoordinationListRequest request;
    ossim::SetGlobalCoordinationListResponse response;

    for (uint32_t i = 0; i < sync_scope->count; i++) {
      request.add_tids(sync_scope->tids[i]);
    }

    grpc::Status status =
        ctl->stub->SetGlobalCoordinationList(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      fprintf(stderr, "Set global coordination list failed: %s\n",
              response.message().c_str());
      return OSSIM_ERR_UNKNOWN;
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
}

/**
 * ossim_ctl_set_sync_enabled - Enable or disable synchronized scheduling
 */
int ossim_ctl_set_sync_enabled(struct ossim_ctl *ctl, bool enabled) {
  if (!ctl) {
    return OSSIM_ERR_INVALID;
  }

  try {
    grpc::ClientContext context;
    ossim::SetSyncEnabledRequest request;
    ossim::SetSyncEnabledResponse response;

    request.set_enabled(enabled);

    grpc::Status status =
        ctl->stub->SetSyncEnabled(&context, request, &response);
    if (!status.ok()) {
      return OSSIM_ERR_WRITE;
    }

    if (!response.success()) {
      fprintf(stderr, "Set sync enabled failed: %s\n",
              response.message().c_str());
      return OSSIM_ERR_UNKNOWN;
    }

    return OSSIM_OK;
  } catch (...) {
    return OSSIM_ERR_UNKNOWN;
  }
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

} // extern "C"
