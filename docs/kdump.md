# Kdump Setup and Debugging Notes for Custom Kernel (`6.18.0-ossim+`)

## Overview

This document summarizes:

- How `kdump` works
- Required setup for crash dumping
- How to validate `kdump`
- The issue encountered with the custom kernel (`6.18.0-ossim+`)
- Root cause analysis
- Final working configuration

This is intended as a future reference for debugging kernel panics on systems running custom kernels.

---

# 1. How kdump Works

`kdump` uses `kexec` to boot a small capture kernel after a kernel panic.

Normal reboot flow:

```text
kernel
→ firmware
→ bootloader
→ next kernel
```

`kexec` reboot flow:

```text
kernel
→ directly jump to another kernel
```

`kdump` flow:

```text
running kernel panics
→ kexec jumps to capture kernel
→ capture kernel saves vmcore
→ system reboots
```

The capture kernel is loaded into reserved memory specified by:

```text
crashkernel=<size>
```

Example:

```text
crashkernel=1G
```

---

# 2. Required Kernel Configuration

The following kernel configs are required for `kdump` support:

```text
CONFIG_KEXEC=y
CONFIG_KEXEC_CORE=y
CONFIG_CRASH_DUMP=y
CONFIG_PROC_VMCORE=y
CONFIG_MAGIC_SYSRQ=y
CONFIG_RELOCATABLE=y
```

Highly recommended for debugging:

```text
CONFIG_DEBUG_INFO=y
CONFIG_FRAME_POINTER=y
CONFIG_KALLSYMS=y
CONFIG_KALLSYMS_ALL=y
```

The most important custom-kernel config for `kexec/kdump` reliability is:

```text
CONFIG_RELOCATABLE=y
```

---

# 3. Kernel Boot Parameters

Recommended kernel command line:

```text
crashkernel=1G
panic=10
panic_on_oops=1
irqpoll
nr_cpus=1
reset_devices
```

Meaning:

| Parameter | Purpose |
|---|---|
| `crashkernel=1G` | Reserve memory for capture kernel |
| `panic=10` | Automatically reboot 10s after panic |
| `panic_on_oops=1` | Convert kernel oops into panic |
| `irqpoll` | Safer interrupt handling during panic |
| `nr_cpus=1` | Avoid SMP shutdown deadlocks |
| `reset_devices` | Reset hardware before capture kernel boots |

---

# 4. Installing kdump

## Ubuntu / Debian

```bash
sudo apt update
sudo apt install kdump-tools crash
```

---

# 5. Configure GRUB

Edit:

```bash
sudo nano /etc/default/grub
```

Example:

```text
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash crashkernel=1G panic=10"
```

Update grub:

```bash
sudo update-grub
```

Reboot:

```bash
sudo reboot
```

---

# 6. Verify kdump Is Armed

Check:

```bash
cat /sys/kernel/kexec_crash_loaded
```

Expected:

```text
1
```

Check running kernel:

```bash
uname -r
```

Check kdump configuration:

```bash
sudo kdump-config show
```

---

# 7. Triggering a Test Panic

Enable SysRq:

```bash
echo 1 | sudo tee /proc/sys/kernel/sysrq
```

Trigger panic:

```bash
echo c | sudo tee /proc/sysrq-trigger
```

Expected flow:

```text
panic
→ capture kernel boots
→ dump saved
→ reboot
```

---

# 8. Dump Locations on Ubuntu

Ubuntu `kdump-tools` may not create a `vmcore` file directly.

Instead, dumps may appear as:

```text
/var/crash/<timestamp>/
    dump.<timestamp>
    dmesg.<timestamp>
```

Example:

```text
/var/crash/202605050402/
    dump.202605050402
    dmesg.202605050402
```

The dump file:

```text
dump.<timestamp>
```

is the actual crash dump.

Verify:

```bash
file dump.<timestamp>
```

---

# 9. Analyzing the Dump

Use the matching unstripped `vmlinux` for the crashed kernel.

Example:

```bash
crash /path/to/vmlinux /var/crash/<timestamp>/dump.<timestamp>
```

Useful commands inside `crash`:

```text
sys
log
bt
ps
mod
kmem -i
```

Check which kernel actually crashed:

```bash
grep -i "Linux version" dmesg.*
```

---

# 10. Initial Problem Encountered

## Symptoms

When running the custom kernel:

```text
6.18.0-ossim+
```

triggering:

```bash
echo c > /proc/sysrq-trigger
```

caused:

```text
system freeze / hang
no automatic reboot
no usable dump
```

Even though:

```text
kernel.panic = 10
```

