/*
 * File Name: MemoryManagerHeapDispatchSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerHeapDispatch.c.
 */

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
