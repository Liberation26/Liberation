#include "MemoryManagerMemoryInternal.h"

static LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY LosDynamicAllocationScratch[LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES];

BOOLEAN LosMemoryManagerInsertDynamicAllocation(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 BaseAddress,
    UINT64 PageCount,
    UINT32 Usage,
    UINT32 Owner)
{
    UINTN SourceIndex;
    UINTN ScratchCount;
    BOOLEAN Inserted;
    LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY NewEntry;

    if (View == 0 || PageCount == 0ULL)
    {
        return 0;
    }

    if (LosMemoryManagerDoesRangeWrap(BaseAddress, PageCount * 4096ULL))
    {
        LosMemoryManagerHardFail("base-plus-size-wrap", BaseAddress, PageCount, 0x1001ULL);
    }

    {
        UINTN ExistingIndex;
        UINT64 NewEndAddress;

        (void)LosMemoryManagerTryGetRangeEnd(BaseAddress, PageCount * 4096ULL, &NewEndAddress);
        for (ExistingIndex = 0U; ExistingIndex < View->DynamicAllocationCount; ++ExistingIndex)
        {
            const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Existing;
            UINT64 ExistingEndAddress;

            Existing = &View->DynamicAllocations[ExistingIndex];
            if (!LosMemoryManagerTryGetRangeEnd(Existing->BaseAddress, Existing->PageCount * 4096ULL, &ExistingEndAddress))
            {
                LosMemoryManagerHardFail("base-plus-size-wrap", Existing->BaseAddress, Existing->PageCount, ExistingIndex);
            }
            if (!(NewEndAddress <= Existing->BaseAddress || ExistingEndAddress <= BaseAddress))
            {
                LosMemoryManagerHardFail("overlapping-physical-ranges", BaseAddress, PageCount, Existing->BaseAddress);
            }
        }
    }

    if (View->DynamicAllocationCount >= LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES)
    {
        return 0;
    }

    NewEntry.BaseAddress = BaseAddress;
    NewEntry.PageCount = PageCount;
    NewEntry.State = LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RESERVED;
    NewEntry.Usage = Usage;
    NewEntry.Owner = Owner;
    NewEntry.Source = LOS_X64_MEMORY_REGION_SOURCE_RUNTIME;
    NewEntry.Attributes = 0ULL;

    LosMemoryManagerZeroMemory(LosDynamicAllocationScratch, sizeof(LosDynamicAllocationScratch));
    ScratchCount = 0U;
    Inserted = 0;

    for (SourceIndex = 0U; SourceIndex < View->DynamicAllocationCount; ++SourceIndex)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Current;

        Current = &View->DynamicAllocations[SourceIndex];
        if (Inserted == 0 && NewEntry.BaseAddress < Current->BaseAddress)
        {
            if (!LosMemoryManagerAppendFrameDatabaseEntryToArray(
                    LosDynamicAllocationScratch,
                    &ScratchCount,
                    NewEntry.BaseAddress,
                    NewEntry.PageCount,
                    NewEntry.State,
                    NewEntry.Usage,
                    NewEntry.Owner,
                    NewEntry.Source,
                    NewEntry.Attributes))
            {
                return 0;
            }
            Inserted = 1;
        }

        if (!LosMemoryManagerAppendFrameDatabaseEntryToArray(
                LosDynamicAllocationScratch,
                &ScratchCount,
                Current->BaseAddress,
                Current->PageCount,
                Current->State,
                Current->Usage,
                Current->Owner,
                Current->Source,
                Current->Attributes))
        {
            return 0;
        }
    }

    if (Inserted == 0)
    {
        if (!LosMemoryManagerAppendFrameDatabaseEntryToArray(
                LosDynamicAllocationScratch,
                &ScratchCount,
                NewEntry.BaseAddress,
                NewEntry.PageCount,
                NewEntry.State,
                NewEntry.Usage,
                NewEntry.Owner,
                NewEntry.Source,
                NewEntry.Attributes))
        {
            return 0;
        }
    }

    LosMemoryManagerZeroMemory(View->DynamicAllocations, sizeof(View->DynamicAllocations));
    LosMemoryManagerCopyBytes(
        View->DynamicAllocations,
        LosDynamicAllocationScratch,
        sizeof(LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY) * ScratchCount);
    View->DynamicAllocationCount = ScratchCount;
    View->AllocationGeneration += 1ULL;
    return 1;
}

