#include "VirtualMemoryInternal.h"
#include "InterruptsInternal.h"

typedef struct
{
    UINT64 BaseAddress;
    UINT64 Length;
} LOS_X64_PHYSICAL_SPAN;

static const char LosKernelStackBackingPhysicalStartPrefix[] LOS_X64_BOOTSTRAP_RODATA = "[Kernel] Higher-half stack backing physical start: ";
static const char LosKernelStackBackingPhysicalEndPrefix[] LOS_X64_BOOTSTRAP_RODATA = "[Kernel] Higher-half stack backing physical end: ";
static const char LosKernelStackBackingVirtualStartPrefix[] LOS_X64_BOOTSTRAP_RODATA = "[Kernel] Higher-half stack virtual start: ";
static const char LosKernelStackBackingMapUncheckedNotice[] LOS_X64_BOOTSTRAP_RODATA = "[Kernel] Stack backing is outside discovered EFI ranges; using trusted bootstrap mapping.\n";
static const char LosKernelStackPageTablePoolUsedPrefix[] LOS_X64_BOOTSTRAP_RODATA = "[Kernel] Bootstrap page-table pages used: ";

static UINT64 LosPageMapLevel4[512] LOS_X64_BOOTSTRAP_DATA;
static UINT64 LosIdentityDirectoryPointer[512] LOS_X64_BOOTSTRAP_DATA;
static UINT64 LosIdentityDirectory[512] LOS_X64_BOOTSTRAP_DATA;
static UINT64 LosKernelWindowDirectoryPointer[512] LOS_X64_BOOTSTRAP_DATA;
static UINT64 LosKernelWindowDirectory[512] LOS_X64_BOOTSTRAP_DATA;
static UINT64 LosPageTablePool[LOS_X64_PAGE_TABLE_POOL_PAGES][512] LOS_X64_BOOTSTRAP_DATA;
static UINT64 LosKernelStackBacking[LOS_X64_KERNEL_STACK_COMMITTED_PAGES][512] LOS_X64_BOOTSTRAP_DATA;
static UINT8 LosBootstrapTransitionStack[LOS_X64_BOOTSTRAP_TRANSITION_STACK_BYTES] LOS_X64_BOOTSTRAP_DATA;
static LOS_X64_PHYSICAL_MEMORY_DESCRIPTOR LosPhysicalMemoryDescriptors[LOS_X64_MAX_PHYSICAL_MEMORY_DESCRIPTORS] LOS_X64_BOOTSTRAP_DATA;
static LOS_X64_PHYSICAL_FRAME_REGION LosPhysicalFrameRegions[LOS_X64_MAX_PHYSICAL_FRAME_REGIONS] LOS_X64_BOOTSTRAP_DATA;
static LOS_X64_PHYSICAL_FRAME_REGION LosPhysicalFrameRegionScratch[LOS_X64_MAX_PHYSICAL_FRAME_REGIONS] LOS_X64_BOOTSTRAP_DATA;
static LOS_X64_MEMORY_REGION LosBaseMemoryRegions[LOS_X64_MAX_MEMORY_REGIONS] LOS_X64_BOOTSTRAP_DATA;
static LOS_X64_MEMORY_REGION LosOverlayMemoryRegions[LOS_X64_MAX_MEMORY_REGIONS] LOS_X64_BOOTSTRAP_DATA;
static LOS_X64_MEMORY_REGION LosMemoryRegions[LOS_X64_MAX_MEMORY_REGIONS] LOS_X64_BOOTSTRAP_DATA;
static LOS_X64_MEMORY_REGION LosMemoryRegionScratch[LOS_X64_MAX_MEMORY_REGIONS] LOS_X64_BOOTSTRAP_DATA;
static LOS_X64_PHYSICAL_SPAN LosPhysicalSpanScratch[LOS_X64_MAX_PHYSICAL_FRAME_REGIONS] LOS_X64_BOOTSTRAP_DATA;
static LOS_X64_MEMORY_MANAGER_HANDOFF LosMemoryManagerHandoff LOS_X64_BOOTSTRAP_DATA;
static LOS_X64_VIRTUAL_MEMORY_LAYOUT LosLayout __attribute__((section(".bootstrap.data")));
static UINTN LosPageTablePoolNextIndex __attribute__((section(".bootstrap.data")));
static UINTN LosPhysicalMemoryDescriptorCount __attribute__((section(".bootstrap.data")));
static UINTN LosPhysicalFrameRegionCount __attribute__((section(".bootstrap.data")));
static UINTN LosBaseMemoryRegionCount __attribute__((section(".bootstrap.data")));
static UINTN LosOverlayMemoryRegionCount __attribute__((section(".bootstrap.data")));
static UINTN LosMemoryRegionCount __attribute__((section(".bootstrap.data")));
static UINT64 LosAddressSpaceGapBytes __attribute__((section(".bootstrap.data")));

