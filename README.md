- 0.2.13: Scheduler wake-ups now carry an explicit pending-wake dispatch marker plus a short resume window, so a freshly woken task gets the next available post-wake run slice and the serial log can prove that slice with `resume-window`, `wake-pending`, and `resume-boost` diagnostics.

- 0.2.12: Scheduler wake-ups now carry a short-lived dispatch boost plus `wakeups` and `wake-dispatch` diagnostics, so freshly woken transient work can prove resume -> exit -> reap more reliably under busy-thread load.

- 0.2.11: Scheduler context switching now preserves the full general-purpose register set across task switches, and ephemeral workers log an explicit resume point after sleep so wake/exit/reap can be proven cleanly.

- 0.2.10: Scheduler lifecycle now serializes transient distinct-root processes so LOS does not keep hammering the memory-manager bootstrap path while an earlier transient process is still alive or waiting to be reaped.

- 0.2.9: Scheduler process creation now keeps distinct-root processes hidden until their address-space bind succeeds, and deferred bind spam was removed from the scheduler loop.

Version 0.2.13
- Serialized scheduler-side transient process address-space binding so only one `CreateAddressSpace` request can be in flight for a given process at a time. This closes the race where a just-created process could be bound twice and end up leaking the first address-space object.
- Added a `bind-in-progress` process flag plus `bind-count` and `bind-deferred` scheduler counters, so the serial log can now prove whether a non-kernel process root was bound once, deferred, or already being handled by another path.
- Kept the existing rule that transient lifecycle processes require a real distinct address space; this update makes that rule race-safe rather than letting the lifecycle thread and the scheduler binder compete on the same process object.
- This stage hardens process/root ownership before the first true user-mode transition path.

- Hardened transient scheduler-process creation so lifecycle-spawned non-kernel processes now **require** a distinct memory-manager-created address space instead of silently falling back to an inherited kernel root when a bind attempt cannot be completed.
- Added scheduler-side rebinding of any pending inherited transient processes from the main dispatch loop, so if a distinct process root is temporarily unavailable at creation time the scheduler keeps retrying the bind point instead of leaving the process permanently on the kernel root.
- Added explicit diagnostics for deferred process address-space binding and process-creation rejection when a distinct root is mandatory. This keeps the serial log honest about whether LOS really created a separate process root or intentionally refused that process.
- This stage tightens the process/address-space boundary before the first user-mode transition work.

- Added **memory-manager-backed distinct process address spaces** for transient scheduler processes. The lifecycle thread now creates ephemeral processes that receive their own address-space object, address-space id, and root table instead of always inheriting the kernel root.
- Added scheduler bookkeeping for `address-space-object` ownership and an `owns-address-space` marker in process diagnostics, so the serial log can now prove which process is carrying a distinct memory-manager-created root.
- Added cleanup-time **DestroyAddressSpace** integration for transient processes that owned a distinct address space, so process reaping now tears down the owned address-space object before the scheduler frees the process slot.
- This is still not a ring transition into user mode. It is the step that proves LOS can create, dispatch, switch, and later destroy distinct process roots before the first user-mode entry path is added.

Version 0.1.98

- Added a first-stage **process layer above scheduler threads**. The scheduler now creates a persistent `KernelProcess` during initialization, attaches all bootstrap kernel threads to it, and keeps explicit per-process bookkeeping alongside the existing per-task bookkeeping.
- Added transient `EphemeralProcess` objects in the lifecycle test path. Each spawned ephemeral worker now belongs to its own short-lived process object, so the live scheduler can prove process creation, thread ownership, thread exit, task reclamation, and then process reclamation once the last thread is gone.
- Extended scheduler diagnostics so serial logs now include `process=` on task registration, `processes=` in heartbeat/state lines, and per-process create/terminate/reap counters. That gives you direct proof that LOS is no longer tracking only raw threads.
- Each process object now carries: process id, owner process id, generation, flags, thread count, address-space id metadata, root-table metadata, exit status, and deferred cleanup state. In this stage the metadata is bookkeeping only; the scheduler still dispatches kernel threads on the kernel address space.
- This is a staging step toward safe kernel-to-user entry. The next logical step remains a true user-mode transition path that can bind one of these process objects to a non-kernel address space and fault/account against the owning process instead of only the thread.

Version 0.1.97

- Fixed the next live scheduler issue shown by your serial log: lower-priority `EphemeralWorker` tasks were being created successfully but were starving behind the higher-priority non-yielding `BusyWorker`, so they never reached their enter/sleep/exit path.
- Added scheduler ready-time tracking plus a small aging-based starvation-relief policy, so long-waiting ready tasks can temporarily gain enough effective priority to run even while a higher-priority busy thread keeps getting preempted and re-queued.
- Added `ready-since` and `starvation-relief` diagnostics to scheduler task/state tracing, so the serial log now shows when starvation recovery is taking effect.
- Updated `Docs/Kernel/Scheduler.md` and `ToDo.md` to reflect that the scheduler now has first-stage starvation control as well as preemption and task cleanup.

Version 0.1.96

- Added first-stage persistent task-object ownership and lifecycle bookkeeping to the kernel scheduler, including owner task id, generation, exit status, cleanup-pending state, and cumulative created/terminated/reaped counters.
- Added deferred scheduler-side reclamation of terminated thread stacks and task slots, using the memory-manager free-frames path when a thread stack came from claimed frames and returning bootstrap fallback stacks back to the reserved early-boot pool.
- Added a lifecycle-manager kernel thread plus short-lived ephemeral worker tasks so LOS now proves create -> run -> sleep -> terminate -> reclaim -> reuse under the live preemptive scheduler.
- Updated `Docs/Kernel/Scheduler.md` and `ToDo.md` to reflect that kernel-thread lifetime cleanup is now in place and that user-mode transition work is next.

Version 0.1.95

- Added first-stage timer-driven scheduler preemption so the PIT interrupt can now force a running kernel thread back into the scheduler when its quantum expires.
- Woke sleeping scheduler threads directly from the timer tick path and added scheduler state/task preemption counters for serial diagnostics.
- Added a non-cooperative `BusyWorker` kernel thread so LOS can now prove that heartbeat work still runs even when another thread spins forever without yielding.
- Updated `Docs/Kernel/Scheduler.md` and `ToDo.md` to reflect that the scheduler is now preemptive at the kernel-thread stage.

Version 0.1.94

- Moved scheduler initialization and bootstrap-task registration later in kernel bring-up, so the scheduler is created only after the memory-manager bootstrap address space has been staged.
- Hardened scheduler stack bring-up by keeping the normal claimed-frame path first, but falling back to reserved in-image bootstrap stacks if early frame claiming is not yet available.
- Added explicit scheduler stack-claim diagnostics so failures now report the returned claim status and page count before the fallback path is used.
- Updated `Docs/Kernel/Scheduler.md` and `ToDo.md` to reflect the early-boot scheduler hardening work.

