/*
 * File Name: VirtualMemoryStateSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from VirtualMemoryState.c.
 */

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
