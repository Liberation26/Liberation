/*
 * File Name: MemoryManagerDispatchSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerDispatch.c.
 */

void LosMemoryManagerPostEvent(UINT32 EventType, UINT32 Status, UINT64 Value0, UINT64 Value1)
{
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;
    LOS_MEMORY_MANAGER_EVENT_MAILBOX *Mailbox;
    UINT64 Index;
    LOS_MEMORY_MANAGER_EVENT_SLOT *Slot;

    State = LosMemoryManagerServiceState();
    Mailbox = State->EventMailbox;
    if (Mailbox == 0 || State->EventEndpoint == 0 || State->EventEndpoint->State < LOS_MEMORY_MANAGER_ENDPOINT_STATE_BOUND)
    {
        return;
    }

    Index = Mailbox->Header.ProduceIndex % Mailbox->Header.SlotCount;
    Slot = &Mailbox->Slots[Index];
    if (Slot->SlotState == LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return;
    }

    Slot->Message.EventType = EventType;
    Slot->Message.Status = Status;
    Slot->Message.Sequence = State->Heartbeat;
    Slot->Message.Value0 = Value0;
    Slot->Message.Value1 = Value1;
    Slot->Sequence = State->Heartbeat;
    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY;
    Mailbox->Header.ProduceIndex += 1ULL;
}

