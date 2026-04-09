/*
 * File Name: CapabilitiesTransportSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from CapabilitiesTransport.c.
 */

static void LosCapabilitiesCopyBytes(void *Destination, const void *Source, UINTN Size)
{
    UINT8 *Dest = (UINT8 *)Destination;
    const UINT8 *Src = (const UINT8 *)Source;
    UINTN Index;

    if (Destination == 0 || Source == 0)
    {
        return;
    }

    for (Index = 0; Index < Size; ++Index)
    {
        Dest[Index] = Src[Index];
    }
}

static void LosCapabilitiesZeroBytes(void *Destination, UINTN Size)
{
    UINT8 *Dest = (UINT8 *)Destination;
    UINTN Index;

    if (Destination == 0)
    {
        return;
    }

    for (Index = 0U; Index < Size; ++Index)
    {
        Dest[Index] = 0U;
    }
}

static UINTN LosCapabilitiesStringLength(const char *Text)
{
    UINTN Length;

    if (Text == 0)
    {
        return 0U;
    }

    for (Length = 0U; Text[Length] != 0; ++Length)
    {
    }

    return Length;
}

static BOOLEAN LosCapabilitiesStringsEqual(const char *Left, const char *Right)
{
    UINTN Index;

    if (Left == 0 || Right == 0)
    {
        return 0;
    }

    for (Index = 0U; Left[Index] != 0 || Right[Index] != 0; ++Index)
    {
        if (Left[Index] != Right[Index])
        {
            return 0;
        }
    }

    return 1;
}

static UINT64 LosCapabilitiesRotateLeft64(UINT64 Value, UINTN Shift)
{
    return (Value << Shift) | (Value >> (64U - Shift));
}

static UINT64 LosCapabilitiesReadLe64(const UINT8 *Bytes)
{
    return ((UINT64)Bytes[0]) |
           ((UINT64)Bytes[1] << 8U) |
           ((UINT64)Bytes[2] << 16U) |
           ((UINT64)Bytes[3] << 24U) |
           ((UINT64)Bytes[4] << 32U) |
           ((UINT64)Bytes[5] << 40U) |
           ((UINT64)Bytes[6] << 48U) |
           ((UINT64)Bytes[7] << 56U);
}

static void LosCapabilitiesSipRound(UINT64 *V0, UINT64 *V1, UINT64 *V2, UINT64 *V3)
{
    *V0 += *V1;
    *V1 = LosCapabilitiesRotateLeft64(*V1, 13U);
    *V1 ^= *V0;
    *V0 = LosCapabilitiesRotateLeft64(*V0, 32U);
    *V2 += *V3;
    *V3 = LosCapabilitiesRotateLeft64(*V3, 16U);
    *V3 ^= *V2;
    *V0 += *V3;
    *V3 = LosCapabilitiesRotateLeft64(*V3, 21U);
    *V3 ^= *V0;
    *V2 += *V1;
    *V1 = LosCapabilitiesRotateLeft64(*V1, 17U);
    *V1 ^= *V2;
    *V2 = LosCapabilitiesRotateLeft64(*V2, 32U);
}

