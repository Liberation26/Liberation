#include "MemoryManagerMemoryInternal.h"

BOOLEAN LosMemoryManagerServiceClaimTrackedFrames(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageCount,
    UINT64 AlignmentBytes,
    UINT32 Flags,
    UINT32 Owner,
    UINT32 Usage,
    UINT64 *BaseAddress)
{
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View;
    UINT64 MinimumPhysicalAddress;
    UINT64 MaximumPhysicalAddress;
    UINT64 RequiredBytes;
    UINT64 ChosenBaseAddress;
    UINT64 EffectiveAlignment;
    UINT32 EffectiveOwner;

    if (BaseAddress == 0)
    {
        return 0;
    }

    *BaseAddress = 0ULL;
    if (State == 0 || PageCount == 0ULL || State->MemoryView.Ready == 0U)
    {
        return 0;
    }

    RequiredBytes = PageCount * 4096ULL;
    if (RequiredBytes == 0ULL || RequiredBytes / 4096ULL != PageCount)
    {
        return 0;
    }

    EffectiveAlignment = AlignmentBytes == 0ULL ? 4096ULL : AlignmentBytes;
    if (EffectiveAlignment < 4096ULL || (EffectiveAlignment & 0xFFFULL) != 0ULL || !LosMemoryManagerIsPowerOfTwo(EffectiveAlignment))
    {
        return 0;
    }

    View = &State->MemoryView;
    MinimumPhysicalAddress = 0x1000ULL;
    MaximumPhysicalAddress = 0ULL;
    if (View->PageFrameDatabaseEntryCount != 0U)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *LastEntry;

        LastEntry = &View->PageFrameDatabase[View->PageFrameDatabaseEntryCount - 1U];
        MaximumPhysicalAddress = LastEntry->BaseAddress + (LastEntry->PageCount * 4096ULL);
    }
    if ((Flags & LOS_X64_CLAIM_FRAMES_FLAG_BELOW_4G) != 0U && MaximumPhysicalAddress > 0x100000000ULL)
    {
        MaximumPhysicalAddress = 0x100000000ULL;
    }
    if (MaximumPhysicalAddress <= MinimumPhysicalAddress)
    {
        return 0;
    }

    if (!LosMemoryManagerFindClaimableContiguousRange(
            View->PageFrameDatabase,
            View->PageFrameDatabaseEntryCount,
            MinimumPhysicalAddress,
            MaximumPhysicalAddress,
            EffectiveAlignment,
            PageCount,
            &ChosenBaseAddress))
    {
        return 0;
    }

    EffectiveOwner = Owner == LOS_X64_PHYSICAL_FRAME_RESERVED_NONE ? LOS_X64_MEMORY_REGION_OWNER_CLAIMED : Owner;
    if (!LosMemoryManagerInsertDynamicAllocation(View, ChosenBaseAddress, PageCount, Usage, EffectiveOwner) ||
        !LosMemoryManagerRebuildCurrentPageFrameDatabase(View))
    {
        return 0;
    }

    *BaseAddress = ChosenBaseAddress;
    return 1;
}

BOOLEAN LosMemoryManagerServiceFreeTrackedFrames(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 PhysicalAddress, UINT64 PageCount)
{
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View;

    if (State == 0 || PageCount == 0ULL || State->MemoryView.Ready == 0U)
    {
        return 0;
    }
    if ((PhysicalAddress & 0xFFFULL) != 0ULL)
    {
        return 0;
    }

    View = &State->MemoryView;
    if (!LosMemoryManagerRemoveDynamicAllocationRange(View, PhysicalAddress, PageCount))
    {
        return 0;
    }
    return LosMemoryManagerRebuildCurrentPageFrameDatabase(View);
}

static const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *FindExactDynamicAllocation(
    const LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 PhysicalAddress,
    UINT64 PageCount)
{
    UINTN Index;

    if (View == 0 || PageCount == 0ULL)
    {
        return 0;
    }

    for (Index = 0U; Index < View->DynamicAllocationCount; ++Index)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry;

        Entry = &View->DynamicAllocations[Index];
        if (Entry->BaseAddress == PhysicalAddress && Entry->PageCount == PageCount)
        {
            return Entry;
        }
    }

    return 0;
}

