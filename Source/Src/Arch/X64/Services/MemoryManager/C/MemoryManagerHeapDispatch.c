/*
 * File Name: MemoryManagerHeapDispatch.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-07T12:35:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#include "MemoryManagerHeapInternal.h"
#include "MemoryManagerAddressSpaceInternal.h"

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

static UINT64 GetPageCountFromBytes(UINT64 ByteCount)
{
    return (ByteCount + 0xFFFULL) / 0x1000ULL;
}

static void InitializeSlabClassState(LOS_MEMORY_MANAGER_HEAP_STATE *Heap)
{
    static const UINT32 BlockSizes[LOS_MEMORY_MANAGER_HEAP_MAX_SLAB_CLASSES] =
    {
        64U,
        128U,
        256U,
        512U,
        1024U,
        2048U
    };
    UINTN Index;

    Heap->SlabClassCount = LOS_MEMORY_MANAGER_HEAP_MAX_SLAB_CLASSES;
    for (Index = 0U; Index < LOS_MEMORY_MANAGER_HEAP_MAX_SLAB_CLASSES; ++Index)
    {
        Heap->SlabClasses[Index].BlockSize = BlockSizes[Index];
        Heap->SlabClasses[Index].ActivePageCount = 0U;
    }
}

static void BuildSlabFreeList(LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *Page, UINT32 BlockSize)
{
    UINTN BlockIndex;
    UINTN BlockCount;
    UINT8 *Base;

    Base = (UINT8 *)Page->PageBase;
    BlockCount = 0x1000U / (UINTN)BlockSize;
    Page->FreeListHead = 0;
    for (BlockIndex = BlockCount; BlockIndex > 0U; --BlockIndex)
    {
        void **Entry;

        Entry = (void **)(UINTN)(Base + ((BlockIndex - 1U) * (UINTN)BlockSize));
        *Entry = Page->FreeListHead;
        Page->FreeListHead = Entry;
    }

    Page->TotalBlocks = (UINT16)BlockCount;
    Page->FreeBlocks = (UINT16)BlockCount;
    Page->InUseBlocks = 0U;
}

static BOOLEAN GrowSlabClass(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINTN ClassIndex,
    UINT32 Usage,
    LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE **SlabPage,
    UINT64 *Detail)
{
    LOS_MEMORY_MANAGER_HEAP_STATE *Heap;
    LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *Page;
    UINT64 PagePhysicalAddress;
    void *PageBase;

    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_NONE;
    }
    if (SlabPage != 0)
    {
        *SlabPage = 0;
    }
    if (State == 0 || SlabPage == 0 || ClassIndex >= State->Heap.SlabClassCount)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_INVALID_REQUEST;
        }
        return 0;
    }

    Heap = &State->Heap;
    Page = LosMemoryManagerHeapAcquireSlabPageDescriptor(Heap);
    if (Page == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_DESCRIPTOR_CAPACITY;
        }
        return 0;
    }

    PagePhysicalAddress = 0ULL;
    PageBase = 0;
    if (!LosMemoryManagerServiceClaimTrackedFrames(
            State,
            1ULL,
            0x1000ULL,
            LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS,
            LOS_X64_MEMORY_REGION_OWNER_CLAIMED,
            Usage,
            &PagePhysicalAddress))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_NO_RESOURCES;
        }
        return 0;
    }

    PageBase = LosMemoryManagerTranslatePhysical(State, PagePhysicalAddress, 0x1000ULL);
    if (PageBase == 0)
    {
        (void)LosMemoryManagerServiceFreeTrackedFrames(State, PagePhysicalAddress, 1ULL);
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_METADATA_TRANSLATION;
        }
        return 0;
    }

    ZeroBytes(PageBase, 0x1000U);
    Page->PagePhysicalAddress = PagePhysicalAddress;
    Page->PageBase = PageBase;
    Page->Usage = Usage;
    Page->ClassIndex = (UINT8)ClassIndex;
    Page->Active = 1U;
    BuildSlabFreeList(Page, Heap->SlabClasses[ClassIndex].BlockSize);
    Heap->SlabClasses[ClassIndex].ActivePageCount += 1U;
    Heap->TotalReservedHeapPages += 1ULL;
    *SlabPage = Page;
    return 1;
}

static BOOLEAN AllocateFromSlabPage(
    LOS_MEMORY_MANAGER_HEAP_STATE *Heap,
    LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *Page,
    void **Address,
    UINT64 *PhysicalAddress)
{
    UINT64 Offset;
    void **Entry;

    if (Heap == 0 || Page == 0 || Address == 0 || PhysicalAddress == 0 || Page->Active == 0U || Page->FreeListHead == 0)
    {
        return 0;
    }

    Entry = (void **)Page->FreeListHead;
    Page->FreeListHead = *Entry;
    Page->FreeBlocks -= 1U;
    Page->InUseBlocks += 1U;
    Heap->ActiveSlabAllocationCount += 1ULL;

    Offset = (UINT64)((UINT8 *)Entry - (UINT8 *)Page->PageBase);
    ZeroBytes(Entry, Heap->SlabClasses[Page->ClassIndex].BlockSize);
    *Address = Entry;
    *PhysicalAddress = Page->PagePhysicalAddress + Offset;
    return 1;
}

static BOOLEAN AllocateFromSlabs(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINTN ClassIndex,
    UINT32 Usage,
    void **Address,
    UINT64 *PhysicalAddress,
    UINT64 *Detail)
{
    LOS_MEMORY_MANAGER_HEAP_STATE *Heap;
    LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *Pages;
    LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *Page;
    UINTN Index;

    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_NONE;
    }
    if (Address == 0 || PhysicalAddress == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_INVALID_REQUEST;
        }
        return 0;
    }

    Heap = &State->Heap;
    Pages = (LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *)Heap->SlabPages;
    for (Index = 0U; Index < Heap->SlabPageCapacity; ++Index)
    {
        if (Pages[Index].Active != 0U &&
            Pages[Index].ClassIndex == (UINT8)ClassIndex &&
            Pages[Index].Usage == Usage &&
            Pages[Index].FreeListHead != 0)
        {
            return AllocateFromSlabPage(Heap, &Pages[Index], Address, PhysicalAddress);
        }
    }

    Page = 0;
    if (!GrowSlabClass(State, ClassIndex, Usage, &Page, Detail))
    {
        return 0;
    }

    return AllocateFromSlabPage(Heap, Page, Address, PhysicalAddress);
}

static BOOLEAN AllocateLarge(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 ByteCount,
    UINT64 AlignmentBytes,
    UINT32 Usage,
    void **Address,
    UINT64 *PhysicalAddress,
    UINT64 *Detail)
{
    LOS_MEMORY_MANAGER_HEAP_STATE *Heap;
    LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *Allocation;
    UINT64 PageCount;
    UINT64 BasePhysicalAddress;
    UINT64 EffectiveAlignment;
    void *BaseAddress;

    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_NONE;
    }
    if (State == 0 || Address == 0 || PhysicalAddress == 0 || ByteCount == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_INVALID_REQUEST;
        }
        return 0;
    }

    Heap = &State->Heap;
    Allocation = LosMemoryManagerHeapAcquireLargeAllocationDescriptor(Heap);
    if (Allocation == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_DESCRIPTOR_CAPACITY;
        }
        return 0;
    }

    PageCount = GetPageCountFromBytes(ByteCount);
    EffectiveAlignment = AlignmentBytes == 0ULL ? 0x1000ULL : AlignmentBytes;
    if (EffectiveAlignment < 0x1000ULL)
    {
        EffectiveAlignment = 0x1000ULL;
    }

    BasePhysicalAddress = 0ULL;
    BaseAddress = 0;
    if (!LosMemoryManagerServiceClaimTrackedFrames(
            State,
            PageCount,
            EffectiveAlignment,
            LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS,
            LOS_X64_MEMORY_REGION_OWNER_CLAIMED,
            Usage,
            &BasePhysicalAddress))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_NO_RESOURCES;
        }
        return 0;
    }

    BaseAddress = LosMemoryManagerTranslatePhysical(State, BasePhysicalAddress, PageCount * 0x1000ULL);
    if (BaseAddress == 0)
    {
        (void)LosMemoryManagerServiceFreeTrackedFrames(State, BasePhysicalAddress, PageCount);
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_METADATA_TRANSLATION;
        }
        return 0;
    }

    ZeroBytes(BaseAddress, (UINTN)(PageCount * 0x1000ULL));
    Allocation->BasePhysicalAddress = BasePhysicalAddress;
    Allocation->BaseAddress = BaseAddress;
    Allocation->ByteCount = ByteCount;
    Allocation->PageCount = PageCount;
    Allocation->Usage = Usage;
    Allocation->Active = 1U;
    Heap->TotalReservedHeapPages += PageCount;
    Heap->ActiveLargeAllocationCount += 1ULL;
    *Address = BaseAddress;
    *PhysicalAddress = BasePhysicalAddress;
    return 1;
}

static BOOLEAN IsSlabBlockAlreadyFree(const LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *Page, void *BlockAddress)
{
    void *Current;

    if (Page == 0)
    {
        return 0;
    }

    Current = Page->FreeListHead;
    while (Current != 0)
    {
        if (Current == BlockAddress)
        {
            return 1;
        }
        Current = *(void **)Current;
    }

    return 0;
}

static BOOLEAN ReleaseEmptySlabPage(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *Page,
    UINT64 *ReleasedPageCount)
{
    LOS_MEMORY_MANAGER_HEAP_STATE *Heap;
    UINTN ClassIndex;

    if (ReleasedPageCount != 0)
    {
        *ReleasedPageCount = 0ULL;
    }
    if (State == 0 || Page == 0 || Page->Active == 0U)
    {
        return 0;
    }

    Heap = &State->Heap;
    ClassIndex = (UINTN)Page->ClassIndex;
    if (ClassIndex >= Heap->SlabClassCount || Heap->SlabClasses[ClassIndex].ActivePageCount <= 1U)
    {
        return 1;
    }
    if (!LosMemoryManagerServiceFreeTrackedFrames(State, Page->PagePhysicalAddress, 1ULL))
    {
        return 0;
    }

    Heap->SlabClasses[ClassIndex].ActivePageCount -= 1U;
    Heap->TotalReservedHeapPages -= 1ULL;
    ZeroBytes(Page, sizeof(*Page));
    if (ReleasedPageCount != 0)
    {
        *ReleasedPageCount = 1ULL;
    }
    return 1;
}

static BOOLEAN FreeSlabAllocation(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *Page,
    UINT64 PhysicalAddress,
    UINT64 *ReleasedPageCount,
    UINT64 *Detail)
{
    LOS_MEMORY_MANAGER_HEAP_STATE *Heap;
    UINT64 Offset;
    UINT32 BlockSize;
    UINT8 *BlockAddress;

    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_NONE;
    }
    if (ReleasedPageCount != 0)
    {
        *ReleasedPageCount = 0ULL;
    }
    if (State == 0 || Page == 0 || Page->Active == 0U)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_INVALID_REQUEST;
        }
        return 0;
    }

    Heap = &State->Heap;
    BlockSize = Heap->SlabClasses[Page->ClassIndex].BlockSize;
    Offset = PhysicalAddress - Page->PagePhysicalAddress;
    if (Offset >= 0x1000ULL || (Offset % BlockSize) != 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_NOT_FOUND;
        }
        return 0;
    }

    BlockAddress = (UINT8 *)Page->PageBase + (UINTN)Offset;
    if (IsSlabBlockAlreadyFree(Page, BlockAddress) || Page->InUseBlocks == 0U)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_CONFLICT;
        }
        return 0;
    }

    ZeroBytes(BlockAddress, BlockSize);
    *(void **)BlockAddress = Page->FreeListHead;
    Page->FreeListHead = BlockAddress;
    Page->FreeBlocks += 1U;
    Page->InUseBlocks -= 1U;
    if (Heap->ActiveSlabAllocationCount != 0ULL)
    {
        Heap->ActiveSlabAllocationCount -= 1ULL;
    }

    if (Page->FreeBlocks == Page->TotalBlocks)
    {
        return ReleaseEmptySlabPage(State, Page, ReleasedPageCount);
    }

    return 1;
}

BOOLEAN LosMemoryManagerHeapInitialize(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 *Detail)
{
    LOS_MEMORY_MANAGER_HEAP_STATE *Heap;
    UINT64 MetadataPhysicalAddress;
    void *MetadataBase;
    LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *SlabPages;
    LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *LargeAllocations;

    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_NONE;
    }
    if (State == 0 || State->MemoryView.Ready == 0U)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_NOT_READY;
        }
        return 0;
    }

    Heap = &State->Heap;
    ZeroBytes(Heap, sizeof(*Heap));
    MetadataPhysicalAddress = 0ULL;
    MetadataBase = 0;
    if (!LosMemoryManagerServiceClaimTrackedFrames(
            State,
            LOS_MEMORY_MANAGER_HEAP_BOOTSTRAP_METADATA_PAGE_COUNT,
            0x1000ULL,
            LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS,
            LOS_X64_MEMORY_REGION_OWNER_CLAIMED,
            LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_HEAP_METADATA,
            &MetadataPhysicalAddress))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_METADATA_RESERVE;
        }
        return 0;
    }

    MetadataBase = LosMemoryManagerTranslatePhysical(
        State,
        MetadataPhysicalAddress,
        LOS_MEMORY_MANAGER_HEAP_BOOTSTRAP_METADATA_PAGE_COUNT * 0x1000ULL);
    if (MetadataBase == 0)
    {
        (void)LosMemoryManagerServiceFreeTrackedFrames(State, MetadataPhysicalAddress, LOS_MEMORY_MANAGER_HEAP_BOOTSTRAP_METADATA_PAGE_COUNT);
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_METADATA_TRANSLATION;
        }
        return 0;
    }

    ZeroBytes(MetadataBase, LOS_MEMORY_MANAGER_HEAP_BOOTSTRAP_METADATA_PAGE_COUNT * 0x1000U);
    Heap->MetadataPhysicalAddress = MetadataPhysicalAddress;
    Heap->MetadataBase = MetadataBase;
    Heap->MetadataPageCount = LOS_MEMORY_MANAGER_HEAP_BOOTSTRAP_METADATA_PAGE_COUNT;
    Heap->MetadataBytes = LOS_MEMORY_MANAGER_HEAP_BOOTSTRAP_METADATA_PAGE_COUNT * 0x1000ULL;
    Heap->MetadataBumpOffset = 0ULL;
    Heap->MetadataBumpCapacity = Heap->MetadataBytes;
    Heap->TotalReservedMetadataPages = LOS_MEMORY_MANAGER_HEAP_BOOTSTRAP_METADATA_PAGE_COUNT;

    InitializeSlabClassState(Heap);

    SlabPages = (LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *)LosMemoryManagerHeapBootstrapAllocateMetadata(
        Heap,
        sizeof(LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE) * LOS_MEMORY_MANAGER_HEAP_MAX_SLAB_PAGES,
        16ULL);
    LargeAllocations = (LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *)LosMemoryManagerHeapBootstrapAllocateMetadata(
        Heap,
        sizeof(LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION) * LOS_MEMORY_MANAGER_HEAP_MAX_LARGE_ALLOCATIONS,
        16ULL);
    if (SlabPages == 0 || LargeAllocations == 0)
    {
        (void)LosMemoryManagerServiceFreeTrackedFrames(State, MetadataPhysicalAddress, LOS_MEMORY_MANAGER_HEAP_BOOTSTRAP_METADATA_PAGE_COUNT);
        ZeroBytes(Heap, sizeof(*Heap));
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_BOOTSTRAP_EXHAUSTED;
        }
        return 0;
    }

    Heap->SlabPages = SlabPages;
    Heap->LargeAllocations = LargeAllocations;
    Heap->SlabPageCapacity = LOS_MEMORY_MANAGER_HEAP_MAX_SLAB_PAGES;
    Heap->LargeAllocationCapacity = LOS_MEMORY_MANAGER_HEAP_MAX_LARGE_ALLOCATIONS;
    Heap->Ready = 1U;
    return 1;
}

BOOLEAN LosMemoryManagerHeapAllocate(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 ByteCount,
    UINT64 AlignmentBytes,
    UINT32 Usage,
    void **Address,
    UINT64 *PhysicalAddress)
{
    LOS_MEMORY_MANAGER_HEAP_STATE *Heap;
    UINT64 Detail;
    UINT64 EffectiveAlignment;
    UINTN ClassIndex;

    if (Address != 0)
    {
        *Address = 0;
    }
    if (PhysicalAddress != 0)
    {
        *PhysicalAddress = 0ULL;
    }
    if (State == 0 || Address == 0 || PhysicalAddress == 0 || ByteCount == 0ULL)
    {
        return 0;
    }

    Heap = &State->Heap;
    if (Heap->Ready == 0U)
    {
        return 0;
    }

    EffectiveAlignment = AlignmentBytes == 0ULL ? 8ULL : AlignmentBytes;
    if (!LosMemoryManagerHeapIsPowerOfTwo(EffectiveAlignment))
    {
        return 0;
    }

    if (LosMemoryManagerHeapSelectSlabClass(ByteCount, EffectiveAlignment, &ClassIndex))
    {
        Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_NONE;
        if (AllocateFromSlabs(State, ClassIndex, Usage, Address, PhysicalAddress, &Detail))
        {
            return 1;
        }
        if (Detail != LOS_MEMORY_MANAGER_HEAP_DETAIL_NO_RESOURCES &&
            Detail != LOS_MEMORY_MANAGER_HEAP_DETAIL_DESCRIPTOR_CAPACITY)
        {
            return 0;
        }
    }

    return AllocateLarge(State, ByteCount, EffectiveAlignment, Usage, Address, PhysicalAddress, &Detail);
}

BOOLEAN LosMemoryManagerHeapAllocatePages(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageCount,
    UINT64 AlignmentBytes,
    UINT32 Usage,
    void **Address,
    UINT64 *PhysicalAddress)
{
    UINT64 ByteCount;

    if (PageCount == 0ULL)
    {
        return 0;
    }

    ByteCount = PageCount * 0x1000ULL;
    if (ByteCount / 0x1000ULL != PageCount)
    {
        return 0;
    }

    return LosMemoryManagerHeapAllocate(State, ByteCount, AlignmentBytes < 0x1000ULL ? 0x1000ULL : AlignmentBytes, Usage, Address, PhysicalAddress);
}

BOOLEAN LosMemoryManagerHeapFree(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PhysicalAddress,
    UINT64 *ReleasedPageCount)
{
    LOS_MEMORY_MANAGER_HEAP_STATE *Heap;
    LOS_MEMORY_MANAGER_HEAP_SLAB_PAGE *SlabPage;
    LOS_MEMORY_MANAGER_HEAP_LARGE_ALLOCATION *Allocation;
    UINT64 Detail;

    if (ReleasedPageCount != 0)
    {
        *ReleasedPageCount = 0ULL;
    }
    if (State == 0 || PhysicalAddress == 0ULL)
    {
        return 0;
    }

    Heap = &State->Heap;
    if (Heap->Ready == 0U)
    {
        return 0;
    }

    SlabPage = LosMemoryManagerHeapFindSlabPageByPhysicalAddress(Heap, PhysicalAddress);
    if (SlabPage != 0)
    {
        Detail = LOS_MEMORY_MANAGER_HEAP_DETAIL_NONE;
        return FreeSlabAllocation(State, SlabPage, PhysicalAddress, ReleasedPageCount, &Detail);
    }

    Allocation = LosMemoryManagerHeapFindLargeAllocationByPhysicalAddress(Heap, PhysicalAddress);
    if (Allocation == 0)
    {
        return 0;
    }

    if (!LosMemoryManagerServiceFreeTrackedFrames(State, Allocation->BasePhysicalAddress, Allocation->PageCount))
    {
        return 0;
    }

    Heap->TotalReservedHeapPages -= Allocation->PageCount;
    if (Heap->ActiveLargeAllocationCount != 0ULL)
    {
        Heap->ActiveLargeAllocationCount -= 1ULL;
    }
    if (ReleasedPageCount != 0)
    {
        *ReleasedPageCount = Allocation->PageCount;
    }
    ZeroBytes(Allocation, sizeof(*Allocation));
    return 1;
}