static UINT64 LosCapabilitiesSipHash24(const UINT8 *Data, UINTN Length, const UINT64 Key[2])
{
    UINT64 V0;
    UINT64 V1;
    UINT64 V2;
    UINT64 V3;
    UINT64 Block;
    UINT64 FinalBlock;
    UINTN Offset;
    UINTN TailIndex;

    V0 = 0x736f6d6570736575ULL ^ Key[0];
    V1 = 0x646f72616e646f6dULL ^ Key[1];
    V2 = 0x6c7967656e657261ULL ^ Key[0];
    V3 = 0x7465646279746573ULL ^ Key[1];

    for (Offset = 0U; Offset + 8U <= Length; Offset += 8U)
    {
        Block = LosCapabilitiesReadLe64(Data + Offset);
        V3 ^= Block;
        LosCapabilitiesSipRound(&V0, &V1, &V2, &V3);
        LosCapabilitiesSipRound(&V0, &V1, &V2, &V3);
        V0 ^= Block;
    }

    FinalBlock = ((UINT64)Length) << 56U;
    for (TailIndex = 0U; Offset + TailIndex < Length; ++TailIndex)
    {
        FinalBlock |= ((UINT64)Data[Offset + TailIndex]) << (8U * TailIndex);
    }

    V3 ^= FinalBlock;
    LosCapabilitiesSipRound(&V0, &V1, &V2, &V3);
    LosCapabilitiesSipRound(&V0, &V1, &V2, &V3);
    V0 ^= FinalBlock;
    V2 ^= 0xffULL;
    LosCapabilitiesSipRound(&V0, &V1, &V2, &V3);
    LosCapabilitiesSipRound(&V0, &V1, &V2, &V3);
    LosCapabilitiesSipRound(&V0, &V1, &V2, &V3);
    LosCapabilitiesSipRound(&V0, &V1, &V2, &V3);
    return V0 ^ V1 ^ V2 ^ V3;
}

static UINT64 LosCapabilitiesComputeMessageTag(UINT64 SessionId,
                                               UINT64 Sequence,
                                               const void *Message,
                                               UINTN MessageSize,
                                               const UINT64 Key[2])
{
    UINT8 Buffer[sizeof(UINT64) * 2U + sizeof(LOS_CAPABILITIES_ACCESS_REQUEST)];
    UINTN Offset;
    const UINT8 *MessageBytes;

    if (Message == 0 || Key == 0 || MessageSize > (sizeof(Buffer) - (sizeof(UINT64) * 2U)))
    {
        return 0ULL;
    }

    LosCapabilitiesZeroBytes(Buffer, sizeof(Buffer));
    Offset = 0U;
    LosCapabilitiesCopyBytes(Buffer + Offset, &SessionId, sizeof(SessionId));
    Offset += sizeof(SessionId);
    LosCapabilitiesCopyBytes(Buffer + Offset, &Sequence, sizeof(Sequence));
    Offset += sizeof(Sequence);
    MessageBytes = (const UINT8 *)Message;
    LosCapabilitiesCopyBytes(Buffer + Offset, MessageBytes, MessageSize);
    Offset += MessageSize;
    return LosCapabilitiesSipHash24(Buffer, Offset, Key);
}

static UINT64 LosCapabilitiesComputeCipherNonce(UINT64 SessionId,
                                                  UINT64 Sequence,
                                                  UINT32 EndpointClass,
                                                  UINT32 SecurityMode)
{
    return SessionId ^ (Sequence << 1U) ^ ((UINT64)EndpointClass << 48U) ^ ((UINT64)SecurityMode << 56U) ^ 0x4349504845524e43ULL;
}

