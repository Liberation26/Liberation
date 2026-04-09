/*
 * File Name: MemoryManagerHeapState.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#include "MemoryManagerHeapInternal.h"

static void ZeroBytes(void *Buffer, UINTN ByteCount)
{
    UINT8 *Bytes;
    UINTN Index;

    if (Buffer == 0)
    {
        return;
    }

    Bytes = (UINT8 *)Buffer;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        Bytes[Index] = 0U;
    }
}

void *LosMemoryManagerHeapBootstrapAllocateMetadata(LOS_MEMORY_MANAGER_HEAP_STATE *Heap, UINT64 ByteCount, UINT64 AlignmentBytes)
{
    UINT64 EffectiveAlignment;
    UINT64 AllocationOffset;
    UINT64 EndOffset;
    UINT8 *Base;
    void *Allocation;

    if (Heap == 0 || Heap->MetadataBase == 0 || ByteCount == 0ULL)
    {
        return 0;
    }

    EffectiveAlignment = AlignmentBytes == 0ULL ? 8ULL : AlignmentBytes;
    if (!LosMemoryManagerHeapIsPowerOfTwo(EffectiveAlignment))
    {
        return 0;
    }

    AllocationOffset = LosMemoryManagerHeapAlignUpValue(Heap->MetadataBumpOffset, EffectiveAlignment);
    EndOffset = AllocationOffset + ByteCount;
    if (EndOffset < AllocationOffset || EndOffset > Heap->MetadataBumpCapacity)
    {
        return 0;
    }

    Base = (UINT8 *)Heap->MetadataBase;
    Allocation = (void *)(UINTN)(Base + AllocationOffset);
    Heap->MetadataBumpOffset = EndOffset;
    ZeroBytes(Allocation, (UINTN)ByteCount);
    return Allocation;
}

LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *LosMemoryManagerHeapAcquireSlabPageDescriptor(LOS_MEMORY_MANAGER_HEAP_STATE *Heap)
{
    LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *Pages;
    UINTN Index;

    if (Heap == 0 || Heap->SlabPages == 0)
    {
        return 0;
    }

    Pages = (LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *)Heap->SlabPages;
    for (Index = 0U; Index < Heap->SlabPageCapacity; ++Index)
    {
        if (Pages[Index].Active == 0U)
        {
            ZeroBytes(&Pages[Index], sizeof(Pages[Index]));
            return &Pages[Index];
        }
    }

    return 0;
}

LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *LosMemoryManagerHeapAcquireLargeAllocationDescriptor(LOS_MEMORY_MANAGER_HEAP_STATE *Heap)
{
    LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *Allocations;
    UINTN Index;

    if (Heap == 0 || Heap->LargeAllocations == 0)
    {
        return 0;
    }

    Allocations = (LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *)Heap->LargeAllocations;
    for (Index = 0U; Index < Heap->LargeAllocationCapacity; ++Index)
    {
        if (Allocations[Index].Active == 0U)
        {
            ZeroBytes(&Allocations[Index], sizeof(Allocations[Index]));
            return &Allocations[Index];
        }
    }

    return 0;
}

LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *LosMemoryManagerHeapFindSlabPageByPhysicalAddress(LOS_MEMORY_MANAGER_HEAP_STATE *Heap, UINT64 PhysicalAddress)
{
    LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *Pages;
    UINTN Index;

    if (Heap == 0 || Heap->SlabPages == 0)
    {
        return 0;
    }

    Pages = (LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *)Heap->SlabPages;
    for (Index = 0U; Index < Heap->SlabPageCapacity; ++Index)
    {
        UINT64 PageBase;
        UINT64 PageEnd;

        if (Pages[Index].Active == 0U)
        {
            continue;
        }

        PageBase = Pages[Index].PagePhysicalAddress;
        PageEnd = PageBase + 0x1000ULL;
        if (PhysicalAddress >= PageBase && PhysicalAddress < PageEnd)
        {
            return &Pages[Index];
        }
    }

    return 0;
}

LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *LosMemoryManagerHeapFindLargeAllocationByPhysicalAddress(LOS_MEMORY_MANAGER_HEAP_STATE *Heap, UINT64 PhysicalAddress)
{
    LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *Allocations;
    UINTN Index;

    if (Heap == 0 || Heap->LargeAllocations == 0)
    {
        return 0;
    }

    Allocations = (LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *)Heap->LargeAllocations;
    for (Index = 0U; Index < Heap->LargeAllocationCapacity; ++Index)
    {
        if (Allocations[Index].Active != 0U && Allocations[Index].BasePhysicalAddress == PhysicalAddress)
        {
            return &Allocations[Index];
        }
    }

    return 0;
}
