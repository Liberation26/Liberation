/*
 * File Name: MemoryManagerBootstrapStateSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapState.c.
 */

void LosMemoryManagerBootstrapTransitionTo(UINT32 NewState)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    if (State->Info.State != NewState)
    {
        State->StateTransitionCount += 1ULL;
    }
    State->Info.State = NewState;
    if (State->LaunchBlock != 0)
    {
        State->LaunchBlock->State = NewState;
    }
}

void LosMemoryManagerBootstrapSetFlag(UINT64 Flag)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    State->Info.Flags |= Flag;
}

UINT64 LosMemoryManagerBootstrapAllocateRequestId(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    UINT64 RequestId;

    State = LosMemoryManagerBootstrapState();
    RequestId = State->NextRequestId;
    State->NextRequestId += 1ULL;
    return RequestId;
}

void LosMemoryManagerBootstrapRecordRequest(UINT32 Operation)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    State->MessagesSent += 1ULL;
    State->LastOperation = (UINT64)Operation;
}

void LosMemoryManagerBootstrapRecordCompletion(UINT32 Operation, UINT32 Status)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    State->MessagesCompleted += 1ULL;
    State->LastOperation = (UINT64)Operation;
    State->LastStatus = (UINT64)Status;
}

void LosMemoryManagerBootstrapRecordBootstrapResult(UINT32 ResultCode)
{
    LosMemoryManagerBootstrapState()->BootstrapResultCode = ResultCode;
}

const LOS_MEMORY_MANAGER_BOOTSTRAP_INFO *LosGetMemoryManagerBootstrapInfo(void)
{
    return &LosMemoryManagerBootstrapState()->Info;
}

const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LosGetMemoryManagerLaunchBlock(void)
{
    return LosMemoryManagerBootstrapState()->LaunchBlock;
}

BOOLEAN LosIsMemoryManagerBootstrapReady(void)
{
    return LosMemoryManagerBootstrapState()->Info.State >= LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_READY;
}

LOS_MEMORY_MANAGER_REQUEST_MAILBOX *LosMemoryManagerBootstrapGetRequestMailbox(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    return (LOS_MEMORY_MANAGER_REQUEST_MAILBOX *)(UINTN)State->RequestMailboxVirtualAddress;
}

LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *LosMemoryManagerBootstrapGetResponseMailbox(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    return (LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *)(UINTN)State->ResponseMailboxVirtualAddress;
}

LOS_MEMORY_MANAGER_EVENT_MAILBOX *LosMemoryManagerBootstrapGetEventMailbox(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    return (LOS_MEMORY_MANAGER_EVENT_MAILBOX *)(UINTN)State->EventMailboxVirtualAddress;
}

void LosMemoryManagerBootstrapInitializeMailboxes(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    InitializeRequestMailbox(LosMemoryManagerBootstrapGetRequestMailbox(), State->Info.Endpoints.KernelToService);
    InitializeResponseMailbox(LosMemoryManagerBootstrapGetResponseMailbox(), State->Info.Endpoints.ServiceToKernel);
    InitializeEventMailbox(LosMemoryManagerBootstrapGetEventMailbox(), State->Info.Endpoints.ServiceEvents);
}

static BOOLEAN MapBootstrapObject(UINT64 PhysicalAddress, UINT64 Size, void **MappedOut)
{
    void *Mapped;

    Mapped = LosX64GetDirectMapVirtualAddress(PhysicalAddress, Size);
    if (Mapped == 0)
    {
        *MappedOut = 0;
        return 0;
    }

    *MappedOut = Mapped;
    return 1;
}

