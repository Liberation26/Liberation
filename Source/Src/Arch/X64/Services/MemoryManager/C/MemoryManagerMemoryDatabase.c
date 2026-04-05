#include "MemoryManagerMemoryInternal.h"

static LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY LosPageFrameDatabaseScratch[LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES];

BOOLEAN LosMemoryManagerAppendInternalDescriptor(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 Base,
    UINT64 Length,
    UINT32 Category,
    UINT32 Flags,
    UINT32 Owner,
    UINT32 Source)
{
    LOS_MEMORY_MANAGER_INTERNAL_MEMORY_DESCRIPTOR *Descriptor;

    if (Length == 0ULL)
    {
        return 1;
    }

    if (View->InternalDescriptorCount != 0U)
    {
        LOS_MEMORY_MANAGER_INTERNAL_MEMORY_DESCRIPTOR *Previous;

        Previous = &View->InternalDescriptors[View->InternalDescriptorCount - 1U];
        if (Previous->Base + Previous->Length == Base &&
            Previous->Category == Category &&
            Previous->Flags == Flags &&
            Previous->Owner == Owner &&
            Previous->Source == Source)
        {
            Previous->Length += Length;
            return 1;
        }
    }

    if (View->InternalDescriptorCount >= LOS_MEMORY_MANAGER_MAX_INTERNAL_MEMORY_DESCRIPTORS)
    {
        return 0;
    }

    Descriptor = &View->InternalDescriptors[View->InternalDescriptorCount++];
    Descriptor->Base = Base;
    Descriptor->Length = Length;
    Descriptor->Category = Category;
    Descriptor->Flags = Flags;
    Descriptor->Owner = Owner;
    Descriptor->Source = Source;
    return 1;
}

BOOLEAN LosMemoryManagerAppendFrameDatabaseEntryToArray(
    LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN *Count,
    UINT64 BaseAddress,
    UINT64 PageCount,
    UINT32 State,
    UINT32 Usage,
    UINT32 Owner,
    UINT32 Source,
    UINT64 Attributes)
{
    LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry;

    if (PageCount == 0ULL)
    {
        return 1;
    }

    if (*Count != 0U)
    {
        LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Previous;

        Previous = &Array[*Count - 1U];
        if (Previous->BaseAddress + (Previous->PageCount * 4096ULL) == BaseAddress &&
            Previous->State == State &&
            Previous->Usage == Usage &&
            Previous->Owner == Owner &&
            Previous->Source == Source &&
            Previous->Attributes == Attributes)
        {
            Previous->PageCount += PageCount;
            return 1;
        }
    }

    if (*Count >= LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES)
    {
        return 0;
    }

    Entry = &Array[*Count];
    Entry->BaseAddress = BaseAddress;
    Entry->PageCount = PageCount;
    Entry->State = State;
    Entry->Usage = Usage;
    Entry->Owner = Owner;
    Entry->Source = Source;
    Entry->Attributes = Attributes;
    *Count += 1U;
    return 1;
}

BOOLEAN LosMemoryManagerAppendFrameDatabaseEntry(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 BaseAddress,
    UINT64 PageCount,
    UINT32 State,
    UINT32 Usage,
    UINT32 Owner,
    UINT32 Source,
    UINT64 Attributes)
{
    return LosMemoryManagerAppendFrameDatabaseEntryToArray(
        View->PageFrameDatabase,
        &View->PageFrameDatabaseEntryCount,
        BaseAddress,
        PageCount,
        State,
        Usage,
        Owner,
        Source,
        Attributes);
}

BOOLEAN LosMemoryManagerCopyFrameDatabaseArray(
    LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Destination,
    UINTN *DestinationCount,
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Source,
    UINTN SourceCount)
{
    if (Destination == 0 || DestinationCount == 0 || Source == 0)
    {
        return 0;
    }

    if (SourceCount > LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES)
    {
        return 0;
    }

    LosMemoryManagerZeroMemory(
        Destination,
        sizeof(LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY) * LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES);
    if (SourceCount != 0U)
    {
        LosMemoryManagerCopyBytes(
            Destination,
            Source,
            sizeof(LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY) * SourceCount);
    }
    *DestinationCount = SourceCount;
    return 1;
}

