/*
 * File Name: ShellRuntimeSection03.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from ShellRuntime.c.
 */

static void LosShellRuntimeCommitCompletionToShell(LOS_USER_IMAGE_CALL *Call)
{
    if (Call == 0)
    {
        return;
    }

    if (Call->ResultAddress != 0ULL && Call->ResultSize >= sizeof(UINT64))
    {
        *(UINT64 *)(UINTN)Call->ResultAddress = LosShellRuntimeCompletionRecord.ResultValue;
    }
}


UINT64 LosShellRuntimeInvokeUserImageCall(LOS_USER_IMAGE_CALL *Call)
{
    UINT64 Status;

    if (Call == 0)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }

    LosShellRuntimeLastCall = *Call;
    LosShellRuntimeStageCompletion(LOS_USER_IMAGE_CALL_STATUS_TRANSITION_FAILED, 0ULL);
    Status = LosUserLoadAndCallImage(Call);
    if (Status != LOS_USER_IMAGE_CALL_STATUS_SUCCESS)
    {
        LosShellRuntimeStageCompletion(Status, 0ULL);
    }
    LosShellRuntimeCommitCompletionToShell(Call);
    return LosShellRuntimeCompletionRecord.Status;
}

__attribute__((weak)) UINT64 LosUserLoadAndCallImage(const LOS_USER_IMAGE_CALL *Call)
{
    if (Call == 0)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }
    return LosShellRuntimeTryDiskBackedImage(Call);
}

static void LosShellRuntimeInitializeImageCall(LOS_USER_IMAGE_CALL *Call,
                                               UINT32 CallKind,
                                               const char *Path,
                                               const void *Request,
                                               UINTN RequestSize,
                                               void *Response,
                                               UINTN ResponseSize,
                                               void *Result,
                                               UINTN ResultSize)
{
    if (Call == 0)
    {
        return;
    }
    LosShellRuntimeZeroMemory(Call, sizeof(*Call));
    Call->Version = LOS_USER_IMAGE_CALL_VERSION;
    Call->CallKind = CallKind;
    Call->Signature = LOS_USER_IMAGE_CALL_SIGNATURE;
    Call->RequestAddress = (UINT64)(UINTN)Request;
    Call->RequestSize = (UINT64)RequestSize;
    Call->ResponseAddress = (UINT64)(UINTN)Response;
    Call->ResponseSize = (UINT64)ResponseSize;
    Call->ResultAddress = (UINT64)(UINTN)Result;
    Call->ResultSize = (UINT64)ResultSize;
    LosShellServiceCopyText(Call->Path, sizeof(Call->Path), Path);
}

__attribute__((weak)) UINT64 LosUserInvokeStringLibrary(const char *LibraryPath,
                                                        const LOS_STRING_LIBRARY_REQUEST *Request,
                                                        LOS_STRING_LIBRARY_RESPONSE *Response)
{
    LOS_USER_IMAGE_CALL Call;
    UINT64 Status;

    if (LibraryPath == 0 || Request == 0 || Response == 0)
    {
        return LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    }

    LosShellRuntimeInitializeImageCall(&Call,
                                       LOS_USER_IMAGE_CALL_KIND_LIBRARY,
                                       LibraryPath,
                                       Request,
                                       sizeof(*Request),
                                       Response,
                                       sizeof(*Response),
                                       0,
                                       0U);
    Status = LosShellRuntimeInvokeUserImageCall(&Call);
    switch (Status)
    {
    case LOS_USER_IMAGE_CALL_STATUS_SUCCESS:
        return LOS_STRING_LIBRARY_STATUS_SUCCESS;
    case LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER:
        return LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    case LOS_USER_IMAGE_CALL_STATUS_TRUNCATED:
        return LOS_STRING_LIBRARY_STATUS_TRUNCATED;
    default:
        return LOS_STRING_LIBRARY_STATUS_UNSUPPORTED;
    }
}

