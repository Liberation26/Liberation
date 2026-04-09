/*
 * File Name: CapabilitiesRegistry.c
 * File Version: 0.3.22
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T11:02:18Z
 * Last Update Timestamp: 2026-04-08T11:10:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements runtime registry, grant state, and mutation support for the capabilities service.
 */

#include "CapabilitiesMain.h"

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
