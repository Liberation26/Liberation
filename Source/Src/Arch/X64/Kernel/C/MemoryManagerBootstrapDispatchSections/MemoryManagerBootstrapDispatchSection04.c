/*
 * File Name: MemoryManagerBootstrapDispatchSection04.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapDispatch.c.
 */

static void DispatchLocalAllocateAddressSpaceStack(
    const LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_REQUEST *Request,
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result,
    UINT32 *Status)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
    UINT64 StackBaseVirtualAddress;
    UINT64 StackPhysicalAddress;

    if (Status != 0)
    {
        *Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    }
    if (Result == 0)
    {
        return;
    }

    ZeroMemory(Result, sizeof(*Result));
    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    if (Request == 0 ||
        Request->AddressSpaceObjectPhysicalAddress == 0ULL ||
        Request->PageCount == 0ULL)
    {
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    Result->AddressSpaceObjectPhysicalAddress = Request->AddressSpaceObjectPhysicalAddress;
    AddressSpaceObject = ResolveAddressSpaceObjectLocal(Request->AddressSpaceObjectPhysicalAddress);
    if (AddressSpaceObject == 0)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }
    if ((AddressSpaceObject->Flags & LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK) != 0ULL)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    StackBaseVirtualAddress = 0ULL;
    if (!SelectStackBaseVirtualAddressLocal(
            AddressSpaceObject,
            Request->DesiredStackBaseVirtualAddress,
            Request->PageCount,
            &StackBaseVirtualAddress))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    StackPhysicalAddress = 0ULL;
    if (!ClaimContiguousPagesLocal(Request->PageCount, &StackPhysicalAddress) ||
        !MapPagesIntoAddressSpaceLocal(
            AddressSpaceObject->RootTablePhysicalAddress,
            StackBaseVirtualAddress,
            StackPhysicalAddress,
            Request->PageCount,
            LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_USER | LOS_X64_PAGE_NX) ||
        !ReserveVirtualRegionLocal(
            AddressSpaceObject,
            StackBaseVirtualAddress,
            Request->PageCount,
            LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_STACK,
            0U,
            StackPhysicalAddress))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    AddressSpaceObject->StackPhysicalAddress = StackPhysicalAddress;
    AddressSpaceObject->StackPageCount = Request->PageCount;
    AddressSpaceObject->StackBaseVirtualAddress = StackBaseVirtualAddress;
    AddressSpaceObject->StackTopVirtualAddress = StackBaseVirtualAddress + (Request->PageCount * 0x1000ULL);
    AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK;

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->StackPhysicalAddress = StackPhysicalAddress;
    Result->StackPageCount = Request->PageCount;
    Result->StackBaseVirtualAddress = StackBaseVirtualAddress;
    Result->StackTopVirtualAddress = AddressSpaceObject->StackTopVirtualAddress;
    if (Status != 0)
    {
        *Status = Result->Status;
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
    UINT32 TaskState;

    State = LosMemoryManagerBootstrapState();
    TaskState = State->ServiceTaskObject != 0 ? State->ServiceTaskObject->State : LOS_MEMORY_MANAGER_TASK_STATE_OFFLINE;
    return State->ServiceTaskObject != 0 &&
           State->ServiceAddressSpaceObject != 0 &&
           TaskState >= LOS_MEMORY_MANAGER_TASK_STATE_READY &&
           TaskState != LOS_MEMORY_MANAGER_TASK_STATE_RUNNING &&
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

    if (Request == 0 || Response == 0)
    {
        return;
    }

    InitializeResponse(Request, Response);

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
            LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
            LOS_X64_MAP_PAGES_REQUEST MapRequest;
            LOS_X64_MAP_PAGES_RESULT MapResult;

            ZeroMemory(&MapRequest, sizeof(MapRequest));
            AddressSpaceObject = ResolveAddressSpaceObjectLocal(Slot->Message.Payload.MapPages.AddressSpaceObjectPhysicalAddress);
            if (AddressSpaceObject == 0)
            {
                Response->Payload.MapPages.Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
                Response->Payload.MapPages.AddressSpaceObjectPhysicalAddress = Slot->Message.Payload.MapPages.AddressSpaceObjectPhysicalAddress;
                Response->Status = Response->Payload.MapPages.Status;
                break;
            }

            MapRequest.PageMapLevel4PhysicalAddress = AddressSpaceObject->RootTablePhysicalAddress;
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
            LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
            LOS_X64_UNMAP_PAGES_REQUEST UnmapRequest;
            LOS_X64_UNMAP_PAGES_RESULT UnmapResult;

            ZeroMemory(&UnmapRequest, sizeof(UnmapRequest));
            AddressSpaceObject = ResolveAddressSpaceObjectLocal(Slot->Message.Payload.UnmapPages.AddressSpaceObjectPhysicalAddress);
            if (AddressSpaceObject == 0)
            {
                Response->Payload.UnmapPages.Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
                Response->Payload.UnmapPages.AddressSpaceObjectPhysicalAddress = Slot->Message.Payload.UnmapPages.AddressSpaceObjectPhysicalAddress;
                Response->Status = Response->Payload.UnmapPages.Status;
                break;
            }

            UnmapRequest.PageMapLevel4PhysicalAddress = AddressSpaceObject->RootTablePhysicalAddress;
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
            DispatchLocalQueryMapping(&Slot->Message.Payload.QueryMapping, &Response->Payload.QueryMapping, &Response->Status);
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
            DispatchLocalAttachStagedImage(&Slot->Message.Payload.AttachStagedImage, &Response->Payload.AttachStagedImage, &Response->Status);
            break;
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK:
            DispatchLocalAllocateAddressSpaceStack(&Slot->Message.Payload.AllocateAddressSpaceStack, &Response->Payload.AllocateAddressSpaceStack, &Response->Status);
            break;
        default:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            break;
    }

    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_COMPLETE;
    RequestMailbox->Header.ConsumeIndex += 1ULL;
    PostResponse(Response);
    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
    Slot->Sequence = 0ULL;
}
