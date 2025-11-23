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
 * ossim_ctl_add_coordination - Add a vCPU to another vCPU's coordination list
 * @ctl: Connection handle
 * @vcpu_tid: Thread ID of the vCPU to modify
 * @related_tid: Thread ID of the vCPU to add to coordination list
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_add_coordination(struct ossim_ctl *ctl, pid_t vcpu_tid,
                               pid_t related_tid);

/**
 * ossim_ctl_remove_coordination - Remove a vCPU from coordination list
 * @ctl: Connection handle
 * @vcpu_tid: Thread ID of the vCPU to modify
 * @related_tid: Thread ID of the vCPU to remove from coordination list
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_remove_coordination(struct ossim_ctl *ctl, pid_t vcpu_tid,
                                  pid_t related_tid);

/**
 * ossim_ctl_set_coordination_list - Set entire coordination list for a vCPU
 * @ctl: Connection handle
 * @vcpu_tid: Thread ID of the vCPU to modify
 * @sync_scope: Synchronization scope containing TIDs to set
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_set_coordination_list(struct ossim_ctl *ctl, pid_t vcpu_tid,
                                    struct ossim_sync_scope *sync_scope);

/**
 * ossim_ctl_get_global_coordination_list - Get the global coordination list
 * @ctl: Connection handle
 * @sync_scope: Output buffer for global coordination list
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_get_global_coordination_list(struct ossim_ctl *ctl,
                                           struct ossim_sync_scope *sync_scope);

/**
 * ossim_ctl_add_global_coordination - Add a TID to the global coordination list
 * @ctl: Connection handle
 * @tid: Thread ID to add
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_add_global_coordination(struct ossim_ctl *ctl, pid_t tid);

/**
 * ossim_ctl_remove_global_coordination - Remove a TID from global coordination
 * list
 * @ctl: Connection handle
 * @tid: Thread ID to remove
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_remove_global_coordination(struct ossim_ctl *ctl, pid_t tid);

/**
 * ossim_ctl_set_global_coordination_list - Set entire global coordination list
 * @ctl: Connection handle
 * @sync_scope: Synchronization scope containing TIDs to set
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 */
int ossim_ctl_set_global_coordination_list(struct ossim_ctl *ctl,
                                           struct ossim_sync_scope *sync_scope);

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
