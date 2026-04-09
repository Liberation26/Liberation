/*
 * File Name: MemoryManagerAddressSpaceDispatchSection07.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerAddressSpaceDispatch.c.
 */

void LosMemoryManagerServiceAllocateAddressSpaceStack(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_REQUEST *Request,
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
    UINT64 StackBaseVirtualAddress;
    UINT64 StackPhysicalAddress;

    if (Result == 0)
    {
        return;
    }

    ZeroBytes(Result, sizeof(*Result));
    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    if (State == 0 || Request == 0 || State->Online == 0U || State->AttachComplete == 0U)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }
    if (Request->PageCount == 0ULL)
    {
        return;
    }

    AddressSpaceObject = 0;
    if (!LosMemoryManagerResolveAddressSpaceObject(State, Request->AddressSpaceObjectPhysicalAddress, 0, &AddressSpaceObject))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        return;
    }
    if ((AddressSpaceObject->Flags & LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK) != 0ULL)
    {
        if (PopulateRequestedStackResult(
                State,
                AddressSpaceObject,
                Request->AddressSpaceObjectPhysicalAddress,
                Request->DesiredStackBaseVirtualAddress,
                Request->PageCount,
                Result))
        {
            return;
        }
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }
    if (AddressSpaceObject->StackBaseVirtualAddress != 0ULL ||
        AddressSpaceObject->StackPhysicalAddress != 0ULL ||
        AddressSpaceObject->StackTopVirtualAddress != 0ULL)
    {
        if (PopulateRequestedStackResult(
                State,
                AddressSpaceObject,
                Request->AddressSpaceObjectPhysicalAddress,
                Request->DesiredStackBaseVirtualAddress,
                Request->PageCount,
                Result))
        {
            AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK;
            return;
        }
    }

    StackBaseVirtualAddress = 0ULL;
    if (!LosMemoryManagerSelectStackBaseVirtualAddress(
            AddressSpaceObject,
            Request->DesiredStackBaseVirtualAddress,
            Request->PageCount,
            &StackBaseVirtualAddress))
    {
        if (PopulateRequestedStackResult(
                State,
                AddressSpaceObject,
                Request->AddressSpaceObjectPhysicalAddress,
                Request->DesiredStackBaseVirtualAddress,
                Request->PageCount,
                Result))
        {
            return;
        }
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }
    if (!CanReserveRegion(AddressSpaceObject, StackBaseVirtualAddress, Request->PageCount))
    {
        if (PopulateRequestedStackResult(
                State,
                AddressSpaceObject,
                Request->AddressSpaceObjectPhysicalAddress,
                StackBaseVirtualAddress,
                Request->PageCount,
                Result))
        {
            return;
        }
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    StackPhysicalAddress = 0ULL;
    if (!LosMemoryManagerServiceClaimTrackedFrames(
            State,
            Request->PageCount,
            0x1000ULL,
            LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS,
            LOS_X64_MEMORY_REGION_OWNER_CLAIMED,
            LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_STACK,
            &StackPhysicalAddress))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    if (!LosMemoryManagerMapPagesIntoAddressSpace(
            State,
            AddressSpaceObject->RootTablePhysicalAddress,
            StackBaseVirtualAddress,
            StackPhysicalAddress,
            Request->PageCount,
            LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_USER | LOS_X64_PAGE_NX))
    {
        if (RepairStackStateFromCurrentMappings(
                State,
                AddressSpaceObject,
                Request->AddressSpaceObjectPhysicalAddress,
                StackBaseVirtualAddress,
                Request->PageCount,
                Result))
        {
            LosMemoryManagerServiceFreeTrackedFrames(State, StackPhysicalAddress, Request->PageCount);
            return;
        }
        LosMemoryManagerServiceFreeTrackedFrames(State, StackPhysicalAddress, Request->PageCount);
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    if (!LosMemoryManagerReserveVirtualRegion(
            AddressSpaceObject,
            StackBaseVirtualAddress,
            Request->PageCount,
            LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_STACK,
            0U,
            StackPhysicalAddress))
    {
        if (PopulateRequestedStackResult(
                State,
                AddressSpaceObject,
                Request->AddressSpaceObjectPhysicalAddress,
                StackBaseVirtualAddress,
                Request->PageCount,
                Result))
        {
            LosMemoryManagerServiceFreeTrackedFrames(State, StackPhysicalAddress, Request->PageCount);
            return;
        }
        LosMemoryManagerServiceFreeTrackedFrames(State, StackPhysicalAddress, Request->PageCount);
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    AddressSpaceObject->StackPhysicalAddress = StackPhysicalAddress;
    AddressSpaceObject->StackPageCount = Request->PageCount;
    AddressSpaceObject->StackBaseVirtualAddress = StackBaseVirtualAddress;
    AddressSpaceObject->StackTopVirtualAddress = StackBaseVirtualAddress + (Request->PageCount * 0x1000ULL);
    AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK;
    (void)EnsureReservedStackRegionRecorded(AddressSpaceObject);

    if (!PopulateExistingStackResult(AddressSpaceObject, Request->AddressSpaceObjectPhysicalAddress, Result))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }
    LosMemoryManagerDiagnosticsClearAttachImageContext();
    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
}
