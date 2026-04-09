/*
 * File Name: CapabilitiesTransportSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from CapabilitiesTransport.c.
 */

BOOLEAN LosCapabilitiesServiceInitializeTransport(void)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;

    State = LosCapabilitiesServiceState();
    State->TransportGeneration += 1ULL;
    State->ServiceClass = LOS_CAPABILITIES_SERVICE_CLASS_SERVICE;
    State->ActiveEndpointClass = LOS_CAPABILITIES_ENDPOINT_CLASS_CONTROL;
    State->MinimumSecurityMode = LOS_CAPABILITIES_SECURITY_MODE_CONFIDENTIAL;
    State->ActiveSecurityMode = State->MinimumSecurityMode;
    State->TransportSessionId = 0x4341505300000000ULL | State->TransportGeneration;
    State->ActiveSessionId = State->TransportSessionId;
    State->ActiveClientNonce = 0ULL;
    State->ActiveServerNonce = 0ULL;
    State->ActiveClientPrincipalId = 0ULL;
    State->ActiveClientPrincipalType = 0U;
    State->LastAcceptedRequestSequence = 0ULL;
    State->LastCompletedResponseSequence = 0ULL;
    State->RequestAuthKey[0] = 0x4c4f532d43415053ULL ^ State->TransportSessionId;
    State->RequestAuthKey[1] = 0x5245512d41555448ULL ^ (State->TransportGeneration << 1U);
    State->ResponseAuthKey[0] = 0x4c4f532d5245504cULL ^ (State->TransportSessionId << 1U);
    State->ResponseAuthKey[1] = 0x59532d4155544821ULL ^ (State->TransportGeneration << 2U);
    State->ActiveRequestAuthKey[0] = State->RequestAuthKey[0];
    State->ActiveRequestAuthKey[1] = State->RequestAuthKey[1];
    State->ActiveResponseAuthKey[0] = State->ResponseAuthKey[0];
    State->ActiveResponseAuthKey[1] = State->ResponseAuthKey[1];
    State->ActiveRequestCipherKey[0] = State->RequestAuthKey[0] ^ 0x434950482d424f4fULL;
    State->ActiveRequestCipherKey[1] = State->RequestAuthKey[1] ^ 0x5453545241502121ULL;
    State->ActiveResponseCipherKey[0] = State->ResponseAuthKey[0] ^ 0x434950482d524550ULL;
    State->ActiveResponseCipherKey[1] = State->ResponseAuthKey[1] ^ 0x4c592d424f4f5421ULL;
    LosCapabilitiesCopyBytes(State->ServiceName, "capsmgr", sizeof("capsmgr"));

    State->ReceiveEndpoint.Signature = LOS_CAPABILITIES_ENDPOINT_REQUEST;
    State->ReceiveEndpoint.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    State->ReceiveEndpoint.Role = LOS_CAPABILITIES_ENDPOINT_ROLE_RECEIVE;
    State->ReceiveEndpoint.State = LOS_CAPABILITIES_ENDPOINT_STATE_ONLINE;
    State->ReceiveEndpoint.EndpointClass = LOS_CAPABILITIES_ENDPOINT_CLASS_CONTROL;
    State->ReceiveEndpoint.SecurityMode = State->MinimumSecurityMode;
    State->ReceiveEndpoint.Flags = LOS_CAPABILITIES_ENDPOINT_FLAG_SERVICE_VISIBLE |
                                   LOS_CAPABILITIES_ENDPOINT_FLAG_MAILBOX_ATTACHED |
                                   LOS_CAPABILITIES_ENDPOINT_FLAG_ISOLATED_TRANSPORT |
                                   LOS_CAPABILITIES_ENDPOINT_FLAG_MESSAGE_AUTH |
                                   LOS_CAPABILITIES_ENDPOINT_FLAG_ANTI_REPLAY |
                                   LOS_CAPABILITIES_ENDPOINT_FLAG_IDENTITY_BOUND |
                                   LOS_CAPABILITIES_ENDPOINT_FLAG_CONFIDENTIAL_CHANNEL |
                                   LOS_CAPABILITIES_ENDPOINT_FLAG_POLICY_DRIVEN_SECURITY;
    State->ReceiveEndpoint.EndpointId = LOS_CAPABILITIES_ENDPOINT_REQUEST;
    State->ReceiveEndpoint.MailboxAddress = (UINT64)(UINTN)&State->RequestMailbox;
    State->ReceiveEndpoint.MailboxSize = sizeof(State->RequestMailbox);
    State->ReceiveEndpoint.PeerEndpointId = LOS_CAPABILITIES_ENDPOINT_RESPONSE;

    State->ReplyEndpoint.Signature = LOS_CAPABILITIES_ENDPOINT_RESPONSE;
    State->ReplyEndpoint.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    State->ReplyEndpoint.Role = LOS_CAPABILITIES_ENDPOINT_ROLE_REPLY;
    State->ReplyEndpoint.State = LOS_CAPABILITIES_ENDPOINT_STATE_ONLINE;
    State->ReplyEndpoint.EndpointClass = State->ReceiveEndpoint.EndpointClass;
    State->ReplyEndpoint.SecurityMode = State->ReceiveEndpoint.SecurityMode;
    State->ReplyEndpoint.Flags = State->ReceiveEndpoint.Flags;
    State->ReplyEndpoint.EndpointId = LOS_CAPABILITIES_ENDPOINT_RESPONSE;
    State->ReplyEndpoint.MailboxAddress = (UINT64)(UINTN)&State->ResponseMailbox;
    State->ReplyEndpoint.MailboxSize = sizeof(State->ResponseMailbox);
    State->ReplyEndpoint.PeerEndpointId = LOS_CAPABILITIES_ENDPOINT_REQUEST;

    State->RequestMailbox.Header.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    State->RequestMailbox.Header.SlotCount = LOS_CAPABILITIES_MAILBOX_SLOT_COUNT;
    State->RequestMailbox.Header.ProduceIndex = 0ULL;
    State->RequestMailbox.Header.ConsumeIndex = 0ULL;
    State->RequestMailbox.Header.Signature = LOS_CAPABILITIES_MAILBOX_SIGNATURE;

    State->ResponseMailbox.Header.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    State->ResponseMailbox.Header.SlotCount = LOS_CAPABILITIES_MAILBOX_SLOT_COUNT;
    State->ResponseMailbox.Header.ProduceIndex = 0ULL;
    State->ResponseMailbox.Header.ConsumeIndex = 0ULL;
    State->ResponseMailbox.Header.Signature = LOS_CAPABILITIES_MAILBOX_SIGNATURE;

    State->TransportExport.Signature = LOS_CAPABILITIES_TRANSPORT_EXPORT_SIGNATURE;
    State->TransportExport.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    State->TransportExport.Generation = State->TransportGeneration;
    State->TransportExport.RequestEndpointId = State->ReceiveEndpoint.EndpointId;
    State->TransportExport.ResponseEndpointId = State->ReplyEndpoint.EndpointId;
    State->TransportExport.RequestMailboxAddress = State->ReceiveEndpoint.MailboxAddress;
    State->TransportExport.ResponseMailboxAddress = State->ReplyEndpoint.MailboxAddress;
    State->TransportExport.RequestMailboxSize = State->ReceiveEndpoint.MailboxSize;
    State->TransportExport.ResponseMailboxSize = State->ReplyEndpoint.MailboxSize;
    State->TransportExport.Flags = State->ReceiveEndpoint.Flags;
    State->TransportExport.ServiceClass = State->ServiceClass;
    State->TransportExport.TransportState = LOS_CAPABILITIES_ENDPOINT_STATE_ONLINE;
    State->TransportExport.EndpointClass = State->ReceiveEndpoint.EndpointClass;
    State->TransportExport.MinimumSecurityMode = State->MinimumSecurityMode;
    LosCapabilitiesCopyBytes(State->TransportExport.ServiceName, State->ServiceName, LosCapabilitiesStringLength(State->ServiceName) + 1U);
    return 1;
}

