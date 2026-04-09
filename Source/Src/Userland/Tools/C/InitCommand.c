/*
 * File Name: InitCommand.c
 * File Version: 0.4.8
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T11:02:18Z
 * Last Update Timestamp: 2026-04-09T18:35:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS userland component.
 */

#include "InitCommand.h"
#include "ShellMain.h"

#define LOS_ELF_MAGIC_0 0x7FU
#define LOS_ELF_MAGIC_1 0x45U
#define LOS_ELF_MAGIC_2 0x4CU
#define LOS_ELF_MAGIC_3 0x46U
#define LOS_ELF_CLASS_64 2U
#define LOS_ELF_DATA_LITTLE_ENDIAN 1U
#define LOS_ELF_MACHINE_X86_64 0x3EU
#define LOS_ELF_TYPE_EXEC 2U

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

static UINT64 LosInitCommandLoadAndRunShellService(const LOS_INIT_COMMAND_CONTEXT *Context)
{
    LOS_INIT_COMMAND_SERVICE_IMAGE ShellImage;
    UINT64 LaunchStatus;
    UINTN Index;
    static const char ShellPath[] = "\\LIBERATION\\SERVICES\\SHELLX64.ELF";

    if (Context == 0)
    {
        return LOS_INIT_COMMAND_STATUS_INVALID_CONTEXT;
    }

    for (Index = 0U; Index < sizeof(ShellImage); ++Index)
    {
        ((UINT8 *)&ShellImage)[Index] = 0U;
    }

    ShellImage.Version = LOS_INIT_COMMAND_VERSION;
    ShellImage.ImageFormat = LOS_INIT_COMMAND_SERVICE_IMAGE_FORMAT_ELF64;
    ShellImage.Flags = LOS_INIT_COMMAND_SERVICE_FLAG_WAIT_UNTIL_ONLINE |
                       LOS_INIT_COMMAND_SERVICE_FLAG_BOOTSTRAP_IMAGE_EMBEDDED;
    ShellImage.Signature = LOS_INIT_COMMAND_SERVICE_IMAGE_SIGNATURE;
    ShellImage.RequestId = Context->ServiceRequest.RequestId + 1ULL;
    ShellImage.ImageAddress = (UINT64)(UINTN)LosShellServiceImageStart;
    ShellImage.ImageSize = (UINT64)(UINTN)LosShellServiceImageSize;
    ShellImage.EntryVirtualAddress = 0ULL;
    ShellImage.BootstrapCallableEntryAddress = (UINT64)(UINTN)LosShellServiceBootstrapEntryWithContext;
    ShellImage.BootstrapContextAddress = (UINT64)(UINTN)LosKernelGetBootstrapCapabilities();
    ShellImage.BootstrapContextSize = (UINT64)sizeof(LOS_CAPABILITIES_BOOTSTRAP_CONTEXT);
    ShellImage.BootstrapStateAddress = (UINT64)(UINTN)LosShellServiceState();
    ShellImage.BootstrapStateSize = (UINT64)sizeof(UINT32);

    for (Index = 0U; Index + 1U < LOS_INIT_COMMAND_SERVICE_PATH_LENGTH && ShellPath[Index] != 0; ++Index)
    {
        ShellImage.ServicePath[Index] = ShellPath[Index];
    }

    LosInitCommandSendBootstrapEvent(Context,
                                     LOS_INIT_COMMAND_EVENT_SERVICE_IMAGE_VALIDATED,
                                     ShellImage.RequestId);
    if (LosInitCommandIsVerbose(Context))
    {
        LosInitCommandWriteText("[InitCmd] Shell service image staged from ");
        LosInitCommandWriteText(ShellImage.ServicePath);
        LosInitCommandWriteText(".\n");
        LosInitCommandWriteText("[InitCmd] Init is now invoking the SHELL bootstrap entry and waiting for ONLINE state.\n");
    }

    LaunchStatus = LosUserLaunchServiceImage(&ShellImage);
    if (LaunchStatus == LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        LosInitCommandSendBootstrapEvent(Context,
                                         LOS_INIT_COMMAND_EVENT_SERVICE_ENTRY_TRANSFER,
                                         ShellImage.RequestId);
        LosInitCommandSendBootstrapEvent(Context,
                                         LOS_INIT_COMMAND_EVENT_SERVICE_ONLINE,
                                         ShellImage.RequestId);
        LosInitCommandWriteText("[InitCmd] SHELL service online.\n");
        if (LosInitCommandIsVerbose(Context))
        {
            LosInitCommandWriteText("[InitCmd] SHELL bootstrap entry returned control to init.\n");
            LosInitCommandRunShellBootstrapSession(Context);
        }
        return LOS_INIT_COMMAND_STATUS_SUCCESS;
    }

    LosInitCommandWriteText("[InitCmd] SHELL bootstrap entry was not callable from init.\n");
    return LOS_INIT_COMMAND_STATUS_SERVICE_LAUNCH_FAILED;
}

