# Higher-Half Paging Fix

## Problem
The kernel bootstrap switched to its own page tables and then jumped into the higher-half kernel text at `0xFFFFFFFF80000000`, but the paging policy mapped that virtual range to the first gigabyte of physical memory instead of to the ELF load addresses for the higher-half kernel segments.

That caused an immediate instruction-fetch page fault or execution from the wrong bytes as soon as the bootstrap jumped to `LosKernelHigherHalfMain`.

A second issue existed for the boot context pointer: after switching away from firmware page tables, the raw low physical pointer handed over by the monitor was no longer guaranteed to remain valid through the small identity map.

## Fix
- Page-align the linker load addresses for the higher-half `.text`, `.data`, and `.bss` sections.
- Export linker symbols for both the higher-half virtual ranges and their physical load ranges.
- Build explicit higher-half mappings for the real kernel image pages instead of treating the kernel base as a generic low-memory window.
- Move the generic kernel mapping window away from `0xFFFFFFFF80000000` so it no longer collides with the higher-half kernel image.
- Convert the boot context handoff pointer to the higher-half direct-map alias before entering `LosKernelHigherHalfMain`.

## Result
The bootstrap stage still starts from the low physical entry point, but once CR3 is replaced:
- higher-half kernel code executes from the correct mapped pages
- higher-half kernel data and bss are mapped at the intended virtual addresses
- the boot context remains reachable through the direct map


## 0.0.59 update

The higher-half text mapping path no longer rejects a segment merely because the requested physical span crosses EFI memory-descriptor boundaries. The earlier bootstrap code pre-validated the entire range as one block and could falsely reject a valid kernel text span even though each 4 KiB page was present in the EFI memory map. Mapping is now validated per page, which matches how the actual page-table builder works.

Bootstrap paging failure messages are also now emitted from bootstrap-resident serial code and bootstrap-resident strings, so a genuine early mapping failure prints a readable kernel-side reason instead of faulting again by trying to execute or read higher-half diagnostic code too early.

## 0.0.60 update

The higher-half kernel image mapper no longer depends on the EFI memory-map descriptor walk to approve the kernel's own linked load pages before they can be mapped. The monitor explicitly loaded those ELF segments, so bootstrap now treats the kernel image load ranges as trusted paging inputs and maps them directly.

This keeps available-RAM discovery and the higher-half direct-map size derived from the EFI memory map, while avoiding a false bootstrap failure when the kernel image pages are not represented the way the early verifier expected.

Bootstrap failure handling is also now fully bootstrap-safe: if paging setup still fails, the kernel prints the requested virtual and physical ranges through the low serial path and halts there instead of trying to call a higher-half halt routine before the higher-half image is usable.