Version 0.1.93

- Replaced the first scheduler step-loop model with saved-context kernel threads, so LOS now switches between the scheduler context and dedicated per-thread kernel stacks.
- Added an X64 scheduler context-switch assembly path, per-thread stack allocation from claimed physical frames, a thread trampoline, and explicit yield/sleep/terminate scheduler operations.
- Converted the bootstrap heartbeat work into a real sleeping kernel thread and the idle path into a dedicated idle thread that yields back to the scheduler after timer wakeups.
- Updated `Docs/Kernel/Scheduler.md` and `ToDo.md` to reflect that the scheduler has now reached the cooperative saved-context kernel-thread stage and that true preemption/user-mode work is next.

Version 0.1.92

- Added a kernel-internal scheduler subsystem split into `Scheduler.h`, `SchedulerInternal.h`, `SchedulerDispatch.c`, `SchedulerLifecycle.c`, `SchedulerPolicy.c`, and `SchedulerDiagnostics.c` so LOS now has a dedicated scheduling authority instead of ending in a pure idle-only path.
- The kernel now initializes the scheduler during bring-up, registers the bootstrap scheduler tasks, and enters the scheduler after interrupts are enabled.
- Timer IRQ handling now feeds scheduler tick accounting, and a periodic heartbeat task proves that the scheduler can dispatch non-idle kernel work on a timer cadence.
- Documented the current scheduler stage in `Docs/Kernel/Scheduler.md` and updated the ToDo list to reflect that the next step is evolving the scheduler from step tasks into saved-context kernel threads.

Version 0.1.90

- Fixed the split-memory packaging hole by adding `Source/Src/Arch/X64/Services/MemoryManager/C/MemoryManagerMemory.c` to `ChangedFiles` as the stub/marker file that replaces the old monolithic implementation during update-only syncs.
- This prevents duplicate-symbol link failures where the repository still had the pre-split `MemoryManagerMemory.c` while also compiling the new `MemoryManagerMemoryDispatch.c` and `MemoryManagerMemoryLifecycle.c` units.
- The duplicate symbols you saw (`LosMemoryManagerServiceBuildMemoryView`, `LosMemoryManagerServiceClaimTrackedFrames`, `LosMemoryManagerServiceFreeTrackedFrames`, `LosMemoryManagerServiceClaimFrames`, `LosMemoryManagerServiceQueryMemoryRegions`, `LosMemoryManagerServiceReserveFrames`, and `LosMemoryManagerServiceFreeFrames`) all come from that mixed-tree condition.

Version 0.1.86

- Fixed a packaging completeness error in 0.1.85 by adding `Source/Include/Public/MemoryManagerServiceAbi.h` to `ChangedFiles`, so update-only syncs now carry the `MapPages`, `UnmapPages`, and `ProtectPages` ABI request/result types needed by the split address-space dispatch code.
- This prevents mixed-tree builds where the updated memory-manager C files are present but the repository still has the older ABI header, which caused unknown-type compile failures in `MemoryManagerAddressSpaceDispatch.c`.

Version 0.1.85

- Added explicit hard-failure guards in the memory-manager service for overlapping physical ranges, freeing unowned or bootstrap-reserved pages, mapping outside reserved virtual regions, invalid protection flags, address-space root mismatches, page-count overflow, and base+size wraparound.
- Tightened the normalized memory-region ingest path so malformed or overlapping physical descriptors now stop memory-view construction immediately instead of being accepted silently.
- Tightened frame-free validation so only fully owned dynamic allocations may be released, preventing accidental frees of bootstrap-reserved or otherwise unowned physical pages.
- Tightened address-space map/protect validation so requests must stay inside reserved virtual regions and use a constrained page-flag set before the service will touch page tables.

Version 0.1.84

- Reworked the public memory-manager request set into a small protocol headed by `BootstrapAttach`, `AllocateFrames`, `FreeFrames`, `CreateAddressSpace`, `DestroyAddressSpace`, `MapPages`, `UnmapPages`, `ProtectPages`, and `QueryMapping`.
- Added first-class ABI message types for `AllocateFrames`, `MapPages`, `UnmapPages`, `ProtectPages`, and `QueryMapping`, while moving older bootstrap-era requests behind legacy/internal operation numbers.
- Extended the memory-manager service dispatch so the real service now handles frame allocation, address-space creation/destruction, page mapping/unmapping, page protection changes, and single-page mapping queries through one-request-per-operation handlers.
- Added service-side page-table helpers to unmap pages, update page protections, and query leaf mappings inside a target address space.
- Updated the kernel/bootstrap side operation tables and request helpers so the negotiated protocol now exposes the smaller request set first while still retaining the older internal helpers for staged-image and stack work.

Version 0.1.83

- Cleaned up the X64 kernel/memory-manager boundary so the memory-manager service side is now clearly split by role instead of relying on large mixed implementation files.
- Split the former `MemoryManagerMain.c` monolith into dedicated main, diagnostics, policy, dispatch, and lifecycle units, with `MemoryManagerMainInternal.h` carrying the shared internal contract.
- Split the former `MemoryManagerMemory.c` monolith into dedicated dispatch, lifecycle, policy, database, and state units, with `MemoryManagerMemoryInternal.h` carrying the shared internal contract.
- Kept the kernel side as the low-level execution layer while the memory-manager service continues to own allocation decisions, address-space lifecycle, region accounting, and virtual-space policy.
- Verified in the container that a full `./Scripts/BuildBoot.sh iso` run completes successfully after the refactor.

Version 0.1.82

- Added `Docs/FileSplittingStandard.md` to the source tree so the file-splitting rules now travel with the LOS archive.
- Added `Docs/FileSplittingQuickReference.md` to the source tree as the short companion guide for everyday review and refactor decisions.
- Included both documents in `ChangedFiles/Docs/` as well, so update-only syncs place them directly into the repository `Docs` folder.

Version 0.1.81

- Fixed the kernel-screen log advance path so reaching the bottom of the visible log area no longer clears the whole framebuffer immediately after the final newline.
- The status-screen log region now scrolls upward inside the reserved log rows while preserving the top title row, live timer row, and bottom screen-geometry row.
- This keeps the final memory-manager and idle-loop notifications visible on screen instead of dropping back to a nearly blank screen once the log fills.

Version 0.1.80

- Added an explicit `Heap subsystem ready.` notification on both the memory-manager service serial path and the kernel bootstrap diagnostics path, so heap bring-up is now visible alongside the existing frame-allocator readiness line.
- Extended the bootstrap-attach result contract to return heap metadata pages, reserved heap pages, slab capacity, and large-allocation capacity from `MEMORYMGR.ELF` back to the kernel.
- The kernel now logs those heap metrics to serial and writes a dedicated on-screen heap summary line after `Heap subsystem ready.` so the visible boot output proves the heap subsystem is live.

