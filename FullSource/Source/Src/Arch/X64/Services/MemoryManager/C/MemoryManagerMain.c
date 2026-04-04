#include "MemoryManagerMain.h"
#include "MemoryManagerAddressSpace.h"

volatile UINT64 LosMemoryManagerServiceHeartbeat = 0ULL;
const char LosMemoryManagerServiceBanner[] = "Liberation Memory Manager Service";

static LOS_MEMORY_MANAGER_SERVICE_STATE LosMemoryManagerServiceGlobalState;

#define LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE 0x3F8U

#define LOS_MEMORY_MANAGER_ATTACH_STAGE_NONE 0ULL
#define LOS_MEMORY_MANAGER_ATTACH_STAGE_LAUNCH_BLOCK 1ULL
#define LOS_MEMORY_MANAGER_ATTACH_STAGE_DIRECT_MAP_OFFSET 2ULL
#define LOS_MEMORY_MANAGER_ATTACH_STAGE_TASK_OBJECT_TRANSLATION 3ULL
#define LOS_MEMORY_MANAGER_ATTACH_STAGE_RECEIVE_ENDPOINT 4ULL
#define LOS_MEMORY_MANAGER_ATTACH_STAGE_REPLY_ENDPOINT 5ULL
#define LOS_MEMORY_MANAGER_ATTACH_STAGE_EVENT_ENDPOINT 6ULL
#define LOS_MEMORY_MANAGER_ATTACH_STAGE_ADDRESS_SPACE_OBJECT 7ULL
#define LOS_MEMORY_MANAGER_ATTACH_STAGE_TASK_OBJECT 8ULL
#define LOS_MEMORY_MANAGER_ATTACH_STAGE_MEMORY_VIEW 9ULL

#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE 0ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_NULL 1ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_SIGNATURE 2ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_VERSION 3ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_ENDPOINT_ID 4ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_ROLE 5ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_MAILBOX_PHYSICAL 6ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_MAILBOX_FLAG 7ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_STATE 8ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_SERVICE_IMAGE_PHYSICAL 9ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_ROOT_TABLE_PHYSICAL 10ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_KERNEL_ROOT_PHYSICAL 11ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_ADDRESS_SPACE_OBJECT_PHYSICAL 12ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_ENTRY_VIRTUAL 13ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_STACK_TOP_PHYSICAL 14ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_STACK_TOP_VIRTUAL 15ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_LAUNCH_BLOCK_PHYSICAL 16ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_REQUEST_MAILBOX_PHYSICAL 17ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_RESPONSE_MAILBOX_PHYSICAL 18ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_EVENT_MAILBOX_PHYSICAL 19ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_RECEIVE_ENDPOINT_PHYSICAL 20ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_REPLY_ENDPOINT_PHYSICAL 21ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_EVENT_ENDPOINT_PHYSICAL 22ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_ADDRESS_SPACE_OBJECT_PHYSICAL_POINTER 23ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_TASK_OBJECT_PHYSICAL_POINTER 24ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_SERVICE_ROOT_PHYSICAL 25ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_MEMORY_REGION_TABLE_PHYSICAL 26ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_MEMORY_REGION_COUNT 27ULL
#define LOS_MEMORY_MANAGER_ATTACH_DETAIL_MEMORY_REGION_ENTRY_SIZE 28ULL

static void RecordAttachDiagnostic(LOS_MEMORY_MANAGER_TASK_OBJECT *TaskObject, UINT64 Stage, UINT64 Detail)
{
    if (TaskObject == 0)
    {
        return;
    }

    TaskObject->LastRequestId = Stage;
    TaskObject->Heartbeat = Detail;
}

static inline void ServiceOut8(UINT16 Port, UINT8 Value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static inline UINT8 ServiceIn8(UINT16 Port)
{
    UINT8 Value;

    __asm__ __volatile__("inb %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

static void ServiceSerialInit(void)
{
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 1U, 0x00U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 3U, 0x80U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 0U, 0x03U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 1U, 0x00U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 3U, 0x03U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 2U, 0xC7U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 4U, 0x0BU);
}

static void ServiceSerialWriteChar(char Character)
{
    while ((ServiceIn8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 5U) & 0x20U) == 0U)
    {
    }

    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 0U, (UINT8)Character);
}

static void ServiceSerialWriteText(const char *Text)
{
    UINTN Index;

    if (Text == 0)
    {
        return;
    }

    for (Index = 0U; Text[Index] != '\0'; ++Index)
    {
        if (Text[Index] == '\n')
        {
            ServiceSerialWriteChar('\r');
        }
        ServiceSerialWriteChar(Text[Index]);
    }
}

static void ServiceSerialWriteHex64(UINT64 Value)
{
    UINTN Shift;

    ServiceSerialWriteText("0x");
    for (Shift = 16U; Shift > 0U; --Shift)
    {
        UINT8 Nibble;

        Nibble = (UINT8)((Value >> ((Shift - 1U) * 4U)) & 0xFULL);
        ServiceSerialWriteChar((char)(Nibble < 10U ? ('0' + Nibble) : ('A' + (Nibble - 10U))));
    }
}

static void ServiceSerialWriteLine(const char *Text)
{
    ServiceSerialWriteText(Text);
    ServiceSerialWriteText("\n");
}

