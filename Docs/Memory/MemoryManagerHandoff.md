<!--
File Name: MemoryManagerHandoff.md
File Version: 0.3.11
Author: OpenAI
Email: dave66samaa@gmail.com
Creation Timestamp: 2026-04-07T07:24:34Z
Last Update Timestamp: 2026-04-07T12:35:00Z
Operating System Name: Liberation OS
Purpose: Documents Liberation OS design, behavior, usage, or integration details.
-->

## Version 0.1.83

This delivery tightens the kernel/MM architectural boundary and applies the project file-splitting standard to the live memory-manager service code.

Concretely:

- the kernel remains the mechanism layer for low-level frame claims, page-table map/unmap primitives, translation helpers, and interrupt/fault entry paths
- the memory-manager service remains the policy owner for frame-allocation decisions, address-space lifecycle, region accounting, and virtual-layout choices
- the former `MemoryManagerMain.c` monolith is now split into dedicated diagnostics, dispatch, and lifecycle files with a shared internal header
- the former `MemoryManagerMemory.c` monolith is now split into dedicated dispatch, lifecycle, policy, and state files with a shared internal header

That means the source tree now reflects the intended architecture more honestly: the kernel side stays as the narrow execution substrate, while the service side owns the higher-level memory-management policy and bookkeeping in files that are separated by role rather than mixed together.

## Version 0.1.61

The X64 memory-manager bootstrap now has a real service-side frame allocator instead of only a service-authored memory inventory.

This delivery does six concrete things:

- keeps a post-attach **baseline page-frame database** owned by `MEMORYMGR.ELF`
- records later reserve/claim operations in a **sorted dynamic-allocation list**
- rebuilds the current live page-frame database from baseline plus those service-owned allocations
- answers `QueryMemoryRegions`, `ReserveFrames`, and `ClaimFrames` from that service-side database
- adds `FreeFrames` so tracked dynamic allocations can be released back to the service baseline
- rejects overlaps and double-frees instead of quietly reusing already-owned pages

That means physical-page ownership decisions after bootstrap attach are now made from the memory manager's own bookkeeping rather than from the older kernel bootstrap fallback path. The kernel still retains the lowest-level page-map execution path, but frame ownership policy and accounting now live in the service where they were always intended to live.

## Version 0.1.60

The visible boot-time memory summary now comes from the live memory-manager service after bootstrap attach succeeds.

The kernel no longer presents the earlier pre-attach physical-memory summary as the main memory display. Instead, `MEMORYMGR.ELF` returns its own computed totals in the bootstrap-attach reply:

- usable bytes
- bootstrap-reserved bytes
- firmware-reserved bytes
- runtime bytes
- MMIO bytes
- ACPI/NVS bytes
- unusable bytes
- total, free, reserved, runtime, and MMIO page counts
- internal-descriptor count
- page-frame database entry count

That means the screen and serial memory report now reflects the memory manager's own post-ingest, post-overlay knowledge, including pages already marked in use by the service's image, stack, mailboxes, launch block, endpoint objects, task/address-space objects, and service root page table.


## Version 0.1.59

The X64 bootstrap path now suppresses serial function-entry tracing for the hottest paging helper routines used during memory-manager launch. This removes repeated `Enter` lines for direct-map lookup, discovery checks, page-table index helpers, and CR3 reads so bootstrap serial output stays readable while fatal reports and higher-level state transitions remain visible.
## 0.1.57 service-side normalized-memory ingest

The first live `MEMORYMGR.ELF` bootstrap now ingests the kernel's normalized physical-memory region table instead of treating the kernel as the only authority on discovered RAM.

This delivery does four concrete things:

- publishes the normalized region-table physical address, region count, and entry size through the shared memory-manager launch block and bootstrap attach contract
- has the service parse that published table into its own internal descriptors split across usable, bootstrap-reserved, firmware-reserved, runtime, MMIO, ACPI/NVS, and unusable spans
- builds a service-side page-frame database from that normalized table
- overlays the memory-manager service's own in-use bootstrap objects over that database: service image, stack, request/response/event mailboxes, launch block, endpoint objects, address-space object, task object, and service root page table

That means the service now has its own memory inventory rather than only trusting the kernel's live bootstrap state. The next natural step is to start answering frame-allocation and region-query requests from this service-side database instead of still routing those requests to kernel-owned low-level state.

## 0.1.26 isolated first-service root

