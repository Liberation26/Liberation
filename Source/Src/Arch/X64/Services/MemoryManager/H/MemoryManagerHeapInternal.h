/*
 * File Name: MemoryManagerHeapInternal.h
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#ifndef LOS_MEMORY_MANAGER_HEAP_INTERNAL_H
#define LOS_MEMORY_MANAGER_HEAP_INTERNAL_H

#include "MemoryManagerMain.h"

typedef struct
{
    UINT64 PagePhysicalAddress;
    void *PageBase;
    void *FreeListHead;
    UINT32 Usage;
    UINT16 TotalBlocks;
    UINT16 FreeBlocks;
    UINT16 InUseBlocks;
    UINT8 ClassIndex;
    UINT8 Active;
} LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE;

typedef struct
{
    UINT64 BasePhysicalAddress;
    void *BaseAddress;
    UINT64 ByteCount;
    UINT64 PageCount;
    UINT32 Usage;
    UINT32 Active;
} LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION;

UINT64 LosMemoryManagerHeapAlignUpValue(UINT64 Value, UINT64 Alignment);
BOOLEAN LosMemoryManagerHeapIsPowerOfTwo(UINT64 Value);
BOOLEAN LosMemoryManagerHeapSelectSlabClass(UINT64 ByteCount, UINT64 AlignmentBytes, UINTN *ClassIndex);
void *LosMemoryManagerHeapBootstrapAllocateMetadata(LOS_MEMORY_MANAGER_HEAP_STATE *Heap, UINT64 ByteCount, UINT64 AlignmentBytes);
LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *LosMemoryManagerHeapAcquireSlabPageDescriptor(LOS_MEMORY_MANAGER_HEAP_STATE *Heap);
LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *LosMemoryManagerHeapAcquireLargeAllocationDescriptor(LOS_MEMORY_MANAGER_HEAP_STATE *Heap);
LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *LosMemoryManagerHeapFindSlabPageByPhysicalAddress(LOS_MEMORY_MANAGER_HEAP_STATE *Heap, UINT64 PhysicalAddress);
LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *LosMemoryManagerHeapFindLargeAllocationByPhysicalAddress(LOS_MEMORY_MANAGER_HEAP_STATE *Heap, UINT64 PhysicalAddress);

#endif
