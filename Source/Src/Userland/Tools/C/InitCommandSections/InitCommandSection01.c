/*
 * File Name: InitCommandSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from InitCommand.c.
 */

typedef struct
{
    UINT8 Ident[16];
    UINT16 Type;
    UINT16 Machine;
    UINT32 Version;
    UINT64 Entry;
    UINT64 ProgramHeaderOffset;
    UINT64 SectionHeaderOffset;
    UINT32 Flags;
    UINT16 HeaderSize;
    UINT16 ProgramHeaderEntrySize;
    UINT16 ProgramHeaderCount;
    UINT16 SectionHeaderEntrySize;
    UINT16 SectionHeaderCount;
    UINT16 SectionNameStringTableIndex;
} LOS_INIT_COMMAND_ELF64_HEADER;

static void LosInitCommandWriteText(const char *Text);
static void LosInitCommandWriteUnsigned(UINT64 Value);
static void LosInitCommandDescribeEndpoint(const char *Label,
                                           const LOS_INIT_COMMAND_ENDPOINT *Endpoint);
static void LosInitCommandDescribeCapabilities(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Capabilities);
static void LosInitCommandDescribeServiceRequest(const LOS_INIT_COMMAND_SERVICE_REQUEST *Request);
static void LosInitCommandSendBootstrapEvent(const LOS_INIT_COMMAND_CONTEXT *Context,
                                             UINT64 EventCode,
                                             UINT64 EventValue);
static void LosInitCommandReturnToKernel(UINT64 ExitStatus);
static UINT64 LosInitCommandBuildEndpointMask(void);
static void LosInitCommandFillCommandName(char *Buffer, UINT32 BufferLength);
static UINT64 LosInitCommandValidateServiceRequest(const LOS_INIT_COMMAND_SERVICE_REQUEST *Request);
static UINT64 LosInitCommandValidateServiceImage(const LOS_INIT_COMMAND_SERVICE_IMAGE *Image);
static UINT64 LosInitCommandValidateSecureChannelPolicy(const LOS_SECURE_ENDPOINT_POLICY *Policy);
static void LosInitCommandDescribeSecureChannelPolicy(const LOS_SECURE_ENDPOINT_POLICY *Policy);
static BOOLEAN LosInitCommandTextEqual(const char *Left, const char *Right);
static BOOLEAN LosInitCommandIsVerbose(const LOS_INIT_COMMAND_CONTEXT *Context);
static const LOS_CAPABILITY_PROFILE_ASSIGNMENT *LosInitCommandFindAssignment(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Capabilities, UINT32 PrincipalType, const char *PrincipalName);
static const LOS_CAPABILITY_GRANT_BLOCK *LosInitCommandFindProfileBlock(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Capabilities, const char *ProfileName);
static BOOLEAN LosInitCommandProfileHasGrant(const LOS_CAPABILITY_GRANT_BLOCK *Block, const char *Namespace, const char *Name);
static UINT64 LosInitCommandValidateLaunchAuthority(const LOS_INIT_COMMAND_CONTEXT *Context);

static UINT64 LosInitCommandSendShellRequestTransport(const LOS_INIT_COMMAND_CONTEXT *Context,
                                                      const LOS_SHELL_SERVICE_REQUEST *Request,
                                                      LOS_SHELL_SERVICE_RESPONSE *Response);
static UINT64 LosInitCommandSendShellRequestSharedMailbox(const LOS_SHELL_SERVICE_REQUEST *Request,
                                                          LOS_SHELL_SERVICE_RESPONSE *Response);
static UINT64 LosInitCommandBindShellTransport(LOS_SHELL_SERVICE_TRANSPORT_BINDING *Binding);
static void LosInitCommandPrepareShellBindRequest(LOS_SHELL_SERVICE_BIND_REQUEST *Request);
static void LosInitCommandPrepareShellResolveRequest(LOS_SHELL_SERVICE_RESOLVE_REQUEST *Request);
static UINT64 LosInitCommandCallShellService(const LOS_INIT_COMMAND_CONTEXT *Context,
                                             const LOS_SHELL_SERVICE_REQUEST *Request,
                                             LOS_SHELL_SERVICE_RESPONSE *Response);
static void LosInitCommandRunShellBootstrapSession(const LOS_INIT_COMMAND_CONTEXT *Context);
static void LosInitCommandParkAfterBootstrap(void);


