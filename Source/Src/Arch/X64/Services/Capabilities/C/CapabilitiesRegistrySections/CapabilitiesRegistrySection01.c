/*
 * File Name: CapabilitiesRegistrySection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from CapabilitiesRegistry.c.
 */

static void LosCapabilitiesCopyText(char *Destination, UINTN Capacity, const char *Source)
{
    UINTN Index;

    if (Destination == 0 || Capacity == 0U)
    {
        return;
    }

    for (Index = 0U; Index < Capacity; ++Index)
    {
        Destination[Index] = 0;
    }

    if (Source == 0)
    {
        return;
    }

    for (Index = 0U; Index + 1U < Capacity && Source[Index] != 0; ++Index)
    {
        Destination[Index] = Source[Index];
    }
}

static BOOLEAN LosCapabilitiesTextEqual(const char *Left, const char *Right)
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

static BOOLEAN LosCapabilitiesTextStartsWith(const char *Text, const char *Prefix)
{
    UINTN Index;

    if (Text == 0 || Prefix == 0)
    {
        return 0;
    }

    for (Index = 0U; Prefix[Index] != 0; ++Index)
    {
        if (Text[Index] != Prefix[Index])
        {
            return 0;
        }
    }

    return 1;
}

static UINT64 LosCapabilitiesComposeId(UINT32 CapabilityClass, UINT32 Index)
{
    return (((UINT64)LOS_CAPABILITIES_SERVICE_VERSION) << 56) |
           (((UINT64)CapabilityClass) << 32) |
           (UINT64)(Index + 1U);
}

static LOS_CAPABILITY_GRANT_ENTRY *LosCapabilitiesFindGrantById(UINT64 GrantId)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    UINT32 Index;

    if (GrantId == 0ULL)
    {
        return 0;
    }

    State = LosCapabilitiesServiceState();
    for (Index = 0U; Index < State->RuntimeGrantCount; ++Index)
    {
        if (State->RuntimeGrants[Index].GrantId == GrantId)
        {
            return &State->RuntimeGrants[Index];
        }
    }

    return 0;
}

static BOOLEAN LosCapabilitiesFindCapabilityRecord(const char *Namespace,
                                                   const char *Name,
                                                   LOS_CAPABILITIES_SERVICE_RECORD *Record)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    UINT32 Index;

    if (Namespace == 0 || Name == 0)
    {
        return 0;
    }

    State = LosCapabilitiesServiceState();
    for (Index = 0U; Index < State->RecordCount; ++Index)
    {
        if (LosCapabilitiesTextEqual(State->Records[Index].Namespace, Namespace) &&
            LosCapabilitiesTextEqual(State->Records[Index].Name, Name))
        {
            if (Record != 0)
            {
                *Record = State->Records[Index];
            }
            return 1;
        }
    }

    return 0;
}

static UINT64 LosCapabilitiesAppendEvent(UINT32 EventType,
                                         UINT32 ActorType,
                                         UINT64 GrantId,
                                         UINT64 EventAtUtc,
                                         UINT64 PrincipalId,
                                         const char *PrincipalName,
                                         const char *CapabilityNamespace,
                                         const char *CapabilityName,
                                         const char *ActorName)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    LOS_CAPABILITY_GRANT_EVENT *Event;

    State = LosCapabilitiesServiceState();
    if (State->RuntimeEventCount >= LOS_CAPABILITIES_RUNTIME_MAX_EVENTS)
    {
        return 0ULL;
    }

    Event = &State->RuntimeEvents[State->RuntimeEventCount];
    Event->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Event->EventType = EventType;
    Event->ActorType = ActorType;
    Event->Reserved = 0U;
    Event->EventId = State->NextEventId;
    Event->GrantId = GrantId;
    Event->EventAtUtc = EventAtUtc;
    Event->PrincipalId = PrincipalId;
    LosCapabilitiesCopyText(Event->PrincipalName, LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH, PrincipalName);
    LosCapabilitiesCopyText(Event->CapabilityNamespace, LOS_CAPABILITIES_SERVICE_NAMESPACE_LENGTH, CapabilityNamespace);
    LosCapabilitiesCopyText(Event->CapabilityName, LOS_CAPABILITIES_SERVICE_NAME_LENGTH, CapabilityName);
    LosCapabilitiesCopyText(Event->ActorName, LOS_CAPABILITIES_AUTHORISER_NAME_LENGTH, ActorName);

    State->RuntimeEventCount += 1U;
    State->NextEventId += 1ULL;
    return Event->EventId;
}