The bootstrap path now gives the memory-manager service its own page-table root instead of only describing the live kernel root.

This delivery does four concrete things:

- claims a dedicated bootstrap page for the first memory-manager PML4 root
- clones the current higher-half/direct-map kernel root into that service root
- maps the `MEMORYMGR.ELF` loadable segments into the service root rather than only into the active kernel root
- switches CR3 to that service root for the hosted bootstrap call into the service entry, then restores the previous kernel root afterward

That means the bootstrap contract is now materially closer to the intended architecture:

1. the kernel still owns the lowest-level frame and page-table primitives
2. the memory-manager service now has its own published root page table
3. the service is entered with its own active CR3 instead of only being called inside the kernel root
4. the launch block now carries the service root physical address and the service stack top virtual address for attach-time verification

What is still missing is the final scheduler-owned user transition. The hosted bootstrap call still runs without a real ring transition or long-lived scheduler task context, but the address-space separation step is now in place.

# X64 Memory-Manager Handoff

## 0.1.22 service-bootstrap contract

The memory-manager path now has a shared bootstrap ABI between the kernel and `MEMORYMGR.ELF`.

This delivery does four concrete things:

- moves the endpoint, mailbox, event-mailbox, and launch-block structures into one shared public header
- fixes the launch ordering so the validated ELF entry address is present in the staged launch block
- gives the service image a real launch-block attach path instead of only a standalone heartbeat loop
- introduces an explicit service-prepared bootstrap state and dedicated service-event mailbox shape

That means the first real userland launch contract is now materially closer to the intended design:

1. the kernel validates `MEMORYMGR.ELF`
2. the kernel stages mailbox pages, the launch block, and the initial stack
3. the launch block already contains the service entry address and mailbox physical addresses
4. the service image can attach to that launch block and publish service events when the first real user task bootstrap is wired in

What is still missing is the actual scheduler-owned user task creation and address-space switch. The kernel still performs the low-level memory operations during bootstrap, but the service-side bootstrap contract is now defined by the same ABI that the future userland launch will consume.

## 0.1.11 update

The memory-manager bootstrap now stages the first real service-launch resources instead of stopping at an abstract endpoint contract.

The kernel now does all of the following during early bring-up:

- claims physical pages for a request mailbox
- claims physical pages for a response mailbox
- claims physical pages for a service-events mailbox
- claims physical pages for a launch block
- claims physical pages for an initial memory-manager service stack
- initializes those mailbox pages through the higher-half direct map
- fills a launch block that a future userland `MEMORYMGR.ELF` can consume immediately
- sends bootstrap memory requests through the staged mailbox transport before dispatching them to the kernel low-level memory primitives

So the design has moved one step closer to the real split:

- the kernel retains only the lowest-level physical-frame and page-table execution
- the future memory-manager service will own allocation policy and communicate through already-staged mailboxes

## Launch block contents

The staged launch block now carries:

- endpoint identifiers
- request mailbox physical address and size
- response mailbox physical address and size
- event mailbox physical address and size
- service stack physical base, page count, and top-of-stack address
- placeholder service image physical address and size fields
- the intended service path: `\LIBERATION\SERVICES\MEMORYMGR.ELF`

## Bootstrap transport behavior

The bootstrap transport is still synchronous for now, because the real userland scheduler handoff is not in place yet.

However, the request flow is now structurally correct:

1. kernel code writes a request into the kernel->service mailbox
2. the bootstrap dispatcher consumes that request
3. the dispatcher executes the low-level kernel memory primitive
4. a response is written into the service->kernel mailbox
5. the caller retrieves the matching response by request id

That gives the codebase a real handoff shape rather than a direct-function placeholder.

## What is still missing

This is still not a real running userland memory-manager service yet.

Still to do:

- install `MEMORYMGR.ELF` into the non-ESP Liberation system area
- load the ELF image into its own address space
- create the first real userland task or thread for the service
- hand the staged launch block physical address to that task
- switch request servicing from bootstrap dispatch to real endpoint delivery

## 0.1.10 update

The next architectural step after exposing `QueryMemoryRegions`, `ReserveFrames`, `ClaimFrames`, `MapPages`, and `UnmapPages` is now in place in bootstrap form. The kernel now defines a memory-manager service bootstrap contract with three fixed endpoints:

- kernel -> memory-manager service
- memory-manager service -> kernel
- memory-manager service events

