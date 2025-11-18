#ifndef OSSIM_OSSIM_CTL_H
#define OSSIM_OSSIM_CTL_H

#include "ossim/config.h"
#include "ossim/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ossim_ctl;

/**
 * ossim_ctl_connect - Connect to the scx_ossim scheduler daemon
 * @socket_path: Path to the UNIX socket (NULL for default)
 *
 * Returns: Connection handle on success, NULL on failure
 */
struct ossim_ctl *ossim_ctl_connect(const char *socket_path);

/**
 * ossim_ctl_disconnect - Disconnect from the scheduler daemon
 * @ctl: Connection handle
 */
void ossim_ctl_disconnect(struct ossim_ctl *ctl);

/**
 * ossim_ctl_get_stats - Get both local and global enqueue statistics
 * @ctl: Connection handle
 * @stats: Output buffer for statistics
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_get_stats(struct ossim_ctl *ctl, struct ossim_stats *stats);

/**
 * ossim_ctl_get_local_enqueues - Get local enqueue count
 * @ctl: Connection handle
 * @count: Output buffer for count
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_get_local_enqueues(struct ossim_ctl *ctl, uint64_t *count);

/**
 * ossim_ctl_get_global_enqueues - Get global enqueue count
 * @ctl: Connection handle
 * @count: Output buffer for count
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_get_global_enqueues(struct ossim_ctl *ctl, uint64_t *count);

/**
 * ossim_ctl_shutdown - Request scheduler to shutdown
 * @ctl: Connection handle
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_shutdown(struct ossim_ctl *ctl);

/**
 * ossim_ctl_register_vcpu - Register a vCPU thread
 * @ctl: Connection handle
 * @vcpu: vCPU registration data
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_register_vcpu(struct ossim_ctl *ctl,
                            struct ossim_ctl_vcpu_registration *vcpu);

/**
 * ossim_ctl_unregister_vcpu - Unregister a vCPU thread
 * @ctl: Connection handle
 * @tid: Thread ID of the vCPU to unregister
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_unregister_vcpu(struct ossim_ctl *ctl, pid_t tid);

/**
 * ossim_ctl_query_vcpu - Query vCPU registration status
 * @ctl: Connection handle
 * @tid: Thread ID of the vCPU to query
 * @metadata: Output buffer for vCPU metadata
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_query_vcpu(struct ossim_ctl *ctl, pid_t tid,
                         struct ossim_vcpu_metadata *metadata);

/**
 * ossim_strerror - Get error message for error code
 * @error: Error code from ossim_error enum
 *
 * Returns: Human-readable error message
 */
const char *ossim_strerror(int error);

#ifdef __cplusplus
}
#endif

#endif /* OSSIM_OSSIM_CTL_H */