static UINT64 LosInitCommandLoadAndRunService(const LOS_INIT_COMMAND_CONTEXT *Context);

__attribute__((weak)) void LosUserWriteText(const char *Text)
{
    (void)Text;
}

__attribute__((weak)) void LosUserWriteUnsigned(UINT64 Value)
{
    (void)Value;
}

__attribute__((weak)) UINT64 LosUserSend(UINT64 EndpointId, const void *Message, UINT64 MessageSize)
{
    (void)EndpointId;
    (void)Message;
    (void)MessageSize;
    return LOS_INIT_COMMAND_STATUS_UNSUPPORTED;
}

__attribute__((weak)) UINT64 LosUserReceive(UINT64 EndpointId, void *Message, UINT64 MessageCapacity)
{
    (void)EndpointId;
    (void)Message;
    (void)MessageCapacity;
    return LOS_INIT_COMMAND_STATUS_UNSUPPORTED;
}

__attribute__((weak)) UINT64 LosUserSendEvent(UINT64 EndpointId, UINT64 EventCode, UINT64 EventValue)
{
    (void)EndpointId;
    (void)EventCode;
    (void)EventValue;
    return LOS_INIT_COMMAND_STATUS_UNSUPPORTED;
}

__attribute__((weak)) UINT64 LosUserReceiveEvent(UINT64 EndpointId, UINT64 *EventCode, UINT64 *EventValue)
{
    if (EventCode != 0)
    {
        *EventCode = 0ULL;
    }
    if (EventValue != 0)
    {
        *EventValue = 0ULL;
    }

    (void)EndpointId;
    return LOS_INIT_COMMAND_STATUS_UNSUPPORTED;
}

typedef void (*LOS_INIT_COMMAND_BOOTSTRAP_ENTRY)(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context);
typedef struct _LOS_CAPABILITIES_SERVICE_STATE LOS_CAPABILITIES_SERVICE_STATE;
extern UINT8 LosShellServiceImageStart[];
extern UINT8 LosShellServiceImageSize[];
extern void LosShellServiceBootstrapEntryWithContext(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context);
extern void *LosShellServiceState(void);
__attribute__((weak)) const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *LosKernelGetBootstrapCapabilities(void);


__attribute__((weak)) UINT64 LosUserLaunchServiceImage(const LOS_INIT_COMMAND_SERVICE_IMAGE *Image)
{
    LOS_INIT_COMMAND_BOOTSTRAP_ENTRY BootstrapEntry;
    const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *BootstrapContext;
    volatile const UINT32 *OnlineState;

    if (Image == 0)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SERVICE_IMAGE;
    }

    if (Image->BootstrapCallableEntryAddress == 0ULL ||
        Image->BootstrapStateAddress == 0ULL)
    {
        return LOS_INIT_COMMAND_STATUS_UNSUPPORTED;
    }

    BootstrapEntry = (LOS_INIT_COMMAND_BOOTSTRAP_ENTRY)(UINTN)Image->BootstrapCallableEntryAddress;
    BootstrapContext = (const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *)(UINTN)Image->BootstrapContextAddress;
    OnlineState = (const UINT32 *)(UINTN)Image->BootstrapStateAddress;
    BootstrapEntry(BootstrapContext);
    return (*OnlineState != 0U) ? LOS_INIT_COMMAND_STATUS_SUCCESS : LOS_INIT_COMMAND_STATUS_SERVICE_NOT_ONLINE;
}

__attribute__((weak)) void LosUserExit(UINT64 ExitStatus)
{
    (void)ExitStatus;

    for (;;)
    {
        __asm__ __volatile__("hlt");
    }
}

static void LosInitCommandWriteText(const char *Text)
{
    if (Text != 0)
    {
        LosUserWriteText(Text);
    }
}

static void LosInitCommandWriteUnsigned(UINT64 Value)
{
    LosUserWriteUnsigned(Value);
}

static BOOLEAN LosInitCommandIsVerbose(const LOS_INIT_COMMAND_CONTEXT *Context)
{
    if (Context == 0)
    {
        return 0;
    }

    return (Context->Version >= LOS_INIT_COMMAND_VERSION &&
            (Context->Flags & LOS_INIT_COMMAND_FLAG_VERBOSE) != 0U) ? 1 : 0;
}

static BOOLEAN LosInitCommandTextEqual(const char *Left, const char *Right)
{
    UINTN Index;

    if (Left == 0 || Right == 0)
    {
        return 0;
    }

    for (Index = 0U;; ++Index)
    {
        if (Left[Index] != Right[Index])
        {
            return 0;
        }
        if (Left[Index] == 0)
        {
            return 1;
        }
    }
}