static void ServiceSerialWriteUnsigned(UINT64 Value)
{
    char Buffer[32];
    UINTN Index;

    if (Value == 0ULL)
    {
        ServiceSerialWriteChar('0');
        return;
    }

    Index = 0U;
    while (Value != 0ULL && Index < (sizeof(Buffer) / sizeof(Buffer[0])))
    {
        Buffer[Index] = (char)('0' + (Value % 10ULL));
        Value /= 10ULL;
        ++Index;
    }

    while (Index > 0U)
    {
        --Index;
        ServiceSerialWriteChar(Buffer[Index]);
    }
}

static void ServiceSerialWriteNamedHex(const char *Name, UINT64 Value)
{
    ServiceSerialWriteText("[MemManager] ");
    ServiceSerialWriteText(Name);
    ServiceSerialWriteText(": ");
    ServiceSerialWriteHex64(Value);
    ServiceSerialWriteText("\n");
}

static void ServiceSerialWriteNamedUnsigned(const char *Name, UINT64 Value)
{
    ServiceSerialWriteText("[MemManager] ");
    ServiceSerialWriteText(Name);
    ServiceSerialWriteText(": ");
    ServiceSerialWriteUnsigned(Value);
    ServiceSerialWriteText("\n");
}

static const char *BootstrapAttachResultName(UINT32 BootstrapResult)
{
    switch (BootstrapResult)
    {
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY:
            return "ready";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ALREADY_ATTACHED:
            return "already-attached";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_INVALID_REQUEST:
            return "invalid-request";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_VERSION_MISMATCH:
            return "version-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_LAUNCH_BLOCK_MISMATCH:
            return "launch-block-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ENDPOINT_MISMATCH:
            return "endpoint-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_MAILBOX_MISMATCH:
            return "mailbox-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_ROOT_MISMATCH:
            return "service-root-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_IMAGE_MISMATCH:
            return "service-image-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_STATE_INVALID:
            return "service-state-invalid";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_OPERATION_SET_INVALID:
            return "operation-set-invalid";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ENTRY_MISMATCH:
            return "entry-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_STACK_TOP_MISMATCH:
            return "stack-top-mismatch";
        default:
            return "unknown";
    }
}

static void ZeroMemory(void *Buffer, UINTN ByteCount)
{
    UINT8 *Bytes;
    UINTN Index;

    if (Buffer == 0)
    {
        return;
    }

    Bytes = (UINT8 *)Buffer;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        Bytes[Index] = 0U;
    }
}

static void CopyBytes(void *Destination, const void *Source, UINTN ByteCount)
{
    UINT8 *DestinationBytes;
    const UINT8 *SourceBytes;
    UINTN Index;

    if (Destination == 0 || Source == 0)
    {
        return;
    }

    DestinationBytes = (UINT8 *)Destination;
    SourceBytes = (const UINT8 *)Source;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        DestinationBytes[Index] = SourceBytes[Index];
    }
}

static UINT64 ResolveDirectMapOffset(const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock, UINT64 LaunchBlockAddress)
{
    if (LaunchBlock == 0 || LaunchBlock->LaunchBlockPhysicalAddress == 0ULL || LaunchBlockAddress < LaunchBlock->LaunchBlockPhysicalAddress)
    {
        return 0ULL;
    }

    return LaunchBlockAddress - LaunchBlock->LaunchBlockPhysicalAddress;
}

static void *TranslateBootstrapAddress(UINT64 DirectMapOffset, UINT64 PhysicalAddress)
{
    if (PhysicalAddress == 0ULL)
    {
        return 0;
    }

    return (void *)(UINTN)(DirectMapOffset + PhysicalAddress);
}


static void RecordEntryBreadcrumbFromLaunchBlock(UINT64 LaunchBlockAddress, UINT64 Stage, UINT64 Value)
{
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    UINT64 DirectMapOffset;
    LOS_MEMORY_MANAGER_TASK_OBJECT *TaskObject;

    if (LaunchBlockAddress == 0ULL)
    {
        return;
    }

    LaunchBlock = (const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *)(UINTN)LaunchBlockAddress;
    if (LaunchBlock == 0)
    {
        return;
    }

    DirectMapOffset = ResolveDirectMapOffset(LaunchBlock, LaunchBlockAddress);
    if (DirectMapOffset == 0ULL)
    {
        return;
    }

    TaskObject = (LOS_MEMORY_MANAGER_TASK_OBJECT *)TranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceTaskObjectPhysicalAddress);
    if (TaskObject == 0)
    {
        return;
    }

    TaskObject->LastRequestId = Stage;
    TaskObject->Heartbeat = Value;
}

static BOOLEAN ValidateEndpointObjectDetailed(
    const LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *Endpoint,
    UINT64 EndpointId,
    UINT32 Role,
    UINT64 MailboxPhysicalAddress,
    UINT64 *Detail)
{
    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE;
    }

    if (Endpoint == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NULL;
        }
        return 0;
    }

    if (Endpoint->Signature != LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SIGNATURE;
        }
        return 0;
    }

    if (Endpoint->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_VERSION;
        }
        return 0;
    }

    if (Endpoint->EndpointId != EndpointId)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ENDPOINT_ID;
        }
        return 0;
    }

    if (Endpoint->Role != Role)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ROLE;
        }
        return 0;
    }

    if (Endpoint->MailboxPhysicalAddress != MailboxPhysicalAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_MAILBOX_PHYSICAL;
        }
        return 0;
    }

    if ((Endpoint->Flags & LOS_MEMORY_MANAGER_ENDPOINT_FLAG_MAILBOX_ATTACHED) == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_MAILBOX_FLAG;
        }
        return 0;
    }

    return 1;
}