__attribute__((weak)) UINT64 LosUserLaunchExternalCommand(const char *Path,
                                                          const char *Arguments,
                                                          UINT64 *CommandResult,
                                                          char *Output,
                                                          UINTN OutputLength)
{
    LOS_USER_IMAGE_CALL Call;
    UINT64 Status;

    if (CommandResult != 0)
    {
        *CommandResult = LOS_LOGIN_COMMAND_RESULT_NONE;
    }
    if (Output != 0 && OutputLength != 0U)
    {
        Output[0] = 0;
    }
    if (Path == 0)
    {
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    }

    LosShellRuntimeInitializeImageCall(&Call,
                                       LOS_USER_IMAGE_CALL_KIND_COMMAND,
                                       Path,
                                       Arguments,
                                       Arguments != 0 ? (LosShellRuntimeTextLength(Arguments) + 1U) : 0U,
                                       Output,
                                       OutputLength,
                                       CommandResult,
                                       CommandResult != 0 ? sizeof(*CommandResult) : 0U);
    Status = LosShellRuntimeInvokeUserImageCall(&Call);
    switch (Status)
    {
    case LOS_USER_IMAGE_CALL_STATUS_SUCCESS:
        return LOS_SHELL_SERVICE_STATUS_SUCCESS;
    case LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER:
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    case LOS_USER_IMAGE_CALL_STATUS_ACCESS_DENIED:
        return LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED;
    case LOS_USER_IMAGE_CALL_STATUS_TRUNCATED:
        return LOS_SHELL_SERVICE_STATUS_TRUNCATED;
    case LOS_USER_IMAGE_CALL_STATUS_NOT_FOUND:
        return LOS_SHELL_SERVICE_STATUS_NOT_FOUND;
    case LOS_USER_IMAGE_CALL_STATUS_UNSUPPORTED:
        return LOS_SHELL_SERVICE_STATUS_UNSUPPORTED;
    default:
        return LOS_SHELL_SERVICE_STATUS_LAUNCH_FAILED;
    }
}

LOS_SHELL_SERVICE_STATE *LosShellServiceState(void) { return &LosShellGlobalState; }
void LosShellServiceWriteText(const char *Text) { if (Text != 0) { LosUserWriteText(Text); } }
void LosShellServiceWriteUnsigned(UINT64 Value) { LosUserWriteUnsigned(Value); }
void LosShellServiceWriteLine(const char *Text) { LosShellServiceWriteText(Text); LosShellServiceWriteText("\n"); }
void LosShellServiceYield(void) { __asm__ __volatile__("pause"); }

void LosShellServiceCopyText(char *Destination, UINTN DestinationLength, const char *Source)
{
    UINTN Index;
    if (Destination == 0 || DestinationLength == 0U) { return; }
    for (Index = 0U; Index < DestinationLength; ++Index) { Destination[Index] = 0; }
    if (Source == 0) { return; }
    for (Index = 0U; Index + 1U < DestinationLength && Source[Index] != 0; ++Index) { Destination[Index] = Source[Index]; }
}

void LosShellServiceAppendText(char *Destination, UINTN DestinationLength, const char *Source)
{
    UINTN Offset = 0U;
    UINTN Index;
    if (Destination == 0 || DestinationLength == 0U || Source == 0) { return; }
    while (Offset + 1U < DestinationLength && Destination[Offset] != 0) { ++Offset; }
    for (Index = 0U; Offset + 1U < DestinationLength && Source[Index] != 0; ++Index, ++Offset) { Destination[Offset] = Source[Index]; }
    Destination[Offset < DestinationLength ? Offset : (DestinationLength - 1U)] = 0;
}


static char LosShellServiceUppercaseCharacter(char Character)
{
    if (Character >= 'a' && Character <= 'z')
    {
        return (char)(Character - ('a' - 'A'));
    }
    return Character;
}

static UINT64 LosShellServiceUppercaseBootstrapAdapter(const LOS_STRING_LIBRARY_REQUEST *Request,
                                                       LOS_STRING_LIBRARY_RESPONSE *Response)
{
    UINTN Index;
    BOOLEAN Changed = 0;

    if (Request == 0 || Response == 0)
    {
        return LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    }

    for (Index = 0U; Index < sizeof(*Response); ++Index)
    {
        ((UINT8 *)Response)[Index] = 0U;
    }
    Response->Version = LOS_STRING_LIBRARY_VERSION;
    Response->Status = LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    Response->Signature = LOS_STRING_LIBRARY_RESPONSE_SIGNATURE;

    if (Request->Version != LOS_STRING_LIBRARY_VERSION ||
        Request->Signature != LOS_STRING_LIBRARY_REQUEST_SIGNATURE ||
        Request->Operation != LOS_STRING_LIBRARY_OPERATION_UPPERCASE)
    {
        return Response->Status;
    }

    for (Index = 0U; Index + 1U < sizeof(Response->Output) && Request->Input[Index] != 0; ++Index)
    {
        Response->Output[Index] = LosShellServiceUppercaseCharacter(Request->Input[Index]);
        if (Response->Output[Index] != Request->Input[Index])
        {
            Changed = 1;
        }
    }

    Response->Status = LOS_STRING_LIBRARY_STATUS_SUCCESS;
    Response->Result = Changed ? LOS_STRING_LIBRARY_RESULT_CHANGED : LOS_STRING_LIBRARY_RESULT_UNCHANGED;
    Response->Flags = LOS_STRING_LIBRARY_FLAG_BOOTSTRAP_ADAPTER;
    if (Changed)
    {
        Response->Flags |= LOS_STRING_LIBRARY_FLAG_TEXT_CHANGED;
    }
    return Response->Status;
}