const LOS_CAPABILITIES_TRANSPORT_EXPORT *LosCapabilitiesServiceExportTransport(void)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;

    State = LosCapabilitiesServiceState();
    if (State->Online == 0U || State->ReceiveEndpoint.State != LOS_CAPABILITIES_ENDPOINT_STATE_ONLINE)
    {
        return 0;
    }

    return &State->TransportExport;
}

UINT32 LosCapabilitiesServiceBindTransport(LOS_CAPABILITIES_TRANSPORT_BINDING *Binding)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    const LOS_CAPABILITIES_TRANSPORT_EXPORT *Exported;

    if (Binding == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    State = LosCapabilitiesServiceState();
    Exported = LosCapabilitiesServiceExportTransport();
    if (Exported == 0 || Exported->Signature != LOS_CAPABILITIES_TRANSPORT_EXPORT_SIGNATURE)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_TRANSPORT_OFFLINE;
    }

    Binding->Signature = LOS_CAPABILITIES_TRANSPORT_BINDING_SIGNATURE;
    Binding->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Binding->EndpointClass = State->ActiveEndpointClass;
    Binding->SecurityMode = State->ActiveSecurityMode;
    Binding->Generation = Exported->Generation;
    Binding->SessionId = State->ActiveSessionId;
    Binding->NextRequestSequence = 1ULL;
    Binding->LastResponseSequence = 0ULL;
    Binding->RequestAuthKey[0] = State->ActiveRequestAuthKey[0];
    Binding->RequestAuthKey[1] = State->ActiveRequestAuthKey[1];
    Binding->ResponseAuthKey[0] = State->ActiveResponseAuthKey[0];
    Binding->ResponseAuthKey[1] = State->ActiveResponseAuthKey[1];
    Binding->RequestCipherKey[0] = State->ActiveRequestCipherKey[0];
    Binding->RequestCipherKey[1] = State->ActiveRequestCipherKey[1];
    Binding->ResponseCipherKey[0] = State->ActiveResponseCipherKey[0];
    Binding->ResponseCipherKey[1] = State->ActiveResponseCipherKey[1];
    LosCapabilitiesPopulateIdentity(&Binding->LocalIdentity,
                                    State->ActiveClientPrincipalType,
                                    State->ActiveClientPrincipalId,
                                    LOS_CAPABILITIES_ENDPOINT_ROLE_REPLY,
                                    State->ActiveClientPrincipalName,
                                    State->ActiveClientServiceName,
                                    "client");
    LosCapabilitiesPopulateIdentity(&Binding->RemoteIdentity,
                                    LOS_CAPABILITIES_PRINCIPAL_TYPE_SERVICE,
                                    0ULL,
                                    LOS_CAPABILITIES_ENDPOINT_ROLE_RECEIVE,
                                    "capsmgr",
                                    State->ServiceName,
                                    "access.request");
    Binding->RequestEndpoint = &State->ReceiveEndpoint;
    Binding->ResponseEndpoint = &State->ReplyEndpoint;
    Binding->RequestMailbox = (LOS_CAPABILITIES_REQUEST_MAILBOX *)(UINTN)Exported->RequestMailboxAddress;
    Binding->ResponseMailbox = (LOS_CAPABILITIES_RESPONSE_MAILBOX *)(UINTN)Exported->ResponseMailboxAddress;
    return LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS;
}

