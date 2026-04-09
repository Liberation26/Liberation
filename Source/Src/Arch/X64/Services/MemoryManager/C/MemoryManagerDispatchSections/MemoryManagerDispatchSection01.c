/*
 * File Name: MemoryManagerDispatchSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerDispatch.c.
 */

__attribute__((weak)) UINT32 LosCapabilitiesServiceSubmitAccessRequest(const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                                                       LOS_CAPABILITIES_ACCESS_RESULT *Result);
__attribute__((weak)) UINT32 LosCapabilitiesServiceCheckAccess(const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                                               LOS_CAPABILITIES_ACCESS_RESULT *Result);

static UINT8 LosMemoryManagerReadyEventProofPosted = 0U;
static UINT8 LosMemoryManagerFirstEndpointReplyProofPosted = 0U;

static UINTN LosMemoryManagerStringLength(const char *Text)
{
    UINTN Length;

    if (Text == 0)
    {
        return 0U;
    }

    for (Length = 0U; Text[Length] != 0; ++Length)
    {
    }

    return Length;
}

void LosMemoryManagerInitializeServiceResponse(
    const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request,
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    if (Response == 0)
    {
        return;
    }

    LosMemoryManagerZeroMemory(Response, sizeof(*Response));
    if (Request != 0)
    {
        Response->Operation = Request->Operation;
        Response->RequestId = Request->RequestId;
    }
}

BOOLEAN LosMemoryManagerServiceAuthorizeRequest(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request,
                                               const char *Namespace,
                                               const char *Name,
                                               UINT64 *MatchingGrantId)
{
    LOS_CAPABILITIES_ACCESS_REQUEST AccessRequest;
    LOS_CAPABILITIES_ACCESS_RESULT AccessResult;
    UINT32 AccessStatus;

    if (MatchingGrantId != 0)
    {
        *MatchingGrantId = 0ULL;
    }

    if (Request == 0 || Namespace == 0 || Name == 0)
    {
        return 0;
    }

    LosMemoryManagerZeroMemory(&AccessRequest, sizeof(AccessRequest));
    LosMemoryManagerZeroMemory(&AccessResult, sizeof(AccessResult));
    AccessRequest.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    AccessRequest.PrincipalType = Request->CallerPrincipalType;
    AccessRequest.AccessRight = LOS_CAPABILITIES_ACCESS_RIGHT_MUTATE;
    AccessRequest.PrincipalId = Request->CallerPrincipalId;
    LosMemoryManagerCopyBytes(AccessRequest.PrincipalName, Request->CallerPrincipalName, sizeof(AccessRequest.PrincipalName));
    LosMemoryManagerCopyBytes(AccessRequest.Namespace, Namespace, LosMemoryManagerStringLength(Namespace) + 1U);
    LosMemoryManagerCopyBytes(AccessRequest.Name, Name, LosMemoryManagerStringLength(Name) + 1U);

    if (LosCapabilitiesServiceSubmitAccessRequest != 0)
    {
        AccessStatus = LosCapabilitiesServiceSubmitAccessRequest(&AccessRequest, &AccessResult);
    }
    else if (LosCapabilitiesServiceCheckAccess != 0)
    {
        AccessStatus = LosCapabilitiesServiceCheckAccess(&AccessRequest, &AccessResult);
    }
    else
    {
        return 0;
    }

    if (AccessStatus != LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS ||
        AccessResult.Status != LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS ||
        AccessResult.Granted == 0U)
    {
        return 0;
    }

    if (MatchingGrantId != 0)
    {
        *MatchingGrantId = AccessResult.MatchingGrantId;
    }

    return 1;
}

