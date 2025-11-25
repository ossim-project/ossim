# TODO

## scx_ossim

### 1. Per-vCPU Event Processing

The current implementation has some potential issues with race conditions on the
state. One plan is to separate the event queue for each vCPU thread with a hash map,
which allows at most one outstanding event per vCPU. The corresponding event processing
logic is performed in the vCPU's context to avoid race conditions.

In the above plan, we can probably process global events (e.g, managing global synchronization scope, global scheduling group, etc.) in the daemon's context.

## libossim

### 1. Protobuf Files Installation