UINT32 LosCapabilitiesServiceConnectTransport(const LOS_CAPABILITIES_TRANSPORT_CONNECT_REQUEST *Request,
                                              LOS_CAPABILITIES_TRANSPORT_CONNECT_RESULT *Result,
                                              LOS_CAPABILITIES_TRANSPORT_BINDING *Binding)
{
    const LOS_CAPABILITIES_TRANSPORT_EXPORT *Exported;
    LOS_CAPABILITIES_SERVICE_STATE *State;
    UINT64 SessionId;
    UINT64 RequestKey[2];
    UINT64 ResponseKey[2];
    UINT64 RequestCipherKey[2];
    UINT64 ResponseCipherKey[2];
    UINT32 Status;

    if (Request == 0 || Result == 0 || Binding == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    LosCapabilitiesZeroBytes(Result, sizeof(*Result));
    Result->Version = LOS_CAPABILITIES_SERVICE_VERSION;

    Exported = LosCapabilitiesServiceExportTransport();
    if (Exported == 0)
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_TRANSPORT_OFFLINE;
        return Result->Status;
    }

    if (Request->TargetServiceName[0] == 0 ||
        Request->SourceServiceName[0] == 0 ||
        Request->PrincipalName[0] == 0 ||
        LosCapabilitiesStringsEqual(Request->TargetServiceName, Exported->ServiceName) == 0)
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_TARGET_MISMATCH;
        return Result->Status;
    }

    if (Request->RequiredEndpointRole != 0U && Request->RequiredEndpointRole != LOS_CAPABILITIES_ENDPOINT_ROLE_RECEIVE)
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_TARGET_MISMATCH;
        return Result->Status;
    }

    if ((Exported->Flags & Request->RequiredFlags) != Request->RequiredFlags)
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_TARGET_MISMATCH;
        return Result->Status;
    }

    if (Request->TargetEndpointClass != 0U && Request->TargetEndpointClass != Exported->EndpointClass)
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_TARGET_MISMATCH;
        return Result->Status;
    }

    if (Request->DesiredSecurityMode < Exported->MinimumSecurityMode)
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_SECURITY_MODE_MISMATCH;
        return Result->Status;
    }

    State = LosCapabilitiesServiceState();
    State->ActiveClientNonce = Request->ConnectNonce;
    State->ActiveServerNonce = (0x4350534d47525356ULL ^ State->TransportGeneration ^ Request->ConnectNonce);
    State->ActiveClientPrincipalType = Request->PrincipalType;
    State->ActiveClientPrincipalId = Request->PrincipalId;
    LosCapabilitiesZeroBytes(State->ActiveClientPrincipalName, sizeof(State->ActiveClientPrincipalName));
    LosCapabilitiesZeroBytes(State->ActiveClientServiceName, sizeof(State->ActiveClientServiceName));
    LosCapabilitiesCopyBytes(State->ActiveClientPrincipalName, Request->PrincipalName, LosCapabilitiesStringLength(Request->PrincipalName) + 1U);
    LosCapabilitiesCopyBytes(State->ActiveClientServiceName, Request->SourceServiceName, LosCapabilitiesStringLength(Request->SourceServiceName) + 1U);

    LosCapabilitiesDeriveSessionMaterial(Request,
                                         State->ActiveServerNonce,
                                         State->TransportGeneration,
                                         &SessionId,
                                         RequestKey,
                                         ResponseKey,
                                         RequestCipherKey,
                                         ResponseCipherKey);
    if (SessionId == 0ULL)
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_SESSION_REJECTED;
        return Result->Status;
    }

    State->ActiveSessionId = SessionId;
    State->ActiveEndpointClass = Exported->EndpointClass;
    State->ActiveSecurityMode = Request->DesiredSecurityMode;
    State->ReceiveEndpoint.SecurityMode = State->ActiveSecurityMode;
    State->ReplyEndpoint.SecurityMode = State->ActiveSecurityMode;
    State->ActiveRequestAuthKey[0] = RequestKey[0];
    State->ActiveRequestAuthKey[1] = RequestKey[1];
    State->ActiveResponseAuthKey[0] = ResponseKey[0];
    State->ActiveResponseAuthKey[1] = ResponseKey[1];
    State->ActiveRequestCipherKey[0] = RequestCipherKey[0];
    State->ActiveRequestCipherKey[1] = RequestCipherKey[1];
    State->ActiveResponseCipherKey[0] = ResponseCipherKey[0];
    State->ActiveResponseCipherKey[1] = ResponseCipherKey[1];
    State->LastAcceptedRequestSequence = 0ULL;
    State->LastCompletedResponseSequence = 0ULL;
    State->ReceiveEndpoint.Flags |= LOS_CAPABILITIES_ENDPOINT_FLAG_SESSION_ESTABLISHED;
    State->ReplyEndpoint.Flags |= LOS_CAPABILITIES_ENDPOINT_FLAG_SESSION_ESTABLISHED;
    State->TransportExport.Flags = State->ReceiveEndpoint.Flags;

    Status = LosCapabilitiesServiceBindTransport(Binding);
    Binding->EndpointClass = Exported->EndpointClass;
    Binding->SecurityMode = Request->DesiredSecurityMode;
    Binding->SessionId = SessionId;
    Binding->RequestAuthKey[0] = RequestKey[0];
    Binding->RequestAuthKey[1] = RequestKey[1];
    Binding->ResponseAuthKey[0] = ResponseKey[0];
    Binding->ResponseAuthKey[1] = ResponseKey[1];
    Binding->RequestCipherKey[0] = RequestCipherKey[0];
    Binding->RequestCipherKey[1] = RequestCipherKey[1];
    Binding->ResponseCipherKey[0] = ResponseCipherKey[0];
    Binding->ResponseCipherKey[1] = ResponseCipherKey[1];
    LosCapabilitiesPopulateIdentity(&Binding->LocalIdentity,
                                    Request->PrincipalType,
                                    Request->PrincipalId,
                                    LOS_CAPABILITIES_ENDPOINT_ROLE_REPLY,
                                    Request->PrincipalName,
                                    Request->SourceServiceName,
                                    Request->SourceEndpointName);
    LosCapabilitiesPopulateIdentity(&Binding->RemoteIdentity,
                                    LOS_CAPABILITIES_PRINCIPAL_TYPE_SERVICE,
                                    0ULL,
                                    LOS_CAPABILITIES_ENDPOINT_ROLE_RECEIVE,
                                    "capsmgr",
                                    State->ServiceName,
                                    "access.request");

    Result->Status = Status;
    Result->ServiceClass = Exported->ServiceClass;
    Result->TransportState = Exported->TransportState;
    Result->EndpointClass = Exported->EndpointClass;
    Result->SecurityMode = Request->DesiredSecurityMode;
    Result->Generation = Exported->Generation;
    Result->SessionId = SessionId;
    Result->ServerNonce = State->ActiveServerNonce;
    LosCapabilitiesPopulateIdentity(&Result->LocalIdentity,
                                    Request->PrincipalType,
                                    Request->PrincipalId,
                                    LOS_CAPABILITIES_ENDPOINT_ROLE_REPLY,
                                    Request->PrincipalName,
                                    Request->SourceServiceName,
                                    Request->SourceEndpointName);
    LosCapabilitiesPopulateIdentity(&Result->RemoteIdentity,
                                    LOS_CAPABILITIES_PRINCIPAL_TYPE_SERVICE,
                                    0ULL,
                                    LOS_CAPABILITIES_ENDPOINT_ROLE_RECEIVE,
                                    "capsmgr",
                                    State->ServiceName,
                                    "access.request");
    return Status;
}