static const LOS_CAPABILITY_PROFILE_ASSIGNMENT *LosInitCommandFindAssignment(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Capabilities,
                                                                              UINT32 PrincipalType,
                                                                              const char *PrincipalName)
{
    UINT32 Index;

    if (Capabilities == 0 || PrincipalName == 0)
    {
        return 0;
    }

    for (Index = 0U; Index < Capabilities->AssignmentCount && Index < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_ASSIGNMENTS; ++Index)
    {
        const LOS_CAPABILITY_PROFILE_ASSIGNMENT *Assignment;

        Assignment = &Capabilities->Assignments[Index];
        if (Assignment->PrincipalType == PrincipalType &&
            LosInitCommandTextEqual(Assignment->PrincipalName, PrincipalName))
        {
            return Assignment;
        }
    }

    return 0;
}

static const LOS_CAPABILITY_GRANT_BLOCK *LosInitCommandFindProfileBlock(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Capabilities,
                                                                        const char *ProfileName)
{
    UINT32 Index;

    if (Capabilities == 0 || ProfileName == 0)
    {
        return 0;
    }

    for (Index = 0U; Index < Capabilities->BlockCount && Index < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_BLOCKS; ++Index)
    {
        const LOS_CAPABILITY_GRANT_BLOCK *Block;

        Block = &Capabilities->Blocks[Index];
        if (LosInitCommandTextEqual(Block->ProfileName, ProfileName))
        {
            return Block;
        }
    }

    return 0;
}

static BOOLEAN LosInitCommandProfileHasGrant(const LOS_CAPABILITY_GRANT_BLOCK *Block,
                                             const char *Namespace,
                                             const char *Name)
{
    UINT32 Index;

    if (Block == 0 || Namespace == 0 || Name == 0)
    {
        return 0;
    }

    for (Index = 0U; Index < Block->GrantCount && Index < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_GRANTS_PER_BLOCK; ++Index)
    {
        const LOS_CAPABILITY_GRANT_ENTRY *Grant;

        Grant = &Block->Grants[Index];
        if (Grant->State != LOS_CAPABILITIES_GRANT_STATE_ACTIVE)
        {
            continue;
        }
        if (LosInitCommandTextEqual(Grant->Namespace, Namespace) &&
            LosInitCommandTextEqual(Grant->Name, Name))
        {
            return 1;
        }
    }

    return 0;
}

static UINT64 LosInitCommandValidateLaunchAuthority(const LOS_INIT_COMMAND_CONTEXT *Context)
{
    const LOS_CAPABILITY_PROFILE_ASSIGNMENT *Assignment;
    const LOS_CAPABILITY_GRANT_BLOCK *Block;

    if (Context == 0)
    {
        return LOS_INIT_COMMAND_STATUS_INVALID_CONTEXT;
    }

    Assignment = LosInitCommandFindAssignment(&Context->Capabilities,
                                              LOS_CAPABILITIES_PRINCIPAL_TYPE_TASK,
                                              "init");
    if (Assignment == 0)
    {
        return LOS_INIT_COMMAND_STATUS_ACCESS_DENIED;
    }

    Block = LosInitCommandFindProfileBlock(&Context->Capabilities, Assignment->ProfileName);
    if (Block == 0)
    {
        return LOS_INIT_COMMAND_STATUS_ACCESS_DENIED;
    }

    if (!LosInitCommandProfileHasGrant(Block, "service", "start"))
    {
        return LOS_INIT_COMMAND_STATUS_ACCESS_DENIED;
    }

    if ((Context->ServiceRequest.Flags & LOS_INIT_COMMAND_SERVICE_FLAG_CAPABILITIES_IMPORT_REQUIRED) != 0U &&
        !LosInitCommandProfileHasGrant(Block, "service", "bootstrap.import"))
    {
        return LOS_INIT_COMMAND_STATUS_ACCESS_DENIED;
    }

    return LOS_INIT_COMMAND_STATUS_SUCCESS;
}


static void LosInitCommandDescribeEndpoint(const char *Label,
                                           const LOS_INIT_COMMAND_ENDPOINT *Endpoint)
{
    LosInitCommandWriteText("[InitCmd] ");
    LosInitCommandWriteText(Label);
    LosInitCommandWriteText(" endpoint id: ");
    LosInitCommandWriteUnsigned((Endpoint != 0) ? Endpoint->EndpointId : 0ULL);
    LosInitCommandWriteText("\n");
}