Version 0.1.79

- Added a real internal memory-manager heap subsystem with a bootstrap metadata bump allocator, fixed-size slab pools, and page-backed heap growth.
- The heap now reserves dedicated metadata pages with `LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_HEAP_METADATA`, keeping allocator bookkeeping separate from general allocatable pages.
- Runtime small-object allocation now comes from slab pages, while large and page-aligned allocations are tracked through the new heap page path instead of fragile one-off bootstrap allocation code.
- Address-space objects now allocate and free through the internal heap, and service-created page tables now also flow through the same allocator path.
- Verified in the container that all memory-manager service sources compile and link, and a full `./Scripts/BuildBoot.sh` run completes successfully.

Version 0.1.78

- Fixed the memory-manager service link failure by adding `Source/Src/Arch/X64/Services/MemoryManager/C/MemoryManagerMemory.c` to `ChangedFiles`.
- The new address-space code depends on `LosMemoryManagerServiceClaimTrackedFrames` and `LosMemoryManagerServiceFreeTrackedFrames`, and those functions are defined in `MemoryManagerMemory.c`.
- Verified in the container that the full memory-manager service source set now compiles and links successfully into `MEMORYMGR.ELF`.

Version 0.1.77

- Fixed the service build break by adding `Source/Src/Arch/X64/Services/MemoryManager/H/MemoryManagerAddressSpaceInternal.h` to `ChangedFiles`.
- This was the missing header required by `MemoryManagerAddressSpaceDispatch.c`, `MemoryManagerAddressSpacePolicy.c`, and `MemoryManagerAddressSpaceState.c`.
- Syntax-checked the memory-manager address-space service sources after adding the header.

Version 0.1.76

- Fixed the bootstrap address-space notification build break by removing the kernel diagnostics dependency on AddressSpaceId inside LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT.
- Re-added KernelMain.c to ChangedFiles so the bootstrap notification is definitely applied to an existing source tree.

Version 0.1.73

This delivery fixes the missing visible bootstrap address-space notification in two ways. First, the kernel bootstrap diagnostics now emit a raw serial line `[OK] [Kernel] Bootstrap address space created.` before the structured trace lines, so the notification is directly grepable in the serial capture even if you only search for the human-facing message. Second, this archive corrects the `ChangedFiles` layout so updates land at the repository root (`Source/...`, `README.md`, `VERSION`) instead of being nested under an extra `FullSource/` directory during update.

That means applying this tar should finally replace the live kernel source that runs during boot, and the address-space creation notification should now appear in the serial log and on the status screen during the memory-manager bootstrap success path.

Version 0.1.72

This delivery removes the last conditional guard around the visible bootstrap address-space notification in `KernelMain.c`. The kernel now prints `Bootstrap address space created.` unconditionally as soon as `LosLaunchMemoryManagerBootstrap()` returns, because that function only returns on a successful bootstrap path. The address-space ID line is also emitted unconditionally, and the object/root fields are printed either from the published bootstrap info or as explicit zero values if the info pointer were ever unavailable.

That means the notification can no longer disappear merely because the ready-state check or bootstrap-info lookup behaved differently than expected at runtime. If the kernel reaches the later timer and idle-loop lines, the `Bootstrap address space created.` notification must now have already been written to both the screen status path and the serial log.

Version 0.1.71

This delivery moves the visible bootstrap address-space notification into `KernelMain.c` immediately after `LosLaunchMemoryManagerBootstrap()` returns ready. That makes the message impossible to miss in the normal kernel bring-up flow: once the memory-manager bootstrap succeeds, the kernel now always emits `Bootstrap address space created.` through the standard trace path that writes to both the framebuffer status output and the serial log, followed by the bootstrap address-space object and root PML4 physical addresses.

This avoids relying on earlier staging-time diagnostics or later attach-report helpers that can be easy to miss during rapid bring-up output. The notification now sits directly between successful memory-manager bootstrap and the final timer/idle transition, so it should remain visible near the end of the boot log on screen as well as in the serial capture.

Version 0.1.70

This delivery fixes the bootstrap address-space creation notification so it no longer depends on the mapped service-side address-space pointer already being live at the later attach-diagnostics stage. The kernel now announces the bootstrap address-space object immediately when it is staged during transport setup, writing the notification through the normal kernel trace and framebuffer status paths with the address-space ID and object physical address.

The later post-attach diagnostics were also hardened to fall back to the published bootstrap metadata even if the live mapped address-space object pointer is not yet available, so the serial log and on-screen status both keep a visible `Bootstrap address space created.` announcement instead of silently skipping it.

Version 0.1.69

This delivery makes the bootstrap address-space creation notification visible from the kernel side after the memory-manager attach succeeds. In addition to the memory-manager service's own serial announcement, the kernel now writes and displays `Bootstrap address space created.` and a second line showing the bootstrap address-space ID, object physical address, and root PML4 physical address.

That means the first address-space creation is now visible on the framebuffer status output as well as in the kernel trace path, instead of relying only on the service-side serial log during early hosted bootstrap.

Version 0.1.68

This delivery adds explicit address-space creation notifications to the memory-manager service log. During bootstrap attach, `MEMORYMGR.ELF` now announces the initial service address space with its address-space ID, object physical address, and root PML4 physical address so bring-up can prove that the first live address space exists. The normal `CreateAddressSpace` request path now emits the same `Address space created` notification for newly created service-side address spaces.

Version 0.1.67

This delivery introduces a proper service-side address-space object model inside `MEMORYMGR.ELF`. The memory manager can now create and destroy address-space objects, attach a staged ELF image into an address space, allocate a stack inside an address space, track the root PML4 physical address, and keep an in-object list of reserved virtual regions for the attached image and stack.

The shared memory-manager ABI now carries explicit request/response messages for those address-space operations, and the kernel bootstrap bridge advertises and forwards them through the normal memory-manager mailbox path. The bootstrap-owned first service address space is also populated with image/stack geometry and reserved-region metadata so later launches can follow the same contract.

The goal of this step is to move service/process launch preparation toward a single memory-manager authority: future launch code can ask the memory manager for an address space, attach an image, allocate a stack, and consume the published root/table metadata instead of open-coding those details inside custom kernel launch paths.

Version 0.1.66

This delivery adds an explicit frame-allocator readiness announcement once the memory-manager service has finished building its service-owned page-frame database and current memory view. `MEMORYMGR.ELF` now writes `[MemManager] Frame allocator ready.` to the serial log during attach, and the kernel now prints and displays `Frame allocator ready.` on the status screen after the bootstrap attach succeeds and the service-authored memory view is accepted.

Version 0.1.65

This delivery fixes the framebuffer overlay update path so the fixed screen decorations no longer get cleared and repainted on every timer/spinner refresh. The top title row, bottom geometry row, and timer scaffold are now drawn as anchored overlay content, while the live timer state only updates the changing fields in place. That removes the heavy flicker seen during refresh and stops the timer/status overlay from smearing back into the reserved rows.