UINT64 LosShellServiceUppercaseCommandExternal(const char *Input, char *Output, UINTN OutputLength)
{
    LOS_STRING_LIBRARY_REQUEST Request;
    LOS_STRING_LIBRARY_RESPONSE Response;
    UINTN Index;
    UINT64 Status;

    if (Input == 0 || Output == 0 || OutputLength == 0U)
    {
        return LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    }

    for (Index = 0U; Index < sizeof(Request); ++Index)
    {
        ((UINT8 *)&Request)[Index] = 0U;
    }
    for (Index = 0U; Index < sizeof(Response); ++Index)
    {
        ((UINT8 *)&Response)[Index] = 0U;
    }

    Request.Version = LOS_STRING_LIBRARY_VERSION;
    Request.Operation = LOS_STRING_LIBRARY_OPERATION_UPPERCASE;
    Request.Signature = LOS_STRING_LIBRARY_REQUEST_SIGNATURE;
    LosShellServiceCopyText(Request.Input, sizeof(Request.Input), Input);

    if (LosUserInvokeStringLibrary != 0)
    {
        Status = LosUserInvokeStringLibrary("\\LIBERATION\\LIBRARIES\\STRING.ELF", &Request, &Response);
        if (Status == LOS_STRING_LIBRARY_STATUS_SUCCESS &&
            Response.Signature == LOS_STRING_LIBRARY_RESPONSE_SIGNATURE &&
            Response.Status == LOS_STRING_LIBRARY_STATUS_SUCCESS)
        {
            LosShellServiceCopyText(Output, OutputLength, Response.Output);
            return Response.Status;
        }
    }

    Status = LosShellServiceUppercaseBootstrapAdapter(&Request, &Response);
    if (Status == LOS_STRING_LIBRARY_STATUS_SUCCESS)
    {
        LosShellServiceCopyText(Output, OutputLength, Response.Output);
    }
    return Status;
}

void LosShellServiceClearResponse(LOS_SHELL_SERVICE_RESPONSE *Response)
{
    UINTN Index;
    if (Response == 0) { return; }
    for (Index = 0U; Index < sizeof(*Response); ++Index) { ((UINT8 *)Response)[Index] = 0U; }
    Response->Version = LOS_SHELL_SERVICE_VERSION;
    Response->Signature = LOS_SHELL_SERVICE_RESPONSE_SIGNATURE;
}

void LosShellServicePreparePromptText(char *Buffer, UINTN BufferLength)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    if (Buffer == 0 || BufferLength == 0U) { return; }
    if (State != 0 && State->LoginRequired != 0ULL && State->Authenticated == 0ULL)
    {
        LosShellServiceCopyText(Buffer, BufferLength, "login>");
        return;
    }
    LosShellServiceCopyText(Buffer, BufferLength, "shell>");
}

void LosShellServiceInitialize(void)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    UINTN Index;
    if (State == 0) { return; }
    for (Index = 0U; Index < sizeof(*State); ++Index) { ((UINT8 *)State)[Index] = 0U; }
    State->LoginRequired = 1ULL;
    LosShellServiceCopyText(State->ServiceName, sizeof(State->ServiceName), "shell");
    LosShellServiceCopyText(State->WorkingDirectory, sizeof(State->WorkingDirectory), "\\");
    LosShellServiceCopyText(State->LastNormalizedCommand, sizeof(State->LastNormalizedCommand), "");
}

const LOS_SHELL_SERVICE_DISCOVERY_RECORD *LosShellServiceGetDiscoveryRecord(void)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    if (State == 0 || State->Online == 0U || State->TransportReady == 0ULL) { return 0; }
    return &State->DiscoveryRecord;
}