static void LosInitCommandDescribeServiceRequest(const LOS_INIT_COMMAND_SERVICE_REQUEST *Request)
{
    if (Request == 0)
    {
        return;
    }

    LosInitCommandWriteText("[InitCmd] Service request action: ");
    LosInitCommandWriteUnsigned((UINT64)Request->Action);
    LosInitCommandWriteText("\n");
    LosInitCommandWriteText("[InitCmd] Service request flags: ");
    LosInitCommandWriteUnsigned((UINT64)Request->Flags);
    LosInitCommandWriteText("\n");
    LosInitCommandWriteText("[InitCmd] Service request id: ");
    LosInitCommandWriteUnsigned(Request->RequestId);
    LosInitCommandWriteText("\n");
    LosInitCommandWriteText("[InitCmd] Service request path: ");
    LosInitCommandWriteText(Request->ServicePath);
    LosInitCommandWriteText("\n");
    LosInitCommandWriteText("[InitCmd] Service image bytes: ");
    LosInitCommandWriteUnsigned(Request->ServiceImage.ImageSize);
    LosInitCommandWriteText("\n");
}

static UINT64 LosInitCommandValidateEndpoint(const LOS_INIT_COMMAND_ENDPOINT *Endpoint,
                                             UINT32 ExpectedKind)
{
    if (Endpoint == 0)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_ENDPOINT;
    }
    if (Endpoint->Kind != ExpectedKind)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_ENDPOINT;
    }
    if (Endpoint->EndpointId == 0ULL)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_ENDPOINT;
    }

    return LOS_INIT_COMMAND_STATUS_SUCCESS;
}

static UINT64 LosInitCommandValidateSecureChannelPolicy(const LOS_SECURE_ENDPOINT_POLICY *Policy)
{
    if (Policy == 0)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SECURE_CHANNEL;
    }

    if (Policy->Version != LOS_SECURE_ENDPOINT_VERSION)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SECURE_CHANNEL;
    }

    if (Policy->Mode > LOS_SECURE_ENDPOINT_MODE_ENCRYPTED_MUTUAL)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SECURE_CHANNEL;
    }

    if (Policy->KeyExchange > LOS_SECURE_ENDPOINT_KEY_EXCHANGE_SESSION_DERIVATION)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SECURE_CHANNEL;
    }

    if ((Policy->Flags & LOS_SECURE_ENDPOINT_FLAG_REQUIRED) != 0U &&
        Policy->Mode == LOS_SECURE_ENDPOINT_MODE_PLAINTEXT &&
        (Policy->Flags & LOS_SECURE_ENDPOINT_FLAG_ALLOW_BOOTSTRAP_PLAINTEXT) == 0U)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SECURE_CHANNEL;
    }

    return LOS_INIT_COMMAND_STATUS_SUCCESS;
}

static UINT64 LosInitCommandValidateServiceImage(const LOS_INIT_COMMAND_SERVICE_IMAGE *Image)
{
    const LOS_INIT_COMMAND_ELF64_HEADER *Header;
    UINT32 Index;
    UINT32 HasTerminator;

    if (Image == 0)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SERVICE_IMAGE;
    }
    if (Image->Signature != LOS_INIT_COMMAND_SERVICE_IMAGE_SIGNATURE ||
        Image->ImageFormat != LOS_INIT_COMMAND_SERVICE_IMAGE_FORMAT_ELF64 ||
        Image->ImageAddress == 0ULL ||
        Image->ImageSize < sizeof(LOS_INIT_COMMAND_ELF64_HEADER) ||
        Image->BootstrapCallableEntryAddress == 0ULL ||
        Image->BootstrapStateAddress == 0ULL)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SERVICE_IMAGE;
    }

    HasTerminator = 0U;
    for (Index = 0U; Index < LOS_INIT_COMMAND_SERVICE_PATH_LENGTH; ++Index)
    {
        if (Image->ServicePath[Index] == 0)
        {
            HasTerminator = 1U;
            break;
        }
    }
    if (HasTerminator == 0U || Image->ServicePath[0] == 0)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SERVICE_IMAGE;
    }

    Header = (const LOS_INIT_COMMAND_ELF64_HEADER *)(UINTN)Image->ImageAddress;
    if (Header->Ident[0] != LOS_ELF_MAGIC_0 ||
        Header->Ident[1] != LOS_ELF_MAGIC_1 ||
        Header->Ident[2] != LOS_ELF_MAGIC_2 ||
        Header->Ident[3] != LOS_ELF_MAGIC_3 ||
        Header->Ident[4] != LOS_ELF_CLASS_64 ||
        Header->Ident[5] != LOS_ELF_DATA_LITTLE_ENDIAN ||
        Header->Machine != LOS_ELF_MACHINE_X86_64 ||
        Header->Type != LOS_ELF_TYPE_EXEC)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SERVICE_IMAGE;
    }

    return LOS_INIT_COMMAND_STATUS_SUCCESS;
}

