#include "MemoryManagerMain.h"

static LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY LosPageFrameDatabaseScratch[LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES];
static LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY LosDynamicAllocationScratch[LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES];

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

static void CopyBytes(void *Destination, const void *Source, UINTN ByteCount)
{
    UINT8 *DestinationBytes;
    const UINT8 *SourceBytes;
    UINTN Index;

    if (Destination == 0 || Source == 0)
    {
        return;
    }

    DestinationBytes = (UINT8 *)Destination;
    SourceBytes = (const UINT8 *)Source;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        DestinationBytes[Index] = SourceBytes[Index];
    }
}

static UINT64 AlignDownPage(UINT64 Value)
{
    return Value & ~0xFFFULL;
}

static UINT64 AlignUpPage(UINT64 Value)
{
    return (Value + 0xFFFULL) & ~0xFFFULL;
}

static UINT64 AlignUpValue(UINT64 Value, UINT64 Alignment)
{
    if (Alignment == 0ULL)
    {
        return Value;
    }

    return (Value + (Alignment - 1ULL)) & ~(Alignment - 1ULL);
}

static UINT64 GetPageCountFromLength(UINT64 Length)
{
    return Length / 4096ULL;
}

static BOOLEAN IsPowerOfTwo(UINT64 Value)
{
    return Value != 0ULL && (Value & (Value - 1ULL)) == 0ULL;
}

static UINT32 ClassifyMemoryCategory(UINT32 RegionType)
{
    switch (RegionType)
    {
        case LOS_X64_MEMORY_REGION_TYPE_USABLE:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_USABLE;
        case LOS_X64_MEMORY_REGION_TYPE_BOOT_RESERVED:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_BOOTSTRAP_RESERVED;
        case LOS_X64_MEMORY_REGION_TYPE_RUNTIME:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_RUNTIME;
        case LOS_X64_MEMORY_REGION_TYPE_MMIO:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_MMIO;
        case LOS_X64_MEMORY_REGION_TYPE_ACPI_NVS:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_ACPI_NVS;
        case LOS_X64_MEMORY_REGION_TYPE_FIRMWARE_RESERVED:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_FIRMWARE_RESERVED;
        case LOS_X64_MEMORY_REGION_TYPE_UNUSABLE:
        default:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_UNUSABLE;
    }
}

static UINT32 ClassifyFrameState(UINT32 RegionType)
{
    switch (RegionType)
    {
        case LOS_X64_MEMORY_REGION_TYPE_USABLE:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE;
        case LOS_X64_MEMORY_REGION_TYPE_RUNTIME:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RUNTIME;
        case LOS_X64_MEMORY_REGION_TYPE_MMIO:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_MMIO;
        default:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RESERVED;
    }
}

static UINT32 ClassifyFrameUsage(UINT32 RegionType)
{
    switch (RegionType)
    {
        case LOS_X64_MEMORY_REGION_TYPE_USABLE:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_FREE;
        case LOS_X64_MEMORY_REGION_TYPE_BOOT_RESERVED:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_BOOTSTRAP_RESERVED;
        case LOS_X64_MEMORY_REGION_TYPE_RUNTIME:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_RUNTIME;
        case LOS_X64_MEMORY_REGION_TYPE_MMIO:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_MMIO;
        case LOS_X64_MEMORY_REGION_TYPE_ACPI_NVS:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ACPI_NVS;
        case LOS_X64_MEMORY_REGION_TYPE_FIRMWARE_RESERVED:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_FIRMWARE_RESERVED;
        case LOS_X64_MEMORY_REGION_TYPE_UNUSABLE:
        default:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_UNUSABLE;
    }
}

static UINT32 DetermineMemoryCategoryFromFrameEntry(const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry)
{
    if (Entry == 0)
    {
        return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_UNUSABLE;
    }

    switch (Entry->State)
    {
        case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_USABLE;
        case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RUNTIME:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_RUNTIME;
        case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_MMIO:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_MMIO;
        case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RESERVED:
        default:
            switch (Entry->Usage)
            {
                case LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_FIRMWARE_RESERVED:
                    return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_FIRMWARE_RESERVED;
                case LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ACPI_NVS:
                    return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_ACPI_NVS;
                case LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_UNUSABLE:
                    return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_UNUSABLE;
                default:
                    return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_BOOTSTRAP_RESERVED;
            }
    }
}

