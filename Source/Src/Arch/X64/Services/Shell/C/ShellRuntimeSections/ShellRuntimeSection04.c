/*
 * File Name: ShellRuntimeSection04.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from ShellRuntime.c.
 */

const LOS_SHELL_SERVICE_TRANSPORT_EXPORT *LosShellServiceExportTransport(void)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    if (State == 0 || State->Online == 0U || State->TransportReady == 0ULL) { return 0; }
    return &State->TransportExport;
}

UINT64 LosShellServiceBindTransport(LOS_SHELL_SERVICE_TRANSPORT_BINDING *Binding)
{
    const LOS_SHELL_SERVICE_TRANSPORT_EXPORT *Exported;
    if (Binding == 0) { return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; }
    Exported = LosShellServiceExportTransport();
    if (Exported == 0 || Exported->Signature != LOS_SHELL_SERVICE_TRANSPORT_EXPORT_SIGNATURE) { return LOS_SHELL_SERVICE_STATUS_NOT_ATTACHED; }
    Binding->Version = LOS_SHELL_SERVICE_VERSION;
    Binding->Flags = Exported->Flags;
    Binding->TransportState = Exported->TransportState;
    Binding->Signature = LOS_SHELL_SERVICE_TRANSPORT_BINDING_SIGNATURE;
    Binding->Generation = Exported->Generation;
    Binding->ConnectionNonce = Exported->ConnectionNonce;
    Binding->NextRequestSequence = 1ULL;
    Binding->RequestEndpointId = Exported->RequestEndpointId;
    Binding->ResponseEndpointId = Exported->ResponseEndpointId;
    Binding->EventEndpointId = Exported->EventEndpointId;
    Binding->RequestMailbox = (LOS_SHELL_SERVICE_MAILBOX *)(UINTN)Exported->RequestMailboxAddress;
    Binding->ResponseMailbox = (LOS_SHELL_SERVICE_MAILBOX *)(UINTN)Exported->ResponseMailboxAddress;
    return LOS_SHELL_SERVICE_STATUS_SUCCESS;
}

UINT64 LosShellServiceSignalEvent(UINT64 EventCode, UINT64 EventValue) { (void)EventCode; (void)EventValue; return LOS_SHELL_SERVICE_STATUS_SUCCESS; }

UINT64 LosShellServiceBindFromRequest(const LOS_SHELL_SERVICE_BIND_REQUEST *Request, LOS_SHELL_SERVICE_TRANSPORT_BINDING *Binding)
{
    const LOS_SHELL_SERVICE_DISCOVERY_RECORD *Discovery;
    if (Request == 0 || Binding == 0) { return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; }
    if (Request->Version != LOS_SHELL_SERVICE_VERSION || Request->Signature != LOS_SHELL_SERVICE_BIND_REQUEST_SIGNATURE) { return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; }
    Discovery = LosShellServiceGetDiscoveryRecord();
    if (Discovery == 0 || Discovery->Signature != LOS_SHELL_SERVICE_DISCOVERY_SIGNATURE) { return LOS_SHELL_SERVICE_STATUS_NOT_ATTACHED; }
    if (LosShellRuntimeTextEqual(Request->ServiceName, Discovery->ServiceName) == 0) { return LOS_SHELL_SERVICE_STATUS_NOT_FOUND; }
    if (Request->ExpectedVersion != LOS_SHELL_SERVICE_VERSION) { return LOS_SHELL_SERVICE_STATUS_UNSUPPORTED; }
    if ((Discovery->Flags & Request->RequiredFlags) != Request->RequiredFlags) { return LOS_SHELL_SERVICE_STATUS_UNSUPPORTED; }
    if (Request->RequestedGeneration != 0ULL && Request->RequestedGeneration != Discovery->Generation) { return LOS_SHELL_SERVICE_STATUS_RETRY; }
    return LosShellServiceBindTransport(Binding);
}