static void LosCapabilitiesCryptBuffer(void *Buffer,
                                       UINTN BufferSize,
                                       UINT64 SessionId,
                                       UINT64 Sequence,
                                       UINT64 Nonce,
                                       UINT32 EndpointClass,
                                       UINT32 SecurityMode,
                                       const UINT64 Key[2])
{
    UINT8 *Bytes;
    UINTN Offset;
    UINT64 BlockCounter;
    UINT64 Stream;
    UINT8 Seed[40];
    UINTN SeedOffset;

    if (Buffer == 0 || Key == 0 || SecurityMode != LOS_CAPABILITIES_SECURITY_MODE_CONFIDENTIAL)
    {
        return;
    }

    Bytes = (UINT8 *)Buffer;
    for (Offset = 0U, BlockCounter = 0ULL; Offset < BufferSize; ++BlockCounter)
    {
        LosCapabilitiesZeroBytes(Seed, sizeof(Seed));
        SeedOffset = 0U;
        LosCapabilitiesCopyBytes(Seed + SeedOffset, &SessionId, sizeof(SessionId));
        SeedOffset += sizeof(SessionId);
        LosCapabilitiesCopyBytes(Seed + SeedOffset, &Sequence, sizeof(Sequence));
        SeedOffset += sizeof(Sequence);
        LosCapabilitiesCopyBytes(Seed + SeedOffset, &Nonce, sizeof(Nonce));
        SeedOffset += sizeof(Nonce);
        LosCapabilitiesCopyBytes(Seed + SeedOffset, &BlockCounter, sizeof(BlockCounter));
        SeedOffset += sizeof(BlockCounter);
        LosCapabilitiesCopyBytes(Seed + SeedOffset, &EndpointClass, sizeof(EndpointClass));
        SeedOffset += sizeof(EndpointClass);
        LosCapabilitiesCopyBytes(Seed + SeedOffset, &SecurityMode, sizeof(SecurityMode));
        SeedOffset += sizeof(SecurityMode);
        Stream = LosCapabilitiesSipHash24(Seed, SeedOffset, Key);
        for (UINTN ByteIndex = 0U; ByteIndex < sizeof(Stream) && Offset < BufferSize; ++ByteIndex, ++Offset)
        {
            Bytes[Offset] ^= (UINT8)((Stream >> (ByteIndex * 8U)) & 0xffU);
        }
    }
}

static void LosCapabilitiesPopulateIdentity(LOS_CAPABILITIES_ENDPOINT_IDENTITY *Identity,
                                            UINT32 PrincipalType,
                                            UINT64 PrincipalId,
                                            UINT32 EndpointRole,
                                            const char *PrincipalName,
                                            const char *ServiceName,
                                            const char *EndpointName)
{
    if (Identity == 0)
    {
        return;
    }

    LosCapabilitiesZeroBytes(Identity, sizeof(*Identity));
    Identity->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Identity->PrincipalType = PrincipalType;
    Identity->EndpointRole = EndpointRole;
    Identity->PrincipalId = PrincipalId;
    if (PrincipalName != 0)
    {
        LosCapabilitiesCopyBytes(Identity->PrincipalName, PrincipalName, LosCapabilitiesStringLength(PrincipalName) + 1U);
    }
    if (ServiceName != 0)
    {
        LosCapabilitiesCopyBytes(Identity->ServiceName, ServiceName, LosCapabilitiesStringLength(ServiceName) + 1U);
    }
    if (EndpointName != 0)
    {
        LosCapabilitiesCopyBytes(Identity->EndpointName, EndpointName, LosCapabilitiesStringLength(EndpointName) + 1U);
    }
}

