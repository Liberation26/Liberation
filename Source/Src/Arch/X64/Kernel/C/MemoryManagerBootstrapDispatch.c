#include "MemoryManagerBootstrapInternal.h"

static const char *OperationName(UINT32 Operation)
{
    switch (Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH:
            return "BootstrapAttach";
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES:
            return "AllocateFrames";
        case LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES:
            return "FreeFrames";
        case LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE:
            return "CreateAddressSpace";
        case LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE:
            return "DestroyAddressSpace";
        case LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES:
            return "MapPages";
        case LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES:
            return "UnmapPages";
        case LOS_MEMORY_MANAGER_OPERATION_PROTECT_PAGES:
            return "ProtectPages";
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MAPPING:
            return "QueryMapping";
        case LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE:
            return "AttachStagedImage";
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK:
            return "AllocateAddressSpaceStack";
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
            return "QueryMemoryRegions";
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
            return "ReserveFrames";
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            return "ClaimFrames";
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
    if (Request->Operation == LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)
    {
        LosKernelSerialWriteText(" root=");
        LosKernelSerialWriteHex64(Request->Payload.BootstrapAttach.ServicePageMapLevel4PhysicalAddress);
        LosKernelSerialWriteText(" image=");
        LosKernelSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress);
        LosKernelSerialWriteText(" image-bytes=");
        LosKernelSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceImageSize);
        LosKernelSerialWriteText(" entry=");
        LosKernelSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceEntryVirtualAddress);
        LosKernelSerialWriteText(" stack-top=");
        LosKernelSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceStackTopVirtualAddress);
    }
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
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES:
        {
            LOS_X64_CLAIM_FRAMES_REQUEST ClaimRequest;
            LOS_X64_CLAIM_FRAMES_RESULT ClaimResult;

            ZeroMemory(&ClaimRequest, sizeof(ClaimRequest));
            ClaimRequest.DesiredPhysicalAddress = Slot->Message.Payload.AllocateFrames.DesiredPhysicalAddress;
            ClaimRequest.MinimumPhysicalAddress = Slot->Message.Payload.AllocateFrames.MinimumPhysicalAddress;
            ClaimRequest.MaximumPhysicalAddress = Slot->Message.Payload.AllocateFrames.MaximumPhysicalAddress;
            ClaimRequest.AlignmentBytes = Slot->Message.Payload.AllocateFrames.AlignmentBytes;
            ClaimRequest.PageCount = Slot->Message.Payload.AllocateFrames.PageCount;
            ClaimRequest.Flags = Slot->Message.Payload.AllocateFrames.Flags;
            ClaimRequest.Owner = Slot->Message.Payload.AllocateFrames.Owner;
            LosX64ClaimFrames(&ClaimRequest, &ClaimResult);
            Response->Payload.AllocateFrames.Status = ClaimResult.Status;
            Response->Payload.AllocateFrames.BaseAddress = ClaimResult.BaseAddress;
            Response->Payload.AllocateFrames.PageCount = ClaimResult.PageCount;
            Response->Status = Response->Payload.AllocateFrames.Status;
            break;
        }
        case LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.FreeFrames.Status = Response->Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES:
        {
            LOS_X64_MAP_PAGES_REQUEST MapRequest;
            LOS_X64_MAP_PAGES_RESULT MapResult;

            ZeroMemory(&MapRequest, sizeof(MapRequest));
            MapRequest.PageMapLevel4PhysicalAddress = Slot->Message.Payload.MapPages.AddressSpaceObjectPhysicalAddress;
            MapRequest.VirtualAddress = Slot->Message.Payload.MapPages.VirtualAddress;
            MapRequest.PhysicalAddress = Slot->Message.Payload.MapPages.PhysicalAddress;
            MapRequest.PageCount = Slot->Message.Payload.MapPages.PageCount;
            MapRequest.PageFlags = Slot->Message.Payload.MapPages.PageFlags;
            MapRequest.Flags = Slot->Message.Payload.MapPages.Flags;
            LosX64MapPages(&MapRequest, &MapResult);
            Response->Payload.MapPages.Status = MapResult.Status;
            Response->Payload.MapPages.AddressSpaceObjectPhysicalAddress = Slot->Message.Payload.MapPages.AddressSpaceObjectPhysicalAddress;
            Response->Payload.MapPages.PagesProcessed = MapResult.PagesProcessed;
            Response->Payload.MapPages.LastVirtualAddress = MapResult.LastVirtualAddress;
            Response->Status = Response->Payload.MapPages.Status;
            break;
        }
        case LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES:
        {
            LOS_X64_UNMAP_PAGES_REQUEST UnmapRequest;
            LOS_X64_UNMAP_PAGES_RESULT UnmapResult;

            ZeroMemory(&UnmapRequest, sizeof(UnmapRequest));
            UnmapRequest.PageMapLevel4PhysicalAddress = Slot->Message.Payload.UnmapPages.AddressSpaceObjectPhysicalAddress;
            UnmapRequest.VirtualAddress = Slot->Message.Payload.UnmapPages.VirtualAddress;
            UnmapRequest.PageCount = Slot->Message.Payload.UnmapPages.PageCount;
            UnmapRequest.Flags = Slot->Message.Payload.UnmapPages.Flags;
            LosX64UnmapPages(&UnmapRequest, &UnmapResult);
            Response->Payload.UnmapPages.Status = UnmapResult.Status;
            Response->Payload.UnmapPages.AddressSpaceObjectPhysicalAddress = Slot->Message.Payload.UnmapPages.AddressSpaceObjectPhysicalAddress;
            Response->Payload.UnmapPages.PagesProcessed = UnmapResult.PagesProcessed;
            Response->Payload.UnmapPages.LastVirtualAddress = UnmapResult.LastVirtualAddress;
            Response->Status = Response->Payload.UnmapPages.Status;
            break;
        }
        case LOS_MEMORY_MANAGER_OPERATION_PROTECT_PAGES:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.ProtectPages.Status = Response->Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MAPPING:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.QueryMapping.Status = Response->Status;
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
        case LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.CreateAddressSpace.Status = Response->Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.DestroyAddressSpace.Status = Response->Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.AttachStagedImage.Status = Response->Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.AllocateAddressSpaceStack.Status = Response->Status;
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
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MAPPING:
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
        if (State->Info.ServiceImageSize != 0ULL && State->LaunchBlock != 0 && State->LaunchBlock->ServiceImageSize == 0ULL)
        {
            State->LaunchBlock->ServiceImageSize = State->Info.ServiceImageSize;
        }
    }

    Request->Payload.BootstrapAttach.ServicePageMapLevel4PhysicalAddress = ServiceRootPhysicalAddress;
    if (State != 0 && State->ServiceAddressSpaceObject != 0 && State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress != 0ULL)
    {
        Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress = State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress;
    }
    if (State != 0 && State->Info.ServiceImageSize != 0ULL)
    {
        Request->Payload.BootstrapAttach.ServiceImageSize = State->Info.ServiceImageSize;
    }
    Request->Payload.BootstrapAttach.ServiceEntryVirtualAddress = ServiceEntryVirtualAddress;
    Request->Payload.BootstrapAttach.ServiceStackTopVirtualAddress = ServiceStackTopVirtualAddress;
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