static void LosInitCommandDescribeSecureChannelPolicy(const LOS_SECURE_ENDPOINT_POLICY *Policy)
{
    if (Policy == 0)
    {
        return;
    }

    LosInitCommandWriteText("[InitCmd] Secure channel mode: ");
    LosInitCommandWriteUnsigned((UINT64)Policy->Mode);
    LosInitCommandWriteText("\n");
    LosInitCommandWriteText("[InitCmd] Secure channel exchange: ");
    LosInitCommandWriteUnsigned((UINT64)Policy->KeyExchange);
    LosInitCommandWriteText("\n");
    LosInitCommandWriteText("[InitCmd] Secure channel flags: ");
    LosInitCommandWriteUnsigned((UINT64)Policy->Flags);
    LosInitCommandWriteText("\n");
}

static UINT64 LosInitCommandValidateServiceRequest(const LOS_INIT_COMMAND_SERVICE_REQUEST *Request)
{
    UINT32 Index;
    UINT32 HasTerminator;
    UINT64 Status;

    if (Request == 0)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SERVICE_COMMAND;
    }

    if (Request->Action != LOS_INIT_COMMAND_SERVICE_ACTION_LOAD_AND_RUN)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SERVICE_COMMAND;
    }

    HasTerminator = 0U;
    for (Index = 0U; Index < LOS_INIT_COMMAND_SERVICE_PATH_LENGTH; ++Index)
    {
        if (Request->ServicePath[Index] == 0)
        {
            HasTerminator = 1U;
            break;
        }
    }

    if (HasTerminator == 0U || Request->ServicePath[0] == 0)
    {
        return LOS_INIT_COMMAND_STATUS_BAD_SERVICE_COMMAND;
    }

    Status = LosInitCommandValidateServiceImage(&Request->ServiceImage);
    if (Status != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        return Status;
    }

    Status = LosInitCommandValidateSecureChannelPolicy(&Request->SecureChannelPolicy);
    if (Status != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        return Status;
    }

    return LOS_INIT_COMMAND_STATUS_SUCCESS;
}

static UINT64 LosInitCommandValidateContext(const LOS_INIT_COMMAND_CONTEXT *Context)
{
    UINT64 Status;

    if (Context == 0)
    {
        return LOS_INIT_COMMAND_STATUS_INVALID_CONTEXT;
    }
    if (Context->Version < LOS_INIT_COMMAND_VERSION)
    {
        return LOS_INIT_COMMAND_STATUS_INVALID_CONTEXT;
    }

    Status = LosInitCommandValidateEndpoint(&Context->Send, LOS_INIT_COMMAND_ENDPOINT_SEND);
    if (Status != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        return Status;
    }

    Status = LosInitCommandValidateEndpoint(&Context->Receive, LOS_INIT_COMMAND_ENDPOINT_RECEIVE);
    if (Status != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        return Status;
    }

    Status = LosInitCommandValidateEndpoint(&Context->SendEvent, LOS_INIT_COMMAND_ENDPOINT_SEND_EVENT);
    if (Status != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        return Status;
    }

    Status = LosInitCommandValidateEndpoint(&Context->ReceiveEvent, LOS_INIT_COMMAND_ENDPOINT_RECEIVE_EVENT);
    if (Status != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        return Status;
    }

    Status = LosInitCommandValidateServiceRequest(&Context->ServiceRequest);
    if (Status != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        return Status;
    }

    if (Context->Capabilities.Version > LOS_CAPABILITIES_SERVICE_VERSION)
    {
        return LOS_INIT_COMMAND_STATUS_INVALID_CONTEXT;
    }

    return LOS_INIT_COMMAND_STATUS_SUCCESS;
}

