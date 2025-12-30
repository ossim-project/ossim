#ifndef OSSIM_CONFIG_H
#define OSSIM_CONFIG_H

/* Path to the ossim kernel device */
#define OSSIM_DEVICE_PATH "/dev/ossim"

/* Path to ossimd gRPC socket */
#define OSSIMD_DEFAULT_SOCKET_PATH "/var/run/ossim/ossim.sock"

/* Maximum buffer size for RPC messages */
#define OSSIM_RPC_BUFFER_SIZE 256

#endif /* OSSIM_CONFIG_H */
