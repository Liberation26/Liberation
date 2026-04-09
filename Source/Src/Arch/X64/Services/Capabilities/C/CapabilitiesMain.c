/*
 * File Name: CapabilitiesMain.c
 * File Version: 0.3.26
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T11:02:18Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#include "CapabilitiesMain.h"

static void LosCapabilitiesDumpRecord(const LOS_CAPABILITIES_SERVICE_RECORD *Record)
{
    if (Record == 0)
    {
        return;
    }

    LosCapabilitiesServiceWriteText("[Caps] capability ");
    LosCapabilitiesServiceWriteText((const char *)Record->Namespace);
    LosCapabilitiesServiceWriteText(".");
    LosCapabilitiesServiceWriteText((const char *)Record->Name);
    LosCapabilitiesServiceWriteText(" id=");
    LosCapabilitiesServiceWriteHex(Record->CapabilityId);
    LosCapabilitiesServiceWriteText(" flags=");
    LosCapabilitiesServiceWriteUnsigned((UINT64)Record->Flags);
    LosCapabilitiesServiceWriteText("\n");
}

void LosCapabilitiesServiceRunSelfTest(void)
{
    LOS_CAPABILITIES_SERVICE_QUERY Query;
    LOS_CAPABILITIES_SERVICE_QUERY_RESULT QueryResult;
    LOS_CAPABILITIES_MUTATION_REQUEST Request;
    LOS_CAPABILITIES_MUTATION_RESULT Result;

    Query.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Query.QueryType = LOS_CAPABILITIES_SERVICE_QUERY_EXACT;
    Query.Reserved0 = 0U;
    Query.Reserved1 = 0U;
    Query.Namespace[0] = 'c';
    Query.Namespace[1] = 'a';
    Query.Namespace[2] = 'p';
    Query.Namespace[3] = 'a';
    Query.Namespace[4] = 'b';
    Query.Namespace[5] = 'i';
    Query.Namespace[6] = 'l';
    Query.Namespace[7] = 'i';
    Query.Namespace[8] = 't';
    Query.Namespace[9] = 'y';
    Query.Namespace[10] = 0;
    Query.Name[0] = 'g';
    Query.Name[1] = 'r';
    Query.Name[2] = 'a';
    Query.Name[3] = 'n';
    Query.Name[4] = 't';
    Query.Name[5] = 0;

    if (LosCapabilitiesServiceQuery(&Query, &QueryResult) == LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS)
    {
        LosCapabilitiesServiceWriteText("[Caps] query self-test ok ");
        LosCapabilitiesDumpRecord(&QueryResult.Record);
    }

    Request.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Request.MutationType = LOS_CAPABILITIES_MUTATION_GRANT;
    Request.PrincipalType = LOS_CAPABILITIES_PRINCIPAL_TYPE_SERVICE;
    Request.CapabilityClass = LOS_CAPABILITIES_SERVICE_CLASS_SERVICE;
    Request.Flags = LOS_CAPABILITIES_SERVICE_FLAG_GRANTED | LOS_CAPABILITIES_SERVICE_FLAG_AUDIT;
    Request.AuthoriserType = LOS_CAPABILITIES_AUTHORISER_SERVICE;
    Request.Reserved0 = 0U;
    Request.Reserved1 = 0U;
    Request.PrincipalId = 1ULL;
    Request.GrantId = 0ULL;
    Request.ParentGrantId = 0ULL;
    Request.TimestampUtc = 20260408090000ULL;
    Request.EffectiveFromUtc = Request.TimestampUtc;
    Request.EffectiveUntilUtc = 0ULL;
    LosCapabilitiesServiceWriteText("");
    Query.Namespace[0] = 0;
    LosCapabilitiesServiceWriteText("");
    {
        const char PrincipalName[] = "capsmgr";
        const char Namespace[] = "capability";
        const char Name[] = "query";
        const char Authoriser[] = "capsmgr";
        UINTN Index;
        for (Index = 0U; Index < LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH; ++Index) { Request.PrincipalName[Index] = 0; }
        for (Index = 0U; Index < LOS_CAPABILITIES_SERVICE_NAMESPACE_LENGTH; ++Index) { Request.Namespace[Index] = 0; }
        for (Index = 0U; Index < LOS_CAPABILITIES_SERVICE_NAME_LENGTH; ++Index) { Request.Name[Index] = 0; }
        for (Index = 0U; Index < LOS_CAPABILITIES_AUTHORISER_NAME_LENGTH; ++Index) { Request.AuthoriserName[Index] = 0; }
        for (Index = 0U; Index + 1U < LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH && PrincipalName[Index] != 0; ++Index) { Request.PrincipalName[Index] = PrincipalName[Index]; }
        for (Index = 0U; Index + 1U < LOS_CAPABILITIES_SERVICE_NAMESPACE_LENGTH && Namespace[Index] != 0; ++Index) { Request.Namespace[Index] = Namespace[Index]; }
        for (Index = 0U; Index + 1U < LOS_CAPABILITIES_SERVICE_NAME_LENGTH && Name[Index] != 0; ++Index) { Request.Name[Index] = Name[Index]; }
        for (Index = 0U; Index + 1U < LOS_CAPABILITIES_AUTHORISER_NAME_LENGTH && Authoriser[Index] != 0; ++Index) { Request.AuthoriserName[Index] = Authoriser[Index]; }
    }

    if (LosCapabilitiesServiceGrant(&Request, &Result) == LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS)
    {
        LosCapabilitiesServiceWriteText("[Caps] runtime grant created grantId=");
        LosCapabilitiesServiceWriteUnsigned(Result.GrantId);
        LosCapabilitiesServiceWriteText(" eventId=");
        LosCapabilitiesServiceWriteUnsigned(Result.EventId);
        LosCapabilitiesServiceWriteText("\n");

        Request.GrantId = Result.GrantId;
        Request.MutationType = LOS_CAPABILITIES_MUTATION_SUSPEND;
        Request.TimestampUtc += 1ULL;
        if (LosCapabilitiesServiceSuspend(&Request, &Result) == LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS)
        {
            LosCapabilitiesServiceWriteText("[Caps] runtime suspend ok grantId=");
            LosCapabilitiesServiceWriteUnsigned(Result.GrantId);
            LosCapabilitiesServiceWriteText("\n");
        }

        Request.MutationType = LOS_CAPABILITIES_MUTATION_RESTORE;
        Request.TimestampUtc += 1ULL;
        if (LosCapabilitiesServiceRestore(&Request, &Result) == LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS)
        {
            LosCapabilitiesServiceWriteText("[Caps] runtime restore ok grantId=");
            LosCapabilitiesServiceWriteUnsigned(Result.GrantId);
            LosCapabilitiesServiceWriteText("\n");
        }

        Request.MutationType = LOS_CAPABILITIES_MUTATION_REVOKE;
        Request.TimestampUtc += 1ULL;
        if (LosCapabilitiesServiceRevoke(&Request, &Result) == LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS)
        {
            LosCapabilitiesServiceWriteText("[Caps] runtime revoke ok grantId=");
            LosCapabilitiesServiceWriteUnsigned(Result.GrantId);
            LosCapabilitiesServiceWriteText("\n");
        }
    }
}

BOOLEAN LosCapabilitiesServiceBringOnline(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context)
{
    LOS_CAPABILITIES_SERVICE_STATE *State;

    LosCapabilitiesServiceInitialize();
    State = LosCapabilitiesServiceState();
    LosCapabilitiesServiceWriteText("[Caps] CAPSMGR bootstrap starting.\n");

    if (LosCapabilitiesServiceSeedBootstrapRegistry())
    {
        LosCapabilitiesServiceWriteText("[Caps] Seed registry ok.\n");
    }

    if (Context != 0)
    {
        LosCapabilitiesServiceSetBootstrapContext(Context);
    }
    else
    {
        Context = LosCapabilitiesServiceResolveBootstrapContext();
    }

    if (Context != 0)
    {
        if (LosCapabilitiesServiceImportBootstrapContext(Context))
        {
            LosCapabilitiesServiceWriteText("[Caps] Bootstrap context imported.\n");
        }
        else
        {
            LosCapabilitiesServiceWriteText("[Caps] Bootstrap import failed.\n");
            return 0;
        }
    }

    if (LosCapabilitiesServiceInitializeTransport())
    {
        LosCapabilitiesServiceWriteText("[Caps] endpoint transport online.\n");
    }

    LosCapabilitiesServiceRunSelfTest();

    State->Online = 1U;
    LosCapabilitiesServiceWriteText("[Caps] CAPSMGR ONLINE.\n");
    LosCapabilitiesServiceWriteText("[Caps] records=");
    LosCapabilitiesServiceWriteUnsigned((UINT64)State->RecordCount);
    LosCapabilitiesServiceWriteText(" blocks=");
    LosCapabilitiesServiceWriteUnsigned((UINT64)State->BootstrapBlockCount);
    LosCapabilitiesServiceWriteText(" assignments=");
    LosCapabilitiesServiceWriteUnsigned((UINT64)State->BootstrapAssignmentCount);
    LosCapabilitiesServiceWriteText(" events=");
    LosCapabilitiesServiceWriteUnsigned((UINT64)State->BootstrapEventCount);
    LosCapabilitiesServiceWriteText(" runtimeGrants=");
    LosCapabilitiesServiceWriteUnsigned((UINT64)State->RuntimeGrantCount);
    LosCapabilitiesServiceWriteText(" runtimeEvents=");
    LosCapabilitiesServiceWriteUnsigned((UINT64)State->RuntimeEventCount);
    LosCapabilitiesServiceWriteText("\n");
    return 1;
}

void LosCapabilitiesServiceEntry(void)
{
    UINT64 Spin;
    LOS_CAPABILITIES_SERVICE_STATE *State;

    if (!LosCapabilitiesServiceBringOnline(0))
    {
        return;
    }

    State = LosCapabilitiesServiceState();
    for (;;)
    {
        for (Spin = 0ULL; Spin < LOS_CAPABILITIES_SERVICE_HEARTBEAT_SPIN; ++Spin)
        {
            (void)LosCapabilitiesServiceServiceTransportOnce();
            LosCapabilitiesServiceYield();
        }

        State->Heartbeat += 1ULL;
        LosCapabilitiesServiceWriteText("[Caps] heartbeat=");
        LosCapabilitiesServiceWriteUnsigned(State->Heartbeat);
        LosCapabilitiesServiceWriteText(" records=");
        LosCapabilitiesServiceWriteUnsigned((UINT64)State->RecordCount);
        LosCapabilitiesServiceWriteText(" grants=");
        LosCapabilitiesServiceWriteUnsigned((UINT64)State->RuntimeGrantCount);
        LosCapabilitiesServiceWriteText(" events=");
        LosCapabilitiesServiceWriteUnsigned((UINT64)State->RuntimeEventCount);
        LosCapabilitiesServiceWriteText("\n");
    }
}

void LosCapabilitiesServiceBootstrapEntryWithContext(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context)
{
    (void)LosCapabilitiesServiceBringOnline(Context);
}

void LosCapabilitiesServiceBootstrapEntry(void)
{
    LosCapabilitiesServiceBootstrapEntryWithContext(LosCapabilitiesServiceResolveBootstrapContext());
}
