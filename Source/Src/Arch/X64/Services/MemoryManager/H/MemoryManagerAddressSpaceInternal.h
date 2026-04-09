/*
 * File Name: MemoryManagerAddressSpaceInternal.h
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-07T12:35:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#ifndef LOS_MEMORY_MANAGER_ADDRESS_SPACE_INTERNAL_H
#define LOS_MEMORY_MANAGER_ADDRESS_SPACE_INTERNAL_H

#include "MemoryManagerAddressSpace.h"
#include "MemoryManagerMainInternal.h"
#include "MemoryManagerMemoryInternal.h"

#define LOS_X64_PAGE_PRESENT 0x001ULL
#define LOS_X64_PAGE_WRITABLE 0x002ULL
#define LOS_X64_PAGE_USER 0x004ULL
#define LOS_X64_PAGE_LARGE 0x080ULL
#define LOS_X64_PAGE_NX 0x8000000000000000ULL
#define LOS_X64_PAGE_TABLE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#define LOS_MEMORY_MANAGER_ALLOWED_LEAF_PAGE_FLAGS (LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_NX)

#define LOS_MEMORY_MANAGER_ADDRESS_SPACE_DEFAULT_STACK_BASE 0x0000000000800000ULL
#define LOS_MEMORY_MANAGER_ADDRESS_SPACE_STACK_GAP_BYTES 0x0000000000010000ULL
#define LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT 0x0000800000000000ULL

void *LosMemoryManagerTranslatePhysical(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 PhysicalAddress, UINT64 Length);
BOOLEAN LosMemoryManagerResolveAddressSpaceObject(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 AddressSpaceObjectPhysicalAddress,
    BOOLEAN AllowBootstrapObject,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT **AddressSpaceObject);
BOOLEAN LosMemoryManagerCreateAddressSpaceObject(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 *AddressSpaceObjectPhysicalAddress,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT **AddressSpaceObject);
BOOLEAN LosMemoryManagerDestroyAddressSpaceMappings(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 *ReleasedPageCount);
BOOLEAN LosMemoryManagerMapPagesIntoAddressSpace(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 VirtualAddress,
    UINT64 PhysicalAddress,
    UINT64 PageCount,
    UINT64 PageFlags);
BOOLEAN LosMemoryManagerUnmapPagesFromAddressSpace(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 VirtualAddress,
    UINT64 PageCount,
    UINT64 *PagesProcessed,
    UINT64 *LastVirtualAddress);
BOOLEAN LosMemoryManagerProtectPagesInAddressSpace(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 VirtualAddress,
    UINT64 PageCount,
    UINT64 PageFlags,
    UINT64 *PagesProcessed,
    UINT64 *LastVirtualAddress);
BOOLEAN LosMemoryManagerQueryAddressSpaceMapping(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 VirtualAddress,
    UINT64 *PhysicalAddress,
    UINT64 *PageFlags);
BOOLEAN LosMemoryManagerReserveVirtualRegion(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 BaseVirtualAddress,
    UINT64 PageCount,
    UINT32 Type,
    UINT32 Flags,
    UINT64 BackingPhysicalAddress);
BOOLEAN LosMemoryManagerSelectStackBaseVirtualAddress(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 DesiredBaseVirtualAddress,
    UINT64 StackPageCount,
    UINT64 *StackBaseVirtualAddress);
BOOLEAN LosMemoryManagerValidateAddressSpaceAccess(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 VirtualAddress,
    UINT64 PageCount,
    UINT64 PageFlags,
    BOOLEAN RequireValidPageFlags,
    BOOLEAN RequireReservedVirtualRange);

#endif