BOOLEAN LosMemoryManagerRewriteFrameDatabaseRange(
    LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN *Count,
    UINT64 BaseAddress,
    UINT64 Length,
    UINT32 State,
    UINT32 Usage,
    UINT32 Owner)
{
    UINTN SourceIndex;
    UINTN ScratchCount;
    UINT64 EndAddress;
    UINT64 CoveredBytes;

    if (Array == 0 || Count == 0)
    {
        return 0;
    }

    EndAddress = BaseAddress + Length;
    if (Length == 0ULL || EndAddress <= BaseAddress)
    {
        return 0;
    }

    ScratchCount = 0U;
    CoveredBytes = 0ULL;
    LosMemoryManagerZeroMemory(LosPageFrameDatabaseScratch, sizeof(LosPageFrameDatabaseScratch));

    for (SourceIndex = 0U; SourceIndex < *Count; ++SourceIndex)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *SourceEntry;
        UINT64 SourceBase;
        UINT64 SourceEnd;
        UINT64 OverlapBase;
        UINT64 OverlapEnd;

        SourceEntry = &Array[SourceIndex];
        SourceBase = SourceEntry->BaseAddress;
        SourceEnd = SourceBase + (SourceEntry->PageCount * 4096ULL);
        if (SourceEnd <= BaseAddress || SourceBase >= EndAddress)
        {
            if (!LosMemoryManagerAppendFrameDatabaseEntryToArray(
                    LosPageFrameDatabaseScratch,
                    &ScratchCount,
                    SourceEntry->BaseAddress,
                    SourceEntry->PageCount,
                    SourceEntry->State,
                    SourceEntry->Usage,
                    SourceEntry->Owner,
                    SourceEntry->Source,
                    SourceEntry->Attributes))
            {
                return 0;
            }
            continue;
        }

        OverlapBase = BaseAddress > SourceBase ? BaseAddress : SourceBase;
        OverlapEnd = EndAddress < SourceEnd ? EndAddress : SourceEnd;
        CoveredBytes += OverlapEnd - OverlapBase;
        if (!LosMemoryManagerAppendFrameDatabaseEntryToArray(
                LosPageFrameDatabaseScratch,
                &ScratchCount,
                SourceBase,
                LosMemoryManagerGetPageCountFromLength(OverlapBase - SourceBase),
                SourceEntry->State,
                SourceEntry->Usage,
                SourceEntry->Owner,
                SourceEntry->Source,
                SourceEntry->Attributes))
        {
            return 0;
        }
        if (!LosMemoryManagerAppendFrameDatabaseEntryToArray(
                LosPageFrameDatabaseScratch,
                &ScratchCount,
                OverlapBase,
                LosMemoryManagerGetPageCountFromLength(OverlapEnd - OverlapBase),
                State,
                Usage,
                Owner,
                SourceEntry->Source,
                SourceEntry->Attributes))
        {
            return 0;
        }
        if (!LosMemoryManagerAppendFrameDatabaseEntryToArray(
                LosPageFrameDatabaseScratch,
                &ScratchCount,
                OverlapEnd,
                LosMemoryManagerGetPageCountFromLength(SourceEnd - OverlapEnd),
                SourceEntry->State,
                SourceEntry->Usage,
                SourceEntry->Owner,
                SourceEntry->Source,
                SourceEntry->Attributes))
        {
            return 0;
        }
    }

    if (CoveredBytes != Length)
    {
        return 0;
    }

    LosMemoryManagerZeroMemory(
        Array,
        sizeof(LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY) * LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES);
    *Count = 0U;
    for (SourceIndex = 0U; SourceIndex < ScratchCount; ++SourceIndex)
    {
        if (!LosMemoryManagerAppendFrameDatabaseEntryToArray(
                Array,
                Count,
                LosPageFrameDatabaseScratch[SourceIndex].BaseAddress,
                LosPageFrameDatabaseScratch[SourceIndex].PageCount,
                LosPageFrameDatabaseScratch[SourceIndex].State,
                LosPageFrameDatabaseScratch[SourceIndex].Usage,
                LosPageFrameDatabaseScratch[SourceIndex].Owner,
                LosPageFrameDatabaseScratch[SourceIndex].Source,
                LosPageFrameDatabaseScratch[SourceIndex].Attributes))
        {
            return 0;
        }
    }

    return 1;
}