const LOS_SHELL_SERVICE_REGISTRY_ENTRY *LosShellServiceGetRegistryEntry(void)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    if (State == 0 || State->Online == 0U || State->RegistryReady == 0ULL) { return 0; }
    return &State->RegistryEntry;
}

UINT64 LosShellServiceResolveFromRequest(const LOS_SHELL_SERVICE_RESOLVE_REQUEST *Request, LOS_SHELL_SERVICE_RESOLVE_RESPONSE *Response)
{
    const LOS_SHELL_SERVICE_REGISTRY_ENTRY *Entry;
    if (Request == 0 || Response == 0) { return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; }
    LosShellServiceCopyText((char *)Response, sizeof(*Response), "");
    Response->Version = LOS_SHELL_SERVICE_VERSION;
    Response->Signature = LOS_SHELL_SERVICE_RESOLVE_RESPONSE_SIGNATURE;
    if (Request->Version != LOS_SHELL_SERVICE_VERSION || Request->Signature != LOS_SHELL_SERVICE_RESOLVE_REQUEST_SIGNATURE) {
        Response->Status = LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; return Response->Status; }
    Entry = LosShellServiceGetRegistryEntry();
    if (Entry == 0 || Entry->Signature != LOS_SHELL_SERVICE_REGISTRY_ENTRY_SIGNATURE) { Response->Status = LOS_SHELL_SERVICE_STATUS_NOT_ATTACHED; return Response->Status; }
    if (LosShellRuntimeTextEqual(Request->ServiceName, Entry->ServiceName) == 0) { Response->Status = LOS_SHELL_SERVICE_STATUS_NOT_FOUND; return Response->Status; }
    if (Request->ExpectedVersion != Entry->ExpectedVersion) { Response->Status = LOS_SHELL_SERVICE_STATUS_UNSUPPORTED; return Response->Status; }
    if ((Entry->Flags & Request->RequiredFlags) != Request->RequiredFlags) { Response->Status = LOS_SHELL_SERVICE_STATUS_UNSUPPORTED; return Response->Status; }
    Response->Status = LOS_SHELL_SERVICE_STATUS_SUCCESS;
    Response->Flags = Entry->Flags;
    Response->RegistryState = Entry->RegistryState;
    Response->ServiceId = Entry->ServiceId;
    Response->Generation = Entry->Generation;
    Response->ConnectionNonce = LosShellServiceState()->Channel.ConnectionNonce;
    LosShellServiceCopyText(Response->ServiceName, sizeof(Response->ServiceName), Entry->ServiceName);
    return Response->Status;
}

static UINT64 LosShellRuntimeCheckLoginCapability(const char *UserName,
                                                   LOS_CAPABILITIES_ACCESS_RESULT *Result)
{
    LOS_CAPABILITIES_ACCESS_REQUEST Request;
    UINTN Index;

    if (Result == 0 || UserName == 0 || UserName[0] == 0)
    {
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    }

    for (Index = 0U; Index < sizeof(Request); ++Index)
    {
        ((UINT8 *)&Request)[Index] = 0U;
    }
    for (Index = 0U; Index < sizeof(*Result); ++Index)
    {
        ((UINT8 *)Result)[Index] = 0U;
    }

    Request.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Request.PrincipalType = LOS_CAPABILITIES_PRINCIPAL_TYPE_USER;
    Request.AccessRight = LOS_CAPABILITIES_ACCESS_RIGHT_QUERY;
    Request.PrincipalId = 0ULL;
    LosShellServiceCopyText(Request.PrincipalName, sizeof(Request.PrincipalName), UserName);
    LosShellServiceCopyText(Request.Namespace, sizeof(Request.Namespace), "session");
    LosShellServiceCopyText(Request.Name, sizeof(Request.Name), "login");

    if (LosCapabilitiesServiceSubmitAccessRequest != 0)
    {
        return (UINT64)LosCapabilitiesServiceSubmitAccessRequest(&Request, Result);
    }
    if (LosCapabilitiesServiceCheckAccess != 0)
    {
        return (UINT64)LosCapabilitiesServiceCheckAccess(&Request, Result);
    }

    Result->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_UNSUPPORTED;
    Result->Granted = 0U;
    Result->MatchingGrantId = 0ULL;
    return LOS_CAPABILITIES_SERVICE_STATUS_UNSUPPORTED;
}

