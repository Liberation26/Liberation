/*
 * File Name: MemoryManagerHeap.h
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#ifndef LOS_MEMORY_MANAGER_HEAP_H
#define LOS_MEMORY_MANAGER_HEAP_H

#include "Efi.h"

typedef struct LOS_MEMORY_MANAGER_SERVICE_STATE LOS_MEMORY_MANAGER_SERVICE_STATE;

#define LOS_MEMORY_MANAGER_HEAP_DETAIL_NONE 0ULL
#define LOS_MEMORY_MANAGER_HEAP_DETAIL_METADATA_RESERVE 1ULL
#define LOS_MEMORY_MANAGER_HEAP_DETAIL_METADATA_TRANSLATION 2ULL
#define LOS_MEMORY_MANAGER_HEAP_DETAIL_BOOTSTRAP_EXHAUSTED 3ULL
#define LOS_MEMORY_MANAGER_HEAP_DETAIL_DESCRIPTOR_CAPACITY 4ULL
#define LOS_MEMORY_MANAGER_HEAP_DETAIL_NOT_READY 5ULL
#define LOS_MEMORY_MANAGER_HEAP_DETAIL_INVALID_REQUEST 6ULL
#define LOS_MEMORY_MANAGER_HEAP_DETAIL_NO_RESOURCES 7ULL
#define LOS_MEMORY_MANAGER_HEAP_DETAIL_NOT_FOUND 8ULL
#define LOS_MEMORY_MANAGER_HEAP_DETAIL_CONFLICT 9ULL

#define LOS_MEMORY_MANAGER_HEAP_BOOTSTRAP_METADATA_PAGE_COUNT 8U
#define LOS_MEMORY_MANAGER_HEAP_MAX_SLAB_CLASSES 6U
#define LOS_MEMORY_MANAGER_HEAP_MAX_SLAB_PAGES 128U
#define LOS_MEMORY_MANAGER_HEAP_MAX_LARGE_ALLOCATIONS 256U

typedef struct
{
    UINT32 BlockSize;
    UINT32 ActivePageCount;
} LOS_MEMORY_MANAGER_HEAP_SLAB_CLASS;

typedef struct
{
    UINT64 MetadataPhysicalAddress;
    void *MetadataBase;
    UINT64 MetadataPageCount;
    UINT64 MetadataBytes;
    UINT64 MetadataBumpOffset;
    UINT64 MetadataBumpCapacity;
    UINT64 TotalReservedMetadataPages;
    UINT64 TotalReservedHeapPages;
    UINT64 ActiveSlabAllocationCount;
    UINT64 ActiveLargeAllocationCount;
    UINTN SlabPageCapacity;
    UINTN LargeAllocationCapacity;
    UINT32 SlabClassCount;
    UINT32 Ready;
    LOS_MEMORY_MANAGER_HEAP_SLAB_CLASS SlabClasses[LOS_MEMORY_MANAGER_HEAP_MAX_SLAB_CLASSES];
    void *SlabPages;
    void *LargeAllocations;
} LOS_MEMORY_MANAGER_HEAP_STATE;

BOOLEAN LosMemoryManagerHeapInitialize(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 *Detail);
BOOLEAN LosMemoryManagerHeapAllocate(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 ByteCount,
    UINT64 AlignmentBytes,
    UINT32 Usage,
    void **Address,
    UINT64 *PhysicalAddress);
BOOLEAN LosMemoryManagerHeapAllocatePages(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageCount,
    UINT64 AlignmentBytes,
    UINT32 Usage,
    void **Address,
    UINT64 *PhysicalAddress);
BOOLEAN LosMemoryManagerHeapFree(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PhysicalAddress,
    UINT64 *ReleasedPageCount);

#endif