BOOLEAN LosMemoryManagerBootstrapStageTransport(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *KernelToServiceEndpointObject;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *ServiceToKernelEndpointObject;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *ServiceEventsEndpointObject;
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *ServiceAddressSpaceObject;
    LOS_MEMORY_MANAGER_TASK_OBJECT *ServiceTaskObject;
    LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    void *Mapped;

    State = LosMemoryManagerBootstrapState();

    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_REQUEST_MAILBOX_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.RequestMailboxPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_RESPONSE_MAILBOX_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ResponseMailboxPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_EVENT_MAILBOX_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.EventMailboxPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.KernelToServiceEndpointObjectPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ServiceToKernelEndpointObjectPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ServiceEventsEndpointObjectPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ServiceAddressSpaceObjectPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_TASK_OBJECT_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ServiceTaskObjectPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_LAUNCH_BLOCK_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.LaunchBlockPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_SERVICE_STACK_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ServiceStackPhysicalAddress))
    {
        return 0;
    }

    State->Info.RequestMailboxSize = LOS_MEMORY_MANAGER_REQUEST_MAILBOX_PAGE_COUNT * 0x1000ULL;
    State->Info.ResponseMailboxSize = LOS_MEMORY_MANAGER_RESPONSE_MAILBOX_PAGE_COUNT * 0x1000ULL;
    State->Info.EventMailboxSize = LOS_MEMORY_MANAGER_EVENT_MAILBOX_PAGE_COUNT * 0x1000ULL;
    State->Info.LaunchBlockSize = LOS_MEMORY_MANAGER_LAUNCH_BLOCK_PAGE_COUNT * 0x1000ULL;
    State->Info.ServiceStackPageCount = LOS_MEMORY_MANAGER_SERVICE_STACK_PAGE_COUNT;

    if (!MapBootstrapObject(State->Info.RequestMailboxPhysicalAddress, State->Info.RequestMailboxSize, &Mapped))
    {
        return 0;
    }
    State->RequestMailboxVirtualAddress = (UINT64)(UINTN)Mapped;

    if (!MapBootstrapObject(State->Info.ResponseMailboxPhysicalAddress, State->Info.ResponseMailboxSize, &Mapped))
    {
        return 0;
    }
    State->ResponseMailboxVirtualAddress = (UINT64)(UINTN)Mapped;

    if (!MapBootstrapObject(State->Info.EventMailboxPhysicalAddress, State->Info.EventMailboxSize, &Mapped))
    {
        return 0;
    }
    State->EventMailboxVirtualAddress = (UINT64)(UINTN)Mapped;

    if (!MapBootstrapObject(State->Info.KernelToServiceEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 0x1000ULL, &Mapped))
    {
        return 0;
    }
    KernelToServiceEndpointObject = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)Mapped;

    if (!MapBootstrapObject(State->Info.ServiceToKernelEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 0x1000ULL, &Mapped))
    {
        return 0;
    }
    ServiceToKernelEndpointObject = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)Mapped;

    if (!MapBootstrapObject(State->Info.ServiceEventsEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 0x1000ULL, &Mapped))
    {
        return 0;
    }
    ServiceEventsEndpointObject = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)Mapped;

    if (!MapBootstrapObject(State->Info.ServiceAddressSpaceObjectPhysicalAddress, LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT_PAGE_COUNT * 0x1000ULL, &Mapped))
    {
        return 0;
    }
    ServiceAddressSpaceObject = (LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *)Mapped;

    if (!MapBootstrapObject(State->Info.ServiceTaskObjectPhysicalAddress, LOS_MEMORY_MANAGER_TASK_OBJECT_PAGE_COUNT * 0x1000ULL, &Mapped))
    {
        return 0;
    }
    ServiceTaskObject = (LOS_MEMORY_MANAGER_TASK_OBJECT *)Mapped;

    if (!MapBootstrapObject(State->Info.LaunchBlockPhysicalAddress, State->Info.LaunchBlockSize, &Mapped))
    {
        return 0;
    }
    LaunchBlock = (LOS_MEMORY_MANAGER_LAUNCH_BLOCK *)Mapped;

    State->KernelToServiceEndpointObject = KernelToServiceEndpointObject;
    State->ServiceToKernelEndpointObject = ServiceToKernelEndpointObject;
    State->ServiceEventsEndpointObject = ServiceEventsEndpointObject;
    State->ServiceAddressSpaceObject = ServiceAddressSpaceObject;
    State->ServiceTaskObject = ServiceTaskObject;
    State->LaunchBlock = LaunchBlock;

    LosMemoryManagerBootstrapInitializeMailboxes();
    InitializeEndpointObject(
        State->KernelToServiceEndpointObject,
        State->Info.Endpoints.KernelToService,
        LOS_MEMORY_MANAGER_ENDPOINT_ROLE_RECEIVE,
        State->Info.RequestMailboxPhysicalAddress,
        State->Info.RequestMailboxSize,
        State->Info.Endpoints.ServiceToKernel);
    InitializeEndpointObject(
        State->ServiceToKernelEndpointObject,
        State->Info.Endpoints.ServiceToKernel,
        LOS_MEMORY_MANAGER_ENDPOINT_ROLE_REPLY,
        State->Info.ResponseMailboxPhysicalAddress,
        State->Info.ResponseMailboxSize,
        State->Info.Endpoints.KernelToService);
    InitializeEndpointObject(
        State->ServiceEventsEndpointObject,
        State->Info.Endpoints.ServiceEvents,
        LOS_MEMORY_MANAGER_ENDPOINT_ROLE_EVENT,
        State->Info.EventMailboxPhysicalAddress,
        State->Info.EventMailboxSize,
        State->Info.Endpoints.KernelToService);
    InitializeAddressSpaceObject(State->ServiceAddressSpaceObject, State->Info.ServiceImagePhysicalAddress);
    ReportBootstrapAddressSpaceObjectCreated(
        State->ServiceAddressSpaceObject->AddressSpaceId,
        State->Info.ServiceAddressSpaceObjectPhysicalAddress);
    InitializeTaskObject(
        State->ServiceTaskObject,
        State->Info.ServiceAddressSpaceObjectPhysicalAddress,
        State->Info.ServiceEntryVirtualAddress,
        State->Info.ServiceStackPhysicalAddress + (State->Info.ServiceStackPageCount * 0x1000ULL),
        0ULL);
    ZeroMemory(State->LaunchBlock, sizeof(*State->LaunchBlock));
    State->LaunchBlock->Signature = LOS_MEMORY_MANAGER_LAUNCH_BLOCK_SIGNATURE;
    State->LaunchBlock->Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    State->LaunchBlock->State = State->Info.State;
    State->LaunchBlock->Flags = State->Info.Flags;
    State->LaunchBlock->Endpoints = State->Info.Endpoints;
    State->LaunchBlock->KernelToServiceEndpointObjectPhysicalAddress = State->Info.KernelToServiceEndpointObjectPhysicalAddress;
    State->LaunchBlock->ServiceToKernelEndpointObjectPhysicalAddress = State->Info.ServiceToKernelEndpointObjectPhysicalAddress;
    State->LaunchBlock->ServiceEventsEndpointObjectPhysicalAddress = State->Info.ServiceEventsEndpointObjectPhysicalAddress;
    State->LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress = State->Info.ServiceAddressSpaceObjectPhysicalAddress;
    State->LaunchBlock->ServiceTaskObjectPhysicalAddress = State->Info.ServiceTaskObjectPhysicalAddress;
    State->LaunchBlock->ServicePageMapLevel4PhysicalAddress = State->Info.ServicePageMapLevel4PhysicalAddress;
    State->LaunchBlock->MemoryRegionTablePhysicalAddress = State->Info.MemoryRegionTablePhysicalAddress;
    State->LaunchBlock->MemoryRegionCount = State->Info.MemoryRegionCount;
    State->LaunchBlock->MemoryRegionEntrySize = State->Info.MemoryRegionEntrySize;
    State->LaunchBlock->RequestMailboxPhysicalAddress = State->Info.RequestMailboxPhysicalAddress;
    State->LaunchBlock->RequestMailboxSize = State->Info.RequestMailboxSize;
    State->LaunchBlock->ResponseMailboxPhysicalAddress = State->Info.ResponseMailboxPhysicalAddress;
    State->LaunchBlock->ResponseMailboxSize = State->Info.ResponseMailboxSize;
    State->LaunchBlock->EventMailboxPhysicalAddress = State->Info.EventMailboxPhysicalAddress;
    State->LaunchBlock->EventMailboxSize = State->Info.EventMailboxSize;
    State->LaunchBlock->LaunchBlockPhysicalAddress = State->Info.LaunchBlockPhysicalAddress;
    State->LaunchBlock->ServiceStackPhysicalAddress = State->Info.ServiceStackPhysicalAddress;
    State->LaunchBlock->ServiceStackPageCount = State->Info.ServiceStackPageCount;
    State->LaunchBlock->ServiceStackTopPhysicalAddress = State->Info.ServiceStackPhysicalAddress + (State->Info.ServiceStackPageCount * 0x1000ULL);
    State->LaunchBlock->ServiceStackTopVirtualAddress = State->ServiceTaskObject->StackTopVirtualAddress;
    State->LaunchBlock->ServiceImagePhysicalAddress = State->Info.ServiceImagePhysicalAddress;
    State->LaunchBlock->ServiceImageSize = State->Info.ServiceImageSize;
    State->LaunchBlock->ServiceEntryVirtualAddress = State->Info.ServiceEntryVirtualAddress;
    CopyUtf16(State->LaunchBlock->ServicePath, LOS_MEMORY_MANAGER_SERVICE_PATH_CHARACTERS, State->Info.ServicePath);

    LosMemoryManagerBootstrapSetFlag(LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_TRANSPORT_READY);
    LosMemoryManagerBootstrapSetFlag(LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_LAUNCH_BLOCK_READY);
    LosMemoryManagerBootstrapSetFlag(LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_STACK_READY);
    LosMemoryManagerBootstrapTransitionTo(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_STAGED);
    return 1;
}