static BOOLEAN ValidateAddressSpaceObjectDetailed(const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject, UINT64 ServiceImagePhysicalAddress, UINT64 ServiceRootTablePhysicalAddress, UINT64 *Detail)
{
    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE;
    }

    if (AddressSpaceObject == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NULL;
        }
        return 0;
    }

    if (AddressSpaceObject->Signature != LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SIGNATURE;
        }
        return 0;
    }

    if (AddressSpaceObject->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_VERSION;
        }
        return 0;
    }

    if (AddressSpaceObject->State < LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_READY)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_STATE;
        }
        return 0;
    }

    if (AddressSpaceObject->ServiceImagePhysicalAddress != ServiceImagePhysicalAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SERVICE_IMAGE_PHYSICAL;
        }
        return 0;
    }

    if (AddressSpaceObject->RootTablePhysicalAddress != ServiceRootTablePhysicalAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ROOT_TABLE_PHYSICAL;
        }
        return 0;
    }

    if (AddressSpaceObject->KernelRootTablePhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_KERNEL_ROOT_PHYSICAL;
        }
        return 0;
    }

    return 1;
}

static BOOLEAN ValidateTaskObjectDetailed(const LOS_MEMORY_MANAGER_TASK_OBJECT *TaskObject, UINT64 AddressSpaceObjectPhysicalAddress, UINT64 EntryVirtualAddress, UINT64 StackTopPhysicalAddress, UINT64 StackTopVirtualAddress, UINT64 *Detail)
{
    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE;
    }

    if (TaskObject == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NULL;
        }
        return 0;
    }

    if (TaskObject->Signature != LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SIGNATURE;
        }
        return 0;
    }

    if (TaskObject->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_VERSION;
        }
        return 0;
    }

    if (TaskObject->AddressSpaceObjectPhysicalAddress != AddressSpaceObjectPhysicalAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ADDRESS_SPACE_OBJECT_PHYSICAL;
        }
        return 0;
    }

    if (TaskObject->EntryVirtualAddress != EntryVirtualAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ENTRY_VIRTUAL;
        }
        return 0;
    }

    if (TaskObject->StackTopPhysicalAddress != StackTopPhysicalAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_STACK_TOP_PHYSICAL;
        }
        return 0;
    }

    if (TaskObject->StackTopVirtualAddress != StackTopVirtualAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_STACK_TOP_VIRTUAL;
        }
        return 0;
    }

    return 1;
}

static LOS_MEMORY_MANAGER_TASK_OBJECT *TryTranslateTaskObjectForDiagnostics(
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock,
    UINT64 LaunchBlockAddress)
{
    UINT64 DirectMapOffset;

    if (LaunchBlock == 0)
    {
        return 0;
    }

    if (LaunchBlock->ServiceTaskObjectPhysicalAddress == 0ULL)
    {
        return 0;
    }

    if (LaunchBlock->LaunchBlockPhysicalAddress == 0ULL || LaunchBlockAddress < LaunchBlock->LaunchBlockPhysicalAddress)
    {
        return 0;
    }

    DirectMapOffset = LaunchBlockAddress - LaunchBlock->LaunchBlockPhysicalAddress;
    if (DirectMapOffset == 0ULL)
    {
        return 0;
    }

    return (LOS_MEMORY_MANAGER_TASK_OBJECT *)TranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceTaskObjectPhysicalAddress);
}

static BOOLEAN ValidateLaunchBlockDetailed(const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock, UINT64 *Detail)
{
    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE;
    }

    if (LaunchBlock == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NULL;
        }
        return 0;
    }

    if (LaunchBlock->Signature != LOS_MEMORY_MANAGER_LAUNCH_BLOCK_SIGNATURE)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SIGNATURE;
        }
        return 0;
    }

    if (LaunchBlock->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_VERSION;
        }
        return 0;
    }

    if (LaunchBlock->RequestMailboxPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_REQUEST_MAILBOX_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->ResponseMailboxPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_RESPONSE_MAILBOX_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->EventMailboxPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_EVENT_MAILBOX_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->LaunchBlockPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_LAUNCH_BLOCK_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->KernelToServiceEndpointObjectPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_RECEIVE_ENDPOINT_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->ServiceToKernelEndpointObjectPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_REPLY_ENDPOINT_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->ServiceEventsEndpointObjectPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_EVENT_ENDPOINT_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ADDRESS_SPACE_OBJECT_PHYSICAL_POINTER;
        }
        return 0;
    }

    if (LaunchBlock->ServiceTaskObjectPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_TASK_OBJECT_PHYSICAL_POINTER;
        }
        return 0;
    }

    if (LaunchBlock->ServicePageMapLevel4PhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SERVICE_ROOT_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->MemoryRegionTablePhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_MEMORY_REGION_TABLE_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->MemoryRegionCount == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_MEMORY_REGION_COUNT;
        }
        return 0;
    }

    if (LaunchBlock->MemoryRegionEntrySize != (UINT64)sizeof(LOS_X64_MEMORY_REGION))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_MEMORY_REGION_ENTRY_SIZE;
        }
        return 0;
    }

    if (LaunchBlock->ServiceEntryVirtualAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ENTRY_VIRTUAL;
        }
        return 0;
    }

    if (LaunchBlock->ServiceStackTopVirtualAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_STACK_TOP_VIRTUAL;
        }
        return 0;
    }

    return 1;
}

