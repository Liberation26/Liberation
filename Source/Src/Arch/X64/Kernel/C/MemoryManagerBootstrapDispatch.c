#include "MemoryManagerBootstrapInternal.h"

static const char *OperationName(UINT32 Operation)
{
    switch (Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH:
            return "BootstrapAttach";
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
            return "QueryMemoryRegions";
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
            return "ReserveFrames";
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            return "ClaimFrames";
        case LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES:
            return "MapPages";
        case LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES:
            return "UnmapPages";
        default:
            return "Unknown";
    }
}

static void TraceKernelToMemoryManagerRequest(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request)
{
    if (Request == 0)
    {
        return;
    }

    LosKernelSerialWriteText("[MemManager] Kernel -> Memory Manager request=");
    LosKernelSerialWriteText(OperationName(Request->Operation));
    LosKernelSerialWriteText(" id=");
    LosKernelSerialWriteHex64(Request->RequestId);
    LosKernelSerialWriteText("\n");
}

static void TraceMemoryManagerToKernelResponse(const LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    if (Response == 0)
    {
        return;
    }

    LosKernelSerialWriteText("[MemManager] Memory Manager -> Kernel response=");
    LosKernelSerialWriteText(OperationName(Response->Operation));
    LosKernelSerialWriteText(" id=");
    LosKernelSerialWriteHex64(Response->RequestId);
    LosKernelSerialWriteText(" status=");
    LosKernelSerialWriteUnsigned(Response->Status);
    if (Response->Operation == LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)
    {
        LosKernelSerialWriteText(" bootstrap=");
        LosKernelSerialWriteUnsigned(Response->Payload.BootstrapAttach.BootstrapResult);
    }
    LosKernelSerialWriteText("\n");
}

static void ZeroMemory(void *Buffer, UINTN ByteCount)
{
    UINT8 *Bytes;
    UINTN Index;

    if (Buffer == 0)
    {
        return;
    }

    Bytes = (UINT8 *)Buffer;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        Bytes[Index] = 0U;
    }
}

static void CopyBytes(void *Destination, const void *Source, UINTN ByteCount)
{
    UINT8 *DestinationBytes;
    const UINT8 *SourceBytes;
    UINTN Index;

    if (Destination == 0 || Source == 0)
    {
        return;
    }

    DestinationBytes = (UINT8 *)Destination;
    SourceBytes = (const UINT8 *)Source;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        DestinationBytes[Index] = SourceBytes[Index];
    }
}

static BOOLEAN EndpointReady(const LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *Endpoint, UINT64 EndpointId, UINT32 Role)
{
    if (Endpoint == 0)
    {
        return 0;
    }

    return Endpoint->EndpointId == EndpointId &&
           Endpoint->Role == Role &&
           Endpoint->State >= LOS_MEMORY_MANAGER_ENDPOINT_STATE_READY &&
           (Endpoint->Flags & LOS_MEMORY_MANAGER_ENDPOINT_FLAG_MAILBOX_ATTACHED) != 0ULL;
}

static void InitializeResponse(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    if (Response == 0)
    {
        return;
    }

    ZeroMemory(Response, sizeof(*Response));
    if (Request != 0)
    {
        Response->Operation = Request->Operation;
        Response->RequestId = Request->RequestId;
    }
}

