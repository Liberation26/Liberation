/*
 * File Name: InitCommandSection04.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from InitCommand.c.
 */

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

    if (ExitStatus == LOS_INIT_COMMAND_STATUS_SUCCESS)
    {
        LosInitCommandParkAfterBootstrap();
        return;
    }

    LosInitCommandWriteText("[InitCmd] Returning to kernel after bootstrap failure.\n");
    LosInitCommandReturnToKernel(ExitStatus);
}