UINT32 LosCapabilitiesServiceSubmitBoundAccessRequest(LOS_CAPABILITIES_TRANSPORT_BINDING *Binding,
                                                      const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                                      LOS_CAPABILITIES_ACCESS_RESULT *Result)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;

    if (Binding == 0 || Request == 0 || Result == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    State = LosCapabilitiesServiceState();
    if (Binding->Signature != LOS_CAPABILITIES_TRANSPORT_BINDING_SIGNATURE ||
        Binding->Version != LOS_CAPABILITIES_SERVICE_VERSION ||
        Binding->Generation != State->TransportGeneration ||
        Binding->SessionId != State->ActiveSessionId ||
        Binding->RequestEndpoint == 0 ||
        Binding->ResponseEndpoint == 0 ||
        Binding->RequestMailbox == 0 ||
        Binding->ResponseMailbox == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_TRANSPORT_STALE;
    }

    if (Binding->RequestEndpoint->State != LOS_CAPABILITIES_ENDPOINT_STATE_ONLINE ||
        Binding->ResponseEndpoint->State != LOS_CAPABILITIES_ENDPOINT_STATE_ONLINE)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_TRANSPORT_OFFLINE;
    }

    return LosCapabilitiesSubmitRequestUsingBinding(Binding, Request, Result);
}