static UINT32 DetermineRegionFlagsFromFrameEntry(const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry)
{
    UINT32 Flags;

    if (Entry == 0)
    {
        return 0U;
    }

    Flags = (UINT32)Entry->Attributes;
    if (Entry->State == LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RESERVED &&
        (Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_DYNAMIC_RESERVED ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_DYNAMIC_CLAIMED ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_IMAGE ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_STACK ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_REQUEST_MAILBOX ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_RESPONSE_MAILBOX ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_EVENT_MAILBOX ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_LAUNCH_BLOCK ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ENDPOINT_OBJECT ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ADDRESS_SPACE_OBJECT ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_TASK_OBJECT ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_PAGE_TABLE))
    {
        Flags |= LOS_X64_MEMORY_REGION_FLAG_OVERLAY;
        Flags |= LOS_X64_MEMORY_REGION_FLAG_CLAIMED;
    }

    return Flags;
}

static BOOLEAN AppendInternalDescriptor(
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

static BOOLEAN AppendFrameDatabaseEntryToArray(
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

static BOOLEAN AppendFrameDatabaseEntry(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 BaseAddress,
    UINT64 PageCount,
    UINT32 State,
    UINT32 Usage,
    UINT32 Owner,
    UINT32 Source,
    UINT64 Attributes)
{
    return AppendFrameDatabaseEntryToArray(
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

static BOOLEAN CopyFrameDatabaseArray(
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

    ZeroBytes(Destination, sizeof(LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY) * LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES);
    if (SourceCount != 0U)
    {
        CopyBytes(Destination, Source, sizeof(LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY) * SourceCount);
    }
    *DestinationCount = SourceCount;
    return 1;
}

static BOOLEAN RewriteFrameDatabaseRange(
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
    ZeroBytes(LosPageFrameDatabaseScratch, sizeof(LosPageFrameDatabaseScratch));

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
            if (!AppendFrameDatabaseEntryToArray(
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
        if (!AppendFrameDatabaseEntryToArray(
                LosPageFrameDatabaseScratch,
                &ScratchCount,
                SourceBase,
                GetPageCountFromLength(OverlapBase - SourceBase),
                SourceEntry->State,
                SourceEntry->Usage,
                SourceEntry->Owner,
                SourceEntry->Source,
                SourceEntry->Attributes))
        {
            return 0;
        }
        if (!AppendFrameDatabaseEntryToArray(
                LosPageFrameDatabaseScratch,
                &ScratchCount,
                OverlapBase,
                GetPageCountFromLength(OverlapEnd - OverlapBase),
                State,
                Usage,
                Owner,
                SourceEntry->Source,
                SourceEntry->Attributes))
        {
            return 0;
        }
        if (!AppendFrameDatabaseEntryToArray(
                LosPageFrameDatabaseScratch,
                &ScratchCount,
                OverlapEnd,
                GetPageCountFromLength(SourceEnd - OverlapEnd),
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

    ZeroBytes(Array, sizeof(LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY) * LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES);
    *Count = 0U;
    for (SourceIndex = 0U; SourceIndex < ScratchCount; ++SourceIndex)
    {
        if (!AppendFrameDatabaseEntryToArray(
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

static BOOLEAN ReserveFrameDatabaseRange(
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

    AlignedBase = AlignDownPage(BaseAddress);
    AlignedEnd = AlignUpPage(BaseAddress + Length);
    if (AlignedEnd <= AlignedBase)
    {
        return 0;
    }

    return RewriteFrameDatabaseRange(
        View->PageFrameDatabase,
        &View->PageFrameDatabaseEntryCount,
        AlignedBase,
        AlignedEnd - AlignedBase,
        LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RESERVED,
        Usage,
        Owner);
}

static BOOLEAN RefreshInternalDescriptorsFromCurrentDatabase(LOS_MEMORY_MANAGER_MEMORY_VIEW *View)
{
    UINTN Index;

    if (View == 0)
    {
        return 0;
    }

    ZeroBytes(View->InternalDescriptors, sizeof(View->InternalDescriptors));
    View->InternalDescriptorCount = 0U;

    for (Index = 0U; Index < View->PageFrameDatabaseEntryCount; ++Index)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry;
        UINT32 Category;
        UINT32 Flags;
        UINT32 Owner;

        Entry = &View->PageFrameDatabase[Index];
        Category = DetermineMemoryCategoryFromFrameEntry(Entry);
        Flags = DetermineRegionFlagsFromFrameEntry(Entry);
        Owner = (Entry->State == LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE) ? LOS_X64_MEMORY_REGION_OWNER_NONE : Entry->Owner;
        if (!AppendInternalDescriptor(
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

static void RefreshPageTotals(LOS_MEMORY_MANAGER_MEMORY_VIEW *View)
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

static BOOLEAN IsRangeCoveredByPageFrameDatabase(
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN Count,
    UINT64 BaseAddress,
    UINT64 Length)
{
    UINT64 EndAddress;
    UINT64 Cursor;
    UINTN Index;

    if (Array == 0 || Length == 0ULL)
    {
        return 0;
    }

    EndAddress = BaseAddress + Length;
    if (EndAddress <= BaseAddress)
    {
        return 0;
    }

    Cursor = BaseAddress;
    for (Index = 0U; Index < Count && Cursor < EndAddress; ++Index)
    {
        UINT64 EntryBase;
        UINT64 EntryEnd;

        EntryBase = Array[Index].BaseAddress;
        EntryEnd = EntryBase + (Array[Index].PageCount * 4096ULL);
        if (EntryEnd <= Cursor)
        {
            continue;
        }
        if (EntryBase > Cursor)
        {
            return 0;
        }
        if (EntryEnd > Cursor)
        {
            Cursor = EntryEnd;
        }
    }

    return Cursor >= EndAddress;
}

static BOOLEAN IsRangeCoveredByState(
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN Count,
    UINT64 BaseAddress,
    UINT64 Length,
    UINT32 State)
{
    UINT64 EndAddress;
    UINT64 Cursor;
    UINTN Index;

    if (Array == 0 || Length == 0ULL)
    {
        return 0;
    }

    EndAddress = BaseAddress + Length;
    if (EndAddress <= BaseAddress)
    {
        return 0;
    }

    Cursor = BaseAddress;
    for (Index = 0U; Index < Count && Cursor < EndAddress; ++Index)
    {
        UINT64 EntryBase;
        UINT64 EntryEnd;
        UINT64 OverlapBase;
        UINT64 OverlapEnd;

        EntryBase = Array[Index].BaseAddress;
        EntryEnd = EntryBase + (Array[Index].PageCount * 4096ULL);
        if (EntryEnd <= Cursor || EntryEnd <= BaseAddress || EntryBase >= EndAddress)
        {
            continue;
        }
        OverlapBase = Cursor > EntryBase ? Cursor : EntryBase;
        if (OverlapBase > Cursor)
        {
            return 0;
        }
        if (Array[Index].State != State)
        {
            return 0;
        }
        OverlapEnd = EndAddress < EntryEnd ? EndAddress : EntryEnd;
        Cursor = OverlapEnd;
    }

    return Cursor >= EndAddress;
}

static BOOLEAN FindClaimableContiguousRange(
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN Count,
    UINT64 MinimumPhysicalAddress,
    UINT64 MaximumPhysicalAddress,
    UINT64 AlignmentBytes,
    UINT64 PageCount,
    UINT64 *BaseAddress)
{
    UINT64 RequiredBytes;
    UINT64 CurrentRunBase;
    UINT64 CurrentRunEnd;
    BOOLEAN HaveRun;
    UINTN Index;

    if (Array == 0 || BaseAddress == 0 || PageCount == 0ULL)
    {
        return 0;
    }

    RequiredBytes = PageCount * 4096ULL;
    if (RequiredBytes == 0ULL || RequiredBytes / 4096ULL != PageCount)
    {
        return 0;
    }

    CurrentRunBase = 0ULL;
    CurrentRunEnd = 0ULL;
    HaveRun = 0;

    for (Index = 0U; Index < Count; ++Index)
    {
        UINT64 EntryBase;
        UINT64 EntryEnd;
        UINT64 SpanBase;
        UINT64 SpanEnd;
        UINT64 CandidateBase;

        if (Array[Index].State != LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE)
        {
            continue;
        }

        EntryBase = Array[Index].BaseAddress;
        EntryEnd = EntryBase + (Array[Index].PageCount * 4096ULL);
        if (EntryEnd <= MinimumPhysicalAddress || EntryBase >= MaximumPhysicalAddress)
        {
            continue;
        }

        SpanBase = EntryBase > MinimumPhysicalAddress ? EntryBase : MinimumPhysicalAddress;
        SpanEnd = EntryEnd < MaximumPhysicalAddress ? EntryEnd : MaximumPhysicalAddress;
        if (SpanEnd <= SpanBase)
        {
            continue;
        }

        if (HaveRun == 0 || SpanBase > CurrentRunEnd)
        {
            CandidateBase = AlignUpValue(SpanBase, AlignmentBytes);
            if (CandidateBase >= SpanEnd)
            {
                HaveRun = 0;
                continue;
            }

            CurrentRunBase = CandidateBase;
            CurrentRunEnd = SpanEnd;
            HaveRun = 1;
        }
        else
        {
            if (SpanEnd > CurrentRunEnd)
            {
                CurrentRunEnd = SpanEnd;
            }
        }

        if (HaveRun != 0 && CurrentRunEnd - CurrentRunBase >= RequiredBytes)
        {
            *BaseAddress = CurrentRunBase;
            return 1;
        }
    }

    return 0;
}

static BOOLEAN InsertDynamicAllocation(
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

    ZeroBytes(LosDynamicAllocationScratch, sizeof(LosDynamicAllocationScratch));
    ScratchCount = 0U;
    Inserted = 0;

    for (SourceIndex = 0U; SourceIndex < View->DynamicAllocationCount; ++SourceIndex)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Current;

        Current = &View->DynamicAllocations[SourceIndex];
        if (Inserted == 0 && NewEntry.BaseAddress < Current->BaseAddress)
        {
            if (!AppendFrameDatabaseEntryToArray(
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

        if (!AppendFrameDatabaseEntryToArray(
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
        if (!AppendFrameDatabaseEntryToArray(
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

    ZeroBytes(View->DynamicAllocations, sizeof(View->DynamicAllocations));
    CopyBytes(View->DynamicAllocations, LosDynamicAllocationScratch, sizeof(LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY) * ScratchCount);
    View->DynamicAllocationCount = ScratchCount;
    View->AllocationGeneration += 1ULL;
    return 1;
}

static BOOLEAN RemoveDynamicAllocationRange(
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

    EndAddress = BaseAddress + (PageCount * 4096ULL);
    if (EndAddress <= BaseAddress)
    {
        return 0;
    }

    ZeroBytes(LosDynamicAllocationScratch, sizeof(LosDynamicAllocationScratch));
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
            if (!AppendFrameDatabaseEntryToArray(
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

        if (!AppendFrameDatabaseEntryToArray(
                LosDynamicAllocationScratch,
                &ScratchCount,
                EntryBase,
                GetPageCountFromLength(OverlapBase - EntryBase),
                Current->State,
                Current->Usage,
                Current->Owner,
                Current->Source,
                Current->Attributes))
        {
            return 0;
        }
        if (!AppendFrameDatabaseEntryToArray(
                LosDynamicAllocationScratch,
                &ScratchCount,
                OverlapEnd,
                GetPageCountFromLength(EntryEnd - OverlapEnd),
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

    ZeroBytes(View->DynamicAllocations, sizeof(View->DynamicAllocations));
    CopyBytes(View->DynamicAllocations, LosDynamicAllocationScratch, sizeof(LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY) * ScratchCount);
    View->DynamicAllocationCount = ScratchCount;
    View->AllocationGeneration += 1ULL;
    return 1;
}

static BOOLEAN RebuildCurrentPageFrameDatabase(LOS_MEMORY_MANAGER_MEMORY_VIEW *View)
{
    UINTN Index;

    if (View == 0)
    {
        return 0;
    }

    if (!CopyFrameDatabaseArray(
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
        if (!RewriteFrameDatabaseRange(
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

    RefreshPageTotals(View);
    return RefreshInternalDescriptorsFromCurrentDatabase(View);
}

static BOOLEAN IngestNormalizedRegionTable(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 *Detail)
{
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View;
    const LOS_X64_MEMORY_REGION *Regions;
    UINTN Index;

    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_NONE;
    }

    if (State == 0 || State->LaunchBlock == 0 || State->DirectMapOffset == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_TABLE_TRANSLATION;
        }
        return 0;
    }

    View = &State->MemoryView;
    ZeroBytes(View, sizeof(*View));
    View->NormalizedRegionTablePhysicalAddress = State->LaunchBlock->MemoryRegionTablePhysicalAddress;
    View->NormalizedRegionCount = State->LaunchBlock->MemoryRegionCount;
    View->NormalizedRegionEntrySize = State->LaunchBlock->MemoryRegionEntrySize;

    if (View->NormalizedRegionTablePhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_TABLE_PHYSICAL;
        }
        return 0;
    }
    if (View->NormalizedRegionCount == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_TABLE_COUNT;
        }
        return 0;
    }
    if (View->NormalizedRegionEntrySize != (UINT64)sizeof(LOS_X64_MEMORY_REGION))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_TABLE_ENTRY_SIZE;
        }
        return 0;
    }
    if (View->NormalizedRegionCount > LOS_MEMORY_MANAGER_MAX_INTERNAL_MEMORY_DESCRIPTORS)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_DESCRIPTOR_CAPACITY;
        }
        return 0;
    }

    Regions = (const LOS_X64_MEMORY_REGION *)(UINTN)(State->DirectMapOffset + View->NormalizedRegionTablePhysicalAddress);
    if (Regions == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_TABLE_TRANSLATION;
        }
        return 0;
    }

    View->NormalizedRegionTable = Regions;
    for (Index = 0U; Index < (UINTN)View->NormalizedRegionCount; ++Index)
    {
        const LOS_X64_MEMORY_REGION *Region;
        UINT32 Category;

        Region = &Regions[Index];
        if ((Region->Base & 0xFFFULL) != 0ULL || (Region->Length & 0xFFFULL) != 0ULL || Region->Length == 0ULL)
        {
            if (Detail != 0)
            {
                *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_RANGE_INVALID;
            }
            return 0;
        }

        Category = ClassifyMemoryCategory(Region->Type);
        if (!AppendInternalDescriptor(View, Region->Base, Region->Length, Category, Region->Flags, Region->Owner, Region->Source))
        {
            if (Detail != 0)
            {
                *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_DESCRIPTOR_CAPACITY;
            }
            return 0;
        }

        switch (Category)
        {
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_USABLE:
                View->TotalUsableBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_BOOTSTRAP_RESERVED:
                View->TotalBootstrapReservedBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_FIRMWARE_RESERVED:
                View->TotalFirmwareReservedBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_RUNTIME:
                View->TotalRuntimeBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_MMIO:
                View->TotalMmioBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_ACPI_NVS:
                View->TotalAcpiBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_UNUSABLE:
            default:
                View->TotalUnusableBytes += Region->Length;
                break;
        }

        if (!AppendFrameDatabaseEntry(
                View,
                Region->Base,
                GetPageCountFromLength(Region->Length),
                ClassifyFrameState(Region->Type),
                ClassifyFrameUsage(Region->Type),
                Region->Owner,
                Region->Source,
                (UINT64)Region->Flags))
        {
            if (Detail != 0)
            {
                *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_FRAME_DATABASE_CAPACITY;
            }
            return 0;
        }
    }

    return 1;
}

BOOLEAN LosMemoryManagerServiceBuildMemoryView(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 *Detail)
{
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View;

    if (!IngestNormalizedRegionTable(State, Detail))
    {
        return 0;
    }

    View = &State->MemoryView;
    if (!ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceImagePhysicalAddress, State->LaunchBlock->ServiceImageSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_IMAGE, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceStackPhysicalAddress, State->LaunchBlock->ServiceStackPageCount * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_STACK, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->RequestMailboxPhysicalAddress, State->LaunchBlock->RequestMailboxSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_REQUEST_MAILBOX, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ResponseMailboxPhysicalAddress, State->LaunchBlock->ResponseMailboxSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_RESPONSE_MAILBOX, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->EventMailboxPhysicalAddress, State->LaunchBlock->EventMailboxSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_EVENT_MAILBOX, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->LaunchBlockPhysicalAddress, LOS_MEMORY_MANAGER_LAUNCH_BLOCK_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_LAUNCH_BLOCK, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->KernelToServiceEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ENDPOINT_OBJECT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceToKernelEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ENDPOINT_OBJECT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceEventsEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ENDPOINT_OBJECT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress, LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ADDRESS_SPACE_OBJECT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceTaskObjectPhysicalAddress, LOS_MEMORY_MANAGER_TASK_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_TASK_OBJECT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServicePageMapLevel4PhysicalAddress, 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_PAGE_TABLE, LOS_X64_MEMORY_REGION_OWNER_CLAIMED))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_OVERLAY_CONFLICT;
        }
        return 0;
    }

    if (!CopyFrameDatabaseArray(
            View->BaselinePageFrameDatabase,
            &View->BaselinePageFrameDatabaseEntryCount,
            View->PageFrameDatabase,
            View->PageFrameDatabaseEntryCount))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_FRAME_DATABASE_CAPACITY;
        }
        return 0;
    }

    View->DynamicAllocationCount = 0U;
    View->AllocationGeneration = 0ULL;
    RefreshPageTotals(View);
    if (!RefreshInternalDescriptorsFromCurrentDatabase(View))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_DESCRIPTOR_CAPACITY;
        }
        return 0;
    }
    View->Ready = 1U;
    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_NONE;
    }
    return 1;
}

void LosMemoryManagerServiceQueryMemoryRegions(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request,
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    UINTN Index;

    if (Response == 0)
    {
        return;
    }

    Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Response->Payload.QueryMemoryRegions.Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Response->Payload.QueryMemoryRegions.Reserved = 0U;
    Response->Payload.QueryMemoryRegions.TotalRegionCount = 0ULL;
    Response->Payload.QueryMemoryRegions.RegionsWritten = 0ULL;
    Response->Payload.QueryMemoryRegions.RegionEntrySize = (UINT64)sizeof(LOS_X64_MEMORY_REGION);

    if (State == 0 || Request == 0 || State->MemoryView.Ready == 0U)
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        Response->Payload.QueryMemoryRegions.Status = Response->Status;
        return;
    }

    Response->Payload.QueryMemoryRegions.TotalRegionCount = (UINT64)State->MemoryView.InternalDescriptorCount;
    if (Request->Payload.QueryMemoryRegions.Buffer == 0 && Request->Payload.QueryMemoryRegions.BufferRegionCapacity != 0U)
    {
        Response->Payload.QueryMemoryRegions.Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
        Response->Status = Response->Payload.QueryMemoryRegions.Status;
        return;
    }

    for (Index = 0U;
         Index < State->MemoryView.InternalDescriptorCount && Index < Request->Payload.QueryMemoryRegions.BufferRegionCapacity;
         ++Index)
    {
        const LOS_MEMORY_MANAGER_INTERNAL_MEMORY_DESCRIPTOR *Descriptor;
        LOS_X64_MEMORY_REGION *Region;

        Descriptor = &State->MemoryView.InternalDescriptors[Index];
        Region = &Request->Payload.QueryMemoryRegions.Buffer[Index];
        Region->Base = Descriptor->Base;
        Region->Length = Descriptor->Length;
        switch (Descriptor->Category)
        {
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_USABLE:
                Region->Type = LOS_X64_MEMORY_REGION_TYPE_USABLE;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_BOOTSTRAP_RESERVED:
                Region->Type = LOS_X64_MEMORY_REGION_TYPE_BOOT_RESERVED;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_FIRMWARE_RESERVED:
                Region->Type = LOS_X64_MEMORY_REGION_TYPE_FIRMWARE_RESERVED;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_RUNTIME:
                Region->Type = LOS_X64_MEMORY_REGION_TYPE_RUNTIME;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_MMIO:
                Region->Type = LOS_X64_MEMORY_REGION_TYPE_MMIO;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_ACPI_NVS:
                Region->Type = LOS_X64_MEMORY_REGION_TYPE_ACPI_NVS;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_UNUSABLE:
            default:
                Region->Type = LOS_X64_MEMORY_REGION_TYPE_UNUSABLE;
                break;
        }
        Region->Flags = Descriptor->Flags;
        Region->Owner = Descriptor->Owner;
        Region->Source = Descriptor->Source;
        Response->Payload.QueryMemoryRegions.RegionsWritten += 1ULL;
    }

    Response->Payload.QueryMemoryRegions.Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Response->Status = Response->Payload.QueryMemoryRegions.Status;
}

void LosMemoryManagerServiceReserveFrames(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_X64_RESERVE_FRAMES_REQUEST *Request,
    LOS_X64_RESERVE_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View;
    UINT64 AlignedBase;
    UINT64 AlignedEnd;
    UINT32 Owner;

    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->PagesReserved = 0ULL;
    Result->RangeBase = 0ULL;
    Result->RangeLength = 0ULL;

    if (State == 0 || Request == 0 || Request->Length == 0ULL || State->MemoryView.Ready == 0U)
    {
        if (State == 0 || State->MemoryView.Ready == 0U)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        }
        return;
    }

    View = &State->MemoryView;
    AlignedBase = AlignDownPage(Request->PhysicalAddress);
    AlignedEnd = AlignUpPage(Request->PhysicalAddress + Request->Length);
    if (AlignedEnd <= AlignedBase)
    {
        return;
    }

    if (!IsRangeCoveredByPageFrameDatabase(View->PageFrameDatabase, View->PageFrameDatabaseEntryCount, AlignedBase, AlignedEnd - AlignedBase))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
        return;
    }
    if (!IsRangeCoveredByState(View->PageFrameDatabase, View->PageFrameDatabaseEntryCount, AlignedBase, AlignedEnd - AlignedBase, LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    Owner = Request->Owner == LOS_X64_PHYSICAL_FRAME_RESERVED_NONE ? LOS_X64_MEMORY_REGION_OWNER_CLAIMED : Request->Owner;
    if (!InsertDynamicAllocation(View, AlignedBase, GetPageCountFromLength(AlignedEnd - AlignedBase), LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_DYNAMIC_RESERVED, Owner) ||
        !RebuildCurrentPageFrameDatabase(View))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->PagesReserved = GetPageCountFromLength(AlignedEnd - AlignedBase);
    Result->RangeBase = AlignedBase;
    Result->RangeLength = AlignedEnd - AlignedBase;
}

void LosMemoryManagerServiceClaimFrames(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_X64_CLAIM_FRAMES_REQUEST *Request,
    LOS_X64_CLAIM_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View;
    UINT64 AlignmentBytes;
    UINT64 MinimumPhysicalAddress;
    UINT64 MaximumPhysicalAddress;
    UINT64 BaseAddress;
    UINT64 RequiredBytes;
    UINT32 Owner;

    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->BaseAddress = 0ULL;
    Result->PageCount = 0ULL;

    if (State == 0 || Request == 0 || Request->PageCount == 0ULL || State->MemoryView.Ready == 0U)
    {
        if (State == 0 || State->MemoryView.Ready == 0U)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        }
        return;
    }

    View = &State->MemoryView;
    RequiredBytes = Request->PageCount * 4096ULL;
    if (RequiredBytes == 0ULL || RequiredBytes / 4096ULL != Request->PageCount)
    {
        return;
    }

    AlignmentBytes = Request->AlignmentBytes == 0ULL ? 4096ULL : Request->AlignmentBytes;
    if (AlignmentBytes < 4096ULL || (AlignmentBytes & 0xFFFULL) != 0ULL || !IsPowerOfTwo(AlignmentBytes))
    {
        return;
    }

    MinimumPhysicalAddress = AlignDownPage(Request->MinimumPhysicalAddress);
    MaximumPhysicalAddress = Request->MaximumPhysicalAddress == 0ULL ? 0ULL : AlignUpPage(Request->MaximumPhysicalAddress);
    /* Clamp to the highest discovered address represented in the current database. */
    if (View->PageFrameDatabaseEntryCount != 0U)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *LastEntry;
        UINT64 DatabaseLimit;

        LastEntry = &View->PageFrameDatabase[View->PageFrameDatabaseEntryCount - 1U];
        DatabaseLimit = LastEntry->BaseAddress + (LastEntry->PageCount * 4096ULL);
        if (Request->MaximumPhysicalAddress == 0ULL || MaximumPhysicalAddress > DatabaseLimit)
        {
            MaximumPhysicalAddress = DatabaseLimit;
        }
    }
    if ((Request->Flags & LOS_X64_CLAIM_FRAMES_FLAG_BELOW_4G) != 0U && MaximumPhysicalAddress > 0x100000000ULL)
    {
        MaximumPhysicalAddress = 0x100000000ULL;
    }
    if (MaximumPhysicalAddress <= MinimumPhysicalAddress)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
        return;
    }

    Owner = Request->Owner == LOS_X64_PHYSICAL_FRAME_RESERVED_NONE ? LOS_X64_MEMORY_REGION_OWNER_CLAIMED : Request->Owner;
    if ((Request->Flags & LOS_X64_CLAIM_FRAMES_FLAG_EXACT_ADDRESS) != 0U)
    {
        BaseAddress = AlignDownPage(Request->DesiredPhysicalAddress);
        if (BaseAddress < MinimumPhysicalAddress || BaseAddress + RequiredBytes > MaximumPhysicalAddress || BaseAddress + RequiredBytes < BaseAddress)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
            return;
        }
        if (!IsRangeCoveredByPageFrameDatabase(View->PageFrameDatabase, View->PageFrameDatabaseEntryCount, BaseAddress, RequiredBytes))
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
            return;
        }
        if (!IsRangeCoveredByState(View->PageFrameDatabase, View->PageFrameDatabaseEntryCount, BaseAddress, RequiredBytes, LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE))
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
            return;
        }
    }
    else
    {
        if (!FindClaimableContiguousRange(
                View->PageFrameDatabase,
                View->PageFrameDatabaseEntryCount,
                MinimumPhysicalAddress,
                MaximumPhysicalAddress,
                AlignmentBytes,
                Request->PageCount,
                &BaseAddress))
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
            return;
        }
    }

    if (!InsertDynamicAllocation(View, BaseAddress, Request->PageCount, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_DYNAMIC_CLAIMED, Owner) ||
        !RebuildCurrentPageFrameDatabase(View))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->BaseAddress = BaseAddress;
    Result->PageCount = Request->PageCount;
}

void LosMemoryManagerServiceFreeFrames(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_X64_FREE_FRAMES_REQUEST *Request,
    LOS_X64_FREE_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View;

    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->BaseAddress = 0ULL;
    Result->PageCount = 0ULL;

    if (State == 0 || Request == 0 || Request->PageCount == 0ULL || State->MemoryView.Ready == 0U)
    {
        if (State == 0 || State->MemoryView.Ready == 0U)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        }
        return;
    }

    if ((Request->PhysicalAddress & 0xFFFULL) != 0ULL)
    {
        return;
    }

    View = &State->MemoryView;
    if (!RemoveDynamicAllocationRange(View, Request->PhysicalAddress, Request->PageCount))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }
    if (!RebuildCurrentPageFrameDatabase(View))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->BaseAddress = Request->PhysicalAddress;
    Result->PageCount = Request->PageCount;
}
