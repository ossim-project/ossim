#ifndef OSSIM_CONFIG_H
#define OSSIM_CONFIG_H

/* Default socket path for scx_ossim scheduler daemon */
#define SCX_OSSIM_SOCKET_PATH "/tmp/scx_ossim.sock"

/* Legacy daemon socket path (for future use) */
#define OSSIMD_SOCKET_PATH "/var/run/ossim/ossim.sock"

/* Maximum buffer size for RPC messages */
#define OSSIM_RPC_BUFFER_SIZE 256

#endif /* OSSIM_CONFIG_H */