void LosMemoryManagerServicePoll(void)
{
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;
    LOS_MEMORY_MANAGER_REQUEST_SLOT *Slot;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;
    UINT64 Index;

    State = LosMemoryManagerServiceState();
    if (State->Online == 0U || State->RequestMailbox == 0 || State->ReceiveEndpoint == 0)
    {
        return;
    }

    Index = State->RequestMailbox->Header.ConsumeIndex % State->RequestMailbox->Header.SlotCount;
    Slot = &State->RequestMailbox->Slots[Index];
    if (Slot->SlotState != LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return;
    }

    State->LastRequestId = Slot->Message.RequestId;
    if (State->TaskObject != 0)
    {
        State->TaskObject->Heartbeat = State->Heartbeat;
        State->TaskObject->LastRequestId = State->LastRequestId;
    }

    LosMemoryManagerInitializeServiceResponse(&Slot->Message, &Response);
    if (!LosMemoryManagerServiceMayHandleOperation(State, Slot->Message.Operation))
    {
        Response.Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
        if (LosMemoryManagerCompleteRequest(State, &Response))
        {
            if (State->AttachComplete != 0U && LosMemoryManagerFirstEndpointReplyProofPosted == 0U)
            {
                LosMemoryManagerServiceSerialWriteText("[MemManager] Endpoint request/reply path proven.\n");
                LosMemoryManagerFirstEndpointReplyProofPosted = 1U;
            }
        }
        return;
    }

    switch (Slot->Message.Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            if (!LosMemoryManagerServiceAuthorizeRequest(&Slot->Message, "memory", "allocate", 0))
            {
                LosMemoryManagerPopulateAccessDeniedResponse(Slot->Message.Operation, &Response);
                break;
            }
            break;
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
            if (!LosMemoryManagerServiceAuthorizeRequest(&Slot->Message, "memory", "query", 0))
            {
                LosMemoryManagerPopulateAccessDeniedResponse(Slot->Message.Operation, &Response);
                break;
            }
            break;
        default:
            break;
    }

    switch (Slot->Message.Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH:
            LosMemoryManagerServiceSerialWriteText("[MemManager] Processing bootstrap attach request id=");
            LosMemoryManagerServiceSerialWriteHex64(Slot->Message.RequestId);
            LosMemoryManagerServiceSerialWriteText("\n");
            LosMemoryManagerPopulateBootstrapAttachResponse(State, &Slot->Message, &Response);
            break;
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES:
            if (Response.Status != LOS_X64_MEMORY_OPERATION_STATUS_ACCESS_DENIED)
            {
                LosMemoryManagerServiceAllocateFrames(State, &Slot->Message.Payload.AllocateFrames, &Response.Payload.AllocateFrames);
                Response.Status = Response.Payload.AllocateFrames.Status;
            }
            break;
        case LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES:
            LosMemoryManagerServiceFreeFrames(State, &Slot->Message.Payload.FreeFrames, &Response.Payload.FreeFrames);
            Response.Status = Response.Payload.FreeFrames.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE:
            LosMemoryManagerServiceCreateAddressSpace(State, &Slot->Message.Payload.CreateAddressSpace, &Response.Payload.CreateAddressSpace);
            Response.Status = Response.Payload.CreateAddressSpace.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE:
            LosMemoryManagerServiceDestroyAddressSpace(State, &Slot->Message.Payload.DestroyAddressSpace, &Response.Payload.DestroyAddressSpace);
            Response.Status = Response.Payload.DestroyAddressSpace.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES:
            LosMemoryManagerServiceMapPages(State, &Slot->Message.Payload.MapPages, &Response.Payload.MapPages);
            Response.Status = Response.Payload.MapPages.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES:
            LosMemoryManagerServiceUnmapPages(State, &Slot->Message.Payload.UnmapPages, &Response.Payload.UnmapPages);
            Response.Status = Response.Payload.UnmapPages.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_PROTECT_PAGES:
            LosMemoryManagerServiceProtectPages(State, &Slot->Message.Payload.ProtectPages, &Response.Payload.ProtectPages);
            Response.Status = Response.Payload.ProtectPages.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MAPPING:
            LosMemoryManagerServiceQueryMapping(State, &Slot->Message.Payload.QueryMapping, &Response.Payload.QueryMapping);
            Response.Status = Response.Payload.QueryMapping.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
            if (Response.Status != LOS_X64_MEMORY_OPERATION_STATUS_ACCESS_DENIED)
            {
                LosMemoryManagerServiceQueryMemoryRegions(State, &Slot->Message, &Response);
            }
            break;
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
            if (Response.Status != LOS_X64_MEMORY_OPERATION_STATUS_ACCESS_DENIED)
            {
                LosMemoryManagerServiceReserveFrames(State, &Slot->Message.Payload.ReserveFrames, &Response.Payload.ReserveFrames);
                Response.Status = Response.Payload.ReserveFrames.Status;
            }
            break;
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            if (Response.Status != LOS_X64_MEMORY_OPERATION_STATUS_ACCESS_DENIED)
            {
                LosMemoryManagerServiceClaimFrames(State, &Slot->Message.Payload.ClaimFrames, &Response.Payload.ClaimFrames);
                Response.Status = Response.Payload.ClaimFrames.Status;
            }
            break;
        case LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE:
            LosMemoryManagerServiceAttachStagedImage(State, &Slot->Message.Payload.AttachStagedImage, &Response.Payload.AttachStagedImage);
            Response.Status = Response.Payload.AttachStagedImage.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK:
            LosMemoryManagerServiceAllocateAddressSpaceStack(State, &Slot->Message.Payload.AllocateAddressSpaceStack, &Response.Payload.AllocateAddressSpaceStack);
            Response.Status = Response.Payload.AllocateAddressSpaceStack.Status;
            break;
        default:
            return;
    }

    if (LosMemoryManagerCompleteRequest(State, &Response))
    {
        if (Response.Operation == LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)
        {
            LosMemoryManagerServiceSerialWriteText("[MemManager] Bootstrap attach response posted result=");
            LosMemoryManagerServiceSerialWriteUnsigned(Response.Payload.BootstrapAttach.BootstrapResult);
            LosMemoryManagerServiceSerialWriteText(" status=");
            LosMemoryManagerServiceSerialWriteUnsigned(Response.Status);
            LosMemoryManagerServiceSerialWriteText("\n");
        }
        else if (State->AttachComplete != 0U && LosMemoryManagerFirstEndpointReplyProofPosted == 0U)
        {
            LosMemoryManagerFirstEndpointReplyProofPosted = 1U;
            LosMemoryManagerServiceSerialWriteLine("[MemManager] Endpoint reply proof: first successful service reply posted from MM.");
            LosMemoryManagerServiceSerialWriteNamedUnsigned("Reply operation", (UINT64)Response.Operation);
            LosMemoryManagerServiceSerialWriteNamedHex("Reply request id", Response.RequestId);
            LosMemoryManagerServiceSerialWriteNamedUnsigned("Reply status", (UINT64)Response.Status);
        }
    }
    else if (Response.Operation == LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)
    {
        LosMemoryManagerServiceSerialWriteLine("[MemManager] Bootstrap attach response could not be posted.");
    }
}