static BOOLEAN PostResponse(const LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *Mailbox;
    UINT64 Index;
    LOS_MEMORY_MANAGER_RESPONSE_SLOT *Slot;

    Mailbox = LosMemoryManagerBootstrapGetResponseMailbox();
    if (!EndpointReady(LosMemoryManagerBootstrapGetServiceToKernelEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_SERVICE_TO_KERNEL, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_REPLY) ||
        Mailbox == 0 || Response == 0)
    {
        return 0;
    }

    Index = Mailbox->Header.ProduceIndex % Mailbox->Header.SlotCount;
    Slot = &Mailbox->Slots[Index];
    if (Slot->SlotState == LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    CopyBytes(&Slot->Message, Response, sizeof(*Response));
    Slot->Sequence = Response->RequestId;
    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY;
    Mailbox->Header.ProduceIndex += 1ULL;
    return 1;
}

BOOLEAN LosMemoryManagerBootstrapEnqueueRequest(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request)
{
    LOS_MEMORY_MANAGER_REQUEST_MAILBOX *Mailbox;
    UINT64 Index;
    LOS_MEMORY_MANAGER_REQUEST_SLOT *Slot;

    Mailbox = LosMemoryManagerBootstrapGetRequestMailbox();
    if (!EndpointReady(LosMemoryManagerBootstrapGetKernelToServiceEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_KERNEL_TO_SERVICE, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_RECEIVE) ||
        Mailbox == 0 || Request == 0)
    {
        return 0;
    }

    Index = Mailbox->Header.ProduceIndex % Mailbox->Header.SlotCount;
    Slot = &Mailbox->Slots[Index];
    if (Slot->SlotState == LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    CopyBytes(&Slot->Message, Request, sizeof(*Request));
    Slot->Sequence = Request->RequestId;
    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY;
    Mailbox->Header.ProduceIndex += 1ULL;
    return 1;
}


BOOLEAN LosMemoryManagerBootstrapDequeueResponse(UINT64 RequestId, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *Mailbox;
    UINT64 Scanned;

    Mailbox = LosMemoryManagerBootstrapGetResponseMailbox();
    if (!EndpointReady(LosMemoryManagerBootstrapGetServiceToKernelEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_SERVICE_TO_KERNEL, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_REPLY) ||
        Mailbox == 0 || Response == 0)
    {
        return 0;
    }

    for (Scanned = 0ULL; Scanned < Mailbox->Header.SlotCount; ++Scanned)
    {
        UINT64 Index;
        LOS_MEMORY_MANAGER_RESPONSE_SLOT *Slot;

        Index = (Mailbox->Header.ConsumeIndex + Scanned) % Mailbox->Header.SlotCount;
        Slot = &Mailbox->Slots[Index];
        if (Slot->SlotState != LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
        {
            continue;
        }
        if (Slot->Sequence != RequestId)
        {
            continue;
        }

        CopyBytes(Response, &Slot->Message, sizeof(*Response));
        Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
        Slot->Sequence = 0ULL;
        Mailbox->Header.ConsumeIndex = Index + 1ULL;
        return 1;
    }

    return 0;
}

static BOOLEAN HostedServiceTaskReady(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    return State->ServiceTaskObject != 0 &&
           State->ServiceAddressSpaceObject != 0 &&
           State->ServiceTaskObject->State >= LOS_MEMORY_MANAGER_TASK_STATE_READY &&
           State->ServiceAddressSpaceObject->State >= LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_READY;
}

BOOLEAN LosMemoryManagerBootstrapHostedServiceStep(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    LOS_MEMORY_MANAGER_REQUEST_MAILBOX *RequestMailbox;
    LOS_MEMORY_MANAGER_REQUEST_SLOT *Slot;

    State = LosMemoryManagerBootstrapState();
    if (!HostedServiceTaskReady())
    {
        return 0;
    }

    RequestMailbox = LosMemoryManagerBootstrapGetRequestMailbox();
    if (RequestMailbox == 0)
    {
        return 0;
    }

    Slot = &RequestMailbox->Slots[(RequestMailbox->Header.ConsumeIndex) % RequestMailbox->Header.SlotCount];
    if (Slot->SlotState != LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    State->ServiceTaskObject->LastRequestId = 0ULL;
    State->ServiceTaskObject->Heartbeat = 0ULL;
    State->KernelToServiceEndpointObject->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->ServiceToKernelEndpointObject->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->ServiceEventsEndpointObject->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;

    if (!LosMemoryManagerBootstrapInvokeServiceEntry())
    {
        return 0;
    }

    LosMemoryManagerBootstrapTransitionTo(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_SERVICE_ONLINE);
    State->Info.Flags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_ONLINE;
    return 1;
}

void LosMemoryManagerBootstrapDispatch(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    LOS_MEMORY_MANAGER_REQUEST_MAILBOX *RequestMailbox;
    LOS_MEMORY_MANAGER_REQUEST_SLOT *Slot;

    InitializeResponse(Request, Response);

    if (Request == 0 || Response == 0)
    {
        return;
    }

    RequestMailbox = LosMemoryManagerBootstrapGetRequestMailbox();
    if (!LosMemoryManagerBootstrapEndpointsReady() || RequestMailbox == 0)
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    Slot = &RequestMailbox->Slots[(RequestMailbox->Header.ConsumeIndex) % RequestMailbox->Header.SlotCount];
    if (Slot->SlotState != LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        return;
    }

    if (!LosMemoryManagerBootstrapOperationSupported(Slot->Message.Operation))
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
        Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
        Slot->Sequence = 0ULL;
        RequestMailbox->Header.ConsumeIndex += 1ULL;
        LosMemoryManagerBootstrapRecordCompletion(Slot->Message.Operation, Response->Status);
        PostResponse(Response);
        return;
    }

    switch (Slot->Message.Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.BootstrapAttach.BootstrapResult = LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_INVALID_REQUEST;
            Response->Payload.BootstrapAttach.BootstrapState = LosGetMemoryManagerBootstrapInfo()->State;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
            LosX64QueryMemoryRegions(
                Slot->Message.Payload.QueryMemoryRegions.Buffer,
                Slot->Message.Payload.QueryMemoryRegions.BufferRegionCapacity,
                &Response->Payload.QueryMemoryRegions);
            Response->Status = Response->Payload.QueryMemoryRegions.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
            LosX64ReserveFrames(&Slot->Message.Payload.ReserveFrames, &Response->Payload.ReserveFrames);
            Response->Status = Response->Payload.ReserveFrames.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            LosX64ClaimFrames(&Slot->Message.Payload.ClaimFrames, &Response->Payload.ClaimFrames);
            Response->Status = Response->Payload.ClaimFrames.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES:
            LosX64MapPages(&Slot->Message.Payload.MapPages, &Response->Payload.MapPages);
            Response->Status = Response->Payload.MapPages.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES:
            LosX64UnmapPages(&Slot->Message.Payload.UnmapPages, &Response->Payload.UnmapPages);
            Response->Status = Response->Payload.UnmapPages.Status;
            break;
        default:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            break;
    }

    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_COMPLETE;
    RequestMailbox->Header.ConsumeIndex += 1ULL;
    LosMemoryManagerBootstrapRecordCompletion(Slot->Message.Operation, Response->Status);
    PostResponse(Response);
    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
    Slot->Sequence = 0ULL;
}

static BOOLEAN RequestRequiresRealServiceReply(UINT32 Operation)
{
    return Operation == LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH;
}

static UINT32 HostedServiceStepBudgetForOperation(UINT32 Operation)
{
    return RequestRequiresRealServiceReply(Operation) ? 8U : 1U;
}

static void PopulateBootstrapAttachRequest(LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request)
{
    const LOS_MEMORY_MANAGER_BOOTSTRAP_INFO *Info;
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;

    if (Request == 0)
    {
        return;
    }

    Info = LosGetMemoryManagerBootstrapInfo();
    LaunchBlock = LosGetMemoryManagerLaunchBlock();
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
    Request->Payload.BootstrapAttach.ServicePageMapLevel4PhysicalAddress = Info->ServicePageMapLevel4PhysicalAddress;
    Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress = Info->ServiceImagePhysicalAddress;
    Request->Payload.BootstrapAttach.ServiceImageSize = Info->ServiceImageSize;
    Request->Payload.BootstrapAttach.ServiceEntryVirtualAddress = Info->ServiceEntryVirtualAddress;
    Request->Payload.BootstrapAttach.ServiceStackTopVirtualAddress = 0ULL;

    if (LaunchBlock != 0)
    {
        Request->Payload.BootstrapAttach.KernelToServiceEndpointId = LaunchBlock->Endpoints.KernelToService;
        Request->Payload.BootstrapAttach.ServiceToKernelEndpointId = LaunchBlock->Endpoints.ServiceToKernel;
        Request->Payload.BootstrapAttach.ServiceEventsEndpointId = LaunchBlock->Endpoints.ServiceEvents;
        Request->Payload.BootstrapAttach.RequestMailboxPhysicalAddress = LaunchBlock->RequestMailboxPhysicalAddress;
        Request->Payload.BootstrapAttach.ResponseMailboxPhysicalAddress = LaunchBlock->ResponseMailboxPhysicalAddress;
        Request->Payload.BootstrapAttach.EventMailboxPhysicalAddress = LaunchBlock->EventMailboxPhysicalAddress;
        Request->Payload.BootstrapAttach.ServicePageMapLevel4PhysicalAddress = LaunchBlock->ServicePageMapLevel4PhysicalAddress;
        Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress = LaunchBlock->ServiceImagePhysicalAddress;
        Request->Payload.BootstrapAttach.ServiceImageSize = LaunchBlock->ServiceImageSize;
        Request->Payload.BootstrapAttach.ServiceEntryVirtualAddress = LaunchBlock->ServiceEntryVirtualAddress;
        Request->Payload.BootstrapAttach.ServiceStackTopVirtualAddress = LaunchBlock->ServiceStackTopVirtualAddress;
    }
}

static void SendRequestAndAwaitResponse(LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    BOOLEAN ResponsePublishedByService;
    UINT32 Attempt;
    UINT32 AttemptBudget;

    InitializeResponse(Request, Response);

    TraceKernelToMemoryManagerRequest(Request);
    if (!LosMemoryManagerBootstrapEnqueueRequest(Request))
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap failed to enqueue a request.");
    }

    ResponsePublishedByService = 0;
    AttemptBudget = HostedServiceStepBudgetForOperation(Request->Operation);
    for (Attempt = 0U; Attempt < AttemptBudget; ++Attempt)
    {
        if (!LosMemoryManagerBootstrapHostedServiceStep())
        {
            if (Attempt == 0U)
            {
                Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
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
        if (RequestRequiresRealServiceReply(Request->Operation))
        {
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
            LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap expected a real service reply and none was published.");
        }

        LosMemoryManagerBootstrapDispatch(Request, Response);
        if (!LosMemoryManagerBootstrapDequeueResponse(Request->RequestId, Response))
        {
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
            LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap failed to dequeue a response.");
        }
    }

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

void LosMemoryManagerSendReserveFrames(const LOS_X64_RESERVE_FRAMES_REQUEST *RequestData, LOS_X64_RESERVE_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.ReserveFrames, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.ReserveFrames, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendClaimFrames(const LOS_X64_CLAIM_FRAMES_REQUEST *RequestData, LOS_X64_CLAIM_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.ClaimFrames, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.ClaimFrames, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendMapPages(const LOS_X64_MAP_PAGES_REQUEST *RequestData, LOS_X64_MAP_PAGES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.MapPages, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.MapPages, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendUnmapPages(const LOS_X64_UNMAP_PAGES_REQUEST *RequestData, LOS_X64_UNMAP_PAGES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.UnmapPages, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.UnmapPages, sizeof(*Result));
        Result->Status = Response.Status;
    }
}