void LosMemoryManagerPopulateAccessDeniedResponse(UINT32 Operation, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    if (Response == 0)
    {
        return;
    }

    Response->Operation = Operation;
    Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_ACCESS_DENIED;
    switch (Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES:
            Response->Payload.AllocateFrames.Status = LOS_X64_MEMORY_OPERATION_STATUS_ACCESS_DENIED;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
            Response->Payload.ReserveFrames.Status = LOS_X64_MEMORY_OPERATION_STATUS_ACCESS_DENIED;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            Response->Payload.ClaimFrames.Status = LOS_X64_MEMORY_OPERATION_STATUS_ACCESS_DENIED;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
            Response->Payload.QueryMemoryRegions.Status = LOS_X64_MEMORY_OPERATION_STATUS_ACCESS_DENIED;
            break;
        default:
            break;
    }
}

BOOLEAN LosMemoryManagerServiceMayHandleOperation(const LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT32 Operation)
{
    if (State == 0)
    {
        return 0;
    }

    if (Operation == LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)
    {
        return 1;
    }

    if (State->AttachComplete == 0U)
    {
        return 0;
    }

    if ((State->NegotiatedOperations & (1ULL << Operation)) == 0ULL)
    {
        return 0;
    }

    switch (Operation)
    {
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

BOOLEAN LosMemoryManagerCompleteRequest(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    LOS_MEMORY_MANAGER_REQUEST_SLOT *RequestSlot;
    LOS_MEMORY_MANAGER_RESPONSE_SLOT *ResponseSlot;
    UINT64 RequestIndex;
    UINT64 ResponseIndex;

    if (State == 0 || State->RequestMailbox == 0 || State->ResponseMailbox == 0 || Response == 0)
    {
        return 0;
    }

    RequestIndex = State->RequestMailbox->Header.ConsumeIndex % State->RequestMailbox->Header.SlotCount;
    RequestSlot = &State->RequestMailbox->Slots[RequestIndex];
    if (RequestSlot->SlotState != LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    ResponseIndex = State->ResponseMailbox->Header.ProduceIndex % State->ResponseMailbox->Header.SlotCount;
    ResponseSlot = &State->ResponseMailbox->Slots[ResponseIndex];
    if (ResponseSlot->SlotState == LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    LosMemoryManagerCopyBytes(&ResponseSlot->Message, Response, sizeof(*Response));
    ResponseSlot->Sequence = Response->RequestId;
    ResponseSlot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY;
    State->ResponseMailbox->Header.ProduceIndex += 1ULL;

    RequestSlot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_COMPLETE;
    RequestSlot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
    RequestSlot->Sequence = 0ULL;
    LosMemoryManagerZeroMemory(&RequestSlot->Message, sizeof(RequestSlot->Message));
    State->RequestMailbox->Header.ConsumeIndex += 1ULL;
    return 1;
}

UINT32 LosMemoryManagerBootstrapAttachStatusFromResult(UINT32 BootstrapResult)
{
    switch (BootstrapResult)
    {
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY:
            return LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ALREADY_ATTACHED:
            return LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_STATE_INVALID:
            return LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        default:
            return LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    }
}

UINT32 LosMemoryManagerValidateBootstrapAttachRequest(
    const LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_REQUEST *Request)
{
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    UINT64 RequestedServiceRootPhysicalAddress;

    if (State == 0 || Request == 0 || State->LaunchBlock == 0)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_STATE_INVALID;
    }

    LaunchBlock = State->LaunchBlock;
    RequestedServiceRootPhysicalAddress = Request->ServicePageMapLevel4PhysicalAddress;
    if (RequestedServiceRootPhysicalAddress == 0ULL)
    {
        RequestedServiceRootPhysicalAddress = LaunchBlock->ServicePageMapLevel4PhysicalAddress;
    }
    if (RequestedServiceRootPhysicalAddress == 0ULL)
    {
        RequestedServiceRootPhysicalAddress = State->ActiveRootTablePhysicalAddress;
    }

    if (State->Online == 0U)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_STATE_INVALID;
    }
    if (State->AttachComplete != 0U)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ALREADY_ATTACHED;
    }
    if (Request->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_VERSION_MISMATCH;
    }
    if ((Request->OfferedOperations & (1ULL << LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)) == 0ULL)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_OPERATION_SET_INVALID;
    }
    if (Request->LaunchBlockPhysicalAddress != LaunchBlock->LaunchBlockPhysicalAddress)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_LAUNCH_BLOCK_MISMATCH;
    }
    if (Request->KernelToServiceEndpointId != LaunchBlock->Endpoints.KernelToService ||
        Request->ServiceToKernelEndpointId != LaunchBlock->Endpoints.ServiceToKernel ||
        Request->ServiceEventsEndpointId != LaunchBlock->Endpoints.ServiceEvents)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ENDPOINT_MISMATCH;
    }
    if (Request->RequestMailboxPhysicalAddress != LaunchBlock->RequestMailboxPhysicalAddress ||
        Request->ResponseMailboxPhysicalAddress != LaunchBlock->ResponseMailboxPhysicalAddress ||
        Request->EventMailboxPhysicalAddress != LaunchBlock->EventMailboxPhysicalAddress)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_MAILBOX_MISMATCH;
    }
    if (RequestedServiceRootPhysicalAddress != State->ActiveRootTablePhysicalAddress)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_ROOT_MISMATCH;
    }

    return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY;
}