The new bootstrap path wraps each low-level memory request in an endpoint-shaped message and routes it through a dedicated memory-manager bootstrap dispatcher. During early bring-up this still executes in-kernel, because the real userland ELF loader and scheduler handoff are not yet present, but the kernel has now stopped treating those memory operations as anonymous future hooks.

That means the source tree now has a concrete bridge between today's bootstrap memory ownership and the future userland memory-manager service:

- defined bootstrap service contract
- defined endpoint identifiers
- defined request/response message shapes
- defined lifecycle states: defined, launched, ready
- kernel launch path that probes the memory-manager bootstrap endpoint before entering the idle loop

The intended split is now explicit:

- the kernel keeps only the lowest-level physical-frame and page-table execution
- the memory-manager service owns allocation policy and requests those low-level operations through endpoints

## 0.0.75 update

The X64 memory handoff has moved from descriptive bootstrap reporting to concrete kernel-side operations that the future userland memory-manager service can actually use.

## 0.4.108 update

The `LOS_X64_MEMORY_MANAGER_HANDOFF` structure remains at version `3`, but the `Flags` field is now appended at the end of the structure rather than inserted ahead of the existing region-table fields.

This keeps the version-3 extension ABI-safe for memory-manager service binaries that still expect the earlier field order up to `HighestUsablePhysicalAddress`.

The exported handoff region table is now rebuilt as a normalized view rather than an EFI list plus appended overlays. That means the published table always describes the effective ownership of each span with these fields:

- `Base`
- `Length`
- `Type`
- `Flags`
- `Owner`
- `Source`

Bootstrap and runtime overlays now replace the underlying published spans instead of being double-counted in totals.

## Concrete kernel operations

The kernel side now exposes five concrete operations for the future userland memory-manager service:

### QueryMemoryRegions

Returns the normalized region table and reports:

- total region count
- regions written to the caller buffer
- entry size
- status

### ReserveFrames

Reserves a physical range for a supplied owner.

Behavior:

- rounds to page boundaries
- reserves only the currently free subranges inside the request
- does not double-count overlay bytes when the same span is reserved again
- rebuilds the normalized published region table after changes

### ClaimFrames

Claims usable physical frames with explicit constraints.

Supported constraints:

- page count
- alignment
- below-4G limit
- exact-address request
- contiguous-run search

The result returns the claimed base address, claimed page count, and operation status.

### MapPages

Maps claimed physical frames into an explicit target page-map root.

Behavior:

- accepts an explicit target PML4 physical address, or defaults to the active kernel root
- allocates intermediate paging structures from the bootstrap page-table pool
- returns status, processed page count, and the last virtual address reached
- supports optional remap permission

### UnmapPages

Removes mappings from an explicit target page-map root.

Behavior:

- accepts an explicit target PML4 physical address, or defaults to the active kernel root
- returns status, processed page count, and the last virtual address reached
- can optionally allow large-page removal where needed

## Handoff summary

The handoff summary still exports:

- total usable bytes
- total bootstrap-reserved bytes
- total firmware-reserved bytes
- total runtime bytes
- total MMIO bytes
- total ACPI/NVS bytes
- total unusable bytes
- total address-space gap bytes
- highest usable physical address

## Meaning for the roadmap

This is the point where the bootstrap allocator can start shrinking back toward early bring-up only.

The kernel now owns the low-level frame-state and page-table mechanisms, while the real allocation policy can move into the userland memory-manager service exactly as intended.


## 0.1.12 userland service image staging

The monitor no longer loads or validates the memory-manager image. `MEMORYMGR.ELF` is now built as a normal service image and also embedded into a kernel-owned bootstrap package. The kernel-side memory-manager bootstrap validates that embedded ELF64 image in-place, captures the service entry virtual address, and publishes that information through the launch block.

This still does not switch to a real user task or scheduler-owned user address space, but the service image is now a dedicated ELF payload rather than a placeholder path only.


## 0.1.13 monitor boundary cleanup

The monitor now stays focused on the kernel handoff path. It loads the kernel, prepares boot context, and exits boot services. It does not load or name individual runtime services.

The memory-manager service image is still emitted as `Image/LIBERATION/SERVICES/MEMORYMGR.ELF` for installation, but first-launch preparation now uses a kernel-owned embedded copy so the bootstrap contract remains available without making the monitor service-aware.
