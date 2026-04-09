/*
 * File Name: MemoryManagerBootstrapDispatchSection06.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapDispatch.c.
 */

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