LOS_MEMORY_MANAGER_SERVICE_STATE *LosMemoryManagerServiceState(void)
{
    return &LosMemoryManagerServiceGlobalState;
}

BOOLEAN LosMemoryManagerServiceAttach(const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock)
{
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;
    LOS_MEMORY_MANAGER_TASK_OBJECT *DiagnosticTaskObject;
    UINT64 LaunchBlockAddress;
    UINT64 DirectMapOffset;
    UINT64 Detail;

    LaunchBlockAddress = (UINT64)(UINTN)LaunchBlock;
    Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE;
    if (!ValidateLaunchBlockDetailed(LaunchBlock, &Detail))
    {
        DiagnosticTaskObject = TryTranslateTaskObjectForDiagnostics(LaunchBlock, LaunchBlockAddress);
        RecordAttachDiagnostic(DiagnosticTaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_LAUNCH_BLOCK, Detail);
        return 0;
    }

    DirectMapOffset = ResolveDirectMapOffset(LaunchBlock, LaunchBlockAddress);
    if (DirectMapOffset == 0ULL)
    {
        DiagnosticTaskObject = TryTranslateTaskObjectForDiagnostics(LaunchBlock, LaunchBlockAddress);
        RecordAttachDiagnostic(DiagnosticTaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_DIRECT_MAP_OFFSET, LOS_MEMORY_MANAGER_ATTACH_DETAIL_LAUNCH_BLOCK_PHYSICAL);
        return 0;
    }

    State = LosMemoryManagerServiceState();
    ZeroMemory(State, sizeof(*State));
    State->LaunchBlock = (LOS_MEMORY_MANAGER_LAUNCH_BLOCK *)(UINTN)LaunchBlockAddress;
    State->DirectMapOffset = DirectMapOffset;
    State->ReceiveEndpoint = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)TranslateBootstrapAddress(DirectMapOffset, LaunchBlock->KernelToServiceEndpointObjectPhysicalAddress);
    State->ReplyEndpoint = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)TranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceToKernelEndpointObjectPhysicalAddress);
    State->EventEndpoint = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)TranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceEventsEndpointObjectPhysicalAddress);
    State->RequestMailbox = (LOS_MEMORY_MANAGER_REQUEST_MAILBOX *)TranslateBootstrapAddress(DirectMapOffset, LaunchBlock->RequestMailboxPhysicalAddress);
    State->ResponseMailbox = (LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *)TranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ResponseMailboxPhysicalAddress);
    State->EventMailbox = (LOS_MEMORY_MANAGER_EVENT_MAILBOX *)TranslateBootstrapAddress(DirectMapOffset, LaunchBlock->EventMailboxPhysicalAddress);
    State->AddressSpaceObject = (LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *)TranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress);
    State->TaskObject = (LOS_MEMORY_MANAGER_TASK_OBJECT *)TranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceTaskObjectPhysicalAddress);
    if (State->TaskObject == 0)
    {
        return 0;
    }

    RecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_NONE, LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE);

    if (!ValidateEndpointObjectDetailed(State->ReceiveEndpoint, LaunchBlock->Endpoints.KernelToService, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_RECEIVE, LaunchBlock->RequestMailboxPhysicalAddress, &Detail))
    {
        RecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_RECEIVE_ENDPOINT, Detail);
        return 0;
    }

    if (!ValidateEndpointObjectDetailed(State->ReplyEndpoint, LaunchBlock->Endpoints.ServiceToKernel, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_REPLY, LaunchBlock->ResponseMailboxPhysicalAddress, &Detail))
    {
        RecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_REPLY_ENDPOINT, Detail);
        return 0;
    }

    if (!ValidateEndpointObjectDetailed(State->EventEndpoint, LaunchBlock->Endpoints.ServiceEvents, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_EVENT, LaunchBlock->EventMailboxPhysicalAddress, &Detail))
    {
        RecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_EVENT_ENDPOINT, Detail);
        return 0;
    }

    if (!ValidateAddressSpaceObjectDetailed(State->AddressSpaceObject, LaunchBlock->ServiceImagePhysicalAddress, LaunchBlock->ServicePageMapLevel4PhysicalAddress, &Detail))
    {
        RecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_ADDRESS_SPACE_OBJECT, Detail);
        return 0;
    }

    if (!ValidateTaskObjectDetailed(State->TaskObject, LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress, LaunchBlock->ServiceEntryVirtualAddress, LaunchBlock->ServiceStackTopPhysicalAddress, LaunchBlock->ServiceStackTopVirtualAddress, &Detail))
    {
        RecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_TASK_OBJECT, Detail);
        return 0;
    }
    if (!LosMemoryManagerServiceBuildMemoryView(State, &Detail))
    {
        RecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_MEMORY_VIEW, Detail);
        return 0;
    }
    State->ActiveRootTablePhysicalAddress = LaunchBlock->ServicePageMapLevel4PhysicalAddress;
    State->KernelRootTablePhysicalAddress = State->AddressSpaceObject->KernelRootTablePhysicalAddress;
    State->ReceiveEndpoint->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->ReplyEndpoint->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->EventEndpoint->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->AddressSpaceObject->State = LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_ACTIVE;
    State->TaskObject->State = LOS_MEMORY_MANAGER_TASK_STATE_ONLINE;
    if (State->AddressSpaceObject->AddressSpaceId == 0ULL)
    {
        State->AddressSpaceObject->AddressSpaceId = 1ULL;
    }
    State->NextAddressSpaceId = State->AddressSpaceObject->AddressSpaceId + 1ULL;
    State->NegotiatedOperations = 0ULL;
    State->AttachComplete = 0U;
    State->BootstrapResultCode = LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_INVALID_REQUEST;
    State->Online = 1U;
    return 1;
}



