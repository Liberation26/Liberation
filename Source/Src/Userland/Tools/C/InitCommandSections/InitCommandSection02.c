/*
 * File Name: InitCommandSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from InitCommand.c.
 */

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
