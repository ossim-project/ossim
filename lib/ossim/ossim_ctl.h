#ifndef OSSIM_OSSIM_CTL_H
#define OSSIM_OSSIM_CTL_H

/*
 * User APIs to interact with scx_ossim scheduler daemon.
 *
 * This library provides a simple interface to communicate with the
 * scx_ossim scheduler via UNIX domain sockets. It wraps the RPC protocol
 * and provides type-safe functions for common operations.
 *
 * Example usage:
 *   struct ossim_ctl *ctl = ossim_ctl_connect(NULL);
 *   if (!ctl) {
 *     // handle error
 *   }
 *
 *   struct ossim_stats stats;
 *   int ret = ossim_ctl_get_stats(ctl, &stats);
 *   if (ret == OSSIM_OK) {
 *     printf("Local: %lu, Global: %lu\n",
 *            stats.local_enqueues, stats.global_enqueues);
 *   }
 *
 *   ossim_ctl_disconnect(ctl);
 */

#include "ossim/types.h"
#include "ossim/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque connection handle */
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
 * ossim_ctl_send_command - Send a raw command to the scheduler
 * @ctl: Connection handle
 * @command: Command string to send
 * @response: Buffer for response (can be NULL if not needed)
 * @response_size: Size of response buffer
 *
 * Returns: OSSIM_OK on success, negative error code on failure
 *
 * This is a low-level function for extensibility. Most users should use
 * the typed functions above.
 */
int ossim_ctl_send_command(struct ossim_ctl *ctl, const char *command,
                           char *response, size_t response_size);

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
