/*
 * File Name: VirtualMemoryStateSection04.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from VirtualMemoryState.c.
 */

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

    __asm__ __volatile__("mov %%cr3, %0" : "=r"(Value));
    return Value & LOS_X64_PAGE_TABLE_ADDRESS_MASK;
}

LOS_X64_BOOTSTRAP_SECTION
void *LosX64GetDirectMapVirtualAddress(UINT64 PhysicalAddress, UINT64 Length)
{
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
