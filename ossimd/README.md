# Ossim Daemon (ossimd)

ossimd serves as a user-space endpoint for Ossim components and Ossim controllers. 

- **Interaction with scx_ossim.**
During Initialization, ossimd loads scx_ossim using `libbpf` and then interacts with it via BPF maps.

- **Interaction with Ossim components.**
ossimd listens to a Unix socket and runs as a gRPC server to interact with Ossim components (e.g., QEMU). The components talk to ossimd using `libossim`, which is a wrapper around gRPC.
