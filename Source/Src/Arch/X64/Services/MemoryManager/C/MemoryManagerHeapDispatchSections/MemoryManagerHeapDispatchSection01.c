/*
 * File Name: MemoryManagerHeapDispatchSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerHeapDispatch.c.
 */

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