static UINT64 LosInitCommandSendShellRequestTransport(const LOS_INIT_COMMAND_CONTEXT *Context,
                                                      const LOS_SHELL_SERVICE_REQUEST *Request,
                                                      LOS_SHELL_SERVICE_RESPONSE *Response)
{
    UINT64 SendStatus;
    UINT64 ReceiveStatus;

    if (Context == 0 || Request == 0 || Response == 0)
    {
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    }

    if (Context->Send.EndpointId == 0ULL || Context->Receive.EndpointId == 0ULL)
    {
        return LOS_INIT_COMMAND_STATUS_UNSUPPORTED;
    }

    SendStatus = LosUserSend(Context->Send.EndpointId, Request, sizeof(*Request));
    if (SendStatus != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        return SendStatus;
    }

    ReceiveStatus = LosUserReceive(Context->Receive.EndpointId, Response, sizeof(*Response));
    if (ReceiveStatus != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        return ReceiveStatus;
    }

    if (Response->Version != LOS_SHELL_SERVICE_VERSION ||
        Response->Signature != LOS_SHELL_SERVICE_RESPONSE_SIGNATURE)
    {
        return LOS_INIT_COMMAND_STATUS_UNSUPPORTED;
    }

    return LOS_INIT_COMMAND_STATUS_SUCCESS;
}

static void LosInitCommandPrepareShellResolveRequest(LOS_SHELL_SERVICE_RESOLVE_REQUEST *Request)
{
    UINTN Index;
    static const char ServiceName[] = "shell";

    if (Request == 0)
    {
        return;
    }

    for (Index = 0U; Index < sizeof(*Request); ++Index)
    {
        ((UINT8 *)Request)[Index] = 0U;
    }

    Request->Version = LOS_SHELL_SERVICE_VERSION;
    Request->Flags = LOS_SHELL_SERVICE_FLAG_SERVICE_VISIBLE |
                     LOS_SHELL_SERVICE_FLAG_REGISTRY_VISIBLE;
    Request->Signature = LOS_SHELL_SERVICE_RESOLVE_REQUEST_SIGNATURE;
    Request->ExpectedVersion = LOS_SHELL_SERVICE_VERSION;
    Request->RequiredFlags = LOS_SHELL_SERVICE_FLAG_TRANSPORT_LIVE |
                             LOS_SHELL_SERVICE_FLAG_BOOTSTRAP_SHARED_MEMORY |
                             LOS_SHELL_SERVICE_FLAG_SERVICE_VISIBLE |
                             LOS_SHELL_SERVICE_FLAG_REGISTRY_VISIBLE;

    for (Index = 0U; Index + 1U < sizeof(Request->ServiceName) && ServiceName[Index] != 0; ++Index)
    {
        Request->ServiceName[Index] = ServiceName[Index];
    }
}

static void LosInitCommandPrepareShellBindRequest(LOS_SHELL_SERVICE_BIND_REQUEST *Request)
{
    UINTN Index;
    static const char ServiceName[] = "shell";

    if (Request == 0)
    {
        return;
    }

    for (Index = 0U; Index < sizeof(*Request); ++Index)
    {
        ((UINT8 *)Request)[Index] = 0U;
    }

    Request->Version = LOS_SHELL_SERVICE_VERSION;
    Request->Flags = LOS_SHELL_SERVICE_FLAG_SERVICE_VISIBLE;
    Request->Signature = LOS_SHELL_SERVICE_BIND_REQUEST_SIGNATURE;
    Request->RequestedGeneration = 0ULL;
    Request->ExpectedVersion = LOS_SHELL_SERVICE_VERSION;
    Request->RequiredFlags = LOS_SHELL_SERVICE_FLAG_TRANSPORT_LIVE |
                             LOS_SHELL_SERVICE_FLAG_BOOTSTRAP_SHARED_MEMORY |
                             LOS_SHELL_SERVICE_FLAG_SERVICE_VISIBLE;

    for (Index = 0U; Index + 1U < sizeof(Request->ServiceName) && ServiceName[Index] != 0; ++Index)
    {
        Request->ServiceName[Index] = ServiceName[Index];
    }
}

static UINT64 LosInitCommandBindShellTransport(LOS_SHELL_SERVICE_TRANSPORT_BINDING *Binding)
{
    LOS_SHELL_SERVICE_BIND_REQUEST Request;
    LOS_SHELL_SERVICE_RESOLVE_REQUEST ResolveRequest;
    LOS_SHELL_SERVICE_RESOLVE_RESPONSE ResolveResponse;
    UINT64 ResolveStatus;

    LosInitCommandPrepareShellResolveRequest(&ResolveRequest);
    ResolveStatus = LosShellServiceResolveFromRequest(&ResolveRequest, &ResolveResponse);
    if (ResolveStatus != LOS_SHELL_SERVICE_STATUS_SUCCESS ||
        ResolveResponse.Signature != LOS_SHELL_SERVICE_RESOLVE_RESPONSE_SIGNATURE ||
        ResolveResponse.Status != LOS_SHELL_SERVICE_STATUS_SUCCESS)
    {
        return (ResolveStatus != LOS_SHELL_SERVICE_STATUS_SUCCESS) ? ResolveStatus : ResolveResponse.Status;
    }

    LosInitCommandPrepareShellBindRequest(&Request);
    Request.RequestedGeneration = ResolveResponse.Generation;
    return LosShellServiceBindFromRequest(&Request, Binding);
}