It also changes fixed-position overlay text rendering to use direct cell draws instead of the normal cursor/word-wrap path. That means the title, geometry line, and timer digits can no longer wrap into neighbouring rows when the text grid is exactly full width, which is what was causing the timer/status corruption at 1280x800.

Version 0.1.64

This delivery hardens the kernel-screen overlay so the title-row wait spinner is rendered directly into a fixed cell instead of going through the cursor/write path. That removes any remaining wrap-side effects from the spinner draw itself. It also adds visible screen diagnostics on the framebuffer output: a `Q` marker is now drawn in all four text-grid corners, and the bottom row shows the current framebuffer pixel dimensions together with the active text-grid column and row count.

The top title row is now treated as a fixed decoration row, the bottom row is reserved for the geometry readout, and log wrap clears the screen back to those anchored decorations before continuing. This makes it easier to see whether the computed text geometry matches what QEMU is actually presenting when comparing `RunISO`, `RunHD`, and `RunDir`.

Version 0.1.63

This delivery fixes the kernel-screen timer spinner placement. The rotating wait/live indicator is no longer drawn in the final physical column, which was causing the character write path to wrap onto the next row and reappear at the start of the timer line. The spinner is now drawn at the last writable column inside the right margin, so it stays visually anchored at the end of the title row while the timer row remains intact.

Version 0.1.62

This delivery makes the direct hard-disk and directory QEMU launch scripts use the same zoom-to-fit GTK display behaviour as the ISO path so all three bash launch routes present the same smaller on-screen text scale by default. `RunHD.sh` and `RunDir.sh` now also publish explicit QEMU window names and use the same maximize helper pattern as the ISO launcher.

It also moves the kernel log start row down by one text row so the fixed timer status row no longer collides visually with the first scrolling kernel status line. That leaves a dedicated blank separator row between the timer line and the live kernel log output.

Version 0.1.61

This delivery builds the first real service-side frame allocator inside `MEMORYMGR.ELF`. The memory manager now keeps a deterministic post-attach baseline frame database, records later reserve/claim allocations in a sorted dynamic-allocation list, and rebuilds the live current page-frame database from that service-owned bookkeeping instead of falling back to ad hoc kernel bootstrap ownership updates.

Concretely, the service now:
- answers `QueryMemoryRegions` from its own current effective region view
- answers `ReserveFrames` from its own frame database and rejects overlap with already-owned pages
- answers `ClaimFrames` from its own allocator search over the current free spans
- supports `FreeFrames` so later service-owned allocations can be released back to the post-attach baseline
- rejects double-free attempts by requiring the freed range to be fully covered by tracked dynamic allocations
- keeps bookkeeping deterministic by storing dynamic allocations in a sorted, coalesced list and rebuilding the live database from baseline plus overlays

The kernel bootstrap bridge now requires a real service reply for region-query and physical frame ownership requests, so once attach succeeds those ownership decisions come from the memory-manager service rather than silently falling back to the kernel bootstrap path.

Version 0.1.60

This delivery changes the visible memory summary over to the memory-manager service's own computed view instead of the kernel's pre-attach bootstrap accounting. The kernel no longer prints the earlier physical-memory and handoff summaries before the service is online. Instead, once bootstrap attach succeeds, the memory manager returns its own totals for usable, bootstrap-reserved, firmware-reserved, runtime, MMIO, ACPI/NVS, and unusable bytes together with total/free/reserved/runtime/MMIO page counts plus descriptor and frame-database sizes. The kernel now logs that service-authored summary to serial and writes a concise on-screen memory summary from the memory manager's knowledge.

Version 0.1.59

This delivery removes serial function-entry tracing for the hottest X64 paging helpers so normal bring-up no longer floods COM1 with internal address-translation chatter. The kernel keeps runtime tracing available elsewhere, but `LosX64IsPhysicalRangeDiscovered`, `LosX64GetDirectMapVirtualAddress`, `LosX64GetPml4Index`, `LosX64GetPdptIndex`, `LosX64GetPdIndex`, `LosX64GetPtIndex`, and `LosX64GetCurrentPageMapLevel4PhysicalAddress` no longer emit `[Kernel] Enter ...` lines. That keeps the memory-manager bootstrap log focused on meaningful state changes and failures instead of low-level helper noise.

Version 0.1.58

This delivery lets the live memory-manager service ingest the kernel's normalized physical-memory region table and build its own service-side RAM view during attach. The bootstrap launch block now publishes the normalized region table physical address, region count, and entry size. `MEMORYMGR.ELF` copies that table into internal descriptors split across usable, bootstrap-reserved, firmware-reserved, runtime, MMIO, ACPI/NVS, and unusable ranges, then builds a service-side page-frame database and overlays the memory-manager's own in-use objects: service image, stack, request/response/event mailboxes, launch block, endpoint objects, address-space object, task object, and service PML4 root. The service now logs the ingested region-table address plus descriptor/page-database totals at attach time so bring-up can prove that the first userland memory authority knows what RAM exists and what is already taken.

This follow-up delivery also fixes the service-image bootstrap mapper so the kernel batches contiguous image pages with identical flags into a single `LosX64MapPages` request. That removes the apparent endless per-page map loop triggered once the memory-manager image grew to carry the real region-table and page-frame database state.

Version 0.1.49

This delivery removes the current clang warning noise from the memory-manager bootstrap build. The unused `ServiceSerialWriteUnsigned` helper is removed from the memory-manager service source, and the task-transfer trace helper now explicitly marks its parameters as intentionally unused so the kernel build stays clean while preserving the current bootstrap behaviour.

Version 0.1.48

This delivery renames the live service prefix from `[Service]` to `[MemManager]`, stops the memory-manager service loop from writing heartbeat/request spam to COM1, and trims routine kernel-side bootstrap chatter so the kernel now logs only actual memory-manager request/response traffic during the normal bootstrap path. The normal kernel path no longer dumps the full memory-manager bootstrap state on success, while the kernel now emits concise `[MemManager] Kernel -> Memory Manager ...` and `[MemManager] Memory Manager -> Kernel ...` serial lines when it actually exchanges bootstrap messages with the service.

Version 0.1.47

This delivery fixes the first live memory-manager request-completion path. `MEMORYMGR.ELF` now retires a handled request slot after posting the response by marking the request complete, clearing the slot contents, zeroing the sequence, and advancing the request mailbox consume index. That stops the same bootstrap request from being re-read forever, which was causing repeated `[Service] Request operation=1 id=...` traces and noisy on-screen service spam after the handoff succeeded.

Version 0.1.46

This delivery adds service-side serial proof-of-life after the memory-manager task-transfer handoff. `MEMORYMGR.ELF` now writes explicit `[Service]` bootstrap-entry, attach-complete, and service-loop messages to COM1, logs periodic heartbeat updates from inside the live service context, and traces any bootstrap request that reaches the service mailbox.

