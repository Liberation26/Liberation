# X64 Memory-Manager Handoff

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