void LosMemoryManagerServiceAllocateFrames(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_REQUEST *Request,
    LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_RESULT *Result)
{
    LOS_X64_CLAIM_FRAMES_REQUEST ClaimRequest;
    LOS_X64_CLAIM_FRAMES_RESULT ClaimResult;

    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->BaseAddress = 0ULL;
    Result->PageCount = 0ULL;

    if (State == 0 || Request == 0)
    {
        if (State == 0 || State->MemoryView.Ready == 0U)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        }
        return;
    }

    ClaimRequest.DesiredPhysicalAddress = Request->DesiredPhysicalAddress;
    ClaimRequest.MinimumPhysicalAddress = Request->MinimumPhysicalAddress;
    ClaimRequest.MaximumPhysicalAddress = Request->MaximumPhysicalAddress;
    ClaimRequest.AlignmentBytes = Request->AlignmentBytes;
    ClaimRequest.PageCount = Request->PageCount;
    ClaimRequest.Flags = Request->Flags;
    ClaimRequest.Owner = Request->Owner;
    LosMemoryManagerServiceClaimFrames(State, &ClaimRequest, &ClaimResult);

    Result->Status = ClaimResult.Status;
    Result->BaseAddress = ClaimResult.BaseAddress;
    Result->PageCount = ClaimResult.PageCount;
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
    AlignedBase = LosMemoryManagerAlignDownPage(Request->PhysicalAddress);
    if (LosMemoryManagerDoesRangeWrap(Request->PhysicalAddress, Request->Length))
    {
        LosMemoryManagerHardFail("base-plus-size-wrap", Request->PhysicalAddress, Request->Length, LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES);
    }
    AlignedEnd = LosMemoryManagerAlignUpPage(Request->PhysicalAddress + Request->Length);
    if (AlignedEnd <= AlignedBase)
    {
        LosMemoryManagerHardFail("base-plus-size-wrap", Request->PhysicalAddress, Request->Length, LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES);
    }

    if (!LosMemoryManagerIsRangeCoveredByPageFrameDatabase(View->PageFrameDatabase, View->PageFrameDatabaseEntryCount, AlignedBase, AlignedEnd - AlignedBase))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
        return;
    }
    if (!LosMemoryManagerIsRangeCoveredByState(View->PageFrameDatabase, View->PageFrameDatabaseEntryCount, AlignedBase, AlignedEnd - AlignedBase, LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    Owner = Request->Owner == LOS_X64_PHYSICAL_FRAME_RESERVED_NONE ? LOS_X64_MEMORY_REGION_OWNER_CLAIMED : Request->Owner;
    if (!LosMemoryManagerInsertDynamicAllocation(View, AlignedBase, LosMemoryManagerGetPageCountFromLength(AlignedEnd - AlignedBase), LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_DYNAMIC_RESERVED, Owner) ||
        !LosMemoryManagerRebuildCurrentPageFrameDatabase(View))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->PagesReserved = LosMemoryManagerGetPageCountFromLength(AlignedEnd - AlignedBase);
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
        LosMemoryManagerHardFail("page-count-overflow", Request->PageCount, 0x1000ULL, LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES);
    }

    AlignmentBytes = Request->AlignmentBytes == 0ULL ? 4096ULL : Request->AlignmentBytes;
    if (AlignmentBytes < 4096ULL || (AlignmentBytes & 0xFFFULL) != 0ULL || !LosMemoryManagerIsPowerOfTwo(AlignmentBytes))
    {
        return;
    }

    MinimumPhysicalAddress = LosMemoryManagerAlignDownPage(Request->MinimumPhysicalAddress);
    MaximumPhysicalAddress = Request->MaximumPhysicalAddress == 0ULL ? 0ULL : LosMemoryManagerAlignUpPage(Request->MaximumPhysicalAddress);
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
        BaseAddress = LosMemoryManagerAlignDownPage(Request->DesiredPhysicalAddress);
        if (LosMemoryManagerDoesRangeWrap(BaseAddress, RequiredBytes))
        {
            LosMemoryManagerHardFail("base-plus-size-wrap", BaseAddress, RequiredBytes, LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES);
        }
        if (BaseAddress < MinimumPhysicalAddress || BaseAddress + RequiredBytes > MaximumPhysicalAddress || BaseAddress + RequiredBytes < BaseAddress)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
            return;
        }
        if (!LosMemoryManagerIsRangeCoveredByPageFrameDatabase(View->PageFrameDatabase, View->PageFrameDatabaseEntryCount, BaseAddress, RequiredBytes))
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
            return;
        }
        if (!LosMemoryManagerIsRangeCoveredByState(View->PageFrameDatabase, View->PageFrameDatabaseEntryCount, BaseAddress, RequiredBytes, LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE))
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
            return;
        }
    }
    else if (!LosMemoryManagerFindClaimableContiguousRange(
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

    if (!LosMemoryManagerInsertDynamicAllocation(View, BaseAddress, Request->PageCount, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_DYNAMIC_CLAIMED, Owner) ||
        !LosMemoryManagerRebuildCurrentPageFrameDatabase(View))
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
    if (LosMemoryManagerDoesRangeWrap(Request->PhysicalAddress, Request->PageCount * 4096ULL))
    {
        LosMemoryManagerHardFail("page-count-overflow", Request->PhysicalAddress, Request->PageCount, LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES);
    }

    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Allocation;

        Allocation = FindExactDynamicAllocation(&State->MemoryView, Request->PhysicalAddress, Request->PageCount);
        if (Allocation == 0)
        {
            LosMemoryManagerHardFail("freeing-unowned-pages", Request->PhysicalAddress, Request->PageCount, LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES);
        }
        if (Allocation->Usage != LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_DYNAMIC_RESERVED &&
            Allocation->Usage != LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_DYNAMIC_CLAIMED)
        {
            if (Allocation->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_BOOTSTRAP_RESERVED)
            {
                LosMemoryManagerHardFail("freeing-bootstrap-reserved-pages", Request->PhysicalAddress, Request->PageCount, Allocation->Usage);
            }
            LosMemoryManagerHardFail("freeing-unowned-pages", Request->PhysicalAddress, Request->PageCount, Allocation->Usage);
        }
    }

    if (!LosMemoryManagerServiceFreeTrackedFrames(State, Request->PhysicalAddress, Request->PageCount))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->BaseAddress = Request->PhysicalAddress;
    Result->PageCount = Request->PageCount;
}
