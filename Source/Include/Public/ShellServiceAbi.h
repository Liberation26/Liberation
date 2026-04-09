/*
 * File Name: ShellServiceAbi.h
 * File Version: 0.4.10
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-08T15:45:00Z
 * Last Update Timestamp: 2026-04-08T19:05:00Z
 * Operating System Name: Liberation OS
 * Purpose: Declares shared interfaces and data structures for Liberation OS.
 */

#ifndef LOS_PUBLIC_SHELL_SERVICE_ABI_H
#define LOS_PUBLIC_SHELL_SERVICE_ABI_H

#include "Efi.h"

#define LOS_SHELL_SERVICE_VERSION 1U
#define LOS_SHELL_SERVICE_COMMAND_BUFFER_LENGTH 128U
#define LOS_SHELL_SERVICE_OUTPUT_BUFFER_LENGTH 256U

#define LOS_SHELL_SERVICE_REQUEST_SIGNATURE           0x51525348454C4C31ULL
#define LOS_SHELL_SERVICE_RESPONSE_SIGNATURE          0x52535048454C4C31ULL
#define LOS_SHELL_SERVICE_CHANNEL_SIGNATURE           0x43484E5348454C31ULL
#define LOS_SHELL_SERVICE_MAILBOX_SIGNATURE           0x4D424F5853484C31ULL
#define LOS_SHELL_SERVICE_TRANSPORT_EXPORT_SIGNATURE  0x5348545850484C31ULL
#define LOS_SHELL_SERVICE_TRANSPORT_BINDING_SIGNATURE 0x5348424E44484C31ULL
#define LOS_SHELL_SERVICE_DISCOVERY_SIGNATURE         0x5348445348454C31ULL
#define LOS_SHELL_SERVICE_BIND_REQUEST_SIGNATURE      0x5348425251484C31ULL
#define LOS_SHELL_SERVICE_REGISTRY_ENTRY_SIGNATURE    0x5348524745484C31ULL
#define LOS_SHELL_SERVICE_RESOLVE_REQUEST_SIGNATURE   0x5348525251484C31ULL
#define LOS_SHELL_SERVICE_RESOLVE_RESPONSE_SIGNATURE  0x5348525350484C31ULL

#define LOS_SHELL_SERVICE_COMMAND_NOP      0U
#define LOS_SHELL_SERVICE_COMMAND_EXECUTE  1U
#define LOS_SHELL_SERVICE_COMMAND_QUERY    2U

#define LOS_SHELL_SERVICE_STATUS_SUCCESS            0U
#define LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER  1U
#define LOS_SHELL_SERVICE_STATUS_UNSUPPORTED        2U
#define LOS_SHELL_SERVICE_STATUS_UNKNOWN_COMMAND    3U
#define LOS_SHELL_SERVICE_STATUS_TRUNCATED          4U
#define LOS_SHELL_SERVICE_STATUS_OFFLINE            5U
#define LOS_SHELL_SERVICE_STATUS_NOT_ATTACHED       6U
#define LOS_SHELL_SERVICE_STATUS_RETRY              7U
#define LOS_SHELL_SERVICE_STATUS_NOT_FOUND          8U
#define LOS_SHELL_SERVICE_STATUS_LAUNCH_FAILED      9U
#define LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED      10U

#define LOS_SHELL_SERVICE_QUERY_STATUS 1U
#define LOS_SHELL_SERVICE_QUERY_PROMPT 2U

#define LOS_SHELL_SERVICE_FLAG_CAPTURE_OUTPUT          0x00000001U
#define LOS_SHELL_SERVICE_FLAG_TRANSPORT_LIVE          0x00000002U
#define LOS_SHELL_SERVICE_FLAG_BOOTSTRAP_SHARED_MEMORY 0x00000004U
#define LOS_SHELL_SERVICE_FLAG_EVENT_LIVE              0x00000008U
#define LOS_SHELL_SERVICE_FLAG_SERVICE_VISIBLE         0x00000010U
#define LOS_SHELL_SERVICE_FLAG_REGISTRY_VISIBLE        0x00000020U
#define LOS_SHELL_SERVICE_FLAG_INTERNAL_COMMAND      0x00000040U
#define LOS_SHELL_SERVICE_FLAG_EXTERNAL_COMMAND      0x00000080U
#define LOS_SHELL_SERVICE_FLAG_AUTH_REQUIRED         0x00000100U
#define LOS_SHELL_SERVICE_FLAG_COMMAND_UPPERCASED    0x00000200U

#define LOS_SHELL_SERVICE_MAILBOX_STATE_EMPTY    0U
#define LOS_SHELL_SERVICE_MAILBOX_STATE_READY    1U
#define LOS_SHELL_SERVICE_MAILBOX_STATE_CONSUMED 2U

#define LOS_SHELL_SERVICE_EVENT_ONLINE            1ULL
#define LOS_SHELL_SERVICE_EVENT_COMMAND_COMPLETE  2ULL