static void LosInitCommandDescribeCapabilities(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Capabilities)
{
    UINT32 BlockIndex;

    if (Capabilities == 0)
    {
        return;
    }

    LosInitCommandWriteText("[InitCmd] Bootstrap capabilities version: ");
    LosInitCommandWriteUnsigned((UINT64)Capabilities->Version);
    LosInitCommandWriteText("\n");
    LosInitCommandWriteText("[InitCmd] Bootstrap capabilities flags: ");
    LosInitCommandWriteUnsigned((UINT64)Capabilities->Flags);
    LosInitCommandWriteText("\n");
    LosInitCommandWriteText("[InitCmd] Bootstrap capability blocks: ");
    LosInitCommandWriteUnsigned((UINT64)Capabilities->BlockCount);
    LosInitCommandWriteText("\n");

    for (BlockIndex = 0U; BlockIndex < Capabilities->BlockCount && BlockIndex < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_BLOCKS; ++BlockIndex)
    {
        const LOS_CAPABILITY_GRANT_BLOCK *Block;
        UINT32 GrantIndex;

        Block = &Capabilities->Blocks[BlockIndex];
        LosInitCommandWriteText("[InitCmd] Profile ");
        LosInitCommandWriteText(Block->ProfileName);
        LosInitCommandWriteText(" grants=");
        LosInitCommandWriteUnsigned((UINT64)Block->GrantCount);
        LosInitCommandWriteText("\n");

        for (GrantIndex = 0U; GrantIndex < Block->GrantCount && GrantIndex < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_GRANTS_PER_BLOCK; ++GrantIndex)
        {
            const LOS_CAPABILITY_GRANT_ENTRY *Grant;

            Grant = &Block->Grants[GrantIndex];
            LosInitCommandWriteText("[InitCmd] Grant ");
            LosInitCommandWriteUnsigned((UINT64)GrantIndex);
            LosInitCommandWriteText(": ");
            LosInitCommandWriteText(Grant->Namespace);
            LosInitCommandWriteText(".");
            LosInitCommandWriteText(Grant->Name);
            LosInitCommandWriteText(" grantId=");
            LosInitCommandWriteUnsigned(Grant->GrantId);
            LosInitCommandWriteText(" state=");
            LosInitCommandWriteUnsigned((UINT64)Grant->State);
            LosInitCommandWriteText(" auth=");
            LosInitCommandWriteText(Grant->AuthoriserName);
            LosInitCommandWriteText("\n");
        }
    }
}

static void LosInitCommandSendBootstrapEvent(const LOS_INIT_COMMAND_CONTEXT *Context,
                                             UINT64 EventCode,
                                             UINT64 EventValue)
{
    UINT64 EndpointId;

    EndpointId = 0ULL;
    if (Context != 0)
    {
        EndpointId = Context->SendEvent.EndpointId;
    }

    (void)LosUserSendEvent(EndpointId, EventCode, EventValue);
}

static void LosInitCommandReturnToKernel(UINT64 ExitStatus)
{
    LosUserExit(ExitStatus);
}

static UINT64 LosInitCommandBuildEndpointMask(void)
{
    return LOS_INIT_COMMAND_ENDPOINT_MASK_ALL;
}

static void LosInitCommandFillCommandName(char *Buffer, UINT32 BufferLength)
{
    static const char Name[] = "InitCommand";
    UINT32 Index;

    if (Buffer == 0 || BufferLength == 0U)
    {
        return;
    }

    for (Index = 0U; Index < BufferLength; ++Index)
    {
        Buffer[Index] = 0;
    }

    for (Index = 0U; Index + 1U < BufferLength && Name[Index] != 0; ++Index)
    {
        Buffer[Index] = Name[Index];
    }
}

static UINT64 LosInitCommandLoadAndRunService(const LOS_INIT_COMMAND_CONTEXT *Context)
{
    UINT64 LaunchStatus;

    if (Context == 0)
    {
        return LOS_INIT_COMMAND_STATUS_INVALID_CONTEXT;
    }

    LosInitCommandSendBootstrapEvent(Context,
                                     LOS_INIT_COMMAND_EVENT_SERVICE_IMAGE_VALIDATED,
                                     Context->ServiceRequest.RequestId);
    if (LosInitCommandIsVerbose(Context))
    {
        LosInitCommandWriteText("[InitCmd] Validated embedded CAPSMGR image supplied by kernel.\n");
        LosInitCommandWriteText("[InitCmd] Bootstrap capability policy authorised CAPSMGR launch.\n");
        LosInitCommandWriteText("[InitCmd] Init is now invoking the CAPSMGR bootstrap entry and waiting for ONLINE state.\n");
    }
    else
    {
        LosInitCommandWriteText("[InitCmd] Launching CAPSMGR bootstrap.\n");
    }

    LaunchStatus = LosUserLaunchServiceImage(&Context->ServiceRequest.ServiceImage);
    if (LaunchStatus == LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        UINT64 ShellLaunchStatus;

        LosInitCommandSendBootstrapEvent(Context,
                                         LOS_INIT_COMMAND_EVENT_SERVICE_ENTRY_TRANSFER,
                                         Context->ServiceRequest.RequestId);
        LosInitCommandSendBootstrapEvent(Context,
                                         LOS_INIT_COMMAND_EVENT_SERVICE_ONLINE,
                                         Context->ServiceRequest.RequestId);
        LosInitCommandWriteText("[InitCmd] CAPSMGR service online.\n");
        ShellLaunchStatus = LosInitCommandLoadAndRunShellService(Context);
        if (ShellLaunchStatus != LOS_INIT_COMMAND_STATUS_SUCCESS)
        {
            return ShellLaunchStatus;
        }
        return LOS_INIT_COMMAND_STATUS_SUCCESS;
    }

    LosInitCommandWriteText("[InitCmd] CAPSMGR bootstrap entry was not callable from init.\n");
    return LOS_INIT_COMMAND_STATUS_SERVICE_LAUNCH_FAILED;
}

