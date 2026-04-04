#include "MemoryManagerMain.h"

volatile UINT64 LosMemoryManagerServiceHeartbeat = 0ULL;
const char LosMemoryManagerServiceBanner[] = "Liberation Memory Manager Service";

static LOS_MEMORY_MANAGER_SERVICE_STATE LosMemoryManagerServiceGlobalState;

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

static BOOLEAN ValidateEndpointObject(
    const LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *Endpoint,
    UINT64 EndpointId,
    UINT32 Role,
    UINT64 MailboxPhysicalAddress)
{
    if (Endpoint == 0)
    {
        return 0;
    }

    return Endpoint->Version == LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION &&
           Endpoint->EndpointId == EndpointId &&
           Endpoint->Role == Role &&
           Endpoint->MailboxPhysicalAddress == MailboxPhysicalAddress &&
           (Endpoint->Flags & LOS_MEMORY_MANAGER_ENDPOINT_FLAG_MAILBOX_ATTACHED) != 0ULL;
}

static BOOLEAN ValidateAddressSpaceObject(const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject, UINT64 ServiceImagePhysicalAddress, UINT64 ServiceRootTablePhysicalAddress)
{
    if (AddressSpaceObject == 0)
    {
        return 0;
    }

    return AddressSpaceObject->Version == LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION &&
           AddressSpaceObject->State >= LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_READY &&
           AddressSpaceObject->ServiceImagePhysicalAddress == ServiceImagePhysicalAddress &&
           AddressSpaceObject->RootTablePhysicalAddress == ServiceRootTablePhysicalAddress &&
           AddressSpaceObject->KernelRootTablePhysicalAddress != 0ULL;
}

static BOOLEAN ValidateTaskObject(const LOS_MEMORY_MANAGER_TASK_OBJECT *TaskObject, UINT64 AddressSpaceObjectPhysicalAddress, UINT64 EntryVirtualAddress, UINT64 StackTopPhysicalAddress, UINT64 StackTopVirtualAddress)
{
    if (TaskObject == 0)
    {
        return 0;
    }

    return TaskObject->Version == LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION &&
           TaskObject->AddressSpaceObjectPhysicalAddress == AddressSpaceObjectPhysicalAddress &&
           TaskObject->EntryVirtualAddress == EntryVirtualAddress &&
           TaskObject->StackTopPhysicalAddress == StackTopPhysicalAddress &&
           TaskObject->StackTopVirtualAddress == StackTopVirtualAddress;
}

static BOOLEAN ValidateLaunchBlock(const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock)
{
    if (LaunchBlock == 0)
    {
        return 0;
    }

    if (LaunchBlock->Signature != LOS_MEMORY_MANAGER_LAUNCH_BLOCK_SIGNATURE ||
        LaunchBlock->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION ||
        LaunchBlock->RequestMailboxPhysicalAddress == 0ULL ||
        LaunchBlock->ResponseMailboxPhysicalAddress == 0ULL ||
        LaunchBlock->EventMailboxPhysicalAddress == 0ULL ||
        LaunchBlock->LaunchBlockPhysicalAddress == 0ULL ||
        LaunchBlock->KernelToServiceEndpointObjectPhysicalAddress == 0ULL ||
        LaunchBlock->ServiceToKernelEndpointObjectPhysicalAddress == 0ULL ||
        LaunchBlock->ServiceEventsEndpointObjectPhysicalAddress == 0ULL ||
        LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress == 0ULL ||
        LaunchBlock->ServiceTaskObjectPhysicalAddress == 0ULL ||
        LaunchBlock->ServicePageMapLevel4PhysicalAddress == 0ULL ||
        LaunchBlock->ServiceEntryVirtualAddress == 0ULL ||
        LaunchBlock->ServiceStackTopVirtualAddress == 0ULL)
    {
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
    UINT64 DirectMapOffset;

    if (!ValidateLaunchBlock(LaunchBlock))
    {
        return 0;
    }

    DirectMapOffset = ResolveDirectMapOffset(LaunchBlock, (UINT64)(UINTN)LaunchBlock);
    if (DirectMapOffset == 0ULL)
    {
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
    if (!ValidateEndpointObject(State->ReceiveEndpoint, LaunchBlock->Endpoints.KernelToService, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_RECEIVE, LaunchBlock->RequestMailboxPhysicalAddress) ||
        !ValidateEndpointObject(State->ReplyEndpoint, LaunchBlock->Endpoints.ServiceToKernel, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_REPLY, LaunchBlock->ResponseMailboxPhysicalAddress) ||
        !ValidateEndpointObject(State->EventEndpoint, LaunchBlock->Endpoints.ServiceEvents, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_EVENT, LaunchBlock->EventMailboxPhysicalAddress) ||
        !ValidateAddressSpaceObject(State->AddressSpaceObject, LaunchBlock->ServiceImagePhysicalAddress, LaunchBlock->ServicePageMapLevel4PhysicalAddress) ||
        !ValidateTaskObject(State->TaskObject, LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress, LaunchBlock->ServiceEntryVirtualAddress, LaunchBlock->ServiceStackTopPhysicalAddress, LaunchBlock->ServiceStackTopVirtualAddress))
    {
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
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;

    LaunchBlock = (const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *)(UINTN)LaunchBlockAddress;
    State = LosMemoryManagerServiceState();
    if (State->Online == 0U)
    {
        if (!LosMemoryManagerServiceAttach(LaunchBlock))
        {
            return;
        }
        PostEvent(LOS_MEMORY_MANAGER_EVENT_SERVICE_ONLINE, 0U, LaunchBlock->ServiceEntryVirtualAddress, LaunchBlock->ServiceStackTopPhysicalAddress);
    }

    LosMemoryManagerServiceHeartbeat += 1ULL;
    State->Heartbeat = LosMemoryManagerServiceHeartbeat;
    LosMemoryManagerServicePoll();
}

void LosMemoryManagerServiceEntry(void)
{
    for (;;)
    {
        LosMemoryManagerServiceHeartbeat += 1ULL;
        __asm__ __volatile__("pause" : : : "memory");
    }
}