void LosMemoryManagerSendAllocateFrames(const LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_REQUEST *RequestData, LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.AllocateFrames, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.AllocateFrames, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendClaimFrames(const LOS_X64_CLAIM_FRAMES_REQUEST *RequestData, LOS_X64_CLAIM_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_REQUEST AllocateRequest;
    LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_RESULT AllocateResult;

    ZeroMemory(&AllocateRequest, sizeof(AllocateRequest));
    ZeroMemory(&AllocateResult, sizeof(AllocateResult));
    if (RequestData != 0)
    {
        AllocateRequest.DesiredPhysicalAddress = RequestData->DesiredPhysicalAddress;
        AllocateRequest.MinimumPhysicalAddress = RequestData->MinimumPhysicalAddress;
        AllocateRequest.MaximumPhysicalAddress = RequestData->MaximumPhysicalAddress;
        AllocateRequest.AlignmentBytes = RequestData->AlignmentBytes;
        AllocateRequest.PageCount = RequestData->PageCount;
        AllocateRequest.Flags = RequestData->Flags;
        AllocateRequest.Owner = RequestData->Owner;
    }
    LosMemoryManagerSendAllocateFrames(&AllocateRequest, &AllocateResult);
    if (Result != 0)
    {
        Result->Status = AllocateResult.Status;
        Result->Reserved = 0U;
        Result->BaseAddress = AllocateResult.BaseAddress;
        Result->PageCount = AllocateResult.PageCount;
    }
}

void LosMemoryManagerSendFreeFrames(const LOS_X64_FREE_FRAMES_REQUEST *RequestData, LOS_X64_FREE_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.FreeFrames, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.FreeFrames, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendMapPages(const LOS_MEMORY_MANAGER_MAP_PAGES_REQUEST *RequestData, LOS_MEMORY_MANAGER_MAP_PAGES_RESULT *Result)
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

void LosMemoryManagerSendUnmapPages(const LOS_MEMORY_MANAGER_UNMAP_PAGES_REQUEST *RequestData, LOS_MEMORY_MANAGER_UNMAP_PAGES_RESULT *Result)
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

void LosMemoryManagerSendProtectPages(const LOS_MEMORY_MANAGER_PROTECT_PAGES_REQUEST *RequestData, LOS_MEMORY_MANAGER_PROTECT_PAGES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_PROTECT_PAGES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.ProtectPages, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.ProtectPages, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendQueryMapping(const LOS_MEMORY_MANAGER_QUERY_MAPPING_REQUEST *RequestData, LOS_MEMORY_MANAGER_QUERY_MAPPING_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_QUERY_MAPPING;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.QueryMapping, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.QueryMapping, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendCreateAddressSpace(const LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_REQUEST *RequestData, LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.CreateAddressSpace, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.CreateAddressSpace, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendDestroyAddressSpace(const LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_REQUEST *RequestData, LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.DestroyAddressSpace, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.DestroyAddressSpace, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendAttachStagedImage(const LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_REQUEST *RequestData, LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.AttachStagedImage, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.AttachStagedImage, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendAllocateAddressSpaceStack(const LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_REQUEST *RequestData, LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.AllocateAddressSpaceStack, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.AllocateAddressSpaceStack, sizeof(*Result));
        Result->Status = Response.Status;
    }
}
