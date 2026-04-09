/*
 * File Name: MemoryManagerMemoryDispatchSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerMemoryDispatch.c.
 */

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