LOS_X64_BOOTSTRAP_SECTION
static UINT64 AlignUpLargePageBoundary(UINT64 Value)
{
    return (Value + 0x1FFFFFULL) & ~0x1FFFFFULL;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 AlignDownPageBoundary(UINT64 Value)
{
    return Value & ~0xFFFULL;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 AlignUpPageBoundary(UINT64 Value)
{
    return (Value + 0xFFFULL) & ~0xFFFULL;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 GetPageCountFromLength(UINT64 Length)
{
    return Length / 4096ULL;
}

LOS_X64_BOOTSTRAP_SECTION
static void ZeroEntries(UINT64 *Page, UINTN EntryCount)
{
    LOS_KERNEL_ENTER();
    UINTN Index;
    for (Index = 0U; Index < EntryCount; ++Index)
    {
        Page[Index] = 0ULL;
    }
}

LOS_X64_BOOTSTRAP_SECTION
static void ZeroBytes(UINT8 *Buffer, UINTN Size)
{
    UINTN Index;
    for (Index = 0U; Index < Size; ++Index)
    {
        Buffer[Index] = 0U;
    }
}

LOS_X64_BOOTSTRAP_SECTION
static UINT32 GetPhysicalMemoryDescriptorFlags(UINT32 Type)
{
    LOS_KERNEL_ENTER();
    switch (Type)
    {
        case 1U:
        case 2U:
        case 3U:
        case 4U:
        case 7U:
            return 1U;
        default:
            return 0U;
    }
}

LOS_X64_BOOTSTRAP_SECTION
static UINT32 ClassifyFrameState(UINT32 Type)
{
    if (Type == 1U || Type == 2U || Type == 3U || Type == 4U || Type == 7U)
    {
        return LOS_X64_PHYSICAL_FRAME_STATE_FREE;
    }

    if (Type == 5U || Type == 6U)
    {
        return LOS_X64_PHYSICAL_FRAME_STATE_RUNTIME;
    }

    if (Type == 11U || Type == 12U)
    {
        return LOS_X64_PHYSICAL_FRAME_STATE_MMIO;
    }

    return LOS_X64_PHYSICAL_FRAME_STATE_RESERVED;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT32 ClassifyMemoryRegionType(UINT32 Type)
{
    if (Type == 1U || Type == 2U || Type == 3U || Type == 4U || Type == 7U)
    {
        return LOS_X64_MEMORY_REGION_TYPE_USABLE;
    }

    if (Type == 5U || Type == 6U)
    {
        return LOS_X64_MEMORY_REGION_TYPE_RUNTIME;
    }

    if (Type == 8U)
    {
        return LOS_X64_MEMORY_REGION_TYPE_UNUSABLE;
    }

    if (Type == 9U || Type == 10U)
    {
        return LOS_X64_MEMORY_REGION_TYPE_ACPI_NVS;
    }

    if (Type == 11U || Type == 12U)
    {
        return LOS_X64_MEMORY_REGION_TYPE_MMIO;
    }

    return LOS_X64_MEMORY_REGION_TYPE_FIRMWARE_RESERVED;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT32 GetMemoryRegionFlagsForDescriptorType(UINT32 Type)
{
    return (GetPhysicalMemoryDescriptorFlags(Type) & 1U) != 0U ? LOS_X64_MEMORY_REGION_FLAG_DIRECT_MAP : 0U;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT32 GetMemoryRegionOwnerForDescriptorType(UINT32 Type)
{
    if (ClassifyMemoryRegionType(Type) == LOS_X64_MEMORY_REGION_TYPE_FIRMWARE_RESERVED)
    {
        return LOS_X64_MEMORY_REGION_OWNER_FIRMWARE;
    }

    return LOS_X64_MEMORY_REGION_OWNER_NONE;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT32 GetMemoryRegionSourceForDescriptorType(UINT32 Type)
{
    if (ClassifyMemoryRegionType(Type) == LOS_X64_MEMORY_REGION_TYPE_RUNTIME)
    {
        return LOS_X64_MEMORY_REGION_SOURCE_RUNTIME;
    }

    return LOS_X64_MEMORY_REGION_SOURCE_EFI;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT32 GetMemoryRegionSourceForReservationKind(UINT32 ReservationKind)
{
    if (ReservationKind == LOS_X64_MEMORY_REGION_OWNER_KERNEL_IMAGE ||
        ReservationKind == LOS_X64_MEMORY_REGION_OWNER_KERNEL_STACK ||
        ReservationKind == LOS_X64_MEMORY_REGION_OWNER_GDT ||
        ReservationKind == LOS_X64_MEMORY_REGION_OWNER_IDT)
    {
        return LOS_X64_MEMORY_REGION_SOURCE_KERNEL;
    }

    if (ReservationKind == LOS_X64_MEMORY_REGION_OWNER_BOOT_CONTEXT ||
        ReservationKind == LOS_X64_MEMORY_REGION_OWNER_MEMORY_MAP ||
        ReservationKind == LOS_X64_MEMORY_REGION_OWNER_BOOTSTRAP_PAGE_TABLES ||
        ReservationKind == LOS_X64_MEMORY_REGION_OWNER_BOOTSTRAP_STACK ||
        ReservationKind == LOS_X64_MEMORY_REGION_OWNER_DEBUG)
    {
        return LOS_X64_MEMORY_REGION_SOURCE_BOOTSTRAP;
    }

    return LOS_X64_MEMORY_REGION_SOURCE_RUNTIME;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT32 GetMemoryRegionFlagsForReservationKind(UINT32 ReservationKind)
{
    UINT32 Flags;

    Flags = LOS_X64_MEMORY_REGION_FLAG_OVERLAY;
    if (GetMemoryRegionSourceForReservationKind(ReservationKind) == LOS_X64_MEMORY_REGION_SOURCE_RUNTIME)
    {
        Flags |= LOS_X64_MEMORY_REGION_FLAG_CLAIMED;
    }

    return Flags;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN AppendMemoryRegionToArray(LOS_X64_MEMORY_REGION *Array, UINTN *Count, UINT64 BaseAddress, UINT64 Length, UINT32 Type, UINT32 Flags, UINT32 Owner, UINT32 Source)
{
    LOS_X64_MEMORY_REGION *Region;

    if (Length == 0ULL)
    {
        return 1;
    }

    if (*Count != 0U)
    {
        LOS_X64_MEMORY_REGION *Previous;
        Previous = &Array[*Count - 1U];
        if (Previous->Base + Previous->Length == BaseAddress && Previous->Type == Type && Previous->Flags == Flags && Previous->Owner == Owner && Previous->Source == Source)
        {
            Previous->Length += Length;
            return 1;
        }
    }

    if (*Count >= LOS_X64_MAX_MEMORY_REGIONS)
    {
        return 0;
    }

    Region = &Array[*Count];
    Region->Base = BaseAddress;
    Region->Length = Length;
    Region->Type = Type;
    Region->Flags = Flags;
    Region->Owner = Owner;
    Region->Source = Source;
    *Count += 1U;
    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN AppendBaseMemoryRegion(UINT64 BaseAddress, UINT64 Length, UINT32 Type, UINT32 Flags, UINT32 Owner, UINT32 Source)
{
    return AppendMemoryRegionToArray(LosBaseMemoryRegions, &LosBaseMemoryRegionCount, BaseAddress, Length, Type, Flags, Owner, Source);
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN AppendOverlayMemoryRegionRecord(UINT64 BaseAddress, UINT64 Length, UINT32 ReservationKind)
{
    return AppendMemoryRegionToArray(
        LosOverlayMemoryRegions,
        &LosOverlayMemoryRegionCount,
        BaseAddress,
        Length,
        LOS_X64_MEMORY_REGION_TYPE_BOOT_RESERVED,
        GetMemoryRegionFlagsForReservationKind(ReservationKind),
        ReservationKind,
        GetMemoryRegionSourceForReservationKind(ReservationKind));
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN ApplyOverlayMemoryRegion(const LOS_X64_MEMORY_REGION *Overlay)
{
    UINTN SourceIndex;
    UINTN ScratchCount;

    ScratchCount = 0U;
    for (SourceIndex = 0U; SourceIndex < LosMemoryRegionCount; ++SourceIndex)
    {
        const LOS_X64_MEMORY_REGION *SourceRegion;
        UINT64 SourceEnd;
        UINT64 OverlayEnd;
        UINT64 OverlapBase;
        UINT64 OverlapEnd;

        SourceRegion = &LosMemoryRegions[SourceIndex];
        SourceEnd = SourceRegion->Base + SourceRegion->Length;
        OverlayEnd = Overlay->Base + Overlay->Length;
        if (SourceEnd <= Overlay->Base || SourceRegion->Base >= OverlayEnd)
        {
            if (!AppendMemoryRegionToArray(LosMemoryRegionScratch, &ScratchCount, SourceRegion->Base, SourceRegion->Length, SourceRegion->Type, SourceRegion->Flags, SourceRegion->Owner, SourceRegion->Source))
            {
                return 0;
            }
            continue;
        }

        OverlapBase = Overlay->Base > SourceRegion->Base ? Overlay->Base : SourceRegion->Base;
        OverlapEnd = OverlayEnd < SourceEnd ? OverlayEnd : SourceEnd;

        if (SourceRegion->Base < OverlapBase)
        {
            if (!AppendMemoryRegionToArray(LosMemoryRegionScratch, &ScratchCount, SourceRegion->Base, OverlapBase - SourceRegion->Base, SourceRegion->Type, SourceRegion->Flags, SourceRegion->Owner, SourceRegion->Source))
            {
                return 0;
            }
        }

        if (!AppendMemoryRegionToArray(LosMemoryRegionScratch, &ScratchCount, OverlapBase, OverlapEnd - OverlapBase, Overlay->Type, Overlay->Flags, Overlay->Owner, Overlay->Source))
        {
            return 0;
        }

        if (SourceEnd > OverlapEnd)
        {
            if (!AppendMemoryRegionToArray(LosMemoryRegionScratch, &ScratchCount, OverlapEnd, SourceEnd - OverlapEnd, SourceRegion->Type, SourceRegion->Flags, SourceRegion->Owner, SourceRegion->Source))
            {
                return 0;
            }
        }
    }

    for (SourceIndex = 0U; SourceIndex < ScratchCount; ++SourceIndex)
    {
        LosMemoryRegions[SourceIndex] = LosMemoryRegionScratch[SourceIndex];
    }
    LosMemoryRegionCount = ScratchCount;
    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN RebuildPublishedMemoryRegions(void)
{
    UINTN Index;

    LosMemoryRegionCount = 0U;
    for (Index = 0U; Index < LosBaseMemoryRegionCount; ++Index)
    {
        const LOS_X64_MEMORY_REGION *Region;

        Region = &LosBaseMemoryRegions[Index];
        if (!AppendMemoryRegionToArray(LosMemoryRegions, &LosMemoryRegionCount, Region->Base, Region->Length, Region->Type, Region->Flags, Region->Owner, Region->Source))
        {
            return 0;
        }
    }

    for (Index = 0U; Index < LosOverlayMemoryRegionCount; ++Index)
    {
        if (!ApplyOverlayMemoryRegion(&LosOverlayMemoryRegions[Index]))
        {
            return 0;
        }
    }

    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 CalculateAddressSpaceGapBytes(void)
{
    UINT64 Cursor;
    UINT64 GapBytes;
    UINTN Index;

    Cursor = 0ULL;
    GapBytes = 0ULL;
    for (Index = 0U; Index < LosPhysicalMemoryDescriptorCount; ++Index)
    {
        const LOS_X64_PHYSICAL_MEMORY_DESCRIPTOR *Descriptor;
        UINT64 DescriptorEnd;

        Descriptor = &LosPhysicalMemoryDescriptors[Index];
        if (Descriptor->Length == 0ULL)
        {
            continue;
        }

        if (Descriptor->BaseAddress > Cursor)
        {
            GapBytes += Descriptor->BaseAddress - Cursor;
        }

        DescriptorEnd = Descriptor->BaseAddress + Descriptor->Length;
        if (DescriptorEnd > Cursor)
        {
            Cursor = DescriptorEnd;
        }
    }

    return GapBytes;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN AppendPhysicalFrameRegionToArray(LOS_X64_PHYSICAL_FRAME_REGION *Array, UINTN *Count, UINT64 BaseAddress, UINT64 PageCount, UINT32 State, UINT32 SourceType, UINT32 ReservedKind, UINT64 Attributes)
{
    LOS_X64_PHYSICAL_FRAME_REGION *Region;

    if (PageCount == 0ULL)
    {
        return 1;
    }

    if (*Count != 0U)
    {
        LOS_X64_PHYSICAL_FRAME_REGION *Previous;
        Previous = &Array[*Count - 1U];
        if (Previous->BaseAddress + (Previous->PageCount * 4096ULL) == BaseAddress && Previous->State == State && Previous->SourceType == SourceType && Previous->ReservedKind == ReservedKind && Previous->Attributes == Attributes)
        {
            Previous->PageCount += PageCount;
            return 1;
        }
    }

    if (*Count >= LOS_X64_MAX_PHYSICAL_FRAME_REGIONS)
    {
        return 0;
    }

    Region = &Array[*Count];
    Region->BaseAddress = BaseAddress;
    Region->PageCount = PageCount;
    Region->State = State;
    Region->SourceType = SourceType;
    Region->ReservedKind = ReservedKind;
    Region->Reserved = 0U;
    Region->Attributes = Attributes;
    *Count += 1U;
    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN AppendPhysicalFrameRegion(UINT64 BaseAddress, UINT64 PageCount, UINT32 State, UINT32 SourceType, UINT32 ReservedKind, UINT64 Attributes)
{
    return AppendPhysicalFrameRegionToArray(LosPhysicalFrameRegions, &LosPhysicalFrameRegionCount, BaseAddress, PageCount, State, SourceType, ReservedKind, Attributes);
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN IsRangeCoveredByAnyRegion(UINT64 BaseAddress, UINT64 EndAddress)
{
    UINTN Index;
    UINT64 Cursor;

    if (EndAddress <= BaseAddress)
    {
        return 0;
    }

    Cursor = BaseAddress;
    for (Index = 0U; Index < LosPhysicalFrameRegionCount && Cursor < EndAddress; ++Index)
    {
        const LOS_X64_PHYSICAL_FRAME_REGION *Region;
        UINT64 RegionEnd;

        Region = &LosPhysicalFrameRegions[Index];
        RegionEnd = Region->BaseAddress + (Region->PageCount * 4096ULL);
        if (RegionEnd <= Cursor)
        {
            continue;
        }
        if (Region->BaseAddress > Cursor)
        {
            return 0;
        }
        if (RegionEnd > Cursor)
        {
            Cursor = RegionEnd < EndAddress ? RegionEnd : EndAddress;
        }
    }

    return Cursor >= EndAddress;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN IsRangeCoveredByState(UINT64 BaseAddress, UINT64 EndAddress, UINT32 RequiredState)
{
    UINTN Index;
    UINT64 Cursor;

    if (EndAddress <= BaseAddress)
    {
        return 0;
    }

    Cursor = BaseAddress;
    for (Index = 0U; Index < LosPhysicalFrameRegionCount && Cursor < EndAddress; ++Index)
    {
        const LOS_X64_PHYSICAL_FRAME_REGION *Region;
        UINT64 RegionEnd;

        Region = &LosPhysicalFrameRegions[Index];
        RegionEnd = Region->BaseAddress + (Region->PageCount * 4096ULL);
        if (RegionEnd <= Cursor)
        {
            continue;
        }
        if (Region->BaseAddress > Cursor)
        {
            return 0;
        }
        if (Region->State != RequiredState)
        {
            return 0;
        }
        Cursor = RegionEnd < EndAddress ? RegionEnd : EndAddress;
    }

    return Cursor >= EndAddress;
}

LOS_X64_BOOTSTRAP_SECTION
static void RefreshMemoryManagerHandoff(void)
{
    UINTN Index;

    LosMemoryManagerHandoff.Signature = LOS_X64_MEMORY_MANAGER_HANDOFF_SIGNATURE;
    LosMemoryManagerHandoff.Version = LOS_X64_MEMORY_MANAGER_HANDOFF_VERSION;
    LosMemoryManagerHandoff.Reserved = 0U;
    LosMemoryManagerHandoff.Flags =
        LOS_X64_MEMORY_MANAGER_HANDOFF_FLAG_QUERY_MEMORY_REGIONS |
        LOS_X64_MEMORY_MANAGER_HANDOFF_FLAG_RESERVE_FRAMES |
        LOS_X64_MEMORY_MANAGER_HANDOFF_FLAG_CLAIM_FRAMES |
        LOS_X64_MEMORY_MANAGER_HANDOFF_FLAG_MAP_PAGES |
        LOS_X64_MEMORY_MANAGER_HANDOFF_FLAG_UNMAP_PAGES;
    LosMemoryManagerHandoff.RegionDatabaseAddress = (UINT64)(UINTN)&LosMemoryRegions[0];
    LosMemoryManagerHandoff.RegionCount = (UINT64)LosMemoryRegionCount;
    LosMemoryManagerHandoff.RegionEntrySize = (UINT64)sizeof(LOS_X64_MEMORY_REGION);
    LosMemoryManagerHandoff.TotalUsableBytes = 0ULL;
    LosMemoryManagerHandoff.TotalBootstrapReservedBytes = 0ULL;
    LosMemoryManagerHandoff.TotalFirmwareReservedBytes = 0ULL;
    LosMemoryManagerHandoff.TotalRuntimeBytes = 0ULL;
    LosMemoryManagerHandoff.TotalMmioBytes = 0ULL;
    LosMemoryManagerHandoff.TotalAcpiBytes = 0ULL;
    LosMemoryManagerHandoff.TotalUnusableBytes = 0ULL;
    LosMemoryManagerHandoff.TotalAddressSpaceGapBytes = LosAddressSpaceGapBytes;
    LosMemoryManagerHandoff.HighestUsablePhysicalAddress = 0ULL;

    for (Index = 0U; Index < LosMemoryRegionCount; ++Index)
    {
        const LOS_X64_MEMORY_REGION *Region;
        UINT64 EndAddress;

        Region = &LosMemoryRegions[Index];
        EndAddress = Region->Base + Region->Length;

        if (Region->Type == LOS_X64_MEMORY_REGION_TYPE_USABLE)
        {
            LosMemoryManagerHandoff.TotalUsableBytes += Region->Length;
            if (EndAddress > LosMemoryManagerHandoff.HighestUsablePhysicalAddress)
            {
                LosMemoryManagerHandoff.HighestUsablePhysicalAddress = EndAddress;
            }
        }
        else if (Region->Type == LOS_X64_MEMORY_REGION_TYPE_BOOT_RESERVED)
        {
            LosMemoryManagerHandoff.TotalBootstrapReservedBytes += Region->Length;
        }
        else if (Region->Type == LOS_X64_MEMORY_REGION_TYPE_RUNTIME)
        {
            LosMemoryManagerHandoff.TotalRuntimeBytes += Region->Length;
        }
        else if (Region->Type == LOS_X64_MEMORY_REGION_TYPE_MMIO)
        {
            LosMemoryManagerHandoff.TotalMmioBytes += Region->Length;
        }
        else if (Region->Type == LOS_X64_MEMORY_REGION_TYPE_ACPI_NVS)
        {
            LosMemoryManagerHandoff.TotalAcpiBytes += Region->Length;
        }
        else if (Region->Type == LOS_X64_MEMORY_REGION_TYPE_UNUSABLE)
        {
            LosMemoryManagerHandoff.TotalUnusableBytes += Region->Length;
        }
        else
        {
            LosMemoryManagerHandoff.TotalFirmwareReservedBytes += Region->Length;
        }
    }
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN RewritePhysicalFrameRegions(UINT64 BaseAddress, UINT64 EndAddress, UINT32 MatchState, UINT32 NewState, UINT32 NewReservedKind)
{
    UINTN SourceIndex;
    UINTN ScratchCount;

    ScratchCount = 0U;
    for (SourceIndex = 0U; SourceIndex < LosPhysicalFrameRegionCount; ++SourceIndex)
    {
        const LOS_X64_PHYSICAL_FRAME_REGION *SourceRegion;
        UINT64 SourceEnd;
        UINT64 OverlapBase;
        UINT64 OverlapEnd;

        SourceRegion = &LosPhysicalFrameRegions[SourceIndex];
        SourceEnd = SourceRegion->BaseAddress + (SourceRegion->PageCount * 4096ULL);
        if (SourceEnd <= BaseAddress || SourceRegion->BaseAddress >= EndAddress || SourceRegion->State != MatchState)
        {
            if (!AppendPhysicalFrameRegionToArray(LosPhysicalFrameRegionScratch, &ScratchCount, SourceRegion->BaseAddress, SourceRegion->PageCount, SourceRegion->State, SourceRegion->SourceType, SourceRegion->ReservedKind, SourceRegion->Attributes))
            {
                return 0;
            }
            continue;
        }

        OverlapBase = BaseAddress > SourceRegion->BaseAddress ? BaseAddress : SourceRegion->BaseAddress;
        OverlapEnd = EndAddress < SourceEnd ? EndAddress : SourceEnd;

        if (SourceRegion->BaseAddress < OverlapBase)
        {
            if (!AppendPhysicalFrameRegionToArray(LosPhysicalFrameRegionScratch, &ScratchCount, SourceRegion->BaseAddress, GetPageCountFromLength(OverlapBase - SourceRegion->BaseAddress), SourceRegion->State, SourceRegion->SourceType, SourceRegion->ReservedKind, SourceRegion->Attributes))
            {
                return 0;
            }
        }

        if (!AppendPhysicalFrameRegionToArray(LosPhysicalFrameRegionScratch, &ScratchCount, OverlapBase, GetPageCountFromLength(OverlapEnd - OverlapBase), NewState, SourceRegion->SourceType, NewReservedKind, SourceRegion->Attributes))
        {
            return 0;
        }

        if (SourceEnd > OverlapEnd)
        {
            if (!AppendPhysicalFrameRegionToArray(LosPhysicalFrameRegionScratch, &ScratchCount, OverlapEnd, GetPageCountFromLength(SourceEnd - OverlapEnd), SourceRegion->State, SourceRegion->SourceType, SourceRegion->ReservedKind, SourceRegion->Attributes))
            {
                return 0;
            }
        }
    }

    for (SourceIndex = 0U; SourceIndex < ScratchCount; ++SourceIndex)
    {
        LosPhysicalFrameRegions[SourceIndex] = LosPhysicalFrameRegionScratch[SourceIndex];
    }
    LosPhysicalFrameRegionCount = ScratchCount;
    RefreshMemoryManagerHandoff();
    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN TryTranslateHigherHalfPointerToPhysical(UINT64 VirtualAddress, UINT64 *PhysicalAddress)
{
    UINT64 TextStart;
    UINT64 TextEnd;
    UINT64 DataStart;
    UINT64 DataEnd;
    UINT64 BssStart;
    UINT64 BssEnd;

    TextStart = (UINT64)(UINTN)__LosKernelHigherHalfTextStart;
    TextEnd = (UINT64)(UINTN)__LosKernelHigherHalfTextEnd;
    DataStart = (UINT64)(UINTN)__LosKernelHigherHalfDataStart;
    DataEnd = (UINT64)(UINTN)__LosKernelHigherHalfDataEnd;
    BssStart = (UINT64)(UINTN)__LosKernelHigherHalfBssStart;
    BssEnd = (UINT64)(UINTN)__LosKernelHigherHalfBssEnd;

    if (VirtualAddress >= TextStart && VirtualAddress < TextEnd)
    {
        *PhysicalAddress = (UINT64)(UINTN)__LosKernelHigherHalfTextLoadStart + (VirtualAddress - TextStart);
        return 1;
    }

    if (VirtualAddress >= DataStart && VirtualAddress < DataEnd)
    {
        *PhysicalAddress = (UINT64)(UINTN)__LosKernelHigherHalfDataLoadStart + (VirtualAddress - DataStart);
        return 1;
    }

    if (VirtualAddress >= BssStart && VirtualAddress < BssEnd)
    {
        *PhysicalAddress = (UINT64)(UINTN)__LosKernelHigherHalfBssLoadStart + (VirtualAddress - BssStart);
        return 1;
    }

    return 0;
}

LOS_X64_BOOTSTRAP_SECTION
static void ReserveBootstrapRanges(const LOS_BOOT_CONTEXT *BootContext)
{
    UINT64 GdtPhysicalBase;
    UINT64 IdtPhysicalBase;

    if (BootContext == 0)
    {
        return;
    }

    if (BootContext->KernelImagePhysicalAddress != 0ULL && BootContext->KernelImageSize != 0ULL)
    {
        LosX64ReservePhysicalRange(BootContext->KernelImagePhysicalAddress, BootContext->KernelImageSize, LOS_X64_PHYSICAL_FRAME_RESERVED_KERNEL_IMAGE);
    }
    if (BootContext->BootContextAddress != 0ULL && BootContext->BootContextSize != 0ULL)
    {
        LosX64ReservePhysicalRange(BootContext->BootContextAddress, BootContext->BootContextSize, LOS_X64_PHYSICAL_FRAME_RESERVED_BOOT_CONTEXT);
    }
    if (BootContext->MemoryMapAddress != 0ULL && BootContext->MemoryMapBufferSize != 0ULL)
    {
        LosX64ReservePhysicalRange(BootContext->MemoryMapAddress, BootContext->MemoryMapBufferSize, LOS_X64_PHYSICAL_FRAME_RESERVED_MEMORY_MAP);
    }
    if (BootContext->KernelFontPhysicalAddress != 0ULL && BootContext->KernelFontSize != 0ULL)
    {
        LosX64ReservePhysicalRange(BootContext->KernelFontPhysicalAddress, BootContext->KernelFontSize, LOS_X64_PHYSICAL_FRAME_RESERVED_KERNEL_FONT);
    }

    LosX64ReservePhysicalRange(LosX64GetBootstrapPageTableStorageBase(), LosX64GetBootstrapPageTableStorageSize(), LOS_X64_PHYSICAL_FRAME_RESERVED_BOOTSTRAP_PAGE_TABLES);
    LosX64ReservePhysicalRange(LosX64GetBootstrapTransitionStackBase(), LosX64GetBootstrapTransitionStackSize(), LOS_X64_PHYSICAL_FRAME_RESERVED_BOOTSTRAP_STACK);
    LosX64ReservePhysicalRange(LosX64GetKernelStackBackingBase(), LosX64GetKernelStackBackingSize(), LOS_X64_PHYSICAL_FRAME_RESERVED_KERNEL_STACK);

    if (TryTranslateHigherHalfPointerToPhysical((UINT64)(UINTN)&LosGdt[0], &GdtPhysicalBase) != 0U)
    {
        LosX64ReservePhysicalRange(GdtPhysicalBase, sizeof(LosGdt), LOS_X64_PHYSICAL_FRAME_RESERVED_GDT);
    }

    if (TryTranslateHigherHalfPointerToPhysical((UINT64)(UINTN)&LosX64Idt[0], &IdtPhysicalBase) != 0U)
    {
        LosX64ReservePhysicalRange(IdtPhysicalBase, sizeof(LosX64Idt), LOS_X64_PHYSICAL_FRAME_RESERVED_IDT);
    }
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64InitializeVirtualMemoryState(const LOS_BOOT_CONTEXT *BootContext)
{
    LOS_KERNEL_ENTER();
    UINTN Index;
    (void)BootContext;

    ZeroEntries(LosPageMapLevel4, 512U);
    ZeroEntries(LosIdentityDirectoryPointer, 512U);
    ZeroEntries(LosIdentityDirectory, 512U);
    ZeroEntries(LosKernelWindowDirectoryPointer, 512U);
    ZeroEntries(LosKernelWindowDirectory, 512U);
    for (Index = 0U; Index < LOS_X64_PAGE_TABLE_POOL_PAGES; ++Index)
    {
        ZeroEntries(LosPageTablePool[Index], 512U);
    }
    for (Index = 0U; Index < LOS_X64_KERNEL_STACK_COMMITTED_PAGES; ++Index)
    {
        ZeroEntries(LosKernelStackBacking[Index], 512U);
    }
    for (Index = 0U; Index < LOS_X64_MAX_PHYSICAL_MEMORY_DESCRIPTORS; ++Index)
    {
        LosPhysicalMemoryDescriptors[Index].BaseAddress = 0ULL;
        LosPhysicalMemoryDescriptors[Index].Length = 0ULL;
        LosPhysicalMemoryDescriptors[Index].Type = 0U;
        LosPhysicalMemoryDescriptors[Index].Flags = 0U;
        LosPhysicalMemoryDescriptors[Index].Attributes = 0ULL;
    }
    ZeroBytes((UINT8 *)&LosPhysicalFrameRegions[0], sizeof(LosPhysicalFrameRegions));
    ZeroBytes((UINT8 *)&LosPhysicalFrameRegionScratch[0], sizeof(LosPhysicalFrameRegionScratch));
    ZeroBytes((UINT8 *)&LosBaseMemoryRegions[0], sizeof(LosBaseMemoryRegions));
    ZeroBytes((UINT8 *)&LosOverlayMemoryRegions[0], sizeof(LosOverlayMemoryRegions));
    ZeroBytes((UINT8 *)&LosMemoryRegions[0], sizeof(LosMemoryRegions));
    ZeroBytes((UINT8 *)&LosMemoryRegionScratch[0], sizeof(LosMemoryRegionScratch));
    ZeroBytes((UINT8 *)&LosPhysicalSpanScratch[0], sizeof(LosPhysicalSpanScratch));
    ZeroBytes((UINT8 *)&LosMemoryManagerHandoff, sizeof(LosMemoryManagerHandoff));

    LosPageTablePoolNextIndex = 0U;
    LosPhysicalMemoryDescriptorCount = 0U;
    LosPhysicalFrameRegionCount = 0U;
    LosBaseMemoryRegionCount = 0U;
    LosOverlayMemoryRegionCount = 0U;
    LosMemoryRegionCount = 0U;
    LosAddressSpaceGapBytes = 0ULL;
    LosLayout.BootstrapIdentityBase = 0ULL;
    LosLayout.BootstrapIdentitySize = AlignUpLargePageBoundary((UINT64)(UINTN)__LosKernelBootstrapEnd);
    if (LosLayout.BootstrapIdentitySize < LOS_X64_BOOTSTRAP_IDENTITY_BYTES)
    {
        LosLayout.BootstrapIdentitySize = LOS_X64_BOOTSTRAP_IDENTITY_BYTES;
    }
    LosLayout.HigherHalfDirectMapBase = LOS_X64_HIGHER_HALF_BASE;
    LosLayout.HigherHalfDirectMapSize = 0ULL;
    LosLayout.KernelWindowBase = LOS_X64_KERNEL_WINDOW_BASE;
    LosLayout.KernelWindowSize = 0x40000000ULL;
    LosLayout.KernelStackBase = LOS_X64_KERNEL_STACK_BASE + (LOS_X64_KERNEL_STACK_GUARD_PAGES * 4096ULL);
    LosLayout.KernelStackSize = LOS_X64_KERNEL_STACK_SIZE_BYTES;
    LosLayout.KernelStackTop = LOS_X64_KERNEL_STACK_TOP;
    LosLayout.HighestDiscoveredPhysicalAddress = 0ULL;
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64BuildPhysicalMemoryState(const LOS_BOOT_CONTEXT *BootContext)
{
    LOS_KERNEL_ENTER();
    UINTN DescriptorCount;
    UINTN Index;
    UINT64 HighestUsableEnd;

    if (BootContext == 0 || BootContext->MemoryMapAddress == 0ULL || BootContext->MemoryMapDescriptorSize == 0ULL)
    {
        return;
    }

    DescriptorCount = (UINTN)(BootContext->MemoryMapSize / BootContext->MemoryMapDescriptorSize);
    HighestUsableEnd = 0ULL;

    for (Index = 0U; Index < DescriptorCount && LosPhysicalMemoryDescriptorCount < LOS_X64_MAX_PHYSICAL_MEMORY_DESCRIPTORS; ++Index)
    {
        const EFI_MEMORY_DESCRIPTOR *SourceDescriptor;
        LOS_X64_PHYSICAL_MEMORY_DESCRIPTOR *TargetDescriptor;
        UINT64 Length;
        UINT64 EndAddress;
        UINT32 State;

        SourceDescriptor = (const EFI_MEMORY_DESCRIPTOR *)(UINTN)(BootContext->MemoryMapAddress + ((UINT64)Index * BootContext->MemoryMapDescriptorSize));
        Length = SourceDescriptor->NumberOfPages * 4096ULL;
        if (Length == 0ULL)
        {
            continue;
        }

        EndAddress = SourceDescriptor->PhysicalStart + Length;
        if (EndAddress > LosLayout.HighestDiscoveredPhysicalAddress)
        {
            LosLayout.HighestDiscoveredPhysicalAddress = EndAddress;
        }

        TargetDescriptor = &LosPhysicalMemoryDescriptors[LosPhysicalMemoryDescriptorCount++];
        TargetDescriptor->BaseAddress = SourceDescriptor->PhysicalStart;
        TargetDescriptor->Length = Length;
        TargetDescriptor->Type = SourceDescriptor->Type;
        TargetDescriptor->Flags = GetPhysicalMemoryDescriptorFlags(SourceDescriptor->Type);
        TargetDescriptor->Attributes = SourceDescriptor->Attribute;

        State = ClassifyFrameState(SourceDescriptor->Type);
        if (!AppendPhysicalFrameRegion(SourceDescriptor->PhysicalStart, SourceDescriptor->NumberOfPages, State, SourceDescriptor->Type, State == LOS_X64_PHYSICAL_FRAME_STATE_RESERVED ? LOS_X64_PHYSICAL_FRAME_RESERVED_FIRMWARE : LOS_X64_PHYSICAL_FRAME_RESERVED_NONE, SourceDescriptor->Attribute))
        {
            LosX64BootstrapSerialWriteText("[Kernel] Physical frame region database overflow.\n");
            LosX64BootstrapHaltForever();
        }

        if (!AppendBaseMemoryRegion(
                SourceDescriptor->PhysicalStart,
                Length,
                ClassifyMemoryRegionType(SourceDescriptor->Type),
                GetMemoryRegionFlagsForDescriptorType(SourceDescriptor->Type),
                GetMemoryRegionOwnerForDescriptorType(SourceDescriptor->Type),
                GetMemoryRegionSourceForDescriptorType(SourceDescriptor->Type)))
        {
            LosX64BootstrapSerialWriteText("[Kernel] Memory-manager region database overflow.\n");
            LosX64BootstrapHaltForever();
        }

        if ((TargetDescriptor->Flags & 1U) != 0U && EndAddress > HighestUsableEnd)
        {
            HighestUsableEnd = EndAddress;
        }
    }

    LosAddressSpaceGapBytes = CalculateAddressSpaceGapBytes();
    LosLayout.HigherHalfDirectMapSize = HighestUsableEnd;
    if (!RebuildPublishedMemoryRegions())
    {
        LosX64BootstrapSerialWriteText("[Kernel] Normalized memory-region database overflow.\n");
        LosX64BootstrapHaltForever();
    }
    RefreshMemoryManagerHandoff();
    ReserveBootstrapRanges(BootContext);
    RefreshMemoryManagerHandoff();
}

LOS_X64_BOOTSTRAP_SECTION
const LOS_X64_VIRTUAL_MEMORY_LAYOUT *LosX64GetVirtualMemoryLayout(void)
{
    LOS_KERNEL_ENTER();
    return &LosLayout;
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64GetPageMapLevel4(void)
{
    LOS_KERNEL_ENTER();
    return &LosPageMapLevel4[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64GetIdentityDirectoryPointer(void)
{
    LOS_KERNEL_ENTER();
    return &LosIdentityDirectoryPointer[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64GetIdentityDirectory(void)
{
    LOS_KERNEL_ENTER();
    return &LosIdentityDirectory[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64GetKernelWindowDirectoryPointer(void)
{
    LOS_KERNEL_ENTER();
    return &LosKernelWindowDirectoryPointer[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64GetKernelWindowDirectory(void)
{
    LOS_KERNEL_ENTER();
    return &LosKernelWindowDirectory[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64AllocatePageTablePage(void)
{
    LOS_KERNEL_ENTER();
    if (LosPageTablePoolNextIndex >= LOS_X64_PAGE_TABLE_POOL_PAGES)
    {
        return 0;
    }

    return &LosPageTablePool[LosPageTablePoolNextIndex++][0];
}

LOS_X64_BOOTSTRAP_SECTION
UINTN LosX64GetPhysicalMemoryDescriptorCount(void)
{
    LOS_KERNEL_ENTER();
    return LosPhysicalMemoryDescriptorCount;
}

LOS_X64_BOOTSTRAP_SECTION
const LOS_X64_PHYSICAL_MEMORY_DESCRIPTOR *LosX64GetPhysicalMemoryDescriptor(UINTN Index)
{
    LOS_KERNEL_ENTER();
    if (Index >= LosPhysicalMemoryDescriptorCount)
    {
        return 0;
    }

    return &LosPhysicalMemoryDescriptors[Index];
}

LOS_X64_BOOTSTRAP_SECTION
UINTN LosX64GetPhysicalFrameRegionCount(void)
{
    LOS_KERNEL_ENTER();
    return LosPhysicalFrameRegionCount;
}

LOS_X64_BOOTSTRAP_SECTION
const LOS_X64_PHYSICAL_FRAME_REGION *LosX64GetPhysicalFrameRegion(UINTN Index)
{
    LOS_KERNEL_ENTER();
    if (Index >= LosPhysicalFrameRegionCount)
    {
        return 0;
    }

    return &LosPhysicalFrameRegions[Index];
}

LOS_X64_BOOTSTRAP_SECTION
UINTN LosX64GetMemoryRegionCount(void)
{
    LOS_KERNEL_ENTER();
    return LosMemoryRegionCount;
}

LOS_X64_BOOTSTRAP_SECTION
const LOS_X64_MEMORY_REGION *LosX64GetMemoryRegion(UINTN Index)
{
    LOS_KERNEL_ENTER();
    if (Index >= LosMemoryRegionCount)
    {
        return 0;
    }

    return &LosMemoryRegions[Index];
}

LOS_X64_BOOTSTRAP_SECTION
const LOS_X64_MEMORY_MANAGER_HANDOFF *LosX64GetMemoryManagerHandoff(void)
{
    LOS_KERNEL_ENTER();
    return &LosMemoryManagerHandoff;
}

LOS_X64_BOOTSTRAP_SECTION
UINTN LosX64GetPageTablePoolUsedCount(void)
{
    LOS_KERNEL_ENTER();
    return LosPageTablePoolNextIndex;
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64IsPhysicalRangeDiscovered(UINT64 PhysicalAddress, UINT64 Length)
{
    LOS_KERNEL_ENTER();
    UINT64 EndAddress;

    if (Length == 0ULL)
    {
        return 0;
    }

    EndAddress = PhysicalAddress + Length;
    if (EndAddress < PhysicalAddress)
    {
        return 0;
    }

    return IsRangeCoveredByAnyRegion(PhysicalAddress, EndAddress);
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64IsPhysicalRangeDirectMapCandidate(UINT64 PhysicalAddress, UINT64 Length)
{
    LOS_KERNEL_ENTER();
    UINTN Index;
    UINT64 EndAddress;

    if (Length == 0ULL)
    {
        return 0;
    }

    EndAddress = PhysicalAddress + Length;
    if (EndAddress < PhysicalAddress)
    {
        return 0;
    }

    for (Index = 0U; Index < LosPhysicalMemoryDescriptorCount; ++Index)
    {
        const LOS_X64_PHYSICAL_MEMORY_DESCRIPTOR *Descriptor;
        UINT64 DescriptorEnd;

        Descriptor = &LosPhysicalMemoryDescriptors[Index];
        DescriptorEnd = Descriptor->BaseAddress + Descriptor->Length;
        if ((Descriptor->Flags & 1U) == 0U)
        {
            continue;
        }
        if (PhysicalAddress >= Descriptor->BaseAddress && EndAddress <= DescriptorEnd)
        {
            return 1;
        }
    }

    return 0;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN IsPowerOfTwo(UINT64 Value)
{
    return Value != 0ULL && (Value & (Value - 1ULL)) == 0ULL;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 AlignUpToBoundary(UINT64 Value, UINT64 Alignment)
{
    return (Value + (Alignment - 1ULL)) & ~(Alignment - 1ULL);
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN AppendPhysicalSpanToArray(LOS_X64_PHYSICAL_SPAN *Array, UINTN *Count, UINT64 BaseAddress, UINT64 Length)
{
    LOS_X64_PHYSICAL_SPAN *Span;

    if (Length == 0ULL)
    {
        return 1;
    }

    if (*Count != 0U)
    {
        LOS_X64_PHYSICAL_SPAN *Previous;

        Previous = &Array[*Count - 1U];
        if (Previous->BaseAddress + Previous->Length == BaseAddress)
        {
            Previous->Length += Length;
            return 1;
        }
    }

    if (*Count >= LOS_X64_MAX_PHYSICAL_FRAME_REGIONS)
    {
        return 0;
    }

    Span = &Array[*Count];
    Span->BaseAddress = BaseAddress;
    Span->Length = Length;
    *Count += 1U;
    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 CollectFreePhysicalSpans(UINT64 BaseAddress, UINT64 EndAddress, LOS_X64_PHYSICAL_SPAN *Array, UINTN *Count)
{
    UINTN Index;
    UINT64 PagesFound;

    *Count = 0U;
    PagesFound = 0ULL;
    for (Index = 0U; Index < LosPhysicalFrameRegionCount; ++Index)
    {
        const LOS_X64_PHYSICAL_FRAME_REGION *Region;
        UINT64 RegionEnd;
        UINT64 OverlapBase;
        UINT64 OverlapEnd;

        Region = &LosPhysicalFrameRegions[Index];
        if (Region->State != LOS_X64_PHYSICAL_FRAME_STATE_FREE)
        {
            continue;
        }

        RegionEnd = Region->BaseAddress + (Region->PageCount * 4096ULL);
        if (RegionEnd <= BaseAddress || Region->BaseAddress >= EndAddress)
        {
            continue;
        }

        OverlapBase = BaseAddress > Region->BaseAddress ? BaseAddress : Region->BaseAddress;
        OverlapEnd = EndAddress < RegionEnd ? EndAddress : RegionEnd;
        if (!AppendPhysicalSpanToArray(Array, Count, OverlapBase, OverlapEnd - OverlapBase))
        {
            *Count = LOS_X64_MAX_PHYSICAL_FRAME_REGIONS + 1U;
            return 0ULL;
        }

        PagesFound += GetPageCountFromLength(OverlapEnd - OverlapBase);
    }

    return PagesFound;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN ReserveFreePhysicalRangeInternal(UINT64 BaseAddress, UINT64 Length, UINT32 ReservationKind, BOOLEAN RequireFullCoverage, UINT64 *PagesReserved)
{
    UINT64 EndAddress;
    UINT64 FreePages;
    UINTN SpanCount;
    UINTN Index;

    if (PagesReserved != 0)
    {
        *PagesReserved = 0ULL;
    }

    if (Length == 0ULL)
    {
        return 0;
    }

    EndAddress = BaseAddress + Length;
    if (EndAddress <= BaseAddress)
    {
        return 0;
    }

    if (!IsRangeCoveredByAnyRegion(BaseAddress, EndAddress))
    {
        return 0;
    }

    FreePages = CollectFreePhysicalSpans(BaseAddress, EndAddress, LosPhysicalSpanScratch, &SpanCount);
    if (SpanCount > LOS_X64_MAX_PHYSICAL_FRAME_REGIONS)
    {
        return 0;
    }
    if (RequireFullCoverage != 0U && FreePages != GetPageCountFromLength(Length))
    {
        return 0;
    }

    if (FreePages == 0ULL)
    {
        return 1;
    }

    if (LosOverlayMemoryRegionCount + SpanCount > LOS_X64_MAX_MEMORY_REGIONS)
    {
        return 0;
    }

    if (!RewritePhysicalFrameRegions(BaseAddress, EndAddress, LOS_X64_PHYSICAL_FRAME_STATE_FREE, LOS_X64_PHYSICAL_FRAME_STATE_RESERVED, ReservationKind))
    {
        return 0;
    }

    for (Index = 0U; Index < SpanCount; ++Index)
    {
        if (!AppendOverlayMemoryRegionRecord(LosPhysicalSpanScratch[Index].BaseAddress, LosPhysicalSpanScratch[Index].Length, ReservationKind))
        {
            return 0;
        }
    }

    if (!RebuildPublishedMemoryRegions())
    {
        return 0;
    }

    RefreshMemoryManagerHandoff();
    if (PagesReserved != 0)
    {
        *PagesReserved = FreePages;
    }
    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN FindClaimableContiguousRange(UINT64 MinimumPhysicalAddress, UINT64 MaximumPhysicalAddress, UINT64 AlignmentBytes, UINT64 PageCount, UINT64 *BaseAddress)
{
    UINTN Index;
    UINT64 RunBase;
    UINT64 RunEnd;
    UINT64 RequiredBytes;
    BOOLEAN HaveRun;

    RequiredBytes = PageCount * 4096ULL;
    if (RequiredBytes == 0ULL || RequiredBytes / 4096ULL != PageCount)
    {
        return 0;
    }

    HaveRun = 0U;
    RunBase = 0ULL;
    RunEnd = 0ULL;
    for (Index = 0U; Index < LosPhysicalFrameRegionCount; ++Index)
    {
        const LOS_X64_PHYSICAL_FRAME_REGION *Region;
        UINT64 RegionBase;
        UINT64 RegionEnd;
        UINT64 CandidateBase;

        Region = &LosPhysicalFrameRegions[Index];
        if (Region->State != LOS_X64_PHYSICAL_FRAME_STATE_FREE)
        {
            HaveRun = 0U;
            continue;
        }

        RegionBase = Region->BaseAddress;
        RegionEnd = RegionBase + (Region->PageCount * 4096ULL);
        if (RegionEnd <= MinimumPhysicalAddress || RegionBase >= MaximumPhysicalAddress)
        {
            continue;
        }

        if (RegionBase < MinimumPhysicalAddress)
        {
            RegionBase = MinimumPhysicalAddress;
        }
        if (RegionEnd > MaximumPhysicalAddress)
        {
            RegionEnd = MaximumPhysicalAddress;
        }
        if (RegionEnd <= RegionBase)
        {
            continue;
        }

        if (HaveRun == 0U || RegionBase > RunEnd)
        {
            RunBase = RegionBase;
            RunEnd = RegionEnd;
            HaveRun = 1U;
        }
        else if (RegionEnd > RunEnd)
        {
            RunEnd = RegionEnd;
        }

        CandidateBase = AlignUpToBoundary(RunBase, AlignmentBytes);
        if (CandidateBase >= RunBase && CandidateBase <= RunEnd && RunEnd - CandidateBase >= RequiredBytes)
        {
            *BaseAddress = CandidateBase;
            return 1;
        }
    }

    return 0;
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64TryTranslateKernelVirtualToPhysical(UINT64 VirtualAddress, UINT64 *PhysicalAddress)
{
    LOS_KERNEL_ENTER();
    if (PhysicalAddress == 0)
    {
        return 0;
    }

    if (VirtualAddress < LosLayout.BootstrapIdentitySize)
    {
        *PhysicalAddress = VirtualAddress;
        return 1;
    }

    if (VirtualAddress >= LosLayout.HigherHalfDirectMapBase && VirtualAddress - LosLayout.HigherHalfDirectMapBase < LosLayout.HigherHalfDirectMapSize)
    {
        *PhysicalAddress = VirtualAddress - LosLayout.HigherHalfDirectMapBase;
        return 1;
    }

    return TryTranslateHigherHalfPointerToPhysical(VirtualAddress, PhysicalAddress);
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 LosX64GetCurrentPageMapLevel4PhysicalAddress(void)
{
    UINT64 Value;

    LOS_KERNEL_ENTER();
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(Value));
    return Value & LOS_X64_PAGE_TABLE_ADDRESS_MASK;
}

LOS_X64_BOOTSTRAP_SECTION
void *LosX64GetDirectMapVirtualAddress(UINT64 PhysicalAddress, UINT64 Length)
{
    LOS_KERNEL_ENTER();
    if (Length == 0ULL || !LosX64IsPhysicalRangeDiscovered(PhysicalAddress, Length))
    {
        return 0;
    }

    return (void *)(UINTN)(LosLayout.HigherHalfDirectMapBase + PhysicalAddress);
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64QueryMemoryRegions(LOS_X64_MEMORY_REGION *Buffer, UINTN BufferRegionCapacity, LOS_X64_QUERY_MEMORY_REGIONS_RESULT *Result)
{
    UINTN Index;

    LOS_KERNEL_ENTER();
    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->Reserved = 0U;
    Result->TotalRegionCount = (UINT64)LosMemoryRegionCount;
    Result->RegionsWritten = 0ULL;
    Result->RegionEntrySize = (UINT64)sizeof(LOS_X64_MEMORY_REGION);

    if (Buffer == 0 && BufferRegionCapacity != 0U)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
        return;
    }

    for (Index = 0U; Index < LosMemoryRegionCount && Index < BufferRegionCapacity; ++Index)
    {
        Buffer[Index] = LosMemoryRegions[Index];
        Result->RegionsWritten += 1ULL;
    }
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64ReserveFrames(const LOS_X64_RESERVE_FRAMES_REQUEST *Request, LOS_X64_RESERVE_FRAMES_RESULT *Result)
{
    UINT64 AlignedBase;
    UINT64 AlignedEnd;
    UINT64 PagesReserved;
    UINT32 Owner;

    LOS_KERNEL_ENTER();
    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->PagesReserved = 0ULL;
    Result->RangeBase = 0ULL;
    Result->RangeLength = 0ULL;

    if (Request == 0 || Request->Length == 0ULL)
    {
        return;
    }

    AlignedBase = AlignDownPageBoundary(Request->PhysicalAddress);
    AlignedEnd = AlignUpPageBoundary(Request->PhysicalAddress + Request->Length);
    if (AlignedEnd <= AlignedBase)
    {
        return;
    }

    if (!IsRangeCoveredByAnyRegion(AlignedBase, AlignedEnd))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
        return;
    }

    Owner = Request->Owner == LOS_X64_PHYSICAL_FRAME_RESERVED_NONE ? LOS_X64_PHYSICAL_FRAME_RESERVED_CLAIMED : Request->Owner;
    if (!ReserveFreePhysicalRangeInternal(AlignedBase, AlignedEnd - AlignedBase, Owner, 0U, &PagesReserved))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->PagesReserved = PagesReserved;
    Result->RangeBase = AlignedBase;
    Result->RangeLength = AlignedEnd - AlignedBase;
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64ClaimFrames(const LOS_X64_CLAIM_FRAMES_REQUEST *Request, LOS_X64_CLAIM_FRAMES_RESULT *Result)
{
    UINT64 AlignmentBytes;
    UINT64 MinimumPhysicalAddress;
    UINT64 MaximumPhysicalAddress;
    UINT64 BaseAddress;
    UINT64 RequiredBytes;
    UINT64 PagesReserved;
    UINT32 Owner;

    LOS_KERNEL_ENTER();
    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->BaseAddress = 0ULL;
    Result->PageCount = 0ULL;

    if (Request == 0 || Request->PageCount == 0ULL)
    {
        return;
    }

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

    MinimumPhysicalAddress = AlignDownPageBoundary(Request->MinimumPhysicalAddress);
    MaximumPhysicalAddress = Request->MaximumPhysicalAddress == 0ULL ? LosLayout.HighestDiscoveredPhysicalAddress : AlignUpPageBoundary(Request->MaximumPhysicalAddress);
    if ((Request->Flags & LOS_X64_CLAIM_FRAMES_FLAG_BELOW_4G) != 0U && MaximumPhysicalAddress > 0x100000000ULL)
    {
        MaximumPhysicalAddress = 0x100000000ULL;
    }
    if (MaximumPhysicalAddress <= MinimumPhysicalAddress)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
        return;
    }

    Owner = Request->Owner == LOS_X64_PHYSICAL_FRAME_RESERVED_NONE ? LOS_X64_PHYSICAL_FRAME_RESERVED_CLAIMED : Request->Owner;
    if ((Request->Flags & LOS_X64_CLAIM_FRAMES_FLAG_EXACT_ADDRESS) != 0U)
    {
        BaseAddress = AlignDownPageBoundary(Request->DesiredPhysicalAddress);
        if (BaseAddress < MinimumPhysicalAddress || BaseAddress + RequiredBytes > MaximumPhysicalAddress || BaseAddress + RequiredBytes < BaseAddress)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
            return;
        }
        if (!IsRangeCoveredByState(BaseAddress, BaseAddress + RequiredBytes, LOS_X64_PHYSICAL_FRAME_STATE_FREE))
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
            return;
        }
    }
    else
    {
        if (!FindClaimableContiguousRange(MinimumPhysicalAddress, MaximumPhysicalAddress, AlignmentBytes, Request->PageCount, &BaseAddress))
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
            return;
        }
    }

    if (!ReserveFreePhysicalRangeInternal(BaseAddress, RequiredBytes, Owner, 1U, &PagesReserved) || PagesReserved != Request->PageCount)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->BaseAddress = BaseAddress;
    Result->PageCount = PagesReserved;
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64ReservePhysicalRange(UINT64 PhysicalAddress, UINT64 Length, UINT32 ReservationKind)
{
    LOS_X64_RESERVE_FRAMES_REQUEST Request;
    LOS_X64_RESERVE_FRAMES_RESULT Result;

    LOS_KERNEL_ENTER();
    Request.PhysicalAddress = PhysicalAddress;
    Request.Length = Length;
    Request.Owner = ReservationKind;
    Request.Reserved = 0U;
    LosX64ReserveFrames(&Request, &Result);
    return Result.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64ClaimPhysicalPages(UINT64 PhysicalAddress, UINT64 PageCount)
{
    LOS_X64_CLAIM_FRAMES_REQUEST Request;
    LOS_X64_CLAIM_FRAMES_RESULT Result;

    LOS_KERNEL_ENTER();
    Request.DesiredPhysicalAddress = PhysicalAddress;
    Request.MinimumPhysicalAddress = 0ULL;
    Request.MaximumPhysicalAddress = 0ULL;
    Request.AlignmentBytes = 4096ULL;
    Request.PageCount = PageCount;
    Request.Flags = LOS_X64_CLAIM_FRAMES_FLAG_EXACT_ADDRESS | LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS;
    Request.Owner = LOS_X64_PHYSICAL_FRAME_RESERVED_CLAIMED;
    LosX64ClaimFrames(&Request, &Result);
    return Result.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
}

LOS_X64_BOOTSTRAP_SECTION
void *LosX64GetBootstrapTransitionStackTop(void)
{
    UINT64 Top;

    Top = (UINT64)(UINTN)&LosBootstrapTransitionStack[LOS_X64_BOOTSTRAP_TRANSITION_STACK_BYTES];
    Top &= ~0xFULL;
    return (void *)(UINTN)Top;
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 LosX64GetBootstrapTransitionStackBase(void)
{
    LOS_KERNEL_ENTER();
    return (UINT64)(UINTN)&LosBootstrapTransitionStack[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 LosX64GetBootstrapTransitionStackSize(void)
{
    LOS_KERNEL_ENTER();
    return (UINT64)LOS_X64_BOOTSTRAP_TRANSITION_STACK_BYTES;
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 LosX64GetBootstrapPageTableStorageBase(void)
{
    LOS_KERNEL_ENTER();
    return (UINT64)(UINTN)&LosPageMapLevel4[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 LosX64GetBootstrapPageTableStorageSize(void)
{
    UINT64 BaseAddress;
    UINT64 EndAddress;

    LOS_KERNEL_ENTER();
    BaseAddress = (UINT64)(UINTN)&LosPageMapLevel4[0];
    EndAddress = (UINT64)(UINTN)&LosPageTablePool[LOS_X64_PAGE_TABLE_POOL_PAGES - 1U][512];
    return EndAddress - BaseAddress;
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 LosX64GetKernelStackBackingBase(void)
{
    LOS_KERNEL_ENTER();
    return (UINT64)(UINTN)&LosKernelStackBacking[0][0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 LosX64GetKernelStackBackingSize(void)
{
    LOS_KERNEL_ENTER();
    return LOS_X64_KERNEL_STACK_SIZE_BYTES;
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64InstallKernelStackMapping(void)
{
    LOS_KERNEL_ENTER();
    UINT64 VirtualAddress;
    UINT64 PhysicalAddress;
    UINT64 PhysicalEnd;
    BOOLEAN BackingDiscovered;

    VirtualAddress = LOS_X64_KERNEL_STACK_BASE + (LOS_X64_KERNEL_STACK_GUARD_PAGES * 4096ULL);
    PhysicalAddress = (UINT64)(UINTN)&LosKernelStackBacking[0][0];
    PhysicalEnd = PhysicalAddress + LOS_X64_KERNEL_STACK_SIZE_BYTES;
    BackingDiscovered = LosX64IsPhysicalRangeDiscovered(PhysicalAddress, LOS_X64_KERNEL_STACK_SIZE_BYTES);

    LosX64BootstrapSerialWriteLineHex64(LosKernelStackBackingVirtualStartPrefix, VirtualAddress);
    LosX64BootstrapSerialWriteLineHex64(LosKernelStackBackingPhysicalStartPrefix, PhysicalAddress);
    LosX64BootstrapSerialWriteLineHex64(LosKernelStackBackingPhysicalEndPrefix, PhysicalEnd);
    LosX64BootstrapSerialWriteLineHex64(LosKernelStackPageTablePoolUsedPrefix, (UINT64)LosX64GetPageTablePoolUsedCount());

    if (BackingDiscovered != 0U)
    {
        return LosX64MapVirtualRange(
            VirtualAddress,
            PhysicalAddress,
            LOS_X64_KERNEL_STACK_COMMITTED_PAGES,
            LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_GLOBAL | LOS_X64_PAGE_NX);
    }

    LosX64BootstrapSerialWriteText(LosKernelStackBackingMapUncheckedNotice);
    return LosX64MapVirtualRangeUnchecked(
        VirtualAddress,
        PhysicalAddress,
        LOS_X64_KERNEL_STACK_COMMITTED_PAGES,
        LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_GLOBAL | LOS_X64_PAGE_NX);
}