static void PostEvent(UINT32 EventType, UINT32 Status, UINT64 Value0, UINT64 Value1);

static void InitializeServiceResponse(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    if (Response == 0)
    {
        return;
    }

    ZeroMemory(Response, sizeof(*Response));
    if (Request != 0)
    {
        Response->Operation = Request->Operation;
        Response->RequestId = Request->RequestId;
    }
}

static BOOLEAN ServiceMayHandleOperation(const LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT32 Operation)
{
    if (State == 0)
    {
        return 0;
    }

    if (Operation == LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)
    {
        return 1;
    }

    if (State->AttachComplete == 0U)
    {
        return 0;
    }

    if ((State->NegotiatedOperations & (1ULL << Operation)) == 0ULL)
    {
        return 0;
    }

    switch (Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE:
        case LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE:
        case LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE:
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK:
            return 1;
        default:
            return 0;
    }
}

static BOOLEAN CompleteRequest(LOS_MEMORY_MANAGER_SERVICE_STATE *State, const LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    LOS_MEMORY_MANAGER_REQUEST_SLOT *RequestSlot;
    LOS_MEMORY_MANAGER_RESPONSE_SLOT *ResponseSlot;
    UINT64 RequestIndex;
    UINT64 ResponseIndex;

    if (State == 0 || State->RequestMailbox == 0 || State->ResponseMailbox == 0 || Response == 0)
    {
        return 0;
    }

    RequestIndex = State->RequestMailbox->Header.ConsumeIndex % State->RequestMailbox->Header.SlotCount;
    RequestSlot = &State->RequestMailbox->Slots[RequestIndex];
    if (RequestSlot->SlotState != LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    ResponseIndex = State->ResponseMailbox->Header.ProduceIndex % State->ResponseMailbox->Header.SlotCount;
    ResponseSlot = &State->ResponseMailbox->Slots[ResponseIndex];
    if (ResponseSlot->SlotState == LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    CopyBytes(&ResponseSlot->Message, Response, sizeof(*Response));
    ResponseSlot->Sequence = Response->RequestId;
    ResponseSlot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY;
    State->ResponseMailbox->Header.ProduceIndex += 1ULL;

    RequestSlot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_COMPLETE;
    RequestSlot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
    RequestSlot->Sequence = 0ULL;
    ZeroMemory(&RequestSlot->Message, sizeof(RequestSlot->Message));
    State->RequestMailbox->Header.ConsumeIndex += 1ULL;
    return 1;
}

static UINT32 BootstrapAttachStatusFromResult(UINT32 BootstrapResult)
{
    switch (BootstrapResult)
    {
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY:
            return LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ALREADY_ATTACHED:
            return LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_STATE_INVALID:
            return LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        default:
            return LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    }
}

static UINT32 ValidateBootstrapAttachRequest(
    const LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_REQUEST *Request)
{
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    UINT64 RequestedServiceRootPhysicalAddress;

    if (State == 0 || Request == 0 || State->LaunchBlock == 0)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_STATE_INVALID;
    }

    LaunchBlock = State->LaunchBlock;
    RequestedServiceRootPhysicalAddress = Request->ServicePageMapLevel4PhysicalAddress;
    if (RequestedServiceRootPhysicalAddress == 0ULL)
    {
        RequestedServiceRootPhysicalAddress = LaunchBlock->ServicePageMapLevel4PhysicalAddress;
    }
    if (RequestedServiceRootPhysicalAddress == 0ULL)
    {
        RequestedServiceRootPhysicalAddress = State->ActiveRootTablePhysicalAddress;
    }


    if (State->Online == 0U)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_STATE_INVALID;
    }
    if (State->AttachComplete != 0U)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ALREADY_ATTACHED;
    }
    if (Request->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_VERSION_MISMATCH;
    }
    if ((Request->OfferedOperations & (1ULL << LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)) == 0ULL)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_OPERATION_SET_INVALID;
    }
    if (Request->LaunchBlockPhysicalAddress != LaunchBlock->LaunchBlockPhysicalAddress)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_LAUNCH_BLOCK_MISMATCH;
    }
    if (Request->KernelToServiceEndpointId != LaunchBlock->Endpoints.KernelToService ||
        Request->ServiceToKernelEndpointId != LaunchBlock->Endpoints.ServiceToKernel ||
        Request->ServiceEventsEndpointId != LaunchBlock->Endpoints.ServiceEvents)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ENDPOINT_MISMATCH;
    }
    if (Request->RequestMailboxPhysicalAddress != LaunchBlock->RequestMailboxPhysicalAddress ||
        Request->ResponseMailboxPhysicalAddress != LaunchBlock->ResponseMailboxPhysicalAddress ||
        Request->EventMailboxPhysicalAddress != LaunchBlock->EventMailboxPhysicalAddress)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_MAILBOX_MISMATCH;
    }
    if (RequestedServiceRootPhysicalAddress != State->ActiveRootTablePhysicalAddress)
    {
        return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_ROOT_MISMATCH;
    }

    /*
     * The launch block and attached runtime objects are authoritative for the
     * staged image, entry point, and stack geometry. Those were already
     * validated before the service announced that attach was complete.
     * BootstrapAttach therefore validates transport identity and the active
     * service root, then accepts the attach and transitions to the normal
     * request path.
     */
    return LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY;
}