UINT32 LosCapabilitiesServiceSubmitAccessRequest(const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                                 LOS_CAPABILITIES_ACCESS_RESULT *Result)
{
    LOS_CAPABILITIES_TRANSPORT_BINDING Binding;
    UINT32 Status;

    if (Request == 0 || Result == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    Status = LosCapabilitiesServiceBindTransport(&Binding);
    if (Status != LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS)
    {
        return Status;
    }

    return LosCapabilitiesSubmitRequestUsingBinding(&Binding, Request, Result);
}

BOOLEAN LosCapabilitiesServiceServiceTransportOnce(void)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    LOS_CAPABILITIES_ACCESS_REQUEST_SLOT *RequestSlot;
    LOS_CAPABILITIES_ACCESS_RESPONSE_SLOT *ResponseSlot;
    UINT64 RequestIndex;
    UINT64 ResponseIndex;
    UINT64 ExpectedTag;

    State = LosCapabilitiesServiceState();
    RequestIndex = State->RequestMailbox.Header.ConsumeIndex % State->RequestMailbox.Header.SlotCount;
    RequestSlot = &State->RequestMailbox.Slots[RequestIndex];
    if (RequestSlot->SlotState != LOS_CAPABILITIES_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    ResponseIndex = State->ResponseMailbox.Header.ProduceIndex % State->ResponseMailbox.Header.SlotCount;
    ResponseSlot = &State->ResponseMailbox.Slots[ResponseIndex];
    if (ResponseSlot->SlotState == LOS_CAPABILITIES_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    LosCapabilitiesZeroBytes(&ResponseSlot->Message, sizeof(ResponseSlot->Message));
    ResponseSlot->Message.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    ResponseSlot->EndpointClass = State->ActiveEndpointClass;
    ResponseSlot->SecurityMode = State->ActiveSecurityMode;
    ResponseSlot->Sequence = RequestSlot->Sequence;
    ResponseSlot->SessionId = State->ActiveSessionId;
    ResponseSlot->Nonce = LosCapabilitiesComputeCipherNonce(State->ActiveSessionId, RequestSlot->Sequence, State->ActiveEndpointClass, State->ActiveSecurityMode);

    if (RequestSlot->SessionId != State->ActiveSessionId)
    {
        ResponseSlot->Message.Status = LOS_CAPABILITIES_SERVICE_STATUS_AUTH_FAILED;
        ResponseSlot->Message.Granted = 0U;
    }
    else
    {
        ExpectedTag = LosCapabilitiesComputeMessageTag(RequestSlot->SessionId,
                                                       RequestSlot->Sequence,
                                                       &RequestSlot->Message,
                                                       sizeof(RequestSlot->Message),
                                                       State->ActiveRequestAuthKey);
        if (ExpectedTag != RequestSlot->AuthTag)
        {
            ResponseSlot->Message.Status = LOS_CAPABILITIES_SERVICE_STATUS_AUTH_FAILED;
            ResponseSlot->Message.Granted = 0U;
        }
        else if (RequestSlot->Sequence <= State->LastAcceptedRequestSequence)
        {
            ResponseSlot->Message.Status = LOS_CAPABILITIES_SERVICE_STATUS_REPLAY_DETECTED;
            ResponseSlot->Message.Granted = 0U;
        }
        else
        {
            LosCapabilitiesCryptBuffer(&RequestSlot->Message,
                                       sizeof(RequestSlot->Message),
                                       RequestSlot->SessionId,
                                       RequestSlot->Sequence,
                                       RequestSlot->Nonce,
                                       RequestSlot->EndpointClass,
                                       RequestSlot->SecurityMode,
                                       State->ActiveRequestCipherKey);
            State->LastAcceptedRequestSequence = RequestSlot->Sequence;
            (void)LosCapabilitiesServiceCheckAccess(&RequestSlot->Message, &ResponseSlot->Message);
        }
    }

    LosCapabilitiesCryptBuffer(&ResponseSlot->Message,
                               sizeof(ResponseSlot->Message),
                               ResponseSlot->SessionId,
                               ResponseSlot->Sequence,
                               ResponseSlot->Nonce,
                               ResponseSlot->EndpointClass,
                               ResponseSlot->SecurityMode,
                               State->ActiveResponseCipherKey);
    ResponseSlot->AuthTag = LosCapabilitiesComputeMessageTag(ResponseSlot->SessionId,
                                                             ResponseSlot->Sequence,
                                                             &ResponseSlot->Message,
                                                             sizeof(ResponseSlot->Message),
                                                             State->ActiveResponseAuthKey);
    ResponseSlot->SlotState = LOS_CAPABILITIES_MAILBOX_SLOT_READY;
    State->LastCompletedResponseSequence = ResponseSlot->Sequence;
    State->ResponseMailbox.Header.ProduceIndex += 1ULL;

    RequestSlot->SlotState = LOS_CAPABILITIES_MAILBOX_SLOT_FREE;
    RequestSlot->Sequence = 0ULL;
    RequestSlot->SessionId = 0ULL;
    RequestSlot->Nonce = 0ULL;
    RequestSlot->AuthTag = 0ULL;
    LosCapabilitiesZeroBytes(&RequestSlot->Message, sizeof(RequestSlot->Message));
    State->RequestMailbox.Header.ConsumeIndex += 1ULL;
    return 1;
}
