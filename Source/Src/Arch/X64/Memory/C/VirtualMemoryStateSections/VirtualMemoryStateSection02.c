/*
 * File Name: VirtualMemoryStateSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from VirtualMemoryState.c.
 */

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