Version 0.1.45

This delivery fixes the first memory-manager service handoff so the bootstrap entry no longer falls straight back into the kernel after a single attach/poll pass. The service bootstrap entry now transfers into the long-running service loop, keeps task heartbeat/request breadcrumbs updated from inside the service context, and hard-stops inside the service context if attach fails instead of returning to the kernel transfer helper.

Version 0.1.44

This delivery stages the full first memory-manager service image into one contiguous physical image range before mapping it into the isolated service root. That fixes overlapping ELF load-page conflicts where adjacent PT_LOAD segments shared a page and the previous per-segment mapping path tried to map that page twice.

This delivery hardens the memory-manager bootstrap path into fatal report-and-halt mode. Bootstrap image validation, transport staging, launch-block translation, service-entry preparation, hosted request dispatch failures, and any unexpected service-entry return now report diagnostics and halt immediately instead of silently falling back or continuing. The dedicated service stack must now map successfully into the service address space; bootstrap no longer falls back to a best-effort direct-map stack view.

Version 0.1.39

This delivery hardens memory-manager bootstrap transport staging so every direct-map translation is validated before any mailbox, endpoint, task, address-space, or launch-block object is published as ready. The transport path now fails immediately on a null direct-map translation instead of silently storing unusable bootstrap pointers.

Version 0.1.38

This delivery adds a dedicated assembly bootstrap task-transfer helper for the memory-manager service. The kernel now performs the temporary service handoff by switching into the staged service CR3 root and service stack inside a standalone assembly path and restoring the kernel context only if the bootstrap entry returns. This is the first real task-style switch path instead of the previous inline C-hosted entry shim.

Version 0.1.36

This delivery corrects the packaging omission in 0.1.35 by restoring the `ChangedFiles` folder. The `ChangedFiles` tree now includes the RunDir/RunHD staging fixes, the hard-disk image staging updates, the cleanup updates, and the version/readme files so a ChangedFiles-only apply picks up the intended boot-path changes.

## Delivery note for 0.1.35

This delivery updates the direct-run and hard-disk bash paths so they stage the kernel at the monitor-visible lookup locations rather than relying only on the ESP fallback layout that the ISO-installed path already satisfied.

Specifically:
- `RunDir.sh` now resets the writable OVMF vars file each run, stages `Image/LIBERATION/KERNELX64.ELF`, writes `EFI/BOOT/BOOTINFO.TXT`, and refreshes the UEFI shell mapping before launch.
- `RunHD.sh` now resets the writable OVMF vars file each run.
- `BuildBoot.sh` now stages `Image/LIBERATION/KERNELX64.ELF`, `Image/EFI/BOOT/Boot.psf`, and `Image/LIBERATION/SERVICES/MEMORYMGR.ELF`.
- `MakeHardDisk.sh` now writes both the ESP fallback kernel path and the monitor-facing `\LIBERATION\KERNELX64.ELF` path, plus `BOOTINFO.TXT`, the active PSF font, and `MEMORYMGR.ELF`.
- `clean.sh` now removes the extra staged direct-boot artifacts as well.

Version 0.1.34

This update clears the task-object request bookkeeping before the hosted service probe so attach diagnostics are no longer misreported as stage 1/detail 1 when the service never publishes real attach data.

It also changes the kernel-side bootstrap diagnostics to print unset when the service entry did not publish attach diagnostics before the probe failed.

## Delivery note for 0.1.33

This update hardens the first memory-manager service entry handoff. The kernel now places the published launch-block address into `RDI`, `RSI`, `RCX`, and `RDX` before calling the staged `MEMORYMGR.ELF` entry, and the service bootstrap entry now falls back to those incoming registers if its formal launch-block argument arrives as zero. This targets the remaining probe failure where the service still reported a null launch-block at the very first attach step.

## Delivery note for 0.1.31

This update corrects a ChangedFiles packaging omission from 0.1.30. The new memory-manager bootstrap state code already expects the extended boot-context fields for the staged memory-manager image, but `KernelMain.h` was not included in `ChangedFiles`. A ChangedFiles-only apply therefore left an older boot-context definition in place and caused the kernel build to fail.

## Delivery note for 0.1.30

This update fixes the memory-manager launch-block publication bug that caused the endpoint probe to fail at the launch-block stage. Bootstrap page claims now avoid physical page 0, and the kernel now keeps the embedded service image virtual address separate from the published service image physical address.

## Delivery note for 0.1.27

- Packaging fix: `Scripts/BuildBoot.sh` is now included in `ChangedFiles` so ChangedFiles-only updates pick up the public include path needed for `MemoryManagerServiceAbi.h`.
- Added a distinct bootstrap-owned **memory-manager PML4 root** so the first service image is no longer mapped only into the live kernel root. The service address-space object now carries both the new service root and the original kernel root.
- The kernel now clones the current higher-half/direct-map root into a dedicated memory-manager page-map, maps `MEMORYMGR.ELF` into that root, and records the service root physical address in the shared launch block.
- The hosted bootstrap invocation now performs a real **CR3 switch into the service root** before calling the mapped `MEMORYMGR.ELF` entry, then restores the previous kernel root on return.
- The task object and launch block now publish the service stack top virtual address as well as the physical stack top, so the service can verify the runnable context it was launched with.
- Source tar excludes build output binaries.

## Delivery note for 0.1.25

- Added a real bootstrap **service-image launch preparation** step for `MEMORYMGR.ELF`: the kernel now parses the service ELF program headers, claims backing frames, maps the loadable segments into the active page tables, and maps a dedicated service stack virtual range.
- The first memory-manager address-space object is now populated with the live root page-table physical address plus the higher-half direct-map layout, so the bootstrap contract describes a real runnable address-space view instead of a placeholder object.
- The hosted bootstrap path now invokes the mapped ELF entry for `MEMORYMGR.ELF` on the staged service stack before falling back to the in-kernel low-level primitive bridge, so the service image is now genuinely in the request path even though isolation and a true scheduler context switch still come next.
- The service ELF entry was changed to a one-step bootstrap entry that attaches to the published launch block, posts online/ready events, and can complete a request slot, while the kernel still retains the lowest-level frame and page-table primitives exactly as intended.
- Source tar excludes build output binaries.

## Delivery note for 0.1.24

- Added concrete bootstrap-owned memory-manager **address-space** and **task** objects alongside the existing receive, reply, and event endpoint objects.
- The kernel now claims and initializes dedicated object pages for the first memory-manager address space and first memory-manager task, then publishes both object addresses through the shared launch block.
- Bootstrap request execution now goes through a hosted **first-task step** which marks the first task and address space active before consuming mailbox traffic, rather than treating dispatch as a purely anonymous in-kernel bridge.
- `MEMORYMGR.ELF` attach validation now checks endpoint objects, the address-space object, and the task object so the future real context-switch launch path already has a stable bootstrap contract.
- Source tar excludes build output binaries.

