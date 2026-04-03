# X64 Monitor Handoff Model

## Current model

`MONITORX64.EFI` is now a **UEFI handoff-stage component only**.

It:
- loads the ELF kernel
- resolves boot text and kernel partition text
- allocates a resident boot-context block and a memory-map buffer before the final handoff
- captures the final firmware memory map
- calls `ExitBootServices()`
- jumps directly to the kernel entry

It does **not** continue running as a live UEFI application after `ExitBootServices()`.

## Why

`ExitBootServices()` is a machine-wide transition. After it succeeds, normal UEFI boot services are no longer available, so a monitor that intends to stay active cannot remain a standard EFI application.

## Implemented safeguards

- no monitor tracing occurs between the final successful `GetMemoryMap()` call and `ExitBootServices()`
- boot context strings are copied into the boot-context block instead of being passed as monitor-owned pointers
- the boot-context block is page-allocated before the final memory-map capture
- the boot context exposes a `MONITOR_HANDOFF_ONLY` flag so the kernel can recognise the contract it was started under

## Next logical step

If Liberation needs persistent monitoring after handoff, that logic should move into:
- privileged kernel code, or
- a kernel-started monitor service using the post-UEFI kernel execution model