static void PopulateBootstrapAttachResponse(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request,
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    UINT32 BootstrapResult;
    UINT64 BootstrapFlags;

    InitializeServiceResponse(Request, Response);
    if (State == 0 || Request == 0 || Response == 0)
    {
        return;
    }

    BootstrapResult = ValidateBootstrapAttachRequest(State, &Request->Payload.BootstrapAttach);
    ServiceSerialWriteText("[MemManager] Bootstrap attach validation result=");
    ServiceSerialWriteUnsigned(BootstrapResult);
    ServiceSerialWriteText(" name=");
    ServiceSerialWriteText(BootstrapAttachResultName(BootstrapResult));
    ServiceSerialWriteText(" request-root=");
    ServiceSerialWriteHex64(Request->Payload.BootstrapAttach.ServicePageMapLevel4PhysicalAddress);
    ServiceSerialWriteText(" active-root=");
    ServiceSerialWriteHex64(State->ActiveRootTablePhysicalAddress);
    ServiceSerialWriteText(" request-image=");
    ServiceSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress);
    ServiceSerialWriteText(" active-image=");
    ServiceSerialWriteHex64(State->LaunchBlock->ServiceImagePhysicalAddress);
    ServiceSerialWriteText(" request-entry=");
    ServiceSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceEntryVirtualAddress);
    ServiceSerialWriteText(" active-entry=");
    ServiceSerialWriteHex64(State->LaunchBlock->ServiceEntryVirtualAddress);
    ServiceSerialWriteText("\n");
    BootstrapFlags = 0ULL;
    if (State->LaunchBlock != 0)
    {
        BootstrapFlags = State->LaunchBlock->Flags;
    }

    if (BootstrapResult == LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY)
    {
        State->AttachComplete = 1U;
        State->BootstrapResultCode = BootstrapResult;
        State->NegotiatedOperations = Request->Payload.BootstrapAttach.OfferedOperations;
        if (State->TaskObject != 0)
        {
            State->TaskObject->Flags |= LOS_MEMORY_MANAGER_TASK_FLAG_SERVICE_READY;
        }
        BootstrapFlags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_ATTACH_COMPLETE;
        if (State->LaunchBlock != 0)
        {
            State->LaunchBlock->Flags = BootstrapFlags;
        }
        PostEvent(
            LOS_MEMORY_MANAGER_EVENT_SERVICE_READY_FOR_REQUESTS,
            BootstrapResult,
            Request->RequestId,
            State->NegotiatedOperations);
    }
    else
    {
        State->BootstrapResultCode = BootstrapResult;
    }

    Response->Status = BootstrapAttachStatusFromResult(BootstrapResult);
    Response->Payload.BootstrapAttach.BootstrapResult = BootstrapResult;
    Response->Payload.BootstrapAttach.BootstrapState =
        (BootstrapResult == LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY)
            ? LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_READY
            : LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_SERVICE_ONLINE;
    Response->Payload.BootstrapAttach.NegotiatedOperations =
        (BootstrapResult == LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY)
            ? State->NegotiatedOperations
            : 0ULL;
    Response->Payload.BootstrapAttach.BootstrapFlags = BootstrapFlags;
    Response->Payload.BootstrapAttach.ActiveRootTablePhysicalAddress = State->ActiveRootTablePhysicalAddress;
    Response->Payload.BootstrapAttach.KernelRootTablePhysicalAddress = State->KernelRootTablePhysicalAddress;
    Response->Payload.BootstrapAttach.ServiceHeartbeat = State->Heartbeat;
    Response->Payload.BootstrapAttach.LastProcessedRequestId = Request->RequestId;
    Response->Payload.BootstrapAttach.TotalUsableBytes = State->MemoryView.TotalUsableBytes;
    Response->Payload.BootstrapAttach.TotalBootstrapReservedBytes = State->MemoryView.TotalBootstrapReservedBytes;
    Response->Payload.BootstrapAttach.TotalFirmwareReservedBytes = State->MemoryView.TotalFirmwareReservedBytes;
    Response->Payload.BootstrapAttach.TotalRuntimeBytes = State->MemoryView.TotalRuntimeBytes;
    Response->Payload.BootstrapAttach.TotalMmioBytes = State->MemoryView.TotalMmioBytes;
    Response->Payload.BootstrapAttach.TotalAcpiBytes = State->MemoryView.TotalAcpiBytes;
    Response->Payload.BootstrapAttach.TotalUnusableBytes = State->MemoryView.TotalUnusableBytes;
    Response->Payload.BootstrapAttach.TotalPages = State->MemoryView.TotalPages;
    Response->Payload.BootstrapAttach.FreePages = State->MemoryView.FreePages;
    Response->Payload.BootstrapAttach.ReservedPages = State->MemoryView.ReservedPages;
    Response->Payload.BootstrapAttach.RuntimePages = State->MemoryView.RuntimePages;
    Response->Payload.BootstrapAttach.MmioPages = State->MemoryView.MmioPages;
    Response->Payload.BootstrapAttach.InternalDescriptorCount = (UINT64)State->MemoryView.InternalDescriptorCount;
    Response->Payload.BootstrapAttach.PageFrameDatabaseEntryCount = (UINT64)State->MemoryView.PageFrameDatabaseEntryCount;
}