BOOLEAN LosMemoryManagerRemoveDynamicAllocationRange(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 BaseAddress,
    UINT64 PageCount)
{
    UINT64 EndAddress;
    UINT64 CoveredBytes;
    UINTN SourceIndex;
    UINTN ScratchCount;

    if (View == 0 || PageCount == 0ULL)
    {
        return 0;
    }

    if (!LosMemoryManagerTryGetRangeEnd(BaseAddress, PageCount * 4096ULL, &EndAddress))
    {
        LosMemoryManagerHardFail("base-plus-size-wrap", BaseAddress, PageCount, 0x2001ULL);
    }

    LosMemoryManagerZeroMemory(LosDynamicAllocationScratch, sizeof(LosDynamicAllocationScratch));
    ScratchCount = 0U;
    CoveredBytes = 0ULL;

    for (SourceIndex = 0U; SourceIndex < View->DynamicAllocationCount; ++SourceIndex)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Current;
        UINT64 EntryBase;
        UINT64 EntryEnd;
        UINT64 OverlapBase;
        UINT64 OverlapEnd;

        Current = &View->DynamicAllocations[SourceIndex];
        EntryBase = Current->BaseAddress;
        EntryEnd = EntryBase + (Current->PageCount * 4096ULL);
        if (EntryEnd <= BaseAddress || EntryBase >= EndAddress)
        {
            if (!LosMemoryManagerAppendFrameDatabaseEntryToArray(
                    LosDynamicAllocationScratch,
                    &ScratchCount,
                    Current->BaseAddress,
                    Current->PageCount,
                    Current->State,
                    Current->Usage,
                    Current->Owner,
                    Current->Source,
                    Current->Attributes))
            {
                return 0;
            }
            continue;
        }

        OverlapBase = BaseAddress > EntryBase ? BaseAddress : EntryBase;
        OverlapEnd = EndAddress < EntryEnd ? EndAddress : EntryEnd;
        CoveredBytes += OverlapEnd - OverlapBase;

        if (!LosMemoryManagerAppendFrameDatabaseEntryToArray(
                LosDynamicAllocationScratch,
                &ScratchCount,
                EntryBase,
                LosMemoryManagerGetPageCountFromLength(OverlapBase - EntryBase),
                Current->State,
                Current->Usage,
                Current->Owner,
                Current->Source,
                Current->Attributes))
        {
            return 0;
        }
        if (!LosMemoryManagerAppendFrameDatabaseEntryToArray(
                LosDynamicAllocationScratch,
                &ScratchCount,
                OverlapEnd,
                LosMemoryManagerGetPageCountFromLength(EntryEnd - OverlapEnd),
                Current->State,
                Current->Usage,
                Current->Owner,
                Current->Source,
                Current->Attributes))
        {
            return 0;
        }
    }

    if (CoveredBytes != (PageCount * 4096ULL))
    {
        return 0;
    }

    LosMemoryManagerZeroMemory(View->DynamicAllocations, sizeof(View->DynamicAllocations));
    LosMemoryManagerCopyBytes(
        View->DynamicAllocations,
        LosDynamicAllocationScratch,
        sizeof(LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY) * ScratchCount);
    View->DynamicAllocationCount = ScratchCount;
    View->AllocationGeneration += 1ULL;
    return 1;
}

BOOLEAN LosMemoryManagerRebuildCurrentPageFrameDatabase(LOS_MEMORY_MANAGER_MEMORY_VIEW *View)
{
    UINTN Index;

    if (View == 0)
    {
        return 0;
    }

    if (!LosMemoryManagerCopyFrameDatabaseArray(
            View->PageFrameDatabase,
            &View->PageFrameDatabaseEntryCount,
            View->BaselinePageFrameDatabase,
            View->BaselinePageFrameDatabaseEntryCount))
    {
        return 0;
    }

    for (Index = 0U; Index < View->DynamicAllocationCount; ++Index)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Allocation;

        Allocation = &View->DynamicAllocations[Index];
        if (!LosMemoryManagerRewriteFrameDatabaseRange(
                View->PageFrameDatabase,
                &View->PageFrameDatabaseEntryCount,
                Allocation->BaseAddress,
                Allocation->PageCount * 4096ULL,
                Allocation->State,
                Allocation->Usage,
                Allocation->Owner))
        {
            return 0;
        }
    }

    LosMemoryManagerRefreshPageTotals(View);
    return LosMemoryManagerRefreshInternalDescriptorsFromCurrentDatabase(View);
}
