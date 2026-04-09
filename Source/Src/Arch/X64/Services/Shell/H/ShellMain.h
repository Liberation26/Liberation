/*
 * File Name: ShellMain.h
 * File Version: 0.4.35
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-08T15:20:00Z
 * Last Update Timestamp: 2026-04-09T18:15:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#ifndef LOS_X64_SHELL_SERVICE_MAIN_H
#define LOS_X64_SHELL_SERVICE_MAIN_H

#include "Efi.h"
#include "CapabilitiesServiceAbi.h"
#include "ShellServiceAbi.h"
#include "LoginCommandAbi.h"
#include "StringLibraryAbi.h"
#include "UserImageAbi.h"

#define LOS_SHELL_SERVICE_HEARTBEAT_SPIN 1000000ULL
#define LOS_SHELL_SERVICE_INPUT_BUFFER_LENGTH LOS_SHELL_SERVICE_COMMAND_BUFFER_LENGTH

typedef struct
{
    UINT32 Online;
    UINT32 Reserved0;
    UINT64 Heartbeat;
    UINT64 CommandSequence;
    UINT64 LastStatus;
    UINT64 LastLaunchStatus;
    UINT64 PromptCount;
    UINT64 RequestsHandled;
    UINT64 ResponsesProduced;
    UINT64 TransportReady;
    UINT64 TransportPolls;
    UINT64 TransportGeneration;
    UINT64 RequestEndpointId;
    UINT64 ResponseEndpointId;
    UINT64 EventEndpointId;
    UINT64 ServiceId;
    UINT64 RegistryReady;
    UINT64 Authenticated;
    UINT64 LoginRequired;
    UINT64 RequestMailboxAddress;
    UINT64 ResponseMailboxAddress;
    LOS_SHELL_SERVICE_CHANNEL Channel;
    LOS_SHELL_SERVICE_DISCOVERY_RECORD DiscoveryRecord;
    LOS_SHELL_SERVICE_TRANSPORT_EXPORT TransportExport;
    LOS_SHELL_SERVICE_REGISTRY_ENTRY RegistryEntry;
    LOS_SHELL_SERVICE_MAILBOX RequestMailbox;
    LOS_SHELL_SERVICE_MAILBOX ResponseMailbox;
    char ServiceName[LOS_CAPABILITIES_SERVICE_NAME_LENGTH];
    char InputBuffer[LOS_SHELL_SERVICE_INPUT_BUFFER_LENGTH];
    char LastCommand[LOS_SHELL_SERVICE_INPUT_BUFFER_LENGTH];
    char LastResolvedPath[LOS_SHELL_SERVICE_INPUT_BUFFER_LENGTH];
    char LastNormalizedCommand[LOS_SHELL_SERVICE_INPUT_BUFFER_LENGTH];
    char WorkingDirectory[64];
} LOS_SHELL_SERVICE_STATE;

void LosShellServiceBootstrapEntry(void);
void LosShellServiceBootstrapEntryWithContext(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context);
void LosShellServiceEntry(void);
BOOLEAN LosShellServiceBringOnline(void);
LOS_SHELL_SERVICE_STATE *LosShellServiceState(void);
void LosShellServiceInitialize(void);
void LosShellServiceWriteText(const char *Text);
void LosShellServiceWriteUnsigned(UINT64 Value);
void LosShellServiceWriteLine(const char *Text);
void LosShellServiceYield(void);
void LosShellServiceShowPrompt(void);
void LosShellServiceRunBootstrapSession(void);
UINT64 LosShellServiceExecuteCommand(const char *Command, char *Output, UINTN OutputLength, UINT32 *ResponseFlags);
UINT64 LosShellServiceHandleRequest(const LOS_SHELL_SERVICE_REQUEST *Request,
                                    LOS_SHELL_SERVICE_RESPONSE *Response);
UINT64 LosShellServiceDispatchMailbox(LOS_SHELL_SERVICE_MAILBOX *RequestMailbox,
                                      LOS_SHELL_SERVICE_MAILBOX *ResponseMailbox);
void LosShellServicePreparePromptText(char *Buffer, UINTN BufferLength);
void LosShellServiceCopyText(char *Destination, UINTN DestinationLength, const char *Source);
void LosShellServiceAppendText(char *Destination, UINTN DestinationLength, const char *Source);
UINT64 LosShellServiceLaunchExternal(const char *CommandPath, const char *Arguments, UINT64 *CommandResult, char *Output, UINTN OutputLength);
void LosShellServiceClearResponse(LOS_SHELL_SERVICE_RESPONSE *Response);
UINT64 LosShellServiceUppercaseCommandExternal(const char *Input, char *Output, UINTN OutputLength);
const LOS_SHELL_SERVICE_DISCOVERY_RECORD *LosShellServiceGetDiscoveryRecord(void);
const LOS_SHELL_SERVICE_TRANSPORT_EXPORT *LosShellServiceExportTransport(void);
const LOS_SHELL_SERVICE_REGISTRY_ENTRY *LosShellServiceGetRegistryEntry(void);
UINT64 LosShellServiceBindTransport(LOS_SHELL_SERVICE_TRANSPORT_BINDING *Binding);
UINT64 LosShellServiceBindFromRequest(const LOS_SHELL_SERVICE_BIND_REQUEST *Request,
                                      LOS_SHELL_SERVICE_TRANSPORT_BINDING *Binding);
UINT64 LosShellServiceResolveFromRequest(const LOS_SHELL_SERVICE_RESOLVE_REQUEST *Request,
                                         LOS_SHELL_SERVICE_RESOLVE_RESPONSE *Response);
UINT64 LosUserLoadAndCallImage(const LOS_USER_IMAGE_CALL *Call);
UINT64 LosUserReadImageFile(const char *Path, void *Buffer, UINTN BufferLength, UINTN *BytesRead);
UINT64 LosUserExecuteLoadedImage(const LOS_USER_IMAGE_EXECUTION_CONTEXT *Context);
UINT64 LosUserExecuteIsolatedImage(const LOS_USER_IMAGE_EXECUTION_CONTEXT *Context,
                                   LOS_USER_IMAGE_ISOLATED_SPACE *IsolatedSpace,
                                   LOS_USER_IMAGE_RING3_CONTEXT *Ring3Context);
UINT64 LosShellEnterUserMode(const LOS_USER_IMAGE_RING3_CONTEXT *Context);
UINT64 LosShellEnterUserModeAsm(const LOS_USER_IMAGE_RING3_CONTEXT *Context);
UINT64 LosShellUserModeResumeTrampoline(void);
UINT64 LosShellServiceSignalEvent(UINT64 EventCode, UINT64 EventValue);

#endif

UINT64 LosMemoryManagerActivateUserAddressSpace(const LOS_USER_IMAGE_ADDRESS_SPACE_DESCRIPTOR *Descriptor);
UINT64 LosMemoryManagerCompleteUserAddressSpaceCall(const LOS_USER_IMAGE_ISOLATED_SPACE *IsolatedSpace, UINT64 *CompletionStatus, UINT64 *CompletionResult);

void LosShellStageUserModeCompletion(UINT64 Status, UINT64 ResultValue);
UINT64 LosShellUserModeReturnAsm(void);

UINT64 LosShellRuntimeInvokeUserImageCall(LOS_USER_IMAGE_CALL *Call);