typedef struct
{
    UINT32 Version;
    UINT32 Command;
    UINT32 Flags;
    UINT32 Reserved0;
    UINT64 Signature;
    UINT64 RequestId;
    UINT64 Sequence;
    UINT64 Argument0;
    UINT64 Argument1;
    char Text[LOS_SHELL_SERVICE_COMMAND_BUFFER_LENGTH];
} LOS_SHELL_SERVICE_REQUEST;

typedef struct
{
    UINT32 Version;
    UINT32 Status;
    UINT32 Flags;
    UINT32 Reserved0;
    UINT64 Signature;
    UINT64 RequestId;
    UINT64 Sequence;
    UINT64 Result;
    UINT64 PromptIndex;
    char Output[LOS_SHELL_SERVICE_OUTPUT_BUFFER_LENGTH];
} LOS_SHELL_SERVICE_RESPONSE;

typedef struct
{
    UINT32 Version;
    UINT32 State;
    UINT32 Flags;
    UINT32 Reserved0;
    UINT64 Signature;
    UINT64 Sequence;
    UINT64 EndpointId;
    UINT64 MessageBytes;
    union
    {
        LOS_SHELL_SERVICE_REQUEST Request;
        LOS_SHELL_SERVICE_RESPONSE Response;
    } Payload;
} LOS_SHELL_SERVICE_MAILBOX;

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT32 Attached;
    UINT32 Reserved0;
    UINT64 Signature;
    UINT64 ConnectionNonce;
    UINT64 RequestEndpointId;
    UINT64 ResponseEndpointId;
    UINT64 EventEndpointId;
    UINT64 RequestMailboxAddress;
    UINT64 RequestMailboxSize;
    UINT64 ResponseMailboxAddress;
    UINT64 ResponseMailboxSize;
    UINT64 RequestsConsumed;
    UINT64 ResponsesCommitted;
} LOS_SHELL_SERVICE_CHANNEL;

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT32 TransportState;
    UINT32 Reserved0;
    UINT64 Signature;
    UINT64 Generation;
    UINT64 ConnectionNonce;
    UINT64 RequestEndpointId;
    UINT64 ResponseEndpointId;
    UINT64 EventEndpointId;
    UINT64 RequestMailboxAddress;
    UINT64 RequestMailboxSize;
    UINT64 ResponseMailboxAddress;
    UINT64 ResponseMailboxSize;
    char ServiceName[32];
} LOS_SHELL_SERVICE_DISCOVERY_RECORD;

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT32 Reserved0;
    UINT32 Reserved1;
    UINT64 Signature;
    UINT64 RequestedGeneration;
    UINT64 ExpectedVersion;
    UINT64 RequiredFlags;
    char ServiceName[32];
} LOS_SHELL_SERVICE_BIND_REQUEST;

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT32 TransportState;
    UINT32 Reserved0;
    UINT64 Signature;
    UINT64 Generation;
    UINT64 ConnectionNonce;
    UINT64 RequestEndpointId;
    UINT64 ResponseEndpointId;
    UINT64 EventEndpointId;
    UINT64 RequestMailboxAddress;
    UINT64 RequestMailboxSize;
    UINT64 ResponseMailboxAddress;
    UINT64 ResponseMailboxSize;
    char ServiceName[32];
} LOS_SHELL_SERVICE_TRANSPORT_EXPORT;

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT32 TransportState;
    UINT32 Reserved0;
    UINT64 Signature;
    UINT64 Generation;
    UINT64 ConnectionNonce;
    UINT64 NextRequestSequence;
    UINT64 RequestEndpointId;
    UINT64 ResponseEndpointId;
    UINT64 EventEndpointId;
    LOS_SHELL_SERVICE_MAILBOX *RequestMailbox;
    LOS_SHELL_SERVICE_MAILBOX *ResponseMailbox;
} LOS_SHELL_SERVICE_TRANSPORT_BINDING;

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT32 RegistryState;
    UINT32 Reserved0;
    UINT64 Signature;
    UINT64 ServiceId;
    UINT64 Generation;
    UINT64 ExpectedVersion;
    char ServiceName[32];
} LOS_SHELL_SERVICE_REGISTRY_ENTRY;

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT32 Reserved0;
    UINT32 Reserved1;
    UINT64 Signature;
    UINT64 ExpectedVersion;
    UINT64 RequiredFlags;
    char ServiceName[32];
} LOS_SHELL_SERVICE_RESOLVE_REQUEST;

typedef struct
{
    UINT32 Version;
    UINT32 Status;
    UINT32 Flags;
    UINT32 RegistryState;
    UINT64 Signature;
    UINT64 ServiceId;
    UINT64 Generation;
    UINT64 ConnectionNonce;
    char ServiceName[32];
} LOS_SHELL_SERVICE_RESOLVE_RESPONSE;

#endif