was configured.

---

# 11. Investigation Performed

## Verified

The following were confirmed:

- `CONFIG_KEXEC=y`
- `CONFIG_CRASH_DUMP=y`
- `CONFIG_PROC_VMCORE=y`
- `CONFIG_RELOCATABLE=y`
- `CONFIG_MAGIC_SYSRQ=y`
- `kexec_crash_loaded = 1`
- Normal `kexec` worked successfully
- `kdump-tools` service was active
- `crashkernel=...` boot parameter existed

Manual `kexec` test:

```bash
sudo kexec -l /boot/vmlinuz-6.18.0-ossim+ \
  --initrd=/boot/initrd.img-6.18.0-ossim+ \
  --command-line="$(cat /proc/cmdline)"

sudo kexec -e
```

worked correctly.

This proved:

```text
normal kexec functionality was OK
```

---

# 12. Important Distinction

Normal `kexec` and panic-time `kexec` are NOT equivalent.

Normal `kexec`:

```text
healthy running kernel
→ kexec into next kernel
```

`kdump`:

```text
panicking/broken kernel
→ emergency kexec into capture kernel
```

During panic:

- interrupts may be broken
- CPUs may be deadlocked
- drivers may hold locks
- memory may be corrupted
- device shutdown may fail

Therefore:

```text
normal kexec success does NOT guarantee kdump success
```

---

# 13. Root Cause

The issue was caused by:

```text
using the custom kernel itself as the kdump capture kernel
```

The custom kernel:

```text
6.18.0-ossim+
```

could:

- boot normally
- run normally
- perform normal kexec

BUT:

```text
failed during panic-time capture-kernel transition
```

Result:

```text
panic
→ attempted crash kexec
→ hang/freeze
→ no reboot
```

This prevented the reboot timer (`panic=10`) from executing.

---

# 14. Solution

The fix was:

```text
use Ubuntu generic kernel as the kdump capture kernel
```

Capture kernel:

```text
7.0.0-14-generic
```

Running kernel:

```text
6.18.0-ossim+
```

This configuration worked:

```text
6.18.0-ossim+ panics
→ kexec into 7.0.0-14-generic
→ dump saved successfully
→ automatic reboot works
```

---

# 15. Final Working Configuration

## Running kernel

```text
6.18.0-ossim+
```

## Capture kernel

```text
7.0.0-14-generic
```

## Recommended boot args

```text
crashkernel=1G
panic=10
panic_on_oops=1
irqpoll
nr_cpus=1
reset_devices
```

## Validation checks

```bash
cat /sys/kernel/kexec_crash_loaded
uname -r
sudo kdump-config show
```

## Test panic

```bash
echo 1 | sudo tee /proc/sys/kernel/sysrq
echo c | sudo tee /proc/sysrq-trigger
```

## Verify dump

```bash
ls -lt /var/crash
grep -i "Linux version" /var/crash/<dir>/dmesg.*
```

Expected:

```text
Linux version 6.18.0-ossim+
```

---

# 16. Key Lessons Learned

## 1. Same kernel as capture kernel is not always safe

A custom kernel may:

- work normally
- support kexec normally
- still fail as a panic-time capture kernel

---

## 2. Generic distro kernels are safer capture kernels

Distribution kernels are usually heavily tested for:

- kexec
- kdump
- initramfs recovery
- hardware reset paths
- crash dump collection

Using a stable distro kernel as the capture kernel is often the best approach.

---

## 3. `panic=10` does not guarantee reboot

If the kernel deadlocks before the reboot timer executes:

```text
panic=10
```

will not help.

The system can still freeze permanently.

---

## 4. Panic-time behavior differs from normal behavior

Success of:

```text
normal boot
normal kexec
```

DOES NOT guarantee:

```text
panic-time kexec / kdump success
```

---

# 17. Recommended Production Setup

For debugging custom kernels:

## Use:

- custom kernel as the running kernel
- stable distro kernel as the capture kernel

## Recommended parameters:

```text
crashkernel=1G
panic=10
panic_on_oops=1
irqpoll
nr_cpus=1
reset_devices
```

## Preserve:

- unstripped `vmlinux`
- `System.map`
- matching kernel modules

## Analyze using:

```bash
crash <custom-vmlinux> <dump>
```

---

# 18. Final Status

## Current working flow

```text
6.18.0-ossim+ panics
→ kexec into 7.0.0-14-generic
→ dump saved to /var/crash
→ automatic reboot succeeds
→ dump analyzable using custom-kernel vmlinux
```

The kdump pipeline is now functioning correctly.