static UINT64 LosShellRuntimeAuthenticateLoginBootstrap(const char *Arguments,
                                                        UINT64 *CommandResult,
                                                        char *Output,
                                                        UINTN OutputLength)
{
    char UserName[32];
    char Password[32];
    const char *Remainder = 0;
    LOS_CAPABILITIES_ACCESS_RESULT AccessResult;
    UINT64 CapabilityStatus;

    if (CommandResult != 0)
    {
        *CommandResult = LOS_LOGIN_COMMAND_RESULT_NONE;
    }
    LosShellRuntimeNextToken(Arguments, UserName, sizeof(UserName), &Remainder);
    LosShellRuntimeNextToken(Remainder, Password, sizeof(Password), &Remainder);
    if (UserName[0] == 0 || Password[0] == 0)
    {
        if (CommandResult != 0)
        {
            *CommandResult = LOS_LOGIN_COMMAND_RESULT_BAD_REQUEST;
        }
        LosShellServiceCopyText(Output, OutputLength, "usage: login <user> <password>");
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    }

    if (LosShellRuntimeTextEqual(Password, "liberation") == 0)
    {
        if (CommandResult != 0)
        {
            *CommandResult = LOS_LOGIN_COMMAND_RESULT_DENIED;
        }
        LosShellServiceCopyText(Output, OutputLength, "login denied");
        return LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED;
    }

    CapabilityStatus = LosShellRuntimeCheckLoginCapability(UserName, &AccessResult);
    if (CapabilityStatus != LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS ||
        AccessResult.Status != LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS ||
        AccessResult.Granted == 0U)
    {
        if (CommandResult != 0)
        {
            *CommandResult = LOS_LOGIN_COMMAND_RESULT_DENIED;
        }
        if (CapabilityStatus == LOS_CAPABILITIES_SERVICE_STATUS_UNSUPPORTED)
        {
            LosShellServiceCopyText(Output, OutputLength, "login denied: capability service unavailable");
            return LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED;
        }
        LosShellServiceCopyText(Output, OutputLength, "login denied by capability policy");
        return LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED;
    }

    if (CommandResult != 0)
    {
        *CommandResult = LOS_LOGIN_COMMAND_RESULT_AUTHENTICATED;
    }
    LosShellServiceCopyText(Output, OutputLength, "login authenticated by capability service");
    return LOS_SHELL_SERVICE_STATUS_SUCCESS;
}

UINT64 LosShellServiceLaunchExternal(const char *CommandPath, const char *Arguments, UINT64 *CommandResult, char *Output, UINTN OutputLength)
{
    UINT64 Status;
    if (CommandResult != 0)
    {
        *CommandResult = LOS_LOGIN_COMMAND_RESULT_NONE;
    }
    Status = LosUserLaunchExternalCommand(CommandPath, Arguments, CommandResult, Output, OutputLength);
    if (Status == LOS_SHELL_SERVICE_STATUS_SUCCESS || Status == LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED)
    {
        return Status;
    }
    if (Status == LOS_SHELL_SERVICE_STATUS_UNSUPPORTED && LosShellRuntimeTextEndsWith(CommandPath, "\\LIBERATION\\COMMANDS\\LOGIN.ELF"))
    {
        return LosShellRuntimeAuthenticateLoginBootstrap(Arguments, CommandResult, Output, OutputLength);
    }
    if (Status == LOS_SHELL_SERVICE_STATUS_UNSUPPORTED)
    {
        LosShellServiceCopyText(Output, OutputLength, "external launch ABI not wired yet");
        return LOS_SHELL_SERVICE_STATUS_LAUNCH_FAILED;
    }
    LosShellServiceCopyText(Output, OutputLength, "external command launch failed");
    return LOS_SHELL_SERVICE_STATUS_LAUNCH_FAILED;
}
