#include "MemoryManagerMain.h"

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

static void ServiceSerialWriteNamedHex(const char *Name, UINT64 Value)
{
    ServiceSerialWriteText("[MemManager] ");
    ServiceSerialWriteText(Name);
    ServiceSerialWriteText(": ");
    ServiceSerialWriteHex64(Value);
    ServiceSerialWriteText("\n");
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
    State->LaunchBlock = LaunchBlock;
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
    State->ActiveRootTablePhysicalAddress = LaunchBlock->ServicePageMapLevel4PhysicalAddress;
    State->KernelRootTablePhysicalAddress = State->AddressSpaceObject->KernelRootTablePhysicalAddress;
    State->ReceiveEndpoint->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->ReplyEndpoint->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->EventEndpoint->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->AddressSpaceObject->State = LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_ACTIVE;
    State->TaskObject->State = LOS_MEMORY_MANAGER_TASK_STATE_ONLINE;
    State->TaskObject->Flags |= LOS_MEMORY_MANAGER_TASK_FLAG_SERVICE_READY;
    State->Online = 1U;
    return 1;
}



static void CompleteRequestWithStatus(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT32 Status)
{
    LOS_MEMORY_MANAGER_REQUEST_SLOT *RequestSlot;
    LOS_MEMORY_MANAGER_RESPONSE_SLOT *ResponseSlot;
    UINT64 RequestIndex;
    UINT64 ResponseIndex;

    if (State == 0 || State->RequestMailbox == 0 || State->ResponseMailbox == 0)
    {
        return;
    }

    RequestIndex = State->RequestMailbox->Header.ConsumeIndex % State->RequestMailbox->Header.SlotCount;
    RequestSlot = &State->RequestMailbox->Slots[RequestIndex];
    if (RequestSlot->SlotState != LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return;
    }

    ResponseIndex = State->ResponseMailbox->Header.ProduceIndex % State->ResponseMailbox->Header.SlotCount;
    ResponseSlot = &State->ResponseMailbox->Slots[ResponseIndex];
    if (ResponseSlot->SlotState == LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return;
    }

    ZeroMemory(&ResponseSlot->Message, sizeof(ResponseSlot->Message));
    ResponseSlot->Message.Operation = RequestSlot->Message.Operation;
    ResponseSlot->Message.RequestId = RequestSlot->Message.RequestId;
    ResponseSlot->Message.Status = Status;
    ResponseSlot->Sequence = RequestSlot->Message.RequestId;
    ResponseSlot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY;
    State->ResponseMailbox->Header.ProduceIndex += 1ULL;

    RequestSlot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_COMPLETE;
    RequestSlot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
    RequestSlot->Sequence = 0ULL;
    ZeroMemory(&RequestSlot->Message, sizeof(RequestSlot->Message));
    State->RequestMailbox->Header.ConsumeIndex += 1ULL;
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

    PostEvent(LOS_MEMORY_MANAGER_EVENT_SERVICE_READY_FOR_REQUESTS, 0U, Slot->Message.Operation, Slot->Message.RequestId);
    CompleteRequestWithStatus(State, LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED);
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
    ServiceSerialWriteLine("[MemManager] Memory-manager bootstrap entry reached.");
    ServiceSerialWriteNamedHex("Launch block", LaunchBlockAddress);

    if (State->Online == 0U)
    {
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
        ServiceSerialWriteLine("[MemManager] Memory-manager attach complete.");
        PostEvent(LOS_MEMORY_MANAGER_EVENT_SERVICE_ONLINE, 0U, LaunchBlock->ServiceEntryVirtualAddress, LaunchBlock->ServiceStackTopPhysicalAddress);
    }

    RecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1007ULL, LaunchBlock->ServiceStackTopVirtualAddress);
    ServiceSerialWriteLine("[MemManager] Entering memory-manager service loop.");
    LosMemoryManagerServiceEntry();
}

void LosMemoryManagerServiceEntry(void)
{
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;

    State = LosMemoryManagerServiceState();
    for (;; )
    {
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

        if (State->Heartbeat == 1ULL)
        {
            ServiceSerialWriteLine("[MemManager] Service loop online.");
        }

        LosMemoryManagerServicePoll();
        __asm__ __volatile__("pause" : : : "memory");
    }
}
