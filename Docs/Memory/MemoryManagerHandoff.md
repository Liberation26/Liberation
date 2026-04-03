# X64 Memory-Manager Handoff

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