## Delivery note for 0.1.23

- Converted the memory-manager bootstrap transport from bare mailbox identifiers into three concrete kernel-managed endpoint objects: receive, reply, and event.
- The kernel now claims, initializes, and publishes dedicated endpoint-object pages alongside the existing request, response, and event mailboxes.
- The launch block now hands the service both mailbox addresses and endpoint-object addresses, so the service attach path validates and binds the real endpoint objects before going online.
- Bootstrap request enqueue/dequeue and response posting now check endpoint role, state, and mailbox attachment before using the transport.
- Source tar excludes build output binaries.

## Delivery note for 0.1.22

- Moved the memory-manager bootstrap contract into a shared ABI header so the kernel bootstrap path and `MEMORYMGR.ELF` now consume the same endpoint, mailbox, launch-block, and event definitions.
- Fixed the launch sequencing bug so the memory-manager ELF entry address is validated and published before the launch block is staged.
- Added a dedicated service-event mailbox type and a launch-prepared state so the first real userland memory-manager entry now has a concrete attach contract instead of a bare heartbeat loop.
- Source tar excludes build output binaries.

# Liberation OS

## Delivery note for 0.1.20

- Added word wrap to kernel console text output so status and log lines move cleanly onto the next line instead of running through word boundaries.
- Increased kernel console line spacing so framebuffer text is easier to read during bring-up.
- Source tar excludes build output binaries.

# Liberation OS

## Delivery note for 0.1.11

This delivery advances the memory-manager bootstrap from a direct in-kernel bridge to a staged service-launch contract. The kernel now claims and initializes dedicated physical pages for request, response, and event mailboxes, builds a concrete memory-manager launch block with endpoint and stack metadata, and routes bootstrap memory requests through those mailboxes before dispatching them to the kernel-owned low-level frame and paging primitives.

That means the next real step is now clearly isolated: load a real `MEMORYMGR.ELF` into userland, point it at the staged launch block, and let it begin consuming the already-defined mailbox transport instead of changing the memory operation interface again.

# Liberation OS

## Delivery note for 0.1.10

This delivery moves the memory path from raw bootstrap-only handoff toward a real memory-manager service bootstrap. The kernel now defines a dedicated memory-manager bootstrap contract with fixed endpoint identifiers, endpoint-shaped request and response messages, lifecycle state, and a launch path that probes the service contract before the idle loop begins.

At this stage the low-level frame and page-table primitives still execute inside the kernel, exactly as intended for the lowest-level physical-memory operations, but the kernel now reaches them through a memory-manager bootstrap endpoint bridge instead of treating them only as direct future hooks. That gives the source tree a concrete place to hang the future userland service image, stack, ELF load, and real endpoint transport without changing the already-defined memory operations again.

# Liberation OS

## Delivery note for 0.1.9

This delivery makes the monitor-passed PSF boot font visibly larger in the kernel by switching `Boot.psf` to the bundled 24x32 font and adding integer glyph scaling in the framebuffer text renderer. The kernel now uses the monitor-loaded PSF2 font when present, defaults the boot console to scale 2, recalculates the text grid from the scaled cell size, and keeps the existing built-in bitmap path as a fallback.

# Liberation OS

## Delivery note for 0.0.74

This delivery fixes a new silent post-`ExitBootServices` bootstrap failure introduced by the serial status-tag helpers. The early bootstrap `[OK]`, `[FAIL]`, and ANSI colour escape literals were being emitted from inline bootstrap helper code without being forced into bootstrap-read-only storage, so the compiler generated references into higher-half rodata before the higher-half mappings existed. That could fault before the bootstrap trap reporter was installed, which is why the log could stop immediately after the final monitor memory-map line.

The bootstrap serial helper path now emits ANSI escapes and status tags character-by-character so it never dereferences higher-half string literals during the identity-mapped bootstrap phase. This keeps coloured host-terminal status tags enabled while making the earliest bootstrap logging safe again.

## Delivery note for 0.0.73

This delivery fixes the next post-`ExitBootServices` bootstrap page fault by removing bootstrap dependence on compiler-generated higher-half jump tables while the kernel is still running from its identity-mapped bootstrap footprint. The bootstrap memory-state path now avoids switch-based jump-table accesses in the early handoff code, and the kernel build now disables jump-table generation for the ELF kernel objects so bootstrap code does not silently reach into unmapped higher-half read-only data before the higher-half mappings are live.

It also adds best-effort serial status tags for the host terminal. Key bootstrap and kernel milestone lines now emit coloured `[OK]` and `[FAIL]` tags over COM1 using ANSI escape sequences, and the QEMU run scripts now keep those colours on the terminal while stripping ANSI escapes from the saved host log files when Perl is available.

# Liberation OS

## Delivery note for 0.0.72

This delivery fixes the physical-memory accounting model so totals are built from EFI descriptor lengths only, not from physical start addresses. EFI memory is now classified into strict buckets for usable, runtime, MMIO, ACPI/NVS, firmware-reserved, and unusable memory, while bootstrap and kernel reservations are exported as overlay records instead of being folded back into firmware-reserved totals.

The memory-manager handoff region table is now shaped for the future userland memory-manager service with base, length, type, flags, owner, and source fields. The default boot log now reports total usable memory, total bootstrap-reserved memory, total firmware-reserved memory, total runtime memory, total MMIO memory, total unusable memory, total address-space gaps, and the highest usable physical address.

# Liberation OS

## Delivery note for 0.0.71

This delivery fixes an early post-`ExitBootServices` bootstrap fault caused by reserving higher-half GDT and IDT backing through higher-half helper functions before the higher-half text mapping existed. Bootstrap memory reservation now uses direct higher-half symbol addresses for the GDT and IDT backing instead of calling into unmapped higher-half code during physical-frame database construction.

The bootstrap memory-ownership path still reserves the loaded kernel image, boot context, copied EFI memory-map buffer, bootstrap page tables, bootstrap transition stack, kernel stack backing, GDT backing, and IDT backing before the userland memory-manager handoff is exposed.

# Liberation OS Starter

## Delivery note for 0.0.70

This delivery hardens the monitor-to-kernel handoff for higher-half mapping. The monitor now records the actual loaded ELF segment layout in the boot context, and the bootstrap virtual-memory policy uses that runtime segment contract when mapping the higher-half kernel image. This avoids relying only on linked load addresses when preparing the post-ExitBootServices jump into higher-half kernel code.

This delivery uses a Linux-first layout rooted at `~/Dev/Los/Src`.

## Archive layout

Each delivery tar contains two top-level folders:

- `FullSource/`
- `ChangedFiles/`

For this restart flow:

