/*
 * File Name: CapabilitiesRegistrySection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from CapabilitiesRegistry.c.
 */

BOOLEAN LosCapabilitiesServiceHasActiveGrant(UINT32 PrincipalType,
                                             UINT64 PrincipalId,
                                             const char *Namespace,
                                             const char *Name,
                                             LOS_CAPABILITY_GRANT_ENTRY *Grant)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    UINT32 Index;

    State = LosCapabilitiesServiceState();
    for (Index = 0U; Index < State->RuntimeGrantCount; ++Index)
    {
        if (State->RuntimeGrants[Index].State != LOS_CAPABILITIES_GRANT_STATE_ACTIVE)
        {
            continue;
        }
        if (State->RuntimeGrants[Index].Reserved0 != PrincipalType)
        {
            continue;
        }
        if (State->RuntimeGrants[Index].Reserved1 != (UINT32)PrincipalId)
        {
            continue;
        }
        if (LosCapabilitiesTextEqual(State->RuntimeGrants[Index].Namespace, Namespace) &&
            LosCapabilitiesTextEqual(State->RuntimeGrants[Index].Name, Name))
        {
            if (Grant != 0)
            {
                *Grant = State->RuntimeGrants[Index];
            }
            return 1;
        }
    }

    return 0;
}

BOOLEAN LosCapabilitiesServiceImportBootstrapContext(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    UINT32 BlockIndex;
    UINT32 GrantIndex;

    if (Context == 0 || Context->Version != LOS_CAPABILITIES_SERVICE_VERSION)
    {
        return 0;
    }

    State = LosCapabilitiesServiceState();
    State->RecordCount = 0U;
    State->RuntimeGrantCount = 0U;
    State->RuntimeEventCount = 0U;
    State->NextGrantId = 1ULL;
    State->NextEventId = 1ULL;
    State->BootstrapContext = *Context;
    State->BootstrapBlockCount = Context->BlockCount;
    State->BootstrapAssignmentCount = Context->AssignmentCount;
    State->BootstrapEventCount = Context->EventCount;

    for (BlockIndex = 0U; BlockIndex < Context->BlockCount && BlockIndex < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_BLOCKS; ++BlockIndex)
    {
        const LOS_CAPABILITY_GRANT_BLOCK *Block;

        Block = &Context->Blocks[BlockIndex];
        for (GrantIndex = 0U; GrantIndex < Block->GrantCount && GrantIndex < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_GRANTS_PER_BLOCK; ++GrantIndex)
        {
            const LOS_CAPABILITY_GRANT_ENTRY *Grant;
            LOS_CAPABILITIES_SERVICE_RECORD *Record;
            LOS_CAPABILITY_GRANT_ENTRY *RuntimeGrant;

            Grant = &Block->Grants[GrantIndex];
            if (State->RecordCount >= LOS_CAPABILITIES_SERVICE_MAX_ENTRIES ||
                State->RuntimeGrantCount >= LOS_CAPABILITIES_RUNTIME_MAX_GRANTS)
            {
                return 0;
            }

            Record = &State->Records[State->RecordCount];
            Record->Version = LOS_CAPABILITIES_SERVICE_VERSION;
            Record->CapabilityClass = Grant->CapabilityClass;
            Record->Flags = Grant->Flags;
            Record->Reserved = 0U;
            Record->CapabilityId = Grant->CapabilityId;
            LosCapabilitiesCopyText(Record->Namespace, LOS_CAPABILITIES_SERVICE_NAMESPACE_LENGTH, Grant->Namespace);
            LosCapabilitiesCopyText(Record->Name, LOS_CAPABILITIES_SERVICE_NAME_LENGTH, Grant->Name);
            State->RecordCount += 1U;

            RuntimeGrant = &State->RuntimeGrants[State->RuntimeGrantCount];
            *RuntimeGrant = *Grant;
            RuntimeGrant->Version = LOS_CAPABILITIES_SERVICE_VERSION;
            RuntimeGrant->Reserved0 = Block->PrincipalType;
            RuntimeGrant->Reserved1 = (UINT32)Block->PrincipalId;
            State->RuntimeGrantCount += 1U;

            if (RuntimeGrant->GrantId >= State->NextGrantId)
            {
                State->NextGrantId = RuntimeGrant->GrantId + 1ULL;
            }

            if (LosCapabilitiesAppendEvent(LOS_CAPABILITIES_GRANT_EVENT_IMPORTED,
                                           RuntimeGrant->AuthoriserType,
                                           RuntimeGrant->GrantId,
                                           RuntimeGrant->GrantedAtUtc,
                                           Block->PrincipalId,
                                           Block->PrincipalName,
                                           RuntimeGrant->Namespace,
                                           RuntimeGrant->Name,
                                           RuntimeGrant->AuthoriserName) == 0ULL)
            {
                return 0;
            }
        }
    }

    return 1;
}

