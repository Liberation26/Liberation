/*
 * File Name: CapabilitiesRegistrySection03.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from CapabilitiesRegistry.c.
 */

UINT32 LosCapabilitiesServiceGrant(const LOS_CAPABILITIES_MUTATION_REQUEST *Request,
                                   LOS_CAPABILITIES_MUTATION_RESULT *Result)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    LOS_CAPABILITIES_SERVICE_RECORD Record;
    LOS_CAPABILITY_GRANT_ENTRY *Grant;
    UINT64 EventId;

    if (Request == 0 || Result == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    Result->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    Result->State = 0U;
    Result->Reserved = 0U;
    Result->GrantId = 0ULL;
    Result->EventId = 0ULL;

    if (!LosCapabilitiesFindCapabilityRecord(Request->Namespace, Request->Name, &Record))
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_NOT_FOUND;
        return Result->Status;
    }

    if ((Request->AuthoriserType != LOS_CAPABILITIES_AUTHORISER_BOOT_POLICY) &&
        (Request->AuthoriserType != LOS_CAPABILITIES_AUTHORISER_KERNEL) &&
        (Request->AuthoriserType != LOS_CAPABILITIES_AUTHORISER_SERVICE) &&
        (Request->AuthoriserType != LOS_CAPABILITIES_AUTHORISER_USER) &&
        (Request->AuthoriserType != LOS_CAPABILITIES_AUTHORISER_DELEGATED))
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_ACCESS_DENIED;
        return Result->Status;
    }

    if (Request->AuthoriserType == LOS_CAPABILITIES_AUTHORISER_DELEGATED && Request->ParentGrantId == 0ULL)
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_ACCESS_DENIED;
        return Result->Status;
    }

    State = LosCapabilitiesServiceState();
    if (State->RuntimeGrantCount >= LOS_CAPABILITIES_RUNTIME_MAX_GRANTS)
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_TABLE_FULL;
        return Result->Status;
    }

    Grant = &State->RuntimeGrants[State->RuntimeGrantCount];
    Grant->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Grant->CapabilityClass = Record.CapabilityClass;
    Grant->Flags = Request->Flags;
    Grant->GrantMode = (Request->MutationType == LOS_CAPABILITIES_MUTATION_GRANT) ? LOS_CAPABILITIES_GRANT_MODE_DIRECT : Request->MutationType;
    Grant->State = LOS_CAPABILITIES_GRANT_STATE_ACTIVE;
    Grant->AuthoriserType = Request->AuthoriserType;
    Grant->Reserved0 = Request->PrincipalType;
    Grant->Reserved1 = (UINT32)Request->PrincipalId;
    Grant->CapabilityId = Record.CapabilityId;
    Grant->GrantId = State->NextGrantId;
    Grant->ParentGrantId = Request->ParentGrantId;
    Grant->GrantedAtUtc = Request->TimestampUtc;
    Grant->EffectiveFromUtc = Request->EffectiveFromUtc;
    Grant->EffectiveUntilUtc = Request->EffectiveUntilUtc;
    Grant->RevokedAtUtc = 0ULL;
    Grant->SuspendedAtUtc = 0ULL;
    LosCapabilitiesCopyText(Grant->Namespace, LOS_CAPABILITIES_SERVICE_NAMESPACE_LENGTH, Request->Namespace);
    LosCapabilitiesCopyText(Grant->Name, LOS_CAPABILITIES_SERVICE_NAME_LENGTH, Request->Name);
    LosCapabilitiesCopyText(Grant->AuthoriserName, LOS_CAPABILITIES_AUTHORISER_NAME_LENGTH, Request->AuthoriserName);

    State->RuntimeGrantCount += 1U;
    State->NextGrantId += 1ULL;

    EventId = LosCapabilitiesAppendEvent(LOS_CAPABILITIES_GRANT_EVENT_CREATED,
                                         Request->AuthoriserType,
                                         Grant->GrantId,
                                         Request->TimestampUtc,
                                         Request->PrincipalId,
                                         Request->PrincipalName,
                                         Grant->Namespace,
                                         Grant->Name,
                                         Request->AuthoriserName);

    Result->Status = (EventId != 0ULL) ? LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS : LOS_CAPABILITIES_SERVICE_STATUS_TABLE_FULL;
    Result->State = Grant->State;
    Result->GrantId = Grant->GrantId;
    Result->EventId = EventId;
    return Result->Status;
}

UINT32 LosCapabilitiesServiceRevoke(const LOS_CAPABILITIES_MUTATION_REQUEST *Request,
                                    LOS_CAPABILITIES_MUTATION_RESULT *Result)
{
    return LosCapabilitiesMutationCommon(Request, Result, LOS_CAPABILITIES_GRANT_STATE_REVOKED);
}

UINT32 LosCapabilitiesServiceSuspend(const LOS_CAPABILITIES_MUTATION_REQUEST *Request,
                                     LOS_CAPABILITIES_MUTATION_RESULT *Result)
{
    return LosCapabilitiesMutationCommon(Request, Result, LOS_CAPABILITIES_GRANT_STATE_SUSPENDED);
}

UINT32 LosCapabilitiesServiceRestore(const LOS_CAPABILITIES_MUTATION_REQUEST *Request,
                                     LOS_CAPABILITIES_MUTATION_RESULT *Result)
{
    return LosCapabilitiesMutationCommon(Request, Result, LOS_CAPABILITIES_GRANT_STATE_ACTIVE);
}

BOOLEAN LosCapabilitiesServiceSeedBootstrapRegistry(void)
{
    static const struct
    {
        const char *Namespace;
        const char *Name;
        UINT32 CapabilityClass;
        UINT32 Flags;
    } BootstrapCapabilities[] = {
        { "ipc", "endpoint.send", LOS_CAPABILITIES_SERVICE_CLASS_SERVICE, LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP },
        { "ipc", "endpoint.receive", LOS_CAPABILITIES_SERVICE_CLASS_SERVICE, LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP },
        { "memory", "query", LOS_CAPABILITIES_SERVICE_CLASS_SERVICE, LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP },
        { "service", "lookup", LOS_CAPABILITIES_SERVICE_CLASS_SERVICE, LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP },
        { "service", "start", LOS_CAPABILITIES_SERVICE_CLASS_SERVICE, LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP },
        { "service", "bootstrap.import", LOS_CAPABILITIES_SERVICE_CLASS_SERVICE, LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP },
        { "capability", "query", LOS_CAPABILITIES_SERVICE_CLASS_SERVICE, LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_AUDIT },
        { "capability", "grant", LOS_CAPABILITIES_SERVICE_CLASS_SERVICE, LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_AUDIT },
        { "capability", "revoke", LOS_CAPABILITIES_SERVICE_CLASS_SERVICE, LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_AUDIT },
        { "capability", "suspend", LOS_CAPABILITIES_SERVICE_CLASS_SERVICE, LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_AUDIT },
        { "capability", "restore", LOS_CAPABILITIES_SERVICE_CLASS_SERVICE, LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_AUDIT }
    };
    UINTN Index;

    for (Index = 0U; Index < sizeof(BootstrapCapabilities) / sizeof(BootstrapCapabilities[0]); ++Index)
    {
        if (!LosCapabilitiesServiceRegister(BootstrapCapabilities[Index].Namespace,
                                            BootstrapCapabilities[Index].Name,
                                            BootstrapCapabilities[Index].CapabilityClass,
                                            BootstrapCapabilities[Index].Flags,
                                            0))
        {
            return 0;
        }
    }

    return 1;
}
