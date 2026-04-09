/*
 * File Name: ShellMain.c
 * File Version: 0.4.21
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-08T15:20:00Z
 * Last Update Timestamp: 2026-04-08T19:05:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#include "ShellMain.h"

static BOOLEAN LosShellTextEqual(const char *Left, const char *Right)
{
    UINTN Index;
    if (Left == 0 || Right == 0) { return 0; }
    for (Index = 0U;; ++Index)
    {
        if (Left[Index] != Right[Index]) { return 0; }
        if (Left[Index] == 0) { return 1; }
    }
}

static UINTN LosShellTextLength(const char *Text)
{
    UINTN Length = 0U;
    if (Text == 0) { return 0U; }
    while (Text[Length] != 0) { ++Length; }
    return Length;
}

static const char *LosShellSkipSpaces(const char *Text)
{
    if (Text == 0) { return 0; }
    while (*Text == ' ' || *Text == '	') { ++Text; }
    return Text;
}

static void LosShellNextToken(const char *Text, char *Token, UINTN TokenLength, const char **Remainder)
{
    UINTN Index = 0U;
    Text = LosShellSkipSpaces(Text);
    if (Token != 0 && TokenLength != 0U) { Token[0] = 0; }
    if (Text == 0) { if (Remainder != 0) { *Remainder = 0; } return; }
    while (Text[Index] != 0 && Text[Index] != ' ' && Text[Index] != '	')
    {
        if (Token != 0 && Index + 1U < TokenLength) { Token[Index] = Text[Index]; }
        ++Index;
    }
    if (Token != 0 && TokenLength != 0U)
    {
        if (Index < TokenLength) { Token[Index] = 0; } else { Token[TokenLength - 1U] = 0; }
    }
    if (Remainder != 0) { *Remainder = LosShellSkipSpaces(Text + Index); }
}

static BOOLEAN LosShellTextStartsWith(const char *Text, const char *Prefix)
{
    UINTN Index;
    if (Text == 0 || Prefix == 0) { return 0; }
    for (Index = 0U; Prefix[Index] != 0; ++Index)
    {
        if (Text[Index] != Prefix[Index]) { return 0; }
    }
    return 1;
}

static void LosShellBuildExternalPath(const char *CommandName, char *Path, UINTN PathLength)
{
    LosShellServiceCopyText(Path, PathLength, "\\LIBERATION\\COMMANDS\\");
    LosShellServiceAppendText(Path, PathLength, CommandName);
    if (!LosShellTextStartsWith(CommandName + (LosShellTextLength(CommandName) >= 4U ? LosShellTextLength(CommandName) - 4U : 0U), ".ELF"))
    {
        LosShellServiceAppendText(Path, PathLength, ".ELF");
    }
}

static UINT64 LosShellHandleBuiltin(const char *Name, const char *Arguments, char *Output, UINTN OutputLength)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    if (Name == 0 || Name[0] == 0 || LosShellTextEqual(Name, "help") || LosShellTextEqual(Name, "HELP"))
    {
        LosShellServiceCopyText(Output, OutputLength, "internal: help version status pwd cd echo clear ; external: login <user> <password>, run <name> [args], <name> [args]");
        return LOS_SHELL_SERVICE_STATUS_SUCCESS;
    }
    if (LosShellTextEqual(Name, "version") || LosShellTextEqual(Name, "VERSION"))
    {
        LosShellServiceCopyText(Output, OutputLength, "Liberation shell service version 0.4.21");
        return LOS_SHELL_SERVICE_STATUS_SUCCESS;
    }
    if (LosShellTextEqual(Name, "status") || LosShellTextEqual(Name, "STATUS"))
    {
        LosShellServiceCopyText(Output, OutputLength, "online=");
        LosShellServiceAppendText(Output, OutputLength, State->Online ? "1" : "0");
        LosShellServiceAppendText(Output, OutputLength, " transportReady=");
        LosShellServiceAppendText(Output, OutputLength, State->TransportReady ? "1" : "0");
        LosShellServiceAppendText(Output, OutputLength, " loggedIn=");
        LosShellServiceAppendText(Output, OutputLength, State->Authenticated ? "1" : "0");
        LosShellServiceAppendText(Output, OutputLength, " cwd=");
        LosShellServiceAppendText(Output, OutputLength, State->WorkingDirectory);
        return LOS_SHELL_SERVICE_STATUS_SUCCESS;
    }
    if (LosShellTextEqual(Name, "pwd") || LosShellTextEqual(Name, "PWD"))
    {
        LosShellServiceCopyText(Output, OutputLength, State->WorkingDirectory);
        return LOS_SHELL_SERVICE_STATUS_SUCCESS;
    }
    if (LosShellTextEqual(Name, "cd") || LosShellTextEqual(Name, "CD"))
    {
        Arguments = LosShellSkipSpaces(Arguments);
        if (Arguments == 0 || Arguments[0] == 0)
        {
            LosShellServiceCopyText(Output, OutputLength, "cd requires a path");
            return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
        }
        LosShellServiceCopyText(State->WorkingDirectory, sizeof(State->WorkingDirectory), Arguments);
        LosShellServiceCopyText(Output, OutputLength, State->WorkingDirectory);
        return LOS_SHELL_SERVICE_STATUS_SUCCESS;
    }
    if (LosShellTextEqual(Name, "echo") || LosShellTextEqual(Name, "ECHO"))
    {
        LosShellServiceCopyText(Output, OutputLength, Arguments != 0 ? Arguments : "");
        return LOS_SHELL_SERVICE_STATUS_SUCCESS;
    }
    if (LosShellTextEqual(Name, "clear") || LosShellTextEqual(Name, "CLEAR"))
    {
        LosShellServiceCopyText(Output, OutputLength, "clear requested");
        return LOS_SHELL_SERVICE_STATUS_SUCCESS;
    }
    return LOS_SHELL_SERVICE_STATUS_UNKNOWN_COMMAND;
}

static void LosShellServiceInitializeMailbox(LOS_SHELL_SERVICE_MAILBOX *Mailbox, UINT64 EndpointId)
{
    if (Mailbox == 0) { return; }
    Mailbox->Version = LOS_SHELL_SERVICE_VERSION;
    Mailbox->State = LOS_SHELL_SERVICE_MAILBOX_STATE_EMPTY;
    Mailbox->Flags = LOS_SHELL_SERVICE_FLAG_TRANSPORT_LIVE | LOS_SHELL_SERVICE_FLAG_BOOTSTRAP_SHARED_MEMORY | LOS_SHELL_SERVICE_FLAG_SERVICE_VISIBLE;
    Mailbox->Signature = LOS_SHELL_SERVICE_MAILBOX_SIGNATURE;
    Mailbox->Sequence = 0ULL;
    Mailbox->EndpointId = EndpointId;
    Mailbox->MessageBytes = 0ULL;
}

static void LosShellServiceAttachBootstrapChannel(void)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    State->TransportGeneration += 1ULL;
    State->RequestEndpointId = 0x53484C0100000001ULL;
    State->ResponseEndpointId = 0x53484C0100000002ULL;
    State->EventEndpointId = 0x53484C0100000003ULL;
    State->TransportReady = 1ULL;
    State->ServiceId = 0x53484C4C00000001ULL;
    State->RegistryReady = 1ULL;
    LosShellServiceInitializeMailbox(&State->RequestMailbox, State->RequestEndpointId);
    LosShellServiceInitializeMailbox(&State->ResponseMailbox, State->ResponseEndpointId);
    State->RequestMailboxAddress = (UINT64)(UINTN)&State->RequestMailbox;
    State->ResponseMailboxAddress = (UINT64)(UINTN)&State->ResponseMailbox;
    State->Channel.Version = LOS_SHELL_SERVICE_VERSION;
    State->Channel.Flags = LOS_SHELL_SERVICE_FLAG_TRANSPORT_LIVE | LOS_SHELL_SERVICE_FLAG_BOOTSTRAP_SHARED_MEMORY | LOS_SHELL_SERVICE_FLAG_EVENT_LIVE | LOS_SHELL_SERVICE_FLAG_SERVICE_VISIBLE | LOS_SHELL_SERVICE_FLAG_REGISTRY_VISIBLE;
    State->Channel.Attached = 1U;
    State->Channel.Signature = LOS_SHELL_SERVICE_CHANNEL_SIGNATURE;
    State->Channel.ConnectionNonce = 0x5348454C4C424F4FULL;
    State->Channel.RequestEndpointId = State->RequestEndpointId;
    State->Channel.ResponseEndpointId = State->ResponseEndpointId;
    State->Channel.EventEndpointId = State->EventEndpointId;
    State->Channel.RequestMailboxAddress = State->RequestMailboxAddress;
    State->Channel.RequestMailboxSize = sizeof(State->RequestMailbox);
    State->Channel.ResponseMailboxAddress = State->ResponseMailboxAddress;
    State->Channel.ResponseMailboxSize = sizeof(State->ResponseMailbox);
    State->DiscoveryRecord.Version = LOS_SHELL_SERVICE_VERSION;
    State->DiscoveryRecord.Flags = State->Channel.Flags;
    State->DiscoveryRecord.TransportState = 1U;
    State->DiscoveryRecord.Signature = LOS_SHELL_SERVICE_DISCOVERY_SIGNATURE;
    State->DiscoveryRecord.Generation = State->TransportGeneration;
    State->DiscoveryRecord.ConnectionNonce = State->Channel.ConnectionNonce;
    State->DiscoveryRecord.RequestEndpointId = State->RequestEndpointId;
    State->DiscoveryRecord.ResponseEndpointId = State->ResponseEndpointId;
    State->DiscoveryRecord.EventEndpointId = State->EventEndpointId;
    State->DiscoveryRecord.RequestMailboxAddress = State->RequestMailboxAddress;
    State->DiscoveryRecord.RequestMailboxSize = sizeof(State->RequestMailbox);
    State->DiscoveryRecord.ResponseMailboxAddress = State->ResponseMailboxAddress;
    State->DiscoveryRecord.ResponseMailboxSize = sizeof(State->ResponseMailbox);
    LosShellServiceCopyText(State->DiscoveryRecord.ServiceName, sizeof(State->DiscoveryRecord.ServiceName), "shell");
    State->RegistryEntry.Version = LOS_SHELL_SERVICE_VERSION;
    State->RegistryEntry.Flags = State->Channel.Flags;
    State->RegistryEntry.RegistryState = 1U;
    State->RegistryEntry.Signature = LOS_SHELL_SERVICE_REGISTRY_ENTRY_SIGNATURE;
    State->RegistryEntry.ServiceId = State->ServiceId;
    State->RegistryEntry.Generation = State->TransportGeneration;
    State->RegistryEntry.ExpectedVersion = LOS_SHELL_SERVICE_VERSION;
    LosShellServiceCopyText(State->RegistryEntry.ServiceName, sizeof(State->RegistryEntry.ServiceName), "shell");
    State->TransportExport.Version = LOS_SHELL_SERVICE_VERSION;
    State->TransportExport.Flags = State->Channel.Flags;
    State->TransportExport.TransportState = 1U;
    State->TransportExport.Signature = LOS_SHELL_SERVICE_TRANSPORT_EXPORT_SIGNATURE;
    State->TransportExport.Generation = State->TransportGeneration;
    State->TransportExport.ConnectionNonce = State->Channel.ConnectionNonce;
    State->TransportExport.RequestEndpointId = State->RequestEndpointId;
    State->TransportExport.ResponseEndpointId = State->ResponseEndpointId;
    State->TransportExport.EventEndpointId = State->EventEndpointId;
    State->TransportExport.RequestMailboxAddress = State->RequestMailboxAddress;
    State->TransportExport.RequestMailboxSize = sizeof(State->RequestMailbox);
    State->TransportExport.ResponseMailboxAddress = State->ResponseMailboxAddress;
    State->TransportExport.ResponseMailboxSize = sizeof(State->ResponseMailbox);
    LosShellServiceCopyText(State->TransportExport.ServiceName, sizeof(State->TransportExport.ServiceName), "shell");
}

BOOLEAN LosShellServiceBringOnline(void)
{
    LOS_SHELL_SERVICE_STATE *State;
    LosShellServiceInitialize();
    State = LosShellServiceState();
    LosShellServiceAttachBootstrapChannel();
    LosShellServiceWriteLine("[Shell] bootstrap starting.");
    LosShellServiceWriteLine("[Shell] login is required before commands run.");
    LosShellServiceWriteLine("[Shell] login is an external command at \LIBERATION\COMMANDS\LOGIN.ELF.");
    LosShellServiceWriteLine("[Shell] authenticated commands are uppercased through \LIBERATION\LIBRARIES\STRING.ELF.");
    LosShellServiceWriteLine("[Shell] external commands resolve under \LIBERATION\COMMANDS\*.ELF.");
    State->Online = 1U;
    (void)LosShellServiceSignalEvent(LOS_SHELL_SERVICE_EVENT_ONLINE, State->TransportGeneration);
    LosShellServiceWriteLine("[Shell] ONLINE.");
    return 1;
}

void LosShellServiceShowPrompt(void)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    char Prompt[16];
    State->PromptCount += 1ULL;
    LosShellServicePreparePromptText(Prompt, sizeof(Prompt));
    LosShellServiceWriteText(Prompt);
    LosShellServiceWriteText(" ");
}

UINT64 LosShellServiceExecuteCommand(const char *Command, char *Output, UINTN OutputLength, UINT32 *ResponseFlags)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    char Name[32];
    char ResolvedPath[LOS_SHELL_SERVICE_INPUT_BUFFER_LENGTH];
    char EffectiveCommand[LOS_SHELL_SERVICE_INPUT_BUFFER_LENGTH];
    const char *Arguments = 0;
    UINT64 Status;
    UINT64 CommandResult = LOS_LOGIN_COMMAND_RESULT_NONE;
    BOOLEAN ForceExternal = 0;

    if (Output != 0 && OutputLength != 0U) { Output[0] = 0; }
    if (ResponseFlags != 0) { *ResponseFlags = 0U; }
    LosShellServiceCopyText(State->LastCommand, sizeof(State->LastCommand), Command);
    LosShellServiceCopyText(EffectiveCommand, sizeof(EffectiveCommand), Command);

    LosShellNextToken(EffectiveCommand, Name, sizeof(Name), &Arguments);
    if (State->LoginRequired != 0ULL && State->Authenticated == 0ULL && !LosShellTextEqual(Name, "login") && !LosShellTextEqual(Name, "LOGIN"))
    {
        if (ResponseFlags != 0) { *ResponseFlags |= LOS_SHELL_SERVICE_FLAG_AUTH_REQUIRED; }
        LosShellServiceCopyText(Output, OutputLength, "login required: run external command login <user> <password>");
        return LOS_SHELL_SERVICE_STATUS_RETRY;
    }

    if (State->Authenticated != 0ULL)
    {
        Status = LosShellServiceUppercaseCommandExternal(EffectiveCommand, EffectiveCommand, sizeof(EffectiveCommand));
        if (Status == LOS_STRING_LIBRARY_STATUS_SUCCESS)
        {
            LosShellServiceCopyText(State->LastNormalizedCommand, sizeof(State->LastNormalizedCommand), EffectiveCommand);
            if (ResponseFlags != 0) { *ResponseFlags |= LOS_SHELL_SERVICE_FLAG_COMMAND_UPPERCASED; }
        }
        else
        {
            LosShellServiceCopyText(Output, OutputLength, "string library failed to uppercase command");
            return LOS_SHELL_SERVICE_STATUS_UNSUPPORTED;
        }
        LosShellNextToken(EffectiveCommand, Name, sizeof(Name), &Arguments);
    }
    else
    {
        LosShellServiceCopyText(State->LastNormalizedCommand, sizeof(State->LastNormalizedCommand), EffectiveCommand);
    }

    if (LosShellTextEqual(Name, "run") || LosShellTextEqual(Name, "RUN"))
    {
        ForceExternal = 1;
        LosShellNextToken(Arguments, Name, sizeof(Name), &Arguments);
    }

    if (!ForceExternal)
    {
        Status = LosShellHandleBuiltin(Name, Arguments, Output, OutputLength);
        if (Status != LOS_SHELL_SERVICE_STATUS_UNKNOWN_COMMAND)
        {
            if (ResponseFlags != 0) { *ResponseFlags |= LOS_SHELL_SERVICE_FLAG_INTERNAL_COMMAND; }
            return Status;
        }
    }

    if (Name[0] == 0)
    {
        LosShellServiceCopyText(Output, OutputLength, "no command provided");
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    }

    if (LosShellTextEqual(Name, "login"))
    {
        LosShellServiceCopyText(Name, sizeof(Name), "LOGIN");
    }

    LosShellBuildExternalPath(Name, ResolvedPath, sizeof(ResolvedPath));
    LosShellServiceCopyText(State->LastResolvedPath, sizeof(State->LastResolvedPath), ResolvedPath);
    if (ResponseFlags != 0) { *ResponseFlags |= LOS_SHELL_SERVICE_FLAG_EXTERNAL_COMMAND; }
    Status = LosShellServiceLaunchExternal(ResolvedPath, Arguments, &CommandResult, Output, OutputLength);
    State->LastLaunchStatus = Status;
    if (LosShellTextEqual(Name, "LOGIN") || LosShellTextEqual(Name, "login"))
    {
        if (Status == LOS_SHELL_SERVICE_STATUS_SUCCESS && CommandResult == LOS_LOGIN_COMMAND_RESULT_AUTHENTICATED)
        {
            State->Authenticated = 1ULL;
            LosShellServiceCopyText(Output, OutputLength, "login successful");
        }
        else if (Status == LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED || CommandResult == LOS_LOGIN_COMMAND_RESULT_DENIED)
        {
            State->Authenticated = 0ULL;
            LosShellServiceCopyText(Output, OutputLength, "login failed");
            return LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED;
        }
    }
    if (Status == LOS_SHELL_SERVICE_STATUS_LAUNCH_FAILED)
    {
        LosShellServiceAppendText(Output, OutputLength, " path=");
        LosShellServiceAppendText(Output, OutputLength, ResolvedPath);
    }
    return Status;
}

UINT64 LosShellServiceHandleRequest(const LOS_SHELL_SERVICE_REQUEST *Request, LOS_SHELL_SERVICE_RESPONSE *Response)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    UINT64 Status;
    LosShellServiceClearResponse(Response);
    if (Request == 0 || Response == 0) { return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; }
    Response->RequestId = Request->RequestId;
    Response->Sequence = Request->Sequence;
    Response->PromptIndex = State->PromptCount + 1ULL;
    if (!State->Online) { Response->Status = LOS_SHELL_SERVICE_STATUS_OFFLINE; LosShellServiceCopyText(Response->Output, sizeof(Response->Output), "shell offline"); return Response->Status; }
    if (Request->Version != LOS_SHELL_SERVICE_VERSION || Request->Signature != LOS_SHELL_SERVICE_REQUEST_SIGNATURE) { Response->Status = LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; LosShellServiceCopyText(Response->Output, sizeof(Response->Output), "bad shell request header"); return Response->Status; }
    State->RequestsHandled += 1ULL;
    if (Request->Command == LOS_SHELL_SERVICE_COMMAND_QUERY)
    {
        if (Request->Argument0 == LOS_SHELL_SERVICE_QUERY_STATUS) { Response->Status = LOS_SHELL_SERVICE_STATUS_SUCCESS; LosShellServiceCopyText(Response->Output, sizeof(Response->Output), State->Authenticated ? "online logged-in" : "online login-required"); }
        else if (Request->Argument0 == LOS_SHELL_SERVICE_QUERY_PROMPT) { Response->Status = LOS_SHELL_SERVICE_STATUS_SUCCESS; LosShellServicePreparePromptText(Response->Output, sizeof(Response->Output)); }
        else { Response->Status = LOS_SHELL_SERVICE_STATUS_UNSUPPORTED; LosShellServiceCopyText(Response->Output, sizeof(Response->Output), "unsupported shell query"); }
        State->ResponsesProduced += 1ULL; return Response->Status;
    }
    if (Request->Command != LOS_SHELL_SERVICE_COMMAND_EXECUTE)
    {
        Response->Status = LOS_SHELL_SERVICE_STATUS_UNSUPPORTED;
        LosShellServiceCopyText(Response->Output, sizeof(Response->Output), "unsupported shell command class");
        State->ResponsesProduced += 1ULL;
        return Response->Status;
    }
    Status = LosShellServiceExecuteCommand(Request->Text, Response->Output, sizeof(Response->Output), &Response->Flags);
    Response->Status = (UINT32)Status;
    Response->Result = Status;
    State->CommandSequence += 1ULL;
    State->LastStatus = Status;
    State->ResponsesProduced += 1ULL;
    return Status;
}

UINT64 LosShellServiceDispatchMailbox(LOS_SHELL_SERVICE_MAILBOX *RequestMailbox, LOS_SHELL_SERVICE_MAILBOX *ResponseMailbox)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    UINT64 Status;
    if (RequestMailbox == 0 || ResponseMailbox == 0) { return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; }
    if (RequestMailbox->Signature != LOS_SHELL_SERVICE_MAILBOX_SIGNATURE || ResponseMailbox->Signature != LOS_SHELL_SERVICE_MAILBOX_SIGNATURE) { return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; }
    if (RequestMailbox->State != LOS_SHELL_SERVICE_MAILBOX_STATE_READY) { return LOS_SHELL_SERVICE_STATUS_RETRY; }
    Status = LosShellServiceHandleRequest(&RequestMailbox->Payload.Request, &ResponseMailbox->Payload.Response);
    ResponseMailbox->Version = LOS_SHELL_SERVICE_VERSION;
    ResponseMailbox->Sequence = RequestMailbox->Sequence;
    ResponseMailbox->EndpointId = State->ResponseEndpointId;
    ResponseMailbox->MessageBytes = sizeof(LOS_SHELL_SERVICE_RESPONSE);
    ResponseMailbox->State = LOS_SHELL_SERVICE_MAILBOX_STATE_READY;
    RequestMailbox->State = LOS_SHELL_SERVICE_MAILBOX_STATE_CONSUMED;
    State->Channel.RequestsConsumed += 1ULL;
    State->Channel.ResponsesCommitted += 1ULL;
    return Status;
}

static void LosShellServiceIssueSyntheticRequest(UINT64 Sequence, UINT32 Command, UINT64 Argument0, const char *Text)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    LosShellServiceCopyText((char *)&State->RequestMailbox.Payload.Request, sizeof(State->RequestMailbox.Payload.Request), "");
    State->RequestMailbox.Payload.Request.Version = LOS_SHELL_SERVICE_VERSION;
    State->RequestMailbox.Payload.Request.Command = Command;
    State->RequestMailbox.Payload.Request.Flags = LOS_SHELL_SERVICE_FLAG_CAPTURE_OUTPUT;
    State->RequestMailbox.Payload.Request.Signature = LOS_SHELL_SERVICE_REQUEST_SIGNATURE;
    State->RequestMailbox.Payload.Request.RequestId = Sequence;
    State->RequestMailbox.Payload.Request.Sequence = Sequence;
    State->RequestMailbox.Payload.Request.Argument0 = Argument0;
    LosShellServiceCopyText(State->RequestMailbox.Payload.Request.Text, sizeof(State->RequestMailbox.Payload.Request.Text), Text);
    State->RequestMailbox.Sequence = Sequence;
    State->RequestMailbox.MessageBytes = sizeof(LOS_SHELL_SERVICE_REQUEST);
    State->RequestMailbox.State = LOS_SHELL_SERVICE_MAILBOX_STATE_READY;
}

void LosShellServiceRunBootstrapSession(void)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    UINTN Index;
    static const char *Commands[] = { "help", "status", "login dave liberation", "echo shell ready" };
    for (Index = 0U; Index < sizeof(Commands) / sizeof(Commands[0]); ++Index)
    {
        LosShellServiceShowPrompt();
        LosShellServiceWriteLine(Commands[Index]);
        LosShellServiceIssueSyntheticRequest((UINT64)(Index + 1U), LOS_SHELL_SERVICE_COMMAND_EXECUTE, 0ULL, Commands[Index]);
        LosShellServiceDispatchMailbox(&State->RequestMailbox, &State->ResponseMailbox);
        LosShellServiceWriteText("[Shell] response: ");
        LosShellServiceWriteLine(State->ResponseMailbox.Payload.Response.Output);
        State->ResponseMailbox.State = LOS_SHELL_SERVICE_MAILBOX_STATE_EMPTY;
    }
}

void LosShellServiceEntry(void)
{
    if (!LosShellServiceBringOnline()) { return; }
    LosShellServiceRunBootstrapSession();
    for (;;) { LosShellServiceYield(); }
}

void LosShellServiceBootstrapEntryWithContext(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context)
{
    (void)Context;
    if (LosShellServiceBringOnline()) { LosShellServiceRunBootstrapSession(); }
}

void LosShellServiceBootstrapEntry(void)
{
    LosShellServiceBootstrapEntryWithContext((const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *)0);
}