static UINT64 LosInitCommandSendShellRequestSharedMailbox(const LOS_SHELL_SERVICE_REQUEST *Request,
                                                          LOS_SHELL_SERVICE_RESPONSE *Response)
{
    LOS_SHELL_SERVICE_TRANSPORT_BINDING Binding;
    LOS_SHELL_SERVICE_MAILBOX *RequestMailbox;
    LOS_SHELL_SERVICE_MAILBOX *ResponseMailbox;
    UINTN Index;
    UINT64 DispatchStatus;
    UINT64 BindStatus;

    if (Request == 0 || Response == 0)
    {
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    }

    for (Index = 0U; Index < sizeof(Binding); ++Index)
    {
        ((UINT8 *)&Binding)[Index] = 0U;
    }

    BindStatus = LosInitCommandBindShellTransport(&Binding);
    if (BindStatus != LOS_SHELL_SERVICE_STATUS_SUCCESS)
    {
        return BindStatus;
    }

    RequestMailbox = Binding.RequestMailbox;
    ResponseMailbox = Binding.ResponseMailbox;
    if (RequestMailbox == 0 || ResponseMailbox == 0 ||
        RequestMailbox->Signature != LOS_SHELL_SERVICE_MAILBOX_SIGNATURE ||
        ResponseMailbox->Signature != LOS_SHELL_SERVICE_MAILBOX_SIGNATURE)
    {
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    }

    for (Index = 0U; Index < sizeof(RequestMailbox->Payload.Request); ++Index)
    {
        ((UINT8 *)&RequestMailbox->Payload.Request)[Index] = 0U;
    }
    for (Index = 0U; Index < sizeof(ResponseMailbox->Payload.Response); ++Index)
    {
        ((UINT8 *)&ResponseMailbox->Payload.Response)[Index] = 0U;
    }

    RequestMailbox->Version = LOS_SHELL_SERVICE_VERSION;
    RequestMailbox->Flags = LOS_SHELL_SERVICE_FLAG_TRANSPORT_LIVE |
                            LOS_SHELL_SERVICE_FLAG_BOOTSTRAP_SHARED_MEMORY |
                            LOS_SHELL_SERVICE_FLAG_SERVICE_VISIBLE;
    RequestMailbox->Signature = LOS_SHELL_SERVICE_MAILBOX_SIGNATURE;
    RequestMailbox->EndpointId = Binding.RequestEndpointId;
    RequestMailbox->Sequence = Request->Sequence;
    RequestMailbox->MessageBytes = sizeof(*Request);
    RequestMailbox->Payload.Request = *Request;
    RequestMailbox->State = LOS_SHELL_SERVICE_MAILBOX_STATE_READY;

    ResponseMailbox->Version = LOS_SHELL_SERVICE_VERSION;
    ResponseMailbox->Flags = LOS_SHELL_SERVICE_FLAG_TRANSPORT_LIVE |
                             LOS_SHELL_SERVICE_FLAG_BOOTSTRAP_SHARED_MEMORY |
                             LOS_SHELL_SERVICE_FLAG_SERVICE_VISIBLE;
    ResponseMailbox->Signature = LOS_SHELL_SERVICE_MAILBOX_SIGNATURE;
    ResponseMailbox->EndpointId = Binding.ResponseEndpointId;
    ResponseMailbox->Sequence = 0ULL;
    ResponseMailbox->MessageBytes = 0ULL;
    ResponseMailbox->State = LOS_SHELL_SERVICE_MAILBOX_STATE_EMPTY;

    DispatchStatus = LosShellServiceDispatchMailbox(RequestMailbox, ResponseMailbox);
    if (DispatchStatus != LOS_SHELL_SERVICE_STATUS_SUCCESS &&
        DispatchStatus != LOS_SHELL_SERVICE_STATUS_UNKNOWN_COMMAND)
    {
        return DispatchStatus;
    }
    if (ResponseMailbox->State != LOS_SHELL_SERVICE_MAILBOX_STATE_READY)
    {
        return LOS_SHELL_SERVICE_STATUS_RETRY;
    }

    *Response = ResponseMailbox->Payload.Response;
    ResponseMailbox->State = LOS_SHELL_SERVICE_MAILBOX_STATE_EMPTY;
    return LOS_INIT_COMMAND_STATUS_SUCCESS;
}