static void PostEvent(UINT32 EventType, UINT32 Status, UINT64 Value0, UINT64 Value1)
{
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;
    LOS_MEMORY_MANAGER_EVENT_MAILBOX *Mailbox;
    UINT64 Index;
    LOS_MEMORY_MANAGER_EVENT_SLOT *Slot;

    State = LosMemoryManagerServiceState();
    Mailbox = State->EventMailbox;
    if (Mailbox == 0 || State->EventEndpoint == 0 || State->EventEndpoint->State < LOS_MEMORY_MANAGER_ENDPOINT_STATE_BOUND)
    {
        return;
    }

    Index = Mailbox->Header.ProduceIndex % Mailbox->Header.SlotCount;
    Slot = &Mailbox->Slots[Index];
    if (Slot->SlotState == LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return;
    }

    Slot->Message.EventType = EventType;
    Slot->Message.Status = Status;
    Slot->Message.Sequence = State->Heartbeat;
    Slot->Message.Value0 = Value0;
    Slot->Message.Value1 = Value1;
    Slot->Sequence = State->Heartbeat;
    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY;
    Mailbox->Header.ProduceIndex += 1ULL;
}

void LosMemoryManagerServicePoll(void)
{
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;
    LOS_MEMORY_MANAGER_REQUEST_SLOT *Slot;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;
    UINT64 Index;

    State = LosMemoryManagerServiceState();
    if (State->Online == 0U || State->RequestMailbox == 0 || State->ReceiveEndpoint == 0)
    {
        return;
    }

    Index = State->RequestMailbox->Header.ConsumeIndex % State->RequestMailbox->Header.SlotCount;
    Slot = &State->RequestMailbox->Slots[Index];
    if (Slot->SlotState != LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return;
    }

    State->LastRequestId = Slot->Message.RequestId;
    if (State->TaskObject != 0)
    {
        State->TaskObject->Heartbeat = State->Heartbeat;
        State->TaskObject->LastRequestId = State->LastRequestId;
    }

    if (!ServiceMayHandleOperation(State, Slot->Message.Operation))
    {
        return;
    }

    InitializeServiceResponse(&Slot->Message, &Response);
    switch (Slot->Message.Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH:
            ServiceSerialWriteText("[MemManager] Processing bootstrap attach request id=");
            ServiceSerialWriteHex64(Slot->Message.RequestId);
            ServiceSerialWriteText("\n");
            PopulateBootstrapAttachResponse(State, &Slot->Message, &Response);
            break;
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
            LosMemoryManagerServiceQueryMemoryRegions(State, &Slot->Message, &Response);
            break;
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
            LosMemoryManagerServiceReserveFrames(State, &Slot->Message.Payload.ReserveFrames, &Response.Payload.ReserveFrames);
            Response.Status = Response.Payload.ReserveFrames.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            LosMemoryManagerServiceClaimFrames(State, &Slot->Message.Payload.ClaimFrames, &Response.Payload.ClaimFrames);
            Response.Status = Response.Payload.ClaimFrames.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES:
            LosMemoryManagerServiceFreeFrames(State, &Slot->Message.Payload.FreeFrames, &Response.Payload.FreeFrames);
            Response.Status = Response.Payload.FreeFrames.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE:
            LosMemoryManagerServiceCreateAddressSpace(State, &Slot->Message.Payload.CreateAddressSpace, &Response.Payload.CreateAddressSpace);
            Response.Status = Response.Payload.CreateAddressSpace.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE:
            LosMemoryManagerServiceDestroyAddressSpace(State, &Slot->Message.Payload.DestroyAddressSpace, &Response.Payload.DestroyAddressSpace);
            Response.Status = Response.Payload.DestroyAddressSpace.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE:
            LosMemoryManagerServiceAttachStagedImage(State, &Slot->Message.Payload.AttachStagedImage, &Response.Payload.AttachStagedImage);
            Response.Status = Response.Payload.AttachStagedImage.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK:
            LosMemoryManagerServiceAllocateAddressSpaceStack(State, &Slot->Message.Payload.AllocateAddressSpaceStack, &Response.Payload.AllocateAddressSpaceStack);
            Response.Status = Response.Payload.AllocateAddressSpaceStack.Status;
            break;
        default:
            return;
    }

    if (CompleteRequest(State, &Response))
    {
        if (Response.Operation == LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)
        {
            ServiceSerialWriteText("[MemManager] Bootstrap attach response posted result=");
            ServiceSerialWriteUnsigned(Response.Payload.BootstrapAttach.BootstrapResult);
            ServiceSerialWriteText(" status=");
            ServiceSerialWriteUnsigned(Response.Status);
            ServiceSerialWriteText("\n");
        }
    }
    else if (Response.Operation == LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)
    {
        ServiceSerialWriteLine("[MemManager] Bootstrap attach response could not be posted.");
    }
}

static void RunServiceStep(void)
{
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;

    State = LosMemoryManagerServiceState();
    LosMemoryManagerServiceHeartbeat += 1ULL;
    State->Heartbeat = LosMemoryManagerServiceHeartbeat;
    if (State->TaskObject != 0)
    {
        if (State->LastRequestId == 0ULL)
        {
            State->TaskObject->LastRequestId = 0x1008ULL;
        }
        else
        {
            State->TaskObject->LastRequestId = State->LastRequestId;
        }
        State->TaskObject->Heartbeat = State->Heartbeat;
    }

    LosMemoryManagerServicePoll();
}

