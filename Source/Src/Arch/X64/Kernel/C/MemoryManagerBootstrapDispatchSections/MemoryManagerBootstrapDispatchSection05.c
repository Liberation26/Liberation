/*
 * File Name: MemoryManagerBootstrapDispatchSection05.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapDispatch.c.
 */

static BOOLEAN RequestRequiresRealServiceReply(UINT32 Operation)
{
    switch (Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH:
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE:
        case LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE:
        case LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES:
        case LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES:
        case LOS_MEMORY_MANAGER_OPERATION_PROTECT_PAGES:
        case LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE:
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK:
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            return 1;
        default:
            return 0;
    }
}

static BOOLEAN RequestMayUseBootstrapFallback(UINT32 Operation)
{
    (void)Operation;
    return 0;
}

static UINT32 HostedServiceStepBudgetForOperation(UINT32 Operation)
{
    return RequestRequiresRealServiceReply(Operation) ? 8U : 1U;
}

static void PopulateBootstrapAttachRequest(LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    const LOS_MEMORY_MANAGER_BOOTSTRAP_INFO *Info;
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    UINT64 ServiceRootPhysicalAddress;
    UINT64 ServiceEntryVirtualAddress;
    UINT64 ServiceStackTopVirtualAddress;

    if (Request == 0)
    {
        return;
    }

    State = LosMemoryManagerBootstrapState();
    Info = LosGetMemoryManagerBootstrapInfo();
    LaunchBlock = LosGetMemoryManagerLaunchBlock();
    ServiceRootPhysicalAddress = 0ULL;
    ServiceEntryVirtualAddress = 0ULL;
    ServiceStackTopVirtualAddress = 0ULL;
    Request->Operation = LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH;
    Request->Payload.BootstrapAttach.Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    Request->Payload.BootstrapAttach.LaunchBlockPhysicalAddress = Info->LaunchBlockPhysicalAddress;
    Request->Payload.BootstrapAttach.OfferedOperations = Info->SupportedOperations;
    Request->Payload.BootstrapAttach.BootstrapFlags = Info->Flags;
    Request->Payload.BootstrapAttach.KernelToServiceEndpointId = Info->Endpoints.KernelToService;
    Request->Payload.BootstrapAttach.ServiceToKernelEndpointId = Info->Endpoints.ServiceToKernel;
    Request->Payload.BootstrapAttach.ServiceEventsEndpointId = Info->Endpoints.ServiceEvents;
    Request->Payload.BootstrapAttach.RequestMailboxPhysicalAddress = Info->RequestMailboxPhysicalAddress;
    Request->Payload.BootstrapAttach.ResponseMailboxPhysicalAddress = Info->ResponseMailboxPhysicalAddress;
    Request->Payload.BootstrapAttach.EventMailboxPhysicalAddress = Info->EventMailboxPhysicalAddress;
    Request->Payload.BootstrapAttach.MemoryRegionTablePhysicalAddress = Info->MemoryRegionTablePhysicalAddress;
    Request->Payload.BootstrapAttach.MemoryRegionCount = Info->MemoryRegionCount;
    Request->Payload.BootstrapAttach.MemoryRegionEntrySize = Info->MemoryRegionEntrySize;
    ServiceRootPhysicalAddress = Info->ServicePageMapLevel4PhysicalAddress;
    Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress = Info->ServiceImagePhysicalAddress;
    Request->Payload.BootstrapAttach.ServiceImageSize = Info->ServiceImageSize;
    ServiceEntryVirtualAddress = Info->ServiceEntryVirtualAddress;

    if (LaunchBlock != 0)
    {
        Request->Payload.BootstrapAttach.LaunchBlockPhysicalAddress = LaunchBlock->LaunchBlockPhysicalAddress;
        Request->Payload.BootstrapAttach.KernelToServiceEndpointId = LaunchBlock->Endpoints.KernelToService;
        Request->Payload.BootstrapAttach.ServiceToKernelEndpointId = LaunchBlock->Endpoints.ServiceToKernel;
        Request->Payload.BootstrapAttach.ServiceEventsEndpointId = LaunchBlock->Endpoints.ServiceEvents;
        Request->Payload.BootstrapAttach.RequestMailboxPhysicalAddress = LaunchBlock->RequestMailboxPhysicalAddress;
        Request->Payload.BootstrapAttach.ResponseMailboxPhysicalAddress = LaunchBlock->ResponseMailboxPhysicalAddress;
        Request->Payload.BootstrapAttach.EventMailboxPhysicalAddress = LaunchBlock->EventMailboxPhysicalAddress;
        Request->Payload.BootstrapAttach.MemoryRegionTablePhysicalAddress = LaunchBlock->MemoryRegionTablePhysicalAddress;
        Request->Payload.BootstrapAttach.MemoryRegionCount = LaunchBlock->MemoryRegionCount;
        Request->Payload.BootstrapAttach.MemoryRegionEntrySize = LaunchBlock->MemoryRegionEntrySize;
        if (LaunchBlock->ServicePageMapLevel4PhysicalAddress != 0ULL)
        {
            ServiceRootPhysicalAddress = LaunchBlock->ServicePageMapLevel4PhysicalAddress;
        }
        Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress = LaunchBlock->ServiceImagePhysicalAddress;
        Request->Payload.BootstrapAttach.ServiceImageSize = LaunchBlock->ServiceImageSize;
        if (LaunchBlock->ServiceEntryVirtualAddress != 0ULL)
        {
            ServiceEntryVirtualAddress = LaunchBlock->ServiceEntryVirtualAddress;
        }
        if (LaunchBlock->ServiceStackTopVirtualAddress != 0ULL)
        {
            ServiceStackTopVirtualAddress = LaunchBlock->ServiceStackTopVirtualAddress;
        }
    }

    if (State != 0)
    {
        if (State->ServiceAddressSpaceObject != 0 && State->ServiceAddressSpaceObject->RootTablePhysicalAddress != 0ULL)
        {
            ServiceRootPhysicalAddress = State->ServiceAddressSpaceObject->RootTablePhysicalAddress;
        }
        if (State->ServiceTaskObject != 0)
        {
            if (State->ServiceTaskObject->EntryVirtualAddress != 0ULL)
            {
                ServiceEntryVirtualAddress = State->ServiceTaskObject->EntryVirtualAddress;
            }
            if (State->ServiceTaskObject->StackTopVirtualAddress != 0ULL)
            {
                ServiceStackTopVirtualAddress = State->ServiceTaskObject->StackTopVirtualAddress;
            }
        }

        if (ServiceRootPhysicalAddress != 0ULL)
        {
            State->Info.ServicePageMapLevel4PhysicalAddress = ServiceRootPhysicalAddress;
            if (State->LaunchBlock != 0)
            {
                State->LaunchBlock->ServicePageMapLevel4PhysicalAddress = ServiceRootPhysicalAddress;
            }
            if (State->ServiceAddressSpaceObject != 0)
            {
                State->ServiceAddressSpaceObject->RootTablePhysicalAddress = ServiceRootPhysicalAddress;
            }
        }
        if (ServiceEntryVirtualAddress != 0ULL)
        {
            State->Info.ServiceEntryVirtualAddress = ServiceEntryVirtualAddress;
            if (State->LaunchBlock != 0)
            {
                State->LaunchBlock->ServiceEntryVirtualAddress = ServiceEntryVirtualAddress;
            }
            if (State->ServiceTaskObject != 0)
            {
                State->ServiceTaskObject->EntryVirtualAddress = ServiceEntryVirtualAddress;
            }
        }
        if (ServiceStackTopVirtualAddress != 0ULL)
        {
            if (State->LaunchBlock != 0)
            {
                State->LaunchBlock->ServiceStackTopVirtualAddress = ServiceStackTopVirtualAddress;
            }
            if (State->ServiceTaskObject != 0)
            {
                State->ServiceTaskObject->StackTopVirtualAddress = ServiceStackTopVirtualAddress;
            }
        }
    }

    if (State != 0)
    {
        if (State->ServiceAddressSpaceObject != 0 && State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress != 0ULL)
        {
            State->Info.ServiceImagePhysicalAddress = State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress;
            if (State->LaunchBlock != 0)
            {
                State->LaunchBlock->ServiceImagePhysicalAddress = State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress;
            }
        }
        if (State->LaunchBlock != 0)
        {
            if (State->ServiceAddressSpaceObject != 0 && State->ServiceAddressSpaceObject->ServiceImageSize != 0ULL)
            {
                State->LaunchBlock->ServiceImageSize = State->ServiceAddressSpaceObject->ServiceImageSize;
            }
            else if (State->Info.ServiceImageSize != 0ULL && State->LaunchBlock->ServiceImageSize == 0ULL)
            {
                State->LaunchBlock->ServiceImageSize = State->Info.ServiceImageSize;
            }
        }
    }

    Request->Payload.BootstrapAttach.ServicePageMapLevel4PhysicalAddress = ServiceRootPhysicalAddress;
    if (State != 0 && State->ServiceAddressSpaceObject != 0 && State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress != 0ULL)
    {
        Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress = State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress;
    }
    if (State != 0)
    {
        if (State->ServiceAddressSpaceObject != 0 && State->ServiceAddressSpaceObject->ServiceImageSize != 0ULL)
        {
            Request->Payload.BootstrapAttach.ServiceImageSize = State->ServiceAddressSpaceObject->ServiceImageSize;
        }
        else if (State->Info.ServiceImageSize != 0ULL)
        {
            Request->Payload.BootstrapAttach.ServiceImageSize = State->Info.ServiceImageSize;
        }
    }
    Request->Payload.BootstrapAttach.ServiceEntryVirtualAddress = ServiceEntryVirtualAddress;
    Request->Payload.BootstrapAttach.ServiceStackTopVirtualAddress = ServiceStackTopVirtualAddress;
}

