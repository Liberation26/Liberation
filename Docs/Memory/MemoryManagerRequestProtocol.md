# Memory Manager Request Protocol

## Version

This document describes the request set introduced in LOS 0.1.84.

## Design goal

The memory-manager protocol should stay small and testable. Each request should perform exactly one operation.

## Primary request set

1. `BootstrapAttach`
2. `AllocateFrames`
3. `FreeFrames`
4. `CreateAddressSpace`
5. `DestroyAddressSpace`
6. `MapPages`
7. `UnmapPages`
8. `ProtectPages`
9. `QueryMapping`

## Request intent

### BootstrapAttach
Brings the service and kernel into the same negotiated protocol state. No memory-management work should be attempted before this is complete.

### AllocateFrames
Allocates physical frames only. It does not map them.

### FreeFrames
Returns previously allocated physical frames only. It does not alter mappings.

### CreateAddressSpace
Creates an address-space object and its root page-table structure only.

### DestroyAddressSpace
Destroys an address-space object and releases the mappings and bookkeeping owned by that address space.

### MapPages
Maps physical frames into a target address space only. It does not allocate those frames.

### UnmapPages
Removes mappings from a target address space only. It does not free the underlying frames.

### ProtectPages
Changes page-table protection bits for existing mappings only. It does not allocate, free, map, or unmap.

### QueryMapping
Reports the current mapping state for a virtual address only. It does not modify state.

## Legacy/internal requests

The ABI still carries a few older helper requests behind later operation numbers for staged bootstrap work and compatibility with existing code paths. They are not part of the preferred small public request set above.

## Testing order

A good test progression is:

1. `BootstrapAttach`
2. `AllocateFrames` and `FreeFrames`
3. `CreateAddressSpace` and `DestroyAddressSpace`
4. `MapPages` and `UnmapPages`
5. `ProtectPages`
6. `QueryMapping`

## Hard failure rules

The memory manager now treats the following as hard-stop conditions that must be reported and then halt the service rather than being handled vaguely:

- overlapping physical ranges in the normalized region table or dynamic allocation overlays
- freeing unowned pages
- freeing bootstrap-reserved pages
- mapping, unmapping, protecting, or querying outside reserved virtual regions
- invalid leaf protection flags outside the allowed writable/NX set
- address-space root mismatches, including bootstrap-root drift
- page-count overflow when converting pages to bytes
- integer wraparound on base + size calculations

These checks are intended to fail fast because they indicate corruption, malformed requests, or broken bootstrap assumptions rather than ordinary runtime pressure.