void LosInitCommandMain(const LOS_INIT_COMMAND_CONTEXT *Context)
{
    UINT32 Verbose;
    UINT64 ContextStatus;
    UINT64 SendStatus;
    UINT64 ReceiveStatus;
    UINT64 ReceiveEventStatus;
    LOS_INIT_COMMAND_MESSAGE Hello;
    LOS_INIT_COMMAND_RESPONSE Response;
    LOS_INIT_COMMAND_EVENT_RECORD EventRecord;
    UINT64 ExitStatus;
    UINT64 LaunchStatus;

    Verbose = 0U;
    if (Context != 0 &&
        Context->Version >= LOS_INIT_COMMAND_VERSION &&
        (Context->Flags & LOS_INIT_COMMAND_FLAG_VERBOSE) != 0U)
    {
        Verbose = 1U;
    }

    LosInitCommandWriteText("[InitCmd] Init command entered in ring 3.\n");
    LosInitCommandWriteText("[InitCmd] Communication contract: send, receive, send-event, receive-event.\n");
    LosInitCommandWriteText("[InitCmd] Kernel delegates first service launch to InitCommand.\n");

    ContextStatus = LosInitCommandValidateContext(Context);
    if (ContextStatus != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        LosInitCommandWriteText("[InitCmd] Invalid bootstrap context, endpoint contract, or service request.\n");
        LosInitCommandReturnToKernel(ContextStatus);
        return;
    }

    if (Verbose != 0U)
    {
        LosInitCommandDescribeEndpoint("send", &Context->Send);
        LosInitCommandDescribeEndpoint("receive", &Context->Receive);
        LosInitCommandDescribeEndpoint("send-event", &Context->SendEvent);
        LosInitCommandDescribeEndpoint("receive-event", &Context->ReceiveEvent);
        LosInitCommandDescribeServiceRequest(&Context->ServiceRequest);
        LosInitCommandDescribeSecureChannelPolicy(&Context->ServiceRequest.SecureChannelPolicy);
    }

    Hello.Version = LOS_INIT_COMMAND_VERSION;
    Hello.Kind = LOS_INIT_COMMAND_MESSAGE_KIND_BOOTSTRAP_HELLO;
    Hello.HeaderBytes = (UINT32)sizeof(LOS_INIT_COMMAND_MESSAGE);
    Hello.Flags = (Context != 0) ? Context->Flags : 0U;
    Hello.RequestTag = LOS_INIT_COMMAND_BOOTSTRAP_TAG;
    Hello.RequestValue = Context->ServiceRequest.RequestId;
    Hello.Sequence = LOS_INIT_COMMAND_BOOTSTRAP_SEQUENCE;
    Hello.EndpointMask = LosInitCommandBuildEndpointMask();
    Hello.SenderEndpointId = Context->Send.EndpointId;
    Hello.ReceiveEndpointId = Context->Receive.EndpointId;
    Hello.SendEventEndpointId = Context->SendEvent.EndpointId;
    Hello.ReceiveEventEndpointId = Context->ReceiveEvent.EndpointId;
    LosInitCommandFillCommandName(Hello.CommandName, LOS_INIT_COMMAND_NAME_LENGTH);

    Response.Version = LOS_INIT_COMMAND_VERSION;
    Response.Status = LOS_INIT_COMMAND_STATUS_SUCCESS;
    Response.HeaderBytes = (UINT32)sizeof(LOS_INIT_COMMAND_RESPONSE);
    Response.Reserved = 0U;
    Response.ResponseTag = LOS_INIT_COMMAND_BOOTSTRAP_TAG;
    Response.ResponseValue = Context->ServiceRequest.RequestId;
    Response.AcceptedEndpointMask = LOS_INIT_COMMAND_ENDPOINT_MASK_ALL;
    Response.Sequence = LOS_INIT_COMMAND_BOOTSTRAP_SEQUENCE;
    Response.SenderEndpointId = Context->Send.EndpointId;
    Response.ReceiverEndpointId = Context->Receive.EndpointId;

    EventRecord.Version = LOS_INIT_COMMAND_VERSION;
    EventRecord.Kind = LOS_INIT_COMMAND_MESSAGE_KIND_BOOTSTRAP_EVENT;
    EventRecord.HeaderBytes = (UINT32)sizeof(LOS_INIT_COMMAND_EVENT_RECORD);
    EventRecord.Reserved = 0U;
    EventRecord.EventCode = LOS_INIT_COMMAND_EVENT_BOOTSTRAP_START;
    EventRecord.EventValue = Context->ServiceRequest.RequestId;
    EventRecord.Sequence = LOS_INIT_COMMAND_BOOTSTRAP_SEQUENCE;
    EventRecord.SenderEndpointId = Context->SendEvent.EndpointId;
    EventRecord.ReceiverEndpointId = Context->ReceiveEvent.EndpointId;

    ExitStatus = LOS_INIT_COMMAND_STATUS_SUCCESS;

    LosInitCommandSendBootstrapEvent(Context,
                                     LOS_INIT_COMMAND_EVENT_BOOTSTRAP_START,
                                     LOS_INIT_COMMAND_BOOTSTRAP_TAG);
    LosInitCommandSendBootstrapEvent(Context,
                                     LOS_INIT_COMMAND_EVENT_SERVICE_LOAD_REQUEST,
                                     Context->ServiceRequest.RequestId);

    SendStatus = LosUserSend(Context->Send.EndpointId,
                             &Hello,
                             sizeof(Hello));
    ReceiveStatus = LosUserReceive(Context->Receive.EndpointId,
                                   &Response,
                                   sizeof(Response));
    ReceiveEventStatus = LosUserReceiveEvent(Context->ReceiveEvent.EndpointId,
                                             &EventRecord.EventCode,
                                             &EventRecord.EventValue);

    if (SendStatus != LOS_INIT_COMMAND_STATUS_SUCCESS ||
        ReceiveStatus != LOS_INIT_COMMAND_STATUS_SUCCESS ||
        ReceiveEventStatus != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        LosInitCommandWriteText("[InitCmd] Endpoint transport is not yet live; continuing with direct service launch path.\n");
    }

    LaunchStatus = LosInitCommandValidateLaunchAuthority(Context);
    if (LaunchStatus == LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        LosInitCommandSendBootstrapEvent(Context,
                                         LOS_INIT_COMMAND_EVENT_SERVICE_ACCESS_GRANTED,
                                         Context->ServiceRequest.RequestId);
        LaunchStatus = LosInitCommandLoadAndRunService(Context);
    }
    else
    {
        LosInitCommandSendBootstrapEvent(Context,
                                         LOS_INIT_COMMAND_EVENT_SERVICE_ACCESS_DENIED,
                                         Context->ServiceRequest.RequestId);
        LosInitCommandWriteText("[InitCmd] Bootstrap capability policy denied CAPSMGR launch.\n");
    }
    if (LaunchStatus != LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        ExitStatus = LaunchStatus;
    }

    LosInitCommandSendBootstrapEvent(Context,
                                     LOS_INIT_COMMAND_EVENT_SERVICE_LOAD_COMPLETE,
                                     ExitStatus);
    LosInitCommandSendBootstrapEvent(Context,
                                     LOS_INIT_COMMAND_EVENT_BOOTSTRAP_COMPLETE,
                                     ExitStatus);

    if (Verbose != 0U)
    {
        LosInitCommandWriteText("[InitCmd] Context version: ");
        LosInitCommandWriteUnsigned((UINT64)Context->Version);
        LosInitCommandWriteText("\n");
        LosInitCommandWriteText("[InitCmd] Context flags: ");
        LosInitCommandWriteUnsigned((UINT64)Context->Flags);
        LosInitCommandWriteText("\n");
        LosInitCommandWriteText("[InitCmd] Message bytes: ");
        LosInitCommandWriteUnsigned((UINT64)Hello.HeaderBytes);
        LosInitCommandWriteText("\n");
        LosInitCommandWriteText("[InitCmd] Message request id: ");
        LosInitCommandWriteUnsigned(Hello.RequestValue);
        LosInitCommandWriteText("\n");
        LosInitCommandWriteText("[InitCmd] Send status: ");
        LosInitCommandWriteUnsigned(SendStatus);
        LosInitCommandWriteText("\n");
        LosInitCommandWriteText("[InitCmd] Receive status: ");
        LosInitCommandWriteUnsigned(ReceiveStatus);
        LosInitCommandWriteText("\n");
        LosInitCommandWriteText("[InitCmd] Receive-event status: ");
        LosInitCommandWriteUnsigned(ReceiveEventStatus);
        LosInitCommandWriteText("\n");
        LosInitCommandWriteText("[InitCmd] Launch status: ");
        LosInitCommandWriteUnsigned(LaunchStatus);
        LosInitCommandWriteText("\n");
        LosInitCommandWriteText("[InitCmd] Final exit status: ");
        LosInitCommandWriteUnsigned(ExitStatus);
        LosInitCommandWriteText("\n");
    }

    if (Verbose != 0U)
    {
        LosInitCommandDescribeCapabilities(&Context->Capabilities);
    }
    LosInitCommandWriteText("[InitCmd] Returning to kernel.\n");
    LosInitCommandReturnToKernel(ExitStatus);
}