static void SendRequestAndAwaitResponse(LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    BOOLEAN ResponsePublishedByService;
    UINT32 Attempt;
    UINT32 AttemptBudget;
    UINT64 InterruptFlags;

    InitializeResponse(Request, Response);
    PopulateCallerIdentity(Request);

    TraceKernelToMemoryManagerRequest(Request);
    InterruptFlags = SaveInterruptFlagsAndDisable();
    if (!LosMemoryManagerBootstrapEnqueueRequest(Request))
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        RestoreInterruptFlags(InterruptFlags);
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap failed to enqueue a request.");
    }

    ResponsePublishedByService = 0;
    AttemptBudget = HostedServiceStepBudgetForOperation(Request->Operation);
    for (Attempt = 0U; Attempt < AttemptBudget; ++Attempt)
    {
        if (!LosMemoryManagerBootstrapHostedServiceStep())
        {
            if (Attempt == 0U && !RequestMayUseBootstrapFallback(Request->Operation) &&
                RequestRequiresRealServiceReply(Request->Operation))
            {
                Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
                RestoreInterruptFlags(InterruptFlags);
                LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager hosted service step failed.");
            }

            continue;
        }

        ResponsePublishedByService = LosMemoryManagerBootstrapDequeueResponse(Request->RequestId, Response);
        if (ResponsePublishedByService)
        {
            break;
        }
    }

    if (!ResponsePublishedByService)
    {
        if (!RequestMayUseBootstrapFallback(Request->Operation) && RequestRequiresRealServiceReply(Request->Operation))
        {
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
            RestoreInterruptFlags(InterruptFlags);
            LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap expected a real service reply and none was published.");
        }

        LosMemoryManagerBootstrapDispatch(Request, Response);
        if (!LosMemoryManagerBootstrapDequeueResponse(Request->RequestId, Response))
        {
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
            RestoreInterruptFlags(InterruptFlags);
            LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap failed to dequeue a response.");
        }
    }

    RestoreInterruptFlags(InterruptFlags);
    LosMemoryManagerBootstrapRecordCompletion(Response->Operation, Response->Status);
    TraceMemoryManagerToKernelResponse(Response);
}