- `update.sh` finds the newest `LOS-*-*-*.tar` in `~/Downloads/`
- if `~/Dev/Los/src/.git` or `~/Dev/Los/Src/.git` exists, it copies `ChangedFiles/` into that Git working tree, cleans build output, commits, and pushes
- if no `.git` entry is present, it copies `FullSource/` into `~/Dev/Los/Src/`
- if `ChangedFiles/Scripts/update.sh` contains a newer updater, the running updater replaces itself first and exits so the user can rerun it

## Project root

The project root is:

```text
~/Dev/Los/Src/
```

## Source layout

The boot loader and the first ELF kernel stage now live under `Source/Src/` and are separated by architecture, purpose, and source type:

```text
Source/Src/
  Arch/
    X64/
      Boot/
        C/
          BootMain.c
        H/
          Efi.h
      Kernel/
        C/
          KernelMain.c
        H/
          KernelMain.h
        Ld/
          KernelX64.ld
```

The wider LOS source tree is scaffolded for growth:

```text
Source/
  Include/
    Public/
    Internal/
  Src/
    Arch/
      Common/
      X64/
      AArch64/
    Kernel/
    Monitor/
    Drivers/
    Services/
    Userland/
```

## Separate run scripts

From the project root:

```bash
./Scripts/RunDir.sh
./Scripts/RunHD.sh
./Scripts/RunISO.sh
```

These now run the three boot methods separately:

- `RunDir.sh` builds the boot payloads, mirrors `KERNELX64.ELF` into `Image/LIBERATION/KERNELX64.ELF`, writes a UTF-16 `BOOTINFO.TXT`, resets the writable OVMF vars file, and then boots from the EFI directory tree presented to QEMU as removable USB media. This keeps the direct directory run aligned with the kernel monitor's normal `\LIBERATION\KERNELX64.ELF` lookup path.
- `RunHD.sh` builds the boot payloads, resets the writable OVMF vars file, creates `Build/LiberationDisk.img`, and then boots from the hard disk image. `MakeHardDisk.sh` now writes both `\EFI\BOOT\KERNELX64.ELF` and `\LIBERATION\KERNELX64.ELF` into the EFI filesystem image, along with `BOOTINFO.TXT` and the active boot font, so the kernel monitor can boot reliably in the direct hard-disk path too.
- `RunISO.sh` builds the ISO installer `BOOTX64.EFI`, the installed-system `LOADERX64.EFI`, and `KERNELX64.ELF`, creates `Build/Liberation.iso`, creates blank install target disks at `Build/LiberationInstallTarget1.img` and `Build/LiberationInstallTarget2.img`, boots the ISO installer, lets the installer choose a writable raw disk target, partition it with GPT, format a real FAT32 ESP plus a FAT32 Liberation data partition, then relaunches QEMU from the installed disk only after the installer requests an EFI reboot. The installed loader stays on the ESP, but the installed kernel is written to the separate Liberation data partition.
- `clean.sh` removes generated build output, installer media, logs, target disk images, and the writable OVMF vars file so the next `RunISO.sh` starts from a clean state instead of reusing stale firmware boot state.

For compatibility, `BuildRun.sh` now forwards to `RunISO.sh`.

## Media creation helpers

From the project root:

```bash
./Scripts/MakeIso.sh
./Scripts/MakeHardDisk.sh
./Scripts/MakeInstallTarget.sh
./Scripts/clean.sh
```

## Update from the latest downloaded archive

From the project root:

```bash
./Scripts/update.sh
```

You can also call:

```bash
./Scripts/Update.sh
```

The updater automatically decides whether to deploy `FullSource/` or apply `ChangedFiles/` based on whether a `.git` entry exists in `~/Dev/Los/src/` or `~/Dev/Los/Src/`.

## Additional packages

Make sure these are installed:

```bash
sudo apt install clang lld llvm qemu-system-x86 ovmf dosfstools mtools parted xorriso
```

## Current boot flow

`BOOTX64.EFI` now has two roles depending on the boot media:

- when booted from the ISO, it runs an EFI installer application
- when booted from a hard disk or directory-based EFI filesystem, it acts as the normal UEFI loader

The ISO installer currently:

- reads `\EFI\BOOT\LOADERX64.EFI` and `\EFI\BOOT\KERNELX64.ELF` from the ISO
- enumerates writable raw disks and lets you choose the installation target
- writes a GPT disk layout with a real EFI System Partition and a Liberation data partition
- formats both partitions as FAT32 for now
- installs `\EFI\BOOT\BOOTX64.EFI` and `\EFI\BOOT\BOOTINFO.TXT` into the new ESP, and installs `\LIBERATION\KERNELX64.ELF` into the separate Liberation data partition
- reports step-by-step install status and prints EFI status codes on failure

The installed loader then:

- opens `\LIBERATION\KERNELX64.ELF` from the installed Liberation data partition, while reading boot metadata from the ESP
- loads it into memory
- jumps into the ELF kernel entry

The current ELF kernel milestone then:

- clears the screen
- writes the OS version string from `VERSION`
- halts

## Update 0.0.20

- added a separate non-EFI x64 kernel ELF image at `Source/Src/Arch/X64/Kernel/`
- changed `BOOTX64.EFI` from a print-only EFI app into a loader that opens `\EFI\BOOT\KERNELX64.ELF`, loads it into memory, and jumps to it
- added `KernelX64.ld` so the kernel is linked as a flat binary image
- updated `Scripts/BuildBoot.sh` to build both `BOOTX64.EFI` and `KERNELX64.ELF`
- updated `Scripts/MakeIso.sh` and `Scripts/MakeHardDisk.sh` so ISO and hard-disk images both include `KERNELX64.ELF`
- updated `Scripts/update.sh` cleanup so it also removes the copied kernel ELF image from `Image/EFI/BOOT/`


## Build Requirements

- clang
- ld.lld
- llvm-objcopy-20 at `/usr/bin/llvm-objcopy-20` by default, or set `LLVM_OBJCOPY` to override

Kernel handoff notes:
- KERNELX64.ELF is linked for and loaded at physical address 0x00100000.
- LosKernelMain uses EFIAPI so the loader and kernel entry agree on the x64 calling convention.


## Update 0.0.24

- added an ISO-only EFI installer path that runs instead of jumping straight into the kernel
- added `LOADERX64.EFI` as the installed-system EFI payload carried inside the ISO
- updated `Scripts/BuildBoot.sh` to build both the installer boot app and the installed-system loader
- updated the installer layout so the installed ESP carries only the loader and boot info while the installed kernel lives in `\LIBERATION\KERNELX64.ELF` on the separate Liberation data partition
- updated `Scripts/MakeHardDisk.sh` so hard-disk images use the installed-system loader rather than the ISO installer app
- added `Scripts/MakeInstallTarget.sh` and updated `Scripts/RunISO.sh` so ISO testing has a blank EFI target disk to install onto
- updated the EFI installer to request a real EFI reboot after installation so the QEMU ISO test can hand off into an installed-disk-only verification boot
- updated `Scripts/RunISO.sh` to use `-action reboot=shutdown` for the installer phase and then relaunch QEMU without the ISO attached