BOOLEAN LosCapabilitiesServiceRegister(const char *Namespace,
                                       const char *Name,
                                       UINT32 CapabilityClass,
                                       UINT32 Flags,
                                       UINT64 *CapabilityId)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    LOS_CAPABILITIES_SERVICE_RECORD *Record;
    UINT32 Index;

    if (Namespace == 0 || Name == 0 || Namespace[0] == 0 || Name[0] == 0)
    {
        return 0;
    }

    State = LosCapabilitiesServiceState();
    for (Index = 0U; Index < State->RecordCount; ++Index)
    {
        Record = &State->Records[Index];
        if (LosCapabilitiesTextEqual(Record->Namespace, Namespace) &&
            LosCapabilitiesTextEqual(Record->Name, Name))
        {
            if (CapabilityId != 0)
            {
                *CapabilityId = Record->CapabilityId;
            }
            return 1;
        }
    }

    if (State->RecordCount >= LOS_CAPABILITIES_SERVICE_MAX_ENTRIES)
    {
        return 0;
    }

    Record = &State->Records[State->RecordCount];
    Record->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Record->CapabilityClass = CapabilityClass;
    Record->Flags = Flags;
    Record->Reserved = 0U;
    Record->CapabilityId = LosCapabilitiesComposeId(CapabilityClass, State->RecordCount);
    LosCapabilitiesCopyText(Record->Namespace, LOS_CAPABILITIES_SERVICE_NAMESPACE_LENGTH, Namespace);
    LosCapabilitiesCopyText(Record->Name, LOS_CAPABILITIES_SERVICE_NAME_LENGTH, Name);
    State->RecordCount += 1U;

    if (CapabilityId != 0)
    {
        *CapabilityId = Record->CapabilityId;
    }

    return 1;
}

UINT32 LosCapabilitiesServiceQuery(const LOS_CAPABILITIES_SERVICE_QUERY *Query,
                                   LOS_CAPABILITIES_SERVICE_QUERY_RESULT *Result)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    UINT32 Index;
    UINT32 MatchCount;
    BOOLEAN Match;

    if (Query == 0 || Result == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    Result->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_NOT_FOUND;
    Result->MatchCount = 0U;
    Result->Reserved = 0U;
    Result->Record.Version = 0U;
    Result->Record.CapabilityClass = 0U;
    Result->Record.Flags = 0U;
    Result->Record.Reserved = 0U;
    Result->Record.CapabilityId = 0ULL;
    Result->Record.Namespace[0] = 0;
    Result->Record.Name[0] = 0;

    State = LosCapabilitiesServiceState();
    MatchCount = 0U;
    for (Index = 0U; Index < State->RecordCount; ++Index)
    {
        Match = 0;
        if (Query->QueryType == LOS_CAPABILITIES_SERVICE_QUERY_EXACT)
        {
            Match = (BOOLEAN)(LosCapabilitiesTextEqual(State->Records[Index].Namespace, Query->Namespace) &&
                              LosCapabilitiesTextEqual(State->Records[Index].Name, Query->Name));
        }
        else if (Query->QueryType == LOS_CAPABILITIES_SERVICE_QUERY_PREFIX)
        {
            Match = (BOOLEAN)(LosCapabilitiesTextEqual(State->Records[Index].Namespace, Query->Namespace) &&
                              LosCapabilitiesTextStartsWith(State->Records[Index].Name, Query->Name));
        }
        else
        {
            return LOS_CAPABILITIES_SERVICE_STATUS_UNSUPPORTED;
        }

        if (Match)
        {
            MatchCount += 1U;
            if (MatchCount == 1U)
            {
                Result->Record = State->Records[Index];
            }
        }
    }

    Result->MatchCount = MatchCount;
    Result->Status = (MatchCount != 0U) ? LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS : LOS_CAPABILITIES_SERVICE_STATUS_NOT_FOUND;
    return Result->Status;
}

UINT32 LosCapabilitiesServiceEnumerate(UINT32 StartIndex,
                                       LOS_CAPABILITIES_SERVICE_ENUMERATION_HEADER *Header,
                                       LOS_CAPABILITIES_SERVICE_RECORD *Records,
                                       UINT32 Capacity)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    UINT32 Remaining;
    UINT32 CopyCount;
    UINT32 Index;

    if (Header == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    State = LosCapabilitiesServiceState();
    Header->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Header->Status = LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS;
    Header->Count = 0U;
    Header->Capacity = Capacity;

    if (StartIndex >= State->RecordCount)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS;
    }

    if (Capacity == 0U || Records == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    Remaining = State->RecordCount - StartIndex;
    CopyCount = (Remaining < Capacity) ? Remaining : Capacity;
    for (Index = 0U; Index < CopyCount; ++Index)
    {
        Records[Index] = State->Records[StartIndex + Index];
    }

    Header->Count = CopyCount;
    return LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS;
}

BOOLEAN LosCapabilitiesServiceBuildBootstrapContext(LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context,
                                                    UINT32 Capacity,
                                                    UINT32 *WrittenCount)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    UINT32 Index;

    if (WrittenCount != 0)
    {
        *WrittenCount = 0U;
    }
    if (Context == 0 || Capacity == 0U)
    {
        return 0;
    }

    State = LosCapabilitiesServiceState();
    Context->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Context->Flags = LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP;
    Context->BlockCount = 0U;
    Context->Capacity = Capacity;
    Context->AssignmentCount = 0U;
    Context->AssignmentCapacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_ASSIGNMENTS;
    Context->EventCount = 0U;
    Context->EventCapacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_EVENTS;

    if (State->BootstrapContext.BlockCount == 0U)
    {
        return 1;
    }

    for (Index = 0U; Index < State->BootstrapContext.BlockCount && Index < Capacity && Index < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_BLOCKS; ++Index)
    {
        Context->Blocks[Index] = State->BootstrapContext.Blocks[Index];
        Context->BlockCount += 1U;
    }

    for (Index = 0U; Index < State->BootstrapContext.AssignmentCount && Index < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_ASSIGNMENTS; ++Index)
    {
        Context->Assignments[Index] = State->BootstrapContext.Assignments[Index];
        Context->AssignmentCount += 1U;
    }

    for (Index = 0U; Index < State->BootstrapContext.EventCount && Index < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_EVENTS; ++Index)
    {
        Context->Events[Index] = State->BootstrapContext.Events[Index];
        Context->EventCount += 1U;
    }

    if (WrittenCount != 0)
    {
        *WrittenCount = Context->BlockCount;
    }

    return 1;
}