static UINT32 LosCapabilitiesMutationCommon(const LOS_CAPABILITIES_MUTATION_REQUEST *Request,
                                            LOS_CAPABILITIES_MUTATION_RESULT *Result,
                                            UINT32 TargetState)
{
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

    Grant = LosCapabilitiesFindGrantById(Request->GrantId);
    if (Grant == 0)
    {
        Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_NOT_FOUND;
        return Result->Status;
    }

    if (TargetState == LOS_CAPABILITIES_GRANT_STATE_REVOKED)
    {
        if (Grant->State == LOS_CAPABILITIES_GRANT_STATE_REVOKED)
        {
            Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_INVALID_STATE;
            Result->State = Grant->State;
            Result->GrantId = Grant->GrantId;
            return Result->Status;
        }
        Grant->State = LOS_CAPABILITIES_GRANT_STATE_REVOKED;
        Grant->RevokedAtUtc = Request->TimestampUtc;
        EventId = LosCapabilitiesAppendEvent(LOS_CAPABILITIES_GRANT_EVENT_REVOKED,
                                             Request->AuthoriserType,
                                             Grant->GrantId,
                                             Request->TimestampUtc,
                                             Request->PrincipalId,
                                             Request->PrincipalName,
                                             Grant->Namespace,
                                             Grant->Name,
                                             Request->AuthoriserName);
    }
    else if (TargetState == LOS_CAPABILITIES_GRANT_STATE_SUSPENDED)
    {
        if (Grant->State != LOS_CAPABILITIES_GRANT_STATE_ACTIVE)
        {
            Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_INVALID_STATE;
            Result->State = Grant->State;
            Result->GrantId = Grant->GrantId;
            return Result->Status;
        }
        Grant->State = LOS_CAPABILITIES_GRANT_STATE_SUSPENDED;
        Grant->SuspendedAtUtc = Request->TimestampUtc;
        EventId = LosCapabilitiesAppendEvent(LOS_CAPABILITIES_GRANT_EVENT_SUSPENDED,
                                             Request->AuthoriserType,
                                             Grant->GrantId,
                                             Request->TimestampUtc,
                                             Request->PrincipalId,
                                             Request->PrincipalName,
                                             Grant->Namespace,
                                             Grant->Name,
                                             Request->AuthoriserName);
    }
    else if (TargetState == LOS_CAPABILITIES_GRANT_STATE_ACTIVE)
    {
        if (Grant->State != LOS_CAPABILITIES_GRANT_STATE_SUSPENDED)
        {
            Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_INVALID_STATE;
            Result->State = Grant->State;
            Result->GrantId = Grant->GrantId;
            return Result->Status;
        }
        Grant->State = LOS_CAPABILITIES_GRANT_STATE_ACTIVE;
        Grant->SuspendedAtUtc = 0ULL;
        EventId = LosCapabilitiesAppendEvent(LOS_CAPABILITIES_GRANT_EVENT_RESTORED,
                                             Request->AuthoriserType,
                                             Grant->GrantId,
                                             Request->TimestampUtc,
                                             Request->PrincipalId,
                                             Request->PrincipalName,
                                             Grant->Namespace,
                                             Grant->Name,
                                             Request->AuthoriserName);
    }
    else
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_UNSUPPORTED;
    }

    Result->Status = (EventId != 0ULL) ? LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS : LOS_CAPABILITIES_SERVICE_STATUS_TABLE_FULL;
    Result->State = Grant->State;
    Result->GrantId = Grant->GrantId;
    Result->EventId = EventId;
    return Result->Status;
}

const LOS_CAPABILITY_PROFILE_ASSIGNMENT *LosCapabilitiesServiceFindAssignment(UINT32 PrincipalType, const char *PrincipalName)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    UINT32 Index;

    if (PrincipalName == 0 || PrincipalName[0] == 0)
    {
        return 0;
    }

    State = LosCapabilitiesServiceState();
    for (Index = 0U; Index < State->BootstrapContext.AssignmentCount && Index < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_ASSIGNMENTS; ++Index)
    {
        const LOS_CAPABILITY_PROFILE_ASSIGNMENT *Assignment;

        Assignment = &State->BootstrapContext.Assignments[Index];
        if (Assignment->PrincipalType != PrincipalType)
        {
            continue;
        }
        if (LosCapabilitiesTextEqual(Assignment->PrincipalName, PrincipalName))
        {
            return Assignment;
        }
    }

    return 0;
}

const LOS_CAPABILITY_GRANT_BLOCK *LosCapabilitiesServiceFindProfileBlock(const char *ProfileName)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;
    UINT32 Index;

    if (ProfileName == 0 || ProfileName[0] == 0)
    {
        return 0;
    }

    State = LosCapabilitiesServiceState();
    for (Index = 0U; Index < State->BootstrapContext.BlockCount && Index < LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_BLOCKS; ++Index)
    {
        const LOS_CAPABILITY_GRANT_BLOCK *Block;

        Block = &State->BootstrapContext.Blocks[Index];
        if (LosCapabilitiesTextEqual(Block->ProfileName, ProfileName))
        {
            return Block;
        }
    }

    return 0;
}

UINT32 LosCapabilitiesServiceCheckAccess(const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                         LOS_CAPABILITIES_ACCESS_RESULT *Result)
{
    LOS_CAPABILITY_GRANT_ENTRY MatchingGrant;
    const LOS_CAPABILITY_PROFILE_ASSIGNMENT *Assignment;
    const LOS_CAPABILITY_GRANT_BLOCK *ProfileBlock;
    BOOLEAN Granted;

    if (Request == 0 || Result == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    Result->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_NOT_FOUND;
    Result->Granted = 0U;
    Result->Reserved0 = 0U;
    Result->MatchingGrantId = 0ULL;

    Granted = LosCapabilitiesServiceHasActiveGrant(Request->PrincipalType,
                                                   Request->PrincipalId,
                                                   Request->Namespace,
                                                   Request->Name,
                                                   &MatchingGrant);
    if (!Granted && Request->PrincipalName[0] != 0)
    {
        Assignment = LosCapabilitiesServiceFindAssignment(Request->PrincipalType, Request->PrincipalName);
        if (Assignment != 0)
        {
            ProfileBlock = LosCapabilitiesServiceFindProfileBlock(Assignment->ProfileName);
            if (ProfileBlock != 0)
            {
                Granted = LosCapabilitiesServiceHasActiveGrant(ProfileBlock->PrincipalType,
                                                               ProfileBlock->PrincipalId,
                                                               Request->Namespace,
                                                               Request->Name,
                                                               &MatchingGrant);
            }
        }
    }
    if (!Granted)
    {
        return Result->Status;
    }

    Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS;
    Result->Granted = 1U;
    Result->MatchingGrantId = MatchingGrant.GrantId;
    return Result->Status;
}
