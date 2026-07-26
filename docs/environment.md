# Environment and build configuration

Ossim's top-level `Makefile` accepts configuration either through exported
environment variables or as command-line assignments:

```sh
make VARIABLE=value target
```

No host-specific paths are assumed by this document.

## Required local-build variables

The top-level Makefile requires all three variables for local targets:

| Variable | Purpose |
| --- | --- |
| `OSSIM_PREFIX` | Installation prefix for Ossim binaries, libraries, and headers |
| `OSSIM_BUILD_DIR` | Root of the out-of-tree build directories |
| `OSSIM_OUT_DIR` | Output and install-staging directory |

Programs installed under `OSSIM_PREFIX` must also be visible to the compiler,
linker, and runtime loader. A typical shell configuration is:

```sh
export PATH="$OSSIM_PREFIX/bin${PATH:+:$PATH}"
export LD_LIBRARY_PATH="$OSSIM_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LIBRARY_PATH="$OSSIM_PREFIX/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
export CPATH="$OSSIM_PREFIX/include${CPATH:+:$CPATH}"
export PKG_CONFIG_PATH="$OSSIM_PREFIX/lib/pkgconfig:$OSSIM_PREFIX/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export CMAKE_PREFIX_PATH="$OSSIM_PREFIX${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
```

## Remote target dispatch

A target named `target-<goal>` optionally synchronizes the repository and then
runs `<goal>` on another host over SSH.

| Variable | Required | Purpose |
| --- | --- | --- |
| `OSSIM_TARGET_LOGIN` | yes | SSH destination, such as `user@host` |
| `OSSIM_TARGET_DIR` | yes | Repository path on the target host |
| `OSSIM_TARGET_SYNC` | yes | `1` to synchronize the workspace; `0` to use the existing target tree |
| `OSSIM_TARGET_RSYNC_PUSH_ARGS` | no | Arguments used when pushing the workspace with rsync |
| `OSSIM_TARGET_RSYNC_PULL_ARGS` | no | Arguments used by `target-pull` |
| `OSSIM_TARGET_SSH_ARGS` | no | Additional SSH arguments for target execution |
| `OSSIM_TARGET_TTY_TARGETS` | no | Goals for which target dispatch automatically allocates a TTY |

Assignments passed to the outer `make target-<goal>` invocation are forwarded
to the target, except variables whose names begin with `OSSIM_TARGET_`.

## Kernel switching

| Variable | Purpose |
| --- | --- |
| `OSSIM_KEXEC_KERNEL_CMDLINE` | Kernel command line for `kexec-local-kernel`; normally includes any required `ossim_cpus=` reservation |
| `OSSIM_KEXEC_DEFAULT_KERNEL` | Installed kernel release selected by `kexec-default-kernel` |
| `OSSIM_KEXEC_DEFAULT_KERNEL_CMDLINE` | Kernel command line used by `kexec-default-kernel` |

When a command-line variable is empty, the kexec helper reuses the current
kernel command line where supported by the corresponding target.

## virtme-ng development kernel

| Variable | Default | Purpose |
| --- | --- | --- |
| `VNG` | `vng` | virtme-ng executable |
| `VNG_CONFIGKERNEL` | `virtme-configkernel` | virtme-ng kernel configuration helper |
| `VNG_CPUS` | `8` | Number of virtual CPUs |
| `VNG_MEM` | `8G` | Guest memory size |
| `VNG_RW` | `1` | Use a writable host filesystem when set to `1` |
| `VNG_KERNEL_CMDLINE_APPEND` | `ossim_cpus=4-7` | Text appended to the guest kernel command line |
| `VNG_OPTS` | derived from the settings above | Complete virtme-ng option override |
| `VNG_SSH_PORT` | `12222` | TCP port used by the persistent VNG instance |
| `VNG_GDB_HOST` | `localhost` | QEMU gdbstub host |
| `VNG_GDB_PORT` | `1234` | QEMU gdbstub port |
| `VNG_GDB_OPTS` | `--append nokaslr --qemu-opts='-s'` | gdbstub launch options |
| `VNG_GDB_PAUSED_OPTS` | `--append nokaslr --qemu-opts='-s -S'` | Paused gdbstub launch options |
| `GDB` | `gdb` | GDB executable |

## KGDB

| Variable | Purpose |
| --- | --- |
| `OSSIM_TARGET_KGDB_PORT` | Serial port used by `kgdboc` on the target host |
| `OSSIM_DEV_LOGIN` | SSH destination for the development/debug host |
| `OSSIM_DEV_KGDB_PORT` | Serial device or `tcp:` endpoint used by GDB |
| `OSSIM_DEV_KGDB_VMLINUX` | Path to the unstripped `vmlinux` used by GDB |
| `OSSIM_KGDB_BAUD` | Serial baud rate; defaults to `115200` |

## Kernel tracing

| Variable | Default | Purpose |
| --- | --- | --- |
| `OSSIM_TRACEFS` | `/sys/kernel/tracing` | tracefs mount point |
| `OSSIM_TRACEPOINTS` | Ossim scheduler/timer diagnostic set | Whitespace-separated `subsystem:event` names enabled by `start-kernel-trace` |

## Optional: common build controls

The defaults are suitable for normal builds. Override these variables only when
the host or development workflow requires it.

| Variable | Default | Purpose |
| --- | --- | --- |
| `JOBS` | number of online CPUs | Parallel build jobs |
| `DEBUG` | `0` | Use debug kernel build directories and enable the Ossim/KUnit debug configuration when set to `1` |
| `HOST_KERNEL_CONFIG` | `/boot/config-$(uname -r)` | Base configuration copied by `configure-local-kernel` |
| `CLANG_FORMAT` | `clang-format` | clang-format executable |
| `CLANG_FORMAT_STYLE` | repository `.clang-format` | Formatting style passed to clang-format |
| `SUDO` | `sudo` with selected environment variables | Privilege-escalation command used by install, tracing, and kexec targets |
| `SUDO_ENV` | `LD_LIBRARY_PATH`, `PATH`, and `PKG_CONFIG_PATH` | Environment preserved by the default `SUDO` command |

Pass a known distro kernel configuration explicitly when configuring a local
kernel from an Ossim boot:

```sh
make HOST_KERNEL_CONFIG=/boot/config-<kernel-release> configure-local-kernel
```

The Makefiles remain authoritative for defaults and target behavior. This file
is the user-facing index of supported configuration knobs; update both when a
variable's behavior changes.
