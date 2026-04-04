#include "MemoryManagerMain.h"

static LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY LosPageFrameDatabaseScratch[LOS_MEMORY_MANAGER_MAX_PAGE_FRAME_DATABASE_ENTRIES];

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

static UINT64 AlignDownPage(UINT64 Value)
{
    return Value & ~0xFFFULL;
}

static UINT64 AlignUpPage(UINT64 Value)
{
    return (Value + 0xFFFULL) & ~0xFFFULL;
}

static UINT64 GetPageCountFromLength(UINT64 Length)
{
    return Length / 4096ULL;
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
    Entry->Source = Source;
    Entry->Reserved = 0U;
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
        Source,
        Attributes);
}

static BOOLEAN RewriteFrameDatabaseRange(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 BaseAddress,
    UINT64 Length,
    UINT32 State,
    UINT32 Usage)
{
    UINTN SourceIndex;
    UINTN ScratchCount;
    UINT64 EndAddress;
    UINT64 CoveredBytes;

    EndAddress = BaseAddress + Length;
    if (Length == 0ULL || EndAddress <= BaseAddress)
    {
        return 0;
    }

    ScratchCount = 0U;
    CoveredBytes = 0ULL;
    ZeroBytes(LosPageFrameDatabaseScratch, sizeof(LosPageFrameDatabaseScratch));

    for (SourceIndex = 0U; SourceIndex < View->PageFrameDatabaseEntryCount; ++SourceIndex)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *SourceEntry;
        UINT64 SourceBase;
        UINT64 SourceEnd;
        UINT64 OverlapBase;
        UINT64 OverlapEnd;

        SourceEntry = &View->PageFrameDatabase[SourceIndex];
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

    ZeroBytes(View->PageFrameDatabase, sizeof(View->PageFrameDatabase));
    View->PageFrameDatabaseEntryCount = 0U;
    for (SourceIndex = 0U; SourceIndex < ScratchCount; ++SourceIndex)
    {
        if (!AppendFrameDatabaseEntry(
                View,
                LosPageFrameDatabaseScratch[SourceIndex].BaseAddress,
                LosPageFrameDatabaseScratch[SourceIndex].PageCount,
                LosPageFrameDatabaseScratch[SourceIndex].State,
                LosPageFrameDatabaseScratch[SourceIndex].Usage,
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
    UINT32 Usage)
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
        View,
        AlignedBase,
        AlignedEnd - AlignedBase,
        LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RESERVED,
        Usage);
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
    if (!ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceImagePhysicalAddress, State->LaunchBlock->ServiceImageSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_IMAGE) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceStackPhysicalAddress, State->LaunchBlock->ServiceStackPageCount * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_STACK) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->RequestMailboxPhysicalAddress, State->LaunchBlock->RequestMailboxSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_REQUEST_MAILBOX) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ResponseMailboxPhysicalAddress, State->LaunchBlock->ResponseMailboxSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_RESPONSE_MAILBOX) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->EventMailboxPhysicalAddress, State->LaunchBlock->EventMailboxSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_EVENT_MAILBOX) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->LaunchBlockPhysicalAddress, LOS_MEMORY_MANAGER_LAUNCH_BLOCK_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_LAUNCH_BLOCK) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->KernelToServiceEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ENDPOINT_OBJECT) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceToKernelEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ENDPOINT_OBJECT) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceEventsEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ENDPOINT_OBJECT) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress, LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ADDRESS_SPACE_OBJECT) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceTaskObjectPhysicalAddress, LOS_MEMORY_MANAGER_TASK_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_TASK_OBJECT) ||
        !ReserveFrameDatabaseRange(View, State->LaunchBlock->ServicePageMapLevel4PhysicalAddress, 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_PAGE_TABLE))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_OVERLAY_CONFLICT;
        }
        return 0;
    }

    RefreshPageTotals(View);
    View->Ready = 1U;
    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_NONE;
    }
    return 1;
}