void LosMemoryManagerBootstrapPublishLaunchReady(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    if (State->LaunchBlock == 0 || State->Info.ServiceEntryVirtualAddress == 0ULL)
    {
        return;
    }

    State->LaunchBlock->Flags = State->Info.Flags | LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_ENTRY_PUBLISHED | LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_LAUNCH_PREPARED;
    State->LaunchBlock->ServiceEntryVirtualAddress = State->Info.ServiceEntryVirtualAddress;
    LosMemoryManagerBootstrapSetEndpointState(LosMemoryManagerBootstrapGetKernelToServiceEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_STATE_BOUND);
    LosMemoryManagerBootstrapSetEndpointState(LosMemoryManagerBootstrapGetServiceToKernelEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_STATE_BOUND);
    LosMemoryManagerBootstrapSetEndpointState(LosMemoryManagerBootstrapGetServiceEventsEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_STATE_BOUND);
    State->Info.Flags = State->LaunchBlock->Flags;
    LosMemoryManagerBootstrapTransitionTo(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_SERVICE_PREPARED);
}

void LosMemoryManagerSendBootstrapAttach(LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT *Result)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    State = LosMemoryManagerBootstrapState();
    if (State->BootstrapAttachRequestId != 0ULL)
    {
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap attach request was attempted more than once.");
    }

    ZeroMemory(&Request, sizeof(Request));
    PopulateBootstrapAttachRequest(&Request);
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    State->BootstrapAttachRequestId = Request.RequestId;
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    LosMemoryManagerBootstrapRecordBootstrapResult(Response.Payload.BootstrapAttach.BootstrapResult);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.BootstrapAttach, sizeof(*Result));
    }
}

void LosMemoryManagerSendQueryMemoryRegions(LOS_X64_MEMORY_REGION *Buffer, UINTN BufferRegionCapacity, LOS_X64_QUERY_MEMORY_REGIONS_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    Request.Payload.QueryMemoryRegions.Buffer = Buffer;
    Request.Payload.QueryMemoryRegions.BufferRegionCapacity = BufferRegionCapacity;
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.QueryMemoryRegions, sizeof(*Result));
        Result->Status = Response.Status;
    }
}
