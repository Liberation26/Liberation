#include "MemoryManagerBootstrapInternal.h"

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
    if (Mailbox == 0 || Response == 0)
    {
        return 0;
    }

    Index = Mailbox->Header.ProduceIndex % Mailbox->Header.SlotCount;
    Slot = &Mailbox->Slots[Index];
    if (Slot->SlotState == LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    Slot->Message = *Response;
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
    if (Mailbox == 0 || Request == 0)
    {
        return 0;
    }

    Index = Mailbox->Header.ProduceIndex % Mailbox->Header.SlotCount;
    Slot = &Mailbox->Slots[Index];
    if (Slot->SlotState == LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    Slot->Message = *Request;
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
    if (Mailbox == 0 || Response == 0)
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

        *Response = Slot->Message;
        Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
        Slot->Sequence = 0ULL;
        Mailbox->Header.ConsumeIndex = Index + 1ULL;
        return 1;
    }

    return 0;
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
    if (RequestMailbox == 0)
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

static void SendRequestAndAwaitResponse(LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    InitializeResponse(Request, Response);

    if (!LosMemoryManagerBootstrapEnqueueRequest(Request))
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    LosMemoryManagerBootstrapDispatch(Request, Response);
    if (!LosMemoryManagerBootstrapDequeueResponse(Request->RequestId, Response))
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
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
        *Result = Response.Payload.QueryMemoryRegions;
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
        Request.Payload.ReserveFrames = *RequestData;
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        *Result = Response.Payload.ReserveFrames;
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
        Request.Payload.ClaimFrames = *RequestData;
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        *Result = Response.Payload.ClaimFrames;
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
        Request.Payload.MapPages = *RequestData;
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        *Result = Response.Payload.MapPages;
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
        Request.Payload.UnmapPages = *RequestData;
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        *Result = Response.Payload.UnmapPages;
        Result->Status = Response.Status;
    }
}