BOOLEAN LosMemoryManagerReserveFrameDatabaseRange(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 BaseAddress,
    UINT64 Length,
    UINT32 Usage,
    UINT32 Owner)
{
    UINT64 AlignedBase;
    UINT64 AlignedEnd;

    if (Length == 0ULL)
    {
        return 1;
    }

    AlignedBase = LosMemoryManagerAlignDownPage(BaseAddress);
    AlignedEnd = LosMemoryManagerAlignUpPage(BaseAddress + Length);
    if (AlignedEnd <= AlignedBase)
    {
        return 0;
    }

    return LosMemoryManagerRewriteFrameDatabaseRange(
        View->PageFrameDatabase,
        &View->PageFrameDatabaseEntryCount,
        AlignedBase,
        AlignedEnd - AlignedBase,
        LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RESERVED,
        Usage,
        Owner);
}

BOOLEAN LosMemoryManagerRefreshInternalDescriptorsFromCurrentDatabase(LOS_MEMORY_MANAGER_MEMORY_VIEW *View)
{
    UINTN Index;

    if (View == 0)
    {
        return 0;
    }

    LosMemoryManagerZeroMemory(View->InternalDescriptors, sizeof(View->InternalDescriptors));
    View->InternalDescriptorCount = 0U;

    for (Index = 0U; Index < View->PageFrameDatabaseEntryCount; ++Index)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry;
        UINT32 Category;
        UINT32 Flags;
        UINT32 Owner;

        Entry = &View->PageFrameDatabase[Index];
        Category = LosMemoryManagerDetermineMemoryCategoryFromFrameEntry(Entry);
        Flags = LosMemoryManagerDetermineRegionFlagsFromFrameEntry(Entry);
        Owner = (Entry->State == LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE)
                    ? LOS_X64_MEMORY_REGION_OWNER_NONE
                    : Entry->Owner;
        if (!LosMemoryManagerAppendInternalDescriptor(
                View,
                Entry->BaseAddress,
                Entry->PageCount * 4096ULL,
                Category,
                Flags,
                Owner,
                Entry->Source))
        {
            return 0;
        }
    }

    return 1;
}

void LosMemoryManagerRefreshPageTotals(LOS_MEMORY_MANAGER_MEMORY_VIEW *View)
{
    UINTN Index;

    View->TotalPages = 0ULL;
    View->FreePages = 0ULL;
    View->ReservedPages = 0ULL;
    View->RuntimePages = 0ULL;
    View->MmioPages = 0ULL;

    for (Index = 0U; Index < View->PageFrameDatabaseEntryCount; ++Index)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry;

        Entry = &View->PageFrameDatabase[Index];
        View->TotalPages += Entry->PageCount;
        switch (Entry->State)
        {
            case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE:
                View->FreePages += Entry->PageCount;
                break;
            case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RUNTIME:
                View->RuntimePages += Entry->PageCount;
                break;
            case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_MMIO:
                View->MmioPages += Entry->PageCount;
                break;
            case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RESERVED:
            default:
                View->ReservedPages += Entry->PageCount;
                break;
        }
    }
}