static void LosCapabilitiesDeriveSessionMaterial(const LOS_CAPABILITIES_TRANSPORT_CONNECT_REQUEST *Request,
                                                 UINT64 ServerNonce,
                                                 UINT64 Generation,
                                                 UINT64 *SessionId,
                                                 UINT64 RequestKey[2],
                                                 UINT64 ResponseKey[2],
                                                 UINT64 RequestCipherKey[2],
                                                 UINT64 ResponseCipherKey[2])
{
    UINT8 Buffer[192];
    UINTN Offset;
    static const UINT64 RootKey[2] =
    {
        0x4c4f532d53455353ULL,
        0x494f4e2d4b455921ULL
    };

    if (Request == 0 || SessionId == 0 || RequestKey == 0 || ResponseKey == 0 || RequestCipherKey == 0 || ResponseCipherKey == 0)
    {
        return;
    }

    LosCapabilitiesZeroBytes(Buffer, sizeof(Buffer));
    Offset = 0U;
    LosCapabilitiesCopyBytes(Buffer + Offset, &Request->ConnectNonce, sizeof(Request->ConnectNonce));
    Offset += sizeof(Request->ConnectNonce);
    LosCapabilitiesCopyBytes(Buffer + Offset, &ServerNonce, sizeof(ServerNonce));
    Offset += sizeof(ServerNonce);
    LosCapabilitiesCopyBytes(Buffer + Offset, &Generation, sizeof(Generation));
    Offset += sizeof(Generation);
    LosCapabilitiesCopyBytes(Buffer + Offset, &Request->PrincipalType, sizeof(Request->PrincipalType));
    Offset += sizeof(Request->PrincipalType);
    LosCapabilitiesCopyBytes(Buffer + Offset, &Request->PrincipalId, sizeof(Request->PrincipalId));
    Offset += sizeof(Request->PrincipalId);
    LosCapabilitiesCopyBytes(Buffer + Offset, Request->PrincipalName, sizeof(Request->PrincipalName));
    Offset += sizeof(Request->PrincipalName);
    LosCapabilitiesCopyBytes(Buffer + Offset, Request->SourceServiceName, sizeof(Request->SourceServiceName));
    Offset += sizeof(Request->SourceServiceName);
    LosCapabilitiesCopyBytes(Buffer + Offset, Request->SourceEndpointName, sizeof(Request->SourceEndpointName));
    Offset += sizeof(Request->SourceEndpointName);
    LosCapabilitiesCopyBytes(Buffer + Offset, Request->TargetServiceName, sizeof(Request->TargetServiceName));
    Offset += sizeof(Request->TargetServiceName);

    *SessionId = LosCapabilitiesSipHash24(Buffer, Offset, RootKey) ^ 0x4341505345535300ULL;
    RequestKey[0] = LosCapabilitiesSipHash24(Buffer, Offset, RootKey) ^ *SessionId;
    RequestKey[1] = LosCapabilitiesSipHash24(Buffer, Offset, RequestKey) ^ ServerNonce;
    ResponseKey[0] = RequestKey[0] ^ 0x52504c592d4b4559ULL;
    ResponseKey[1] = RequestKey[1] ^ 0x2153455256455221ULL;
    RequestCipherKey[0] = RequestKey[0] ^ 0x434950482d524551ULL;
    RequestCipherKey[1] = RequestKey[1] ^ 0x2d4348414e4e454cULL;
    ResponseCipherKey[0] = ResponseKey[0] ^ 0x434950482d524550ULL;
    ResponseCipherKey[1] = ResponseKey[1] ^ 0x2d4348414e4e454cULL;
}