static UINT64 LosInitCommandCallShellService(const LOS_INIT_COMMAND_CONTEXT *Context,
                                             const LOS_SHELL_SERVICE_REQUEST *Request,
                                             LOS_SHELL_SERVICE_RESPONSE *Response)
{
    UINT64 Status;

    Status = LosInitCommandSendShellRequestTransport(Context, Request, Response);
    if (Status == LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        return Status;
    }

    return LosInitCommandSendShellRequestSharedMailbox(Request, Response);
}

static void LosInitCommandParkAfterBootstrap(void)
{
    LosInitCommandWriteText("[InitCmd] Memory manager is already online before init starts. CAPSMGR and SHELL have been bootstrapped; init will remain resident until a real session-manager and console-input path replaces this proof mode.\n");

    for (;;)
    {
        __asm__ __volatile__("pause");
    }
}

static void LosInitCommandRunShellBootstrapSession(const LOS_INIT_COMMAND_CONTEXT *Context)
{
    LOS_SHELL_SERVICE_REQUEST Request;
    LOS_SHELL_SERVICE_RESPONSE Response;
    UINTN Index;
    UINT64 Status;
    static const char *Commands[] =
    {
        "version",
        "pwd",
        "echo init attached",
        "run DEMO"
    };

    LosInitCommandWriteText("[InitCmd] Driving shell input through the shell service. Internal commands execute directly; unknown commands resolve as external ELFs.\n");

    for (Index = 0U; Index < sizeof(Request); ++Index)
    {
        ((UINT8 *)&Request)[Index] = 0U;
    }
    for (Index = 0U; Index < sizeof(Response); ++Index)
    {
        ((UINT8 *)&Response)[Index] = 0U;
    }

    Request.Version = LOS_SHELL_SERVICE_VERSION;
    Request.Command = LOS_SHELL_SERVICE_COMMAND_QUERY;
    Request.Flags = LOS_SHELL_SERVICE_FLAG_CAPTURE_OUTPUT;
    Request.Signature = LOS_SHELL_SERVICE_REQUEST_SIGNATURE;
    Request.RequestId = 1ULL;
    Request.Sequence = 1ULL;
    Request.Argument0 = LOS_SHELL_SERVICE_QUERY_PROMPT;

    Status = LosInitCommandCallShellService(Context, &Request, &Response);
    if (Status == LOS_INIT_COMMAND_STATUS_SUCCESS || Status == LOS_SHELL_SERVICE_STATUS_SUCCESS)
    {
        LosInitCommandWriteText("[InitCmd] shell prompt: ");
        LosInitCommandWriteText(Response.Output);
        LosInitCommandWriteText("\n");
    }
    else
    {
        LosInitCommandWriteText("[InitCmd] shell prompt query failed.\n");
    }

    for (Index = 0U; Index < sizeof(Commands) / sizeof(Commands[0]); ++Index)
    {
        UINTN CommandIndex;

        for (CommandIndex = 0U; CommandIndex < sizeof(Request); ++CommandIndex)
        {
            ((UINT8 *)&Request)[CommandIndex] = 0U;
        }
        for (CommandIndex = 0U; CommandIndex < sizeof(Response); ++CommandIndex)
        {
            ((UINT8 *)&Response)[CommandIndex] = 0U;
        }

        Request.Version = LOS_SHELL_SERVICE_VERSION;
        Request.Command = LOS_SHELL_SERVICE_COMMAND_EXECUTE;
        Request.Flags = LOS_SHELL_SERVICE_FLAG_CAPTURE_OUTPUT;
        Request.Signature = LOS_SHELL_SERVICE_REQUEST_SIGNATURE;
        Request.RequestId = (UINT64)(Index + 2U);
        Request.Sequence = (UINT64)(Index + 2U);
        for (CommandIndex = 0U;
             CommandIndex + 1U < sizeof(Request.Text) && Commands[Index][CommandIndex] != 0;
             ++CommandIndex)
        {
            Request.Text[CommandIndex] = Commands[Index][CommandIndex];
        }

        LosInitCommandWriteText("[InitCmd] shell> ");
        LosInitCommandWriteText(Commands[Index]);
        LosInitCommandWriteText("\n");
        Status = LosInitCommandCallShellService(Context, &Request, &Response);
        if (Status == LOS_INIT_COMMAND_STATUS_SUCCESS ||
            Status == LOS_SHELL_SERVICE_STATUS_SUCCESS ||
            Response.Status == LOS_SHELL_SERVICE_STATUS_SUCCESS)
        {
            LosInitCommandWriteText("[InitCmd] shell response: ");
            LosInitCommandWriteText(Response.Output);
            LosInitCommandWriteText("\n");
        }
        else
        {
            LosInitCommandWriteText("[InitCmd] shell command dispatch failed.\n");
        }
    }
}
