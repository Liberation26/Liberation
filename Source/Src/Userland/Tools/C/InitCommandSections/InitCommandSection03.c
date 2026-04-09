/*
 * File Name: InitCommandSection03.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from InitCommand.c.
 */

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