static UINT32 LosCapabilitiesSubmitRequestUsingBinding(LOS_CAPABILITIES_TRANSPORT_BINDING *Binding,
                                                       const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                                       LOS_CAPABILITIES_ACCESS_RESULT *Result)
{
    LOS_CAPABILITIES_REQUEST_MAILBOX *RequestMailbox;
    LOS_CAPABILITIES_RESPONSE_MAILBOX *ResponseMailbox;
    LOS_CAPABILITIES_ACCESS_REQUEST_SLOT *RequestSlot;
    LOS_CAPABILITIES_ACCESS_RESPONSE_SLOT *ResponseSlot;
    UINT64 RequestIndex;
    UINT64 ResponseIndex;
    UINT64 Sequence;
    UINT64 ExpectedTag;
    UINT64 Spin;

    if (Binding == 0 || Request == 0 || Result == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    RequestMailbox = Binding->RequestMailbox;
    ResponseMailbox = Binding->ResponseMailbox;
    if (RequestMailbox == 0 || ResponseMailbox == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_TRANSPORT_STALE;
    }

    if (RequestMailbox->Header.Signature != LOS_CAPABILITIES_MAILBOX_SIGNATURE ||
        ResponseMailbox->Header.Signature != LOS_CAPABILITIES_MAILBOX_SIGNATURE)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_TRANSPORT_STALE;
    }

    Sequence = Binding->NextRequestSequence;
    if (Sequence == 0ULL)
    {
        Sequence = 1ULL;
    }
    Binding->NextRequestSequence = Sequence + 1ULL;

    RequestIndex = RequestMailbox->Header.ProduceIndex % RequestMailbox->Header.SlotCount;
    RequestSlot = &RequestMailbox->Slots[RequestIndex];
    if (RequestSlot->SlotState == LOS_CAPABILITIES_MAILBOX_SLOT_READY)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_TABLE_FULL;
    }

    LosCapabilitiesCopyBytes(&RequestSlot->Message, Request, sizeof(*Request));
    RequestSlot->EndpointClass = Binding->EndpointClass;
    RequestSlot->SecurityMode = Binding->SecurityMode;
    RequestSlot->Sequence = Sequence;
    RequestSlot->SessionId = Binding->SessionId;
    RequestSlot->Nonce = LosCapabilitiesComputeCipherNonce(Binding->SessionId, Sequence, Binding->EndpointClass, Binding->SecurityMode);
    LosCapabilitiesCryptBuffer(&RequestSlot->Message,
                               sizeof(RequestSlot->Message),
                               Binding->SessionId,
                               Sequence,
                               RequestSlot->Nonce,
                               Binding->EndpointClass,
                               Binding->SecurityMode,
                               Binding->RequestCipherKey);
    RequestSlot->AuthTag = LosCapabilitiesComputeMessageTag(Binding->SessionId,
                                                            Sequence,
                                                            &RequestSlot->Message,
                                                            sizeof(RequestSlot->Message),
                                                            Binding->RequestAuthKey);
    RequestSlot->SlotState = LOS_CAPABILITIES_MAILBOX_SLOT_READY;
    RequestMailbox->Header.ProduceIndex += 1ULL;

    for (Spin = 0ULL; Spin < LOS_CAPABILITIES_SERVICE_TRANSPORT_SPIN; ++Spin)
    {
        (void)LosCapabilitiesServiceServiceTransportOnce();
        ResponseIndex = ResponseMailbox->Header.ConsumeIndex % ResponseMailbox->Header.SlotCount;
        ResponseSlot = &ResponseMailbox->Slots[ResponseIndex];
        if (ResponseSlot->SlotState == LOS_CAPABILITIES_MAILBOX_SLOT_READY &&
            ResponseSlot->Sequence == Sequence)
        {
            if (ResponseSlot->SessionId != Binding->SessionId)
            {
                return LOS_CAPABILITIES_SERVICE_STATUS_AUTH_FAILED;
            }

            if (ResponseSlot->Sequence <= Binding->LastResponseSequence)
            {
                return LOS_CAPABILITIES_SERVICE_STATUS_REPLAY_DETECTED;
            }

            ExpectedTag = LosCapabilitiesComputeMessageTag(Binding->SessionId,
                                                           ResponseSlot->Sequence,
                                                           &ResponseSlot->Message,
                                                           sizeof(ResponseSlot->Message),
                                                           Binding->ResponseAuthKey);
            if (ExpectedTag != ResponseSlot->AuthTag)
            {
                return LOS_CAPABILITIES_SERVICE_STATUS_AUTH_FAILED;
            }

            LosCapabilitiesCryptBuffer(&ResponseSlot->Message,
                               sizeof(ResponseSlot->Message),
                               Binding->SessionId,
                               ResponseSlot->Sequence,
                               ResponseSlot->Nonce,
                               Binding->EndpointClass,
                               Binding->SecurityMode,
                               Binding->ResponseCipherKey);
            LosCapabilitiesCopyBytes(Result, &ResponseSlot->Message, sizeof(*Result));
            Binding->LastResponseSequence = ResponseSlot->Sequence;
            ResponseSlot->SlotState = LOS_CAPABILITIES_MAILBOX_SLOT_FREE;
            ResponseSlot->Sequence = 0ULL;
            ResponseSlot->SessionId = 0ULL;
            ResponseSlot->Nonce = 0ULL;
            ResponseSlot->AuthTag = 0ULL;
            ResponseMailbox->Header.ConsumeIndex += 1ULL;
            return Result->Status;
        }
        LosCapabilitiesServiceYield();
    }

    return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_STATE;
}