void LosMemoryManagerServiceBootstrapEntry(UINT64 LaunchBlockAddress)
{
    UINT64 RegisterLaunchBlockAddressRdi;
    UINT64 RegisterLaunchBlockAddressRsi;
    UINT64 RegisterLaunchBlockAddressRcx;
    UINT64 RegisterLaunchBlockAddressRdx;
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;

    __asm__ __volatile__("" : "=D"(RegisterLaunchBlockAddressRdi), "=S"(RegisterLaunchBlockAddressRsi), "=c"(RegisterLaunchBlockAddressRcx), "=d"(RegisterLaunchBlockAddressRdx));
    RecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1001ULL, LaunchBlockAddress);
    if (LaunchBlockAddress == 0ULL)
    {
        if (RegisterLaunchBlockAddressRdi != 0ULL)
        {
            LaunchBlockAddress = RegisterLaunchBlockAddressRdi;
        }
        else if (RegisterLaunchBlockAddressRsi != 0ULL)
        {
            LaunchBlockAddress = RegisterLaunchBlockAddressRsi;
        }
        else if (RegisterLaunchBlockAddressRcx != 0ULL)
        {
            LaunchBlockAddress = RegisterLaunchBlockAddressRcx;
        }
        else if (RegisterLaunchBlockAddressRdx != 0ULL)
        {
            LaunchBlockAddress = RegisterLaunchBlockAddressRdx;
        }
    }

    RecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1002ULL, RegisterLaunchBlockAddressRdi);
    LaunchBlock = (const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *)(UINTN)LaunchBlockAddress;
    RecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1003ULL, RegisterLaunchBlockAddressRsi);
    State = LosMemoryManagerServiceState();
    RecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1004ULL, RegisterLaunchBlockAddressRcx);

    ServiceSerialInit();
    if (State->Online == 0U)
    {
        ServiceSerialWriteLine("[MemManager] Memory-manager bootstrap entry reached.");
        ServiceSerialWriteNamedHex("Launch block", LaunchBlockAddress);
        RecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1005ULL, RegisterLaunchBlockAddressRdx);
        if (!LosMemoryManagerServiceAttach(LaunchBlock))
        {
            ServiceSerialWriteLine("[MemManager] Memory-manager attach failed. Halting in service context.");
            RecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x10FFULL, 0xFFFFFFFFFFFFFFFFULL);
            for (;; )
            {
                __asm__ __volatile__("pause" : : : "memory");
            }
        }
        RecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1006ULL, LaunchBlock->ServiceEntryVirtualAddress);
        ServiceSerialWriteNamedHex("Service entry", LaunchBlock->ServiceEntryVirtualAddress);
        ServiceSerialWriteNamedHex("Service stack top virtual", LaunchBlock->ServiceStackTopVirtualAddress);
        ServiceSerialWriteNamedHex("Service root", LaunchBlock->ServicePageMapLevel4PhysicalAddress);
        ServiceSerialWriteNamedHex("Normalized region table", LaunchBlock->MemoryRegionTablePhysicalAddress);
        ServiceSerialWriteNamedUnsigned("Normalized region count", State->MemoryView.NormalizedRegionCount);
        ServiceSerialWriteNamedUnsigned("Memory-view descriptors", (UINT64)State->MemoryView.InternalDescriptorCount);
        ServiceSerialWriteNamedUnsigned("Page-frame DB entries", (UINT64)State->MemoryView.PageFrameDatabaseEntryCount);
        ServiceSerialWriteNamedHex("Total usable bytes", State->MemoryView.TotalUsableBytes);
        ServiceSerialWriteNamedHex("Bootstrap reserved bytes", State->MemoryView.TotalBootstrapReservedBytes);
        ServiceSerialWriteNamedHex("Firmware reserved bytes", State->MemoryView.TotalFirmwareReservedBytes);
        ServiceSerialWriteNamedHex("Runtime bytes", State->MemoryView.TotalRuntimeBytes);
        ServiceSerialWriteNamedHex("MMIO bytes", State->MemoryView.TotalMmioBytes);
        ServiceSerialWriteNamedHex("ACPI/NVS bytes", State->MemoryView.TotalAcpiBytes);
        ServiceSerialWriteNamedHex("Unusable bytes", State->MemoryView.TotalUnusableBytes);
        ServiceSerialWriteNamedHex("Total pages", State->MemoryView.TotalPages);
        ServiceSerialWriteNamedHex("Reserved pages", State->MemoryView.ReservedPages);
        ServiceSerialWriteNamedHex("Runtime pages", State->MemoryView.RuntimePages);
        ServiceSerialWriteNamedHex("MMIO pages", State->MemoryView.MmioPages);
        ServiceSerialWriteNamedHex("Free pages", State->MemoryView.FreePages);
        ServiceSerialWriteLine("[MemManager] Frame allocator ready.");
        ServiceSerialWriteLine("[MemManager] Memory-manager attach complete.");
        PostEvent(LOS_MEMORY_MANAGER_EVENT_SERVICE_ONLINE, 0U, LaunchBlock->ServiceEntryVirtualAddress, LaunchBlock->ServiceStackTopPhysicalAddress);
    }

    RecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1007ULL, LaunchBlock->ServiceStackTopVirtualAddress);
    RunServiceStep();
}

void LosMemoryManagerServiceEntry(void)
{
    for (;; )
    {
        RunServiceStep();
        __asm__ __volatile__("pause" : : : "memory");
    }
}
