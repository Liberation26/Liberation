# Bootstrap Trap Reporter

## 0.0.62 update

Liberation now installs a minimal bootstrap trap-reporting layer immediately after the kernel gains control from the handoff monitor and before early paging ownership begins.

That layer exists specifically for the gap after `ExitBootServices()` where UEFI is no longer available but the higher-half kernel exception path is not yet guaranteed to be alive.

## What it does

- initialises COM1 directly with raw port I/O
- installs a temporary bootstrap IDT while still running in the bootstrap identity window
- catches early exception vectors 0 through 31
- prints a direct serial fault dump without using the higher-half kernel diagnostics path
- halts instead of silently resetting or triple-faulting whenever the bootstrap trap reporter itself can still run

## What it reports

When an early exception is captured, the bootstrap trap reporter prints:

- exception vector number and decoded exception name
- error code
- page-fault decode and `CR2` for `#PF`
- `CR0`, `CR3`, and `CR4`
- `RIP`, `CS`, `RFLAGS`, and interrupted `RSP`
- general-purpose registers

## Why this exists

Before this update, a failure after `ExitBootServices()` but before the normal kernel IDT became live could look like a reset with no useful information.

The bootstrap trap reporter closes that blind spot and gives the next failure a real post-UEFI fault dump.


## 0.0.63 update

The bootstrap trap reporter and trap-IDT installation path no longer rely on ordinary higher-half `.rodata` during the pre-higher-half bootstrap phase.

This specifically fixes the case where `LosX64PopulateBootstrapTrapEntries()` faulted while reading its local static stub table from `0xFFFFFFFF8000...` before the higher-half text/rodata mapping was active.

The fix moves the trap stub table, exception-name table, and bootstrap diagnostic strings into `.bootstrap.rodata`, and removes the hex-writer lookup table so early bootstrap diagnostics stay reachable through the identity-mapped bootstrap footprint.
