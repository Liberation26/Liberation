/*
 * File Name: VirtualMemoryStateSection05.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from VirtualMemoryState.c.
 */

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