void LosMemoryManagerPopulateBootstrapAttachResponse(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request,
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    UINT32 BootstrapResult;
    UINT64 BootstrapFlags;

    LosMemoryManagerInitializeServiceResponse(Request, Response);
    if (State == 0 || Request == 0 || Response == 0)
    {
        return;
    }

    BootstrapResult = LosMemoryManagerValidateBootstrapAttachRequest(State, &Request->Payload.BootstrapAttach);
    LosMemoryManagerServiceSerialWriteText("[MemManager] Bootstrap attach validation result=");
    LosMemoryManagerServiceSerialWriteUnsigned(BootstrapResult);
    LosMemoryManagerServiceSerialWriteText(" name=");
    LosMemoryManagerServiceSerialWriteText(LosMemoryManagerBootstrapAttachResultName(BootstrapResult));
    LosMemoryManagerServiceSerialWriteText(" request-root=");
    LosMemoryManagerServiceSerialWriteHex64(Request->Payload.BootstrapAttach.ServicePageMapLevel4PhysicalAddress);
    LosMemoryManagerServiceSerialWriteText(" active-root=");
    LosMemoryManagerServiceSerialWriteHex64(State->ActiveRootTablePhysicalAddress);
    LosMemoryManagerServiceSerialWriteText(" request-image=");
    LosMemoryManagerServiceSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress);
    LosMemoryManagerServiceSerialWriteText(" active-image=");
    LosMemoryManagerServiceSerialWriteHex64(State->LaunchBlock->ServiceImagePhysicalAddress);
    LosMemoryManagerServiceSerialWriteText(" request-entry=");
    LosMemoryManagerServiceSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceEntryVirtualAddress);
    LosMemoryManagerServiceSerialWriteText(" active-entry=");
    LosMemoryManagerServiceSerialWriteHex64(State->LaunchBlock->ServiceEntryVirtualAddress);
    LosMemoryManagerServiceSerialWriteText("\n");
    BootstrapFlags = 0ULL;
    if (State->LaunchBlock != 0)
    {
        BootstrapFlags = State->LaunchBlock->Flags;
    }

    if (BootstrapResult == LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY)
    {
        State->AttachComplete = 1U;
        State->BootstrapResultCode = BootstrapResult;
        State->NegotiatedOperations = Request->Payload.BootstrapAttach.OfferedOperations;
        if (State->TaskObject != 0)
        {
            State->TaskObject->Flags |= LOS_MEMORY_MANAGER_TASK_FLAG_SERVICE_READY;
        }
        BootstrapFlags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_ATTACH_COMPLETE;
        if (State->LaunchBlock != 0)
        {
            State->LaunchBlock->Flags = BootstrapFlags;
        }
        LosMemoryManagerPostEvent(
            LOS_MEMORY_MANAGER_EVENT_SERVICE_READY_FOR_REQUESTS,
            BootstrapResult,
            Request->RequestId,
            State->NegotiatedOperations);
        if (LosMemoryManagerReadyEventProofPosted == 0U)
        {
            LosMemoryManagerReadyEventProofPosted = 1U;
            LosMemoryManagerServiceSerialWriteLine("[MemManager] Endpoint ready proof: service-ready event posted from MM.");
            LosMemoryManagerServiceSerialWriteNamedHex("Ready event request id", Request->RequestId);
            LosMemoryManagerServiceSerialWriteNamedHex("Ready event operations", State->NegotiatedOperations);
        }
    }
    else
    {
        State->BootstrapResultCode = BootstrapResult;
    }

    Response->Status = LosMemoryManagerBootstrapAttachStatusFromResult(BootstrapResult);
    Response->Payload.BootstrapAttach.BootstrapResult = BootstrapResult;
    Response->Payload.BootstrapAttach.BootstrapState =
        (BootstrapResult == LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY)
            ? LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_READY
            : LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_SERVICE_ONLINE;
    Response->Payload.BootstrapAttach.NegotiatedOperations =
        (BootstrapResult == LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY)
            ? State->NegotiatedOperations
            : 0ULL;
    Response->Payload.BootstrapAttach.BootstrapFlags = BootstrapFlags;
    Response->Payload.BootstrapAttach.ActiveRootTablePhysicalAddress = State->ActiveRootTablePhysicalAddress;
    Response->Payload.BootstrapAttach.KernelRootTablePhysicalAddress = State->KernelRootTablePhysicalAddress;
    Response->Payload.BootstrapAttach.ServiceHeartbeat = State->Heartbeat;
    Response->Payload.BootstrapAttach.LastProcessedRequestId = Request->RequestId;
    Response->Payload.BootstrapAttach.TotalUsableBytes = State->MemoryView.TotalUsableBytes;
    Response->Payload.BootstrapAttach.TotalBootstrapReservedBytes = State->MemoryView.TotalBootstrapReservedBytes;
    Response->Payload.BootstrapAttach.TotalFirmwareReservedBytes = State->MemoryView.TotalFirmwareReservedBytes;
    Response->Payload.BootstrapAttach.TotalRuntimeBytes = State->MemoryView.TotalRuntimeBytes;
    Response->Payload.BootstrapAttach.TotalMmioBytes = State->MemoryView.TotalMmioBytes;
    Response->Payload.BootstrapAttach.TotalAcpiBytes = State->MemoryView.TotalAcpiBytes;
    Response->Payload.BootstrapAttach.TotalUnusableBytes = State->MemoryView.TotalUnusableBytes;
    Response->Payload.BootstrapAttach.TotalPages = State->MemoryView.TotalPages;
    Response->Payload.BootstrapAttach.FreePages = State->MemoryView.FreePages;
    Response->Payload.BootstrapAttach.ReservedPages = State->MemoryView.ReservedPages;
    Response->Payload.BootstrapAttach.RuntimePages = State->MemoryView.RuntimePages;
    Response->Payload.BootstrapAttach.MmioPages = State->MemoryView.MmioPages;
    Response->Payload.BootstrapAttach.InternalDescriptorCount = (UINT64)State->MemoryView.InternalDescriptorCount;
    Response->Payload.BootstrapAttach.PageFrameDatabaseEntryCount = (UINT64)State->MemoryView.PageFrameDatabaseEntryCount;
    Response->Payload.BootstrapAttach.HeapMetadataPages = State->Heap.TotalReservedMetadataPages;
    Response->Payload.BootstrapAttach.HeapReservedPages = State->Heap.TotalReservedHeapPages;
    Response->Payload.BootstrapAttach.HeapSlabPageCapacity = (UINT64)State->Heap.SlabPageCapacity;
    Response->Payload.BootstrapAttach.HeapLargeAllocationCapacity = (UINT64)State->Heap.LargeAllocationCapacity;
}