## Update 0.0.25

- changed the ELF kernel milestone output so it prints the OS version string from `VERSION` instead of `Howdy Doody`
- added a 10-second visible countdown at the end of the ISO EFI installer before it requests an EFI cold reboot
- extended the minimal EFI boot services definition to include `Stall`, so the countdown uses firmware timing rather than a guessed busy loop


## Update 0.0.27

- replaced the first-match filesystem installer with a raw-disk EFI installer
- added target disk selection in the EFI installer
- added GPT partition creation from the installer itself
- added a real FAT32 ESP created by the installer
- added a FAT32 Liberation data partition created by the installer
- added clearer step-by-step install status and EFI status-code error reporting
- simplified `Scripts/MakeInstallTarget.sh` so it now creates a blank raw disk and leaves partitioning and formatting to the installer


## 0.0.27 ISO installer test updates
- QEMU ISO run now attaches two blank installer target disks so the EFI installer shows a real [1]/[2] choice.
- The installer echoes the pressed target key and the pressed Y/N confirmation key.
- The reboot countdown is now 5 seconds and refreshes in place on one line.
- The installed ESP now carries BOOTINFO.TXT so the booted kernel can report which installed drive it booted from.
- The GTK QEMU window now starts fullscreen.


## Purpose-based source split

The large X64 boot files have been split into purpose-specific modules:

```text
Source/Src/Arch/X64/Boot/C/
  BootConsole.c
  BootFilesystem.c
  BootKernel.c
  BootMonitor.c
  BootMain.c
Source/Src/Arch/X64/Monitor/C/
  MonitorConsole.c
  MonitorFile.c
  MonitorKernel.c
  MonitorBootContext.c
  MonitorMain.c
Source/Src/Arch/X64/Installer/C/
  InstallerBase.c
  InstallerDisk.c
  InstallerGpt.c
  InstallerFat32.c
  InstallerMain.c
```


## 0.0.38 milestone

This milestone keeps the three-binary boot chain but moves machine ownership further into the ELF kernel:

- the monitor still performs the final `ExitBootServices` handoff
- the kernel now installs a real CPU exception IDT for vectors 0-255
- the kernel now builds and loads its own identity-mapped x64 paging hierarchy
- the kernel now reloads `CR3`, sets `CR0.WP`, and enables `EFER.NXE`

That means the kernel is no longer just accepting a safe post-UEFI handoff. It starts owning the live paging state and exception path itself.


## Re-run helper

Use `./Scripts/Rerunqemu.sh` to relaunch QEMU with the most recently created hard-disk images still attached.

## Update 0.0.69

- reserved the loaded kernel image, boot context, copied EFI memory-map buffer, bootstrap page-table storage, bootstrap stack state, and descriptor-table backing before exposing memory to later code
- replaced the raw EFI-usable interpretation with a physical-frame region database and a first memory-manager handoff contract for the future userland memory-manager service
- reduced the higher-half memory log to summary totals by default, keeping the full descriptor dump behind a debug flag
- added page unmap plus frame reservation and frame claim primitives to the x64 bootstrap memory interface


## Boot fonts

The source tree now includes bundled PSF2 boot-font assets in `Image/LIBERATION/FONTS/`:

- `Boot.psf` (default, currently `Boot-16x32.psf`)
- `Boot-16x28.psf`
- `Boot-16x32.psf`
- `Boot-24x32.psf`

These files are carried in the installation payload so the kernel screen path can switch to a larger boot console font later. The current timer bring-up build still uses the built-in framebuffer font until the PSF loader is wired into `KernelScreen`.


## Memory-manager bootstrap

Version 0.1.13 adds the first dedicated `MEMORYMGR.ELF` image to the installed image tree at `\LIBERATION\SERVICES\MEMORYMGR.ELF`. The monitor now preloads that ELF before `ExitBootServices`, records its physical address and size in the boot context, and the kernel bootstrap validates the ELF header and entry address before publishing the launch block.


## 0.1.13

- The monitor is back to kernel-only responsibility.
- `MEMORYMGR.ELF` is now built as a normal service image and also embedded into a kernel-owned bootstrap package for first-service launch preparation.
- The monitor no longer loads or validates the memory-manager image.


- 0.1.18: kernel console now starts each line with a 1-character indent and uses an 8-character indent for automatic word-wrapped continuation lines.

- 0.1.20: fixed the kernel console build break by forward-declaring ApplyLineIndent before its first use.
## 0.1.28 packaging fix

This package corrects the memory-manager bootstrap update set so ChangedFiles-only updates include the full memory-manager bootstrap source/header set required by the kernel and service build.


## 0.1.32
- Improved memory-manager bootstrap diagnostics so launch-block validation failures are recorded into the task object when possible.
- Service entry invocation now passes the launch-block address in RDI, RSI, and RCX to make the bootstrap handoff more robust during early service entry bring-up.

- 0.1.38 maps the first memory-manager bootstrap stack into the service address space at a dedicated virtual range instead of using the higher-half direct-map stack address, and now prints both stack-top physical and virtual values during bootstrap diagnostics.

- 0.1.40 hardens the first memory-manager service stack handoff by refusing a zero stack-top virtual address and by falling back to the cloned direct-map stack view if the dedicated service-stack mapping is not yet available.


## 0.1.42
- Added deep memory-manager bootstrap transfer tracing for CR3, RSP, RIP, launch-block, and stack handoff values.
- Added early service-entry breadcrumbs so failed first-task entry can be narrowed before attach logic runs.
- Updated kernel diagnostics to report raw service-entry breadcrumb values when present.

## 0.1.44
- Fixed first memory-manager userland image staging so overlapping ELF PT_LOAD pages are staged into one claimed image range before mapping into the isolated service root.
- Updated bootstrap launch diagnostics to record the staged image base and per-page map failures for the first service attach path.
- Published the staged service image physical base into the address-space object and launch block so the first service sees a consistent bootstrap contract.


## 0.1.88
- Added MemoryManagerMemoryDatabase.c to ChangedFiles so update-only syncs include the split frame-database implementation unit.
- Fixes unresolved memory-manager link symbols introduced by partial file-splitting rollout.

## 0.1.90
- Added MemoryManagerBootstrapDispatch.c to ChangedFiles so update-only syncs carry the kernel-side request translation layer for MapPages, UnmapPages, and ProtectPages.
- Fixes the mixed-tree kernel build warning where older bootstrap dispatch code passed memory-manager ABI payload types directly into LosX64* page-operation functions expecting LOS_X64_* request/result structures.

- 0.1.90: Added MemoryManagerBootstrap.h to ChangedFiles so update-only syncs pick up the corrected LosMemoryManagerSendMapPages/UnmapPages prototypes and avoid the conflicting declaration build error.