/*
 * File Name: CapabilitiesMain.h
 * File Version: 0.3.31
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T11:02:18Z
 * Last Update Timestamp: 2026-04-08T14:20:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#ifndef LOS_X64_CAPABILITIES_SERVICE_MAIN_H
#define LOS_X64_CAPABILITIES_SERVICE_MAIN_H

#include "Efi.h"
#include "CapabilitiesServiceAbi.h"

#define LOS_CAPABILITIES_SERVICE_HEARTBEAT_SPIN 1000000ULL
#define LOS_CAPABILITIES_SERVICE_TRANSPORT_SPIN 1024ULL

typedef struct
{
    UINT32 Online;
    UINT32 Reserved0;
    UINT64 Heartbeat;
    UINT64 NextRequestSequence;
    UINT64 TransportGeneration;
    UINT64 TransportSessionId;
    UINT64 ActiveSessionId;
    UINT64 ActiveClientNonce;
    UINT64 ActiveServerNonce;
    UINT64 ActiveClientPrincipalId;
    UINT32 ActiveClientPrincipalType;
    UINT32 ReservedClient;
    UINT64 LastAcceptedRequestSequence;
    UINT64 LastCompletedResponseSequence;
    UINT64 RequestAuthKey[2];
    UINT64 ResponseAuthKey[2];
    UINT64 ActiveRequestAuthKey[2];
    UINT64 ActiveResponseAuthKey[2];
    UINT64 ActiveRequestCipherKey[2];
    UINT64 ActiveResponseCipherKey[2];
    UINT32 ServiceClass;
    UINT32 ActiveEndpointClass;
    UINT32 MinimumSecurityMode;
    UINT32 ActiveSecurityMode;
    char ServiceName[LOS_CAPABILITIES_SERVICE_NAME_LENGTH];
    char ActiveClientPrincipalName[LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH];
    char ActiveClientServiceName[LOS_CAPABILITIES_SERVICE_NAME_LENGTH];
    LOS_CAPABILITIES_TRANSPORT_EXPORT TransportExport;
    LOS_CAPABILITIES_ENDPOINT_OBJECT ReceiveEndpoint;
    LOS_CAPABILITIES_ENDPOINT_OBJECT ReplyEndpoint;
    LOS_CAPABILITIES_REQUEST_MAILBOX RequestMailbox;
    LOS_CAPABILITIES_RESPONSE_MAILBOX ResponseMailbox;
    UINT32 RecordCount;
    UINT32 BootstrapBlockCount;
    UINT32 BootstrapAssignmentCount;
    UINT32 BootstrapEventCount;
    UINT32 RuntimeGrantCount;
    UINT32 RuntimeEventCount;
    UINT64 NextGrantId;
    UINT64 NextEventId;
    LOS_CAPABILITIES_BOOTSTRAP_CONTEXT BootstrapContext;
    LOS_CAPABILITIES_SERVICE_RECORD Records[LOS_CAPABILITIES_SERVICE_MAX_ENTRIES];
    LOS_CAPABILITY_GRANT_ENTRY RuntimeGrants[LOS_CAPABILITIES_RUNTIME_MAX_GRANTS];
    LOS_CAPABILITY_GRANT_EVENT RuntimeEvents[LOS_CAPABILITIES_RUNTIME_MAX_EVENTS];
} LOS_CAPABILITIES_SERVICE_STATE;

void LosCapabilitiesServiceBootstrapEntry(void);
void LosCapabilitiesServiceEntry(void);
BOOLEAN LosCapabilitiesServiceBringOnline(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context);
LOS_CAPABILITIES_SERVICE_STATE *LosCapabilitiesServiceState(void);
void LosCapabilitiesServiceInitialize(void);
BOOLEAN LosCapabilitiesServiceSeedBootstrapRegistry(void);
BOOLEAN LosCapabilitiesServiceImportBootstrapContext(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context);
void LosCapabilitiesServiceSetBootstrapContext(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context);
const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *LosCapabilitiesServiceResolveBootstrapContext(void);
BOOLEAN LosCapabilitiesServiceRegister(const char *Namespace,
                                       const char *Name,
                                       UINT32 CapabilityClass,
                                       UINT32 Flags,
                                       UINT64 *CapabilityId);
UINT32 LosCapabilitiesServiceQuery(const LOS_CAPABILITIES_SERVICE_QUERY *Query,
                                   LOS_CAPABILITIES_SERVICE_QUERY_RESULT *Result);
UINT32 LosCapabilitiesServiceEnumerate(UINT32 StartIndex,
                                       LOS_CAPABILITIES_SERVICE_ENUMERATION_HEADER *Header,
                                       LOS_CAPABILITIES_SERVICE_RECORD *Records,
                                       UINT32 Capacity);
BOOLEAN LosCapabilitiesServiceBuildBootstrapContext(LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context,
                                                    UINT32 Capacity,
                                                    UINT32 *WrittenCount);
UINT32 LosCapabilitiesServiceGrant(const LOS_CAPABILITIES_MUTATION_REQUEST *Request,
                                   LOS_CAPABILITIES_MUTATION_RESULT *Result);
UINT32 LosCapabilitiesServiceRevoke(const LOS_CAPABILITIES_MUTATION_REQUEST *Request,
                                    LOS_CAPABILITIES_MUTATION_RESULT *Result);
UINT32 LosCapabilitiesServiceSuspend(const LOS_CAPABILITIES_MUTATION_REQUEST *Request,
                                     LOS_CAPABILITIES_MUTATION_RESULT *Result);
UINT32 LosCapabilitiesServiceRestore(const LOS_CAPABILITIES_MUTATION_REQUEST *Request,
                                     LOS_CAPABILITIES_MUTATION_RESULT *Result);
const LOS_CAPABILITY_PROFILE_ASSIGNMENT *LosCapabilitiesServiceFindAssignment(UINT32 PrincipalType, const char *PrincipalName);
const LOS_CAPABILITY_GRANT_BLOCK *LosCapabilitiesServiceFindProfileBlock(const char *ProfileName);
BOOLEAN LosCapabilitiesServiceHasActiveGrant(UINT32 PrincipalType,
                                             UINT64 PrincipalId,
                                             const char *Namespace,
                                             const char *Name,
                                             LOS_CAPABILITY_GRANT_ENTRY *Grant);
UINT32 LosCapabilitiesServiceCheckAccess(const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                         LOS_CAPABILITIES_ACCESS_RESULT *Result);
void LosCapabilitiesServiceRunSelfTest(void);
void LosCapabilitiesServiceWriteText(const char *Text);
void LosCapabilitiesServiceWriteUnsigned(UINT64 Value);
void LosCapabilitiesServiceWriteHex(UINT64 Value);
void LosCapabilitiesServiceWriteLine(const char *Text);
void LosCapabilitiesServiceYield(void);

BOOLEAN LosCapabilitiesServiceInitializeTransport(void);
const LOS_CAPABILITIES_TRANSPORT_EXPORT *LosCapabilitiesServiceExportTransport(void);
UINT32 LosCapabilitiesServiceBindTransport(LOS_CAPABILITIES_TRANSPORT_BINDING *Binding);
UINT32 LosCapabilitiesServiceConnectTransport(const LOS_CAPABILITIES_TRANSPORT_CONNECT_REQUEST *Request,
                                              LOS_CAPABILITIES_TRANSPORT_CONNECT_RESULT *Result,
                                              LOS_CAPABILITIES_TRANSPORT_BINDING *Binding);
UINT32 LosCapabilitiesServiceSubmitBoundAccessRequest(LOS_CAPABILITIES_TRANSPORT_BINDING *Binding,
                                                      const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                                      LOS_CAPABILITIES_ACCESS_RESULT *Result);
UINT32 LosCapabilitiesServiceSubmitAccessRequest(const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                                 LOS_CAPABILITIES_ACCESS_RESULT *Result);
BOOLEAN LosCapabilitiesServiceServiceTransportOnce(void);

#endif
