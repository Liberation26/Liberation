# Early Kernel Bootstrap

## 0.0.68 EFI memory-map higher-half access fix

After the kernel switches to its own higher-half paging, `BootContext->MemoryMapAddress` is still a **physical** address captured by the UEFI monitor. That address must not be dereferenced directly any more.

The higher-half kernel now converts the EFI memory-map buffer address through the higher-half direct map before it walks descriptors for reporting. This fixes the post-handoff page fault where the kernel tried to read the EFI memory map through the old physical pointer value after `CR3` ownership had already moved to Liberation paging.

## 0.0.66 update

The higher-half kernel stack mapper now reports the requested higher-half stack virtual address, the bootstrap stack-backing physical range, and the number of bootstrap page-table pages already consumed before the stack map call. If the stack backing lives inside trusted bootstrap-loaded kernel pages but is not covered by the copied EFI discovery ranges, the mapper now falls back to an unchecked bootstrap mapping path instead of failing the handoff immediately.

This keeps usable-RAM discovery tied to the EFI memory map while treating the kernel image pages that the monitor already loaded as trusted bootstrap-resident pages during the final handoff.

## 0.0.65 update

The higher-half handoff now validates the dedicated kernel stack mapping before switching `rsp` away from the bootstrap transition stack. The bootstrap entry also reloads the higher-half boot-context argument immediately before jumping into `LosKernelHigherHalfMain`, and the higher-half kernel entry now uses the normal freestanding SysV calling convention instead of `EFIAPI`.

That change removes two silent handoff hazards:
- switching to an unmapped higher-half stack before any further breadcrumb can be printed
- entering the post-UEFI kernel main routine with the wrong calling convention or a clobbered boot-context register

## 0.0.62 update

A minimal bootstrap trap-reporting layer is now installed before early paging ownership. It uses a temporary bootstrap IDT plus direct serial output so faults after `ExitBootServices()` can be reported even before the normal higher-half kernel exception path is fully live.

## 0.0.61 update

The handoff path now masks interrupts before transferring control from the monitor to the kernel, and the kernel bootstrap entry also executes `cli` and `cld` before any paging or stack work.

That change closes the window between `ExitBootServices()` and early kernel ownership where a pending maskable interrupt could vector through stale firmware state and cause a reset or triple-fault style reboot.

## Rule

After `ExitBootServices()` succeeds:

- no firmware interrupt path may be relied upon
- the monitor must not leave interrupts enabled while jumping into the kernel
- the kernel bootstrap must begin with interrupts masked until its own GDT and IDT are installed

## Practical implication

The correct sequence is now:

1. monitor completes final `ExitBootServices()` handoff
2. monitor executes `cli`
3. kernel bootstrap entry executes `cli` and `cld`
4. kernel builds paging and switches to its own stack
5. kernel installs its own GDT and IDT
6. only then may later code choose to re-enable interrupts


## 0.0.64 bootstrap transition stack

The kernel bootstrap no longer keeps using the monitor-provided stack after `ExitBootServices()`.
A dedicated bootstrap transition stack now lives in `.bootstrap.data`, is identity-mapped by the early page tables, and is switched to before any bootstrap C code runs. This prevents the first `CR3` switch from returning through an unmapped firmware stack.

The bootstrap path also now emits breadcrumb messages for:
- virtual-memory initialization start
- EFI physical-memory capture
- paging policy build
- higher-half stack preparation
- the `CR3` ownership switch
- the final jump into `LosKernelHigherHalfMain`


## 0.0.67 handoff calling convention fix

The monitor is a UEFI application, so its handoff into the kernel entry uses the UEFI X64 calling convention. That means the incoming `LOS_BOOT_CONTEXT*` arrives in `RCX` at the very first bootstrap instruction.

The freestanding ELF kernel then switches back to the normal SysV kernel-side calling convention before entering `LosKernelHigherHalfMain`, where the same pointer is passed in `RDI`.

This fixes a handoff bug where the bootstrap entry was incorrectly treating `RDI` as the incoming monitor argument, which could make the higher-half kernel dereference a bogus boot-context address after paging ownership had already been taken.
