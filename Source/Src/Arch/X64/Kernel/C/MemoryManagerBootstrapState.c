#include "MemoryManagerBootstrapInternal.h"

static LOS_MEMORY_MANAGER_BOOTSTRAP_STATE LosMemoryManagerBootstrapGlobalState;

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

static void CopyUtf16(CHAR16 *Destination, UINTN Capacity, const CHAR16 *Source)
{
    UINTN Index;

    if (Destination == 0 || Capacity == 0U)
    {
        return;
    }

    if (Source == 0)
    {
        Destination[0] = 0;
        return;
    }

    for (Index = 0U; Index + 1U < Capacity && Source[Index] != 0; ++Index)
    {
        Destination[Index] = Source[Index];
    }

    Destination[Index] = 0;
}

static void InitializeEndpointObject(
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *Endpoint,
    UINT64 EndpointId,
    UINT32 Role,
    UINT64 MailboxPhysicalAddress,
    UINT64 MailboxSize,
    UINT64 PeerEndpointId)
{
    if (Endpoint == 0)
    {
        return;
    }

    ZeroMemory(Endpoint, sizeof(*Endpoint));
    Endpoint->Signature = LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE;
    Endpoint->Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    Endpoint->Role = Role;
    Endpoint->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_READY;
    Endpoint->Flags = LOS_MEMORY_MANAGER_ENDPOINT_FLAG_KERNEL_OWNED |
                      LOS_MEMORY_MANAGER_ENDPOINT_FLAG_BOOTSTRAP_OBJECT |
                      LOS_MEMORY_MANAGER_ENDPOINT_FLAG_MAILBOX_ATTACHED |
                      LOS_MEMORY_MANAGER_ENDPOINT_FLAG_SERVICE_VISIBLE;
    Endpoint->EndpointId = EndpointId;
    Endpoint->MailboxPhysicalAddress = MailboxPhysicalAddress;
    Endpoint->MailboxSize = MailboxSize;
    Endpoint->PeerEndpointId = PeerEndpointId;
}

static void InitializeAddressSpaceObject(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpace,
    UINT64 ServiceImagePhysicalAddress)
{
    if (AddressSpace == 0)
    {
        return;
    }

    ZeroMemory(AddressSpace, sizeof(*AddressSpace));
    AddressSpace->Signature = LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE;
    AddressSpace->Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    AddressSpace->State = LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_READY;
    AddressSpace->Flags = LOS_MEMORY_MANAGER_ENDPOINT_FLAG_KERNEL_OWNED |
                          LOS_MEMORY_MANAGER_ENDPOINT_FLAG_BOOTSTRAP_OBJECT |
                          LOS_MEMORY_MANAGER_ENDPOINT_FLAG_SERVICE_VISIBLE;
    AddressSpace->RootTablePhysicalAddress = 0ULL;
    AddressSpace->KernelRootTablePhysicalAddress = 0ULL;
    AddressSpace->DirectMapBase = 0ULL;
    AddressSpace->DirectMapSize = 0ULL;
    AddressSpace->ServiceImagePhysicalAddress = ServiceImagePhysicalAddress;
}

static void InitializeTaskObject(
    LOS_MEMORY_MANAGER_TASK_OBJECT *TaskObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    UINT64 EntryVirtualAddress,
    UINT64 StackTopPhysicalAddress,
    UINT64 StackTopVirtualAddress)
{
    if (TaskObject == 0)
    {
        return;
    }

    ZeroMemory(TaskObject, sizeof(*TaskObject));
    TaskObject->Signature = LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE;
    TaskObject->Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    TaskObject->State = LOS_MEMORY_MANAGER_TASK_STATE_READY;
    TaskObject->Flags = LOS_MEMORY_MANAGER_TASK_FLAG_BOOTSTRAP_HOSTED |
                        LOS_MEMORY_MANAGER_TASK_FLAG_FIRST_USER_TASK;
    TaskObject->TaskId = 1ULL;
    TaskObject->AddressSpaceObjectPhysicalAddress = AddressSpaceObjectPhysicalAddress;
    TaskObject->EntryVirtualAddress = EntryVirtualAddress;
    TaskObject->StackTopPhysicalAddress = StackTopPhysicalAddress;
    TaskObject->StackTopVirtualAddress = StackTopVirtualAddress;
}

static UINT64 ResolveServiceImagePhysicalAddress(const LOS_BOOT_CONTEXT *BootContext, UINT64 ServiceImageVirtualAddress, UINT64 ServiceImageSize)
{
    UINT64 SegmentIndex;

    if (BootContext != 0)
    {
        if ((BootContext->Flags & LOS_BOOT_CONTEXT_FLAG_MEMORY_MANAGER_IMAGE_VALID) != 0ULL &&
            BootContext->MemoryManagerImagePhysicalAddress != 0ULL)
        {
            return BootContext->MemoryManagerImagePhysicalAddress;
        }

        if ((BootContext->Flags & LOS_BOOT_CONTEXT_FLAG_KERNEL_SEGMENTS_VALID) != 0ULL)
        {
            for (SegmentIndex = 0ULL; SegmentIndex < BootContext->KernelLoadSegmentCount && SegmentIndex < LOS_BOOT_CONTEXT_MAX_LOAD_SEGMENTS; ++SegmentIndex)
            {
                const LOS_BOOT_CONTEXT_LOAD_SEGMENT *Segment;
                UINT64 SegmentVirtualStart;
                UINT64 SegmentVirtualEnd;
                UINT64 ServiceImageVirtualEnd;

                Segment = &BootContext->KernelLoadSegments[SegmentIndex];
                SegmentVirtualStart = Segment->VirtualAddress;
                SegmentVirtualEnd = Segment->VirtualAddress + Segment->MemorySize;
                ServiceImageVirtualEnd = ServiceImageVirtualAddress + ServiceImageSize;
                if (SegmentVirtualEnd < SegmentVirtualStart || ServiceImageVirtualEnd < ServiceImageVirtualAddress)
                {
                    continue;
                }

                if (ServiceImageVirtualAddress >= SegmentVirtualStart && ServiceImageVirtualEnd <= SegmentVirtualEnd)
                {
                    return Segment->PhysicalAddress + (ServiceImageVirtualAddress - SegmentVirtualStart);
                }
            }
        }
    }

    return 0ULL;
}

static BOOLEAN ClaimBootstrapPages(UINT64 PageCount, UINT32 Owner, UINT64 *BaseAddress)
{
    LOS_X64_CLAIM_FRAMES_REQUEST Request;
    LOS_X64_CLAIM_FRAMES_RESULT Result;

    ZeroMemory(&Request, sizeof(Request));
    ZeroMemory(&Result, sizeof(Result));

    Request.MinimumPhysicalAddress = 0x1000ULL;
    Request.AlignmentBytes = 0x1000ULL;
    Request.PageCount = PageCount;
    Request.Flags = LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS;
    Request.Owner = Owner;

    LosX64ClaimFrames(&Request, &Result);
    if (Result.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS || Result.PageCount != PageCount)
    {
        return 0;
    }

    if (BaseAddress != 0)
    {
        *BaseAddress = Result.BaseAddress;
    }

    return 1;
}

static void InitializeRequestMailbox(LOS_MEMORY_MANAGER_REQUEST_MAILBOX *Mailbox, UINT64 EndpointId)
{
    UINTN Index;

    if (Mailbox == 0)
    {
        return;
    }

    ZeroMemory(Mailbox, sizeof(*Mailbox));
    Mailbox->Header.Signature = LOS_MEMORY_MANAGER_MAILBOX_SIGNATURE;
    Mailbox->Header.Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    Mailbox->Header.EndpointId = EndpointId;
    Mailbox->Header.SlotCount = LOS_MEMORY_MANAGER_MAILBOX_SLOT_COUNT;

    for (Index = 0U; Index < LOS_MEMORY_MANAGER_MAILBOX_SLOT_COUNT; ++Index)
    {
        Mailbox->Slots[Index].SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
        Mailbox->Slots[Index].Sequence = 0ULL;
    }
}

static void InitializeResponseMailbox(LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *Mailbox, UINT64 EndpointId)
{
    UINTN Index;

    if (Mailbox == 0)
    {
        return;
    }

    ZeroMemory(Mailbox, sizeof(*Mailbox));
    Mailbox->Header.Signature = LOS_MEMORY_MANAGER_MAILBOX_SIGNATURE;
    Mailbox->Header.Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    Mailbox->Header.EndpointId = EndpointId;
    Mailbox->Header.SlotCount = LOS_MEMORY_MANAGER_MAILBOX_SLOT_COUNT;

    for (Index = 0U; Index < LOS_MEMORY_MANAGER_MAILBOX_SLOT_COUNT; ++Index)
    {
        Mailbox->Slots[Index].SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
        Mailbox->Slots[Index].Sequence = 0ULL;
    }
}

static void InitializeEventMailbox(LOS_MEMORY_MANAGER_EVENT_MAILBOX *Mailbox, UINT64 EndpointId)
{
    UINTN Index;

    if (Mailbox == 0)
    {
        return;
    }

    ZeroMemory(Mailbox, sizeof(*Mailbox));
    Mailbox->Header.Signature = LOS_MEMORY_MANAGER_MAILBOX_SIGNATURE;
    Mailbox->Header.Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    Mailbox->Header.EndpointId = EndpointId;
    Mailbox->Header.SlotCount = LOS_MEMORY_MANAGER_MAILBOX_SLOT_COUNT;

    for (Index = 0U; Index < LOS_MEMORY_MANAGER_MAILBOX_SLOT_COUNT; ++Index)
    {
        Mailbox->Slots[Index].SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
        Mailbox->Slots[Index].Sequence = 0ULL;
    }
}

LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *LosMemoryManagerBootstrapState(void)
{
    return &LosMemoryManagerBootstrapGlobalState;
}

LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *LosMemoryManagerBootstrapGetKernelToServiceEndpointObject(void)
{
    return LosMemoryManagerBootstrapState()->KernelToServiceEndpointObject;
}

LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *LosMemoryManagerBootstrapGetServiceToKernelEndpointObject(void)
{
    return LosMemoryManagerBootstrapState()->ServiceToKernelEndpointObject;
}

LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *LosMemoryManagerBootstrapGetServiceEventsEndpointObject(void)
{
    return LosMemoryManagerBootstrapState()->ServiceEventsEndpointObject;
}

BOOLEAN LosMemoryManagerBootstrapEndpointsReady(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    return State->KernelToServiceEndpointObject != 0 &&
           State->ServiceToKernelEndpointObject != 0 &&
           State->ServiceEventsEndpointObject != 0;
}

void LosMemoryManagerBootstrapSetEndpointState(LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *Endpoint, UINT32 State)
{
    if (Endpoint == 0)
    {
        return;
    }

    Endpoint->State = State;
}

void LosMemoryManagerBootstrapReset(const LOS_BOOT_CONTEXT *BootContext)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    ZeroMemory(State, sizeof(*State));
    State->Info.Signature = LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE;
    State->Info.Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    State->Info.State = LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_DEFINED;
    State->Info.Flags = LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_KERNEL_LOW_LEVEL_FRAMES |
                        LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_KERNEL_LOW_LEVEL_PAGING |
                        LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_ENDPOINT_BRIDGE;
    State->Info.Endpoints.KernelToService = LOS_MEMORY_MANAGER_ENDPOINT_KERNEL_TO_SERVICE;
    State->Info.Endpoints.ServiceToKernel = LOS_MEMORY_MANAGER_ENDPOINT_SERVICE_TO_KERNEL;
    State->Info.Endpoints.ServiceEvents = LOS_MEMORY_MANAGER_ENDPOINT_SERVICE_EVENTS;
    State->Info.KernelToServiceEndpointObjectPhysicalAddress = 0ULL;
    State->Info.ServiceToKernelEndpointObjectPhysicalAddress = 0ULL;
    State->Info.ServiceEventsEndpointObjectPhysicalAddress = 0ULL;
    State->Info.SupportedOperations =
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES);
    State->Info.ServiceImagePhysicalAddress = 0ULL;
    State->Info.ServiceImageSize = 0ULL;
    State->Info.ServiceAddressSpaceObjectPhysicalAddress = 0ULL;
    State->Info.ServiceTaskObjectPhysicalAddress = 0ULL;
    State->Info.ServiceEntryVirtualAddress = 0ULL;
    State->Info.ServiceStackPhysicalAddress = 0ULL;
    State->Info.ServiceStackPageCount = 0ULL;
    State->Info.RequestMailboxPhysicalAddress = 0ULL;
    State->Info.RequestMailboxSize = 0ULL;
    State->Info.ResponseMailboxPhysicalAddress = 0ULL;
    State->Info.ResponseMailboxSize = 0ULL;
    State->Info.EventMailboxPhysicalAddress = 0ULL;
    State->Info.EventMailboxSize = 0ULL;
    State->Info.LaunchBlockPhysicalAddress = 0ULL;
    State->Info.LaunchBlockSize = 0ULL;
    State->ServiceImageVirtualAddress = 0ULL;
    State->NextRequestId = 1ULL;
    CopyUtf16(State->Info.ServicePath, LOS_MEMORY_MANAGER_SERVICE_PATH_CHARACTERS, L"\\LIBERATION\\SERVICES\\MEMORYMGR.ELF");

    if (BootContext != 0)
    {
        (void)BootContext;
    }

    State->ServiceImageVirtualAddress = (UINT64)(UINTN)LosMemoryManagerServiceImageStart;
    State->Info.ServiceImageSize = (UINT64)((UINTN)LosMemoryManagerServiceImageEnd - (UINTN)LosMemoryManagerServiceImageStart);
    State->Info.ServiceImagePhysicalAddress = ResolveServiceImagePhysicalAddress(BootContext, State->ServiceImageVirtualAddress, State->Info.ServiceImageSize);
}

void LosMemoryManagerBootstrapUpdateState(UINT32 NewState)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    State->Info.State = NewState;
}

void LosMemoryManagerBootstrapSetFlag(UINT64 Flag)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    State->Info.Flags |= Flag;
}

UINT64 LosMemoryManagerBootstrapAllocateRequestId(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    UINT64 RequestId;

    State = LosMemoryManagerBootstrapState();
    RequestId = State->NextRequestId;
    State->NextRequestId += 1ULL;
    return RequestId;
}

void LosMemoryManagerBootstrapRecordRequest(UINT32 Operation)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    State->MessagesSent += 1ULL;
    State->LastOperation = (UINT64)Operation;
}

void LosMemoryManagerBootstrapRecordCompletion(UINT32 Operation, UINT32 Status)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    State->MessagesCompleted += 1ULL;
    State->LastOperation = (UINT64)Operation;
    State->LastStatus = (UINT64)Status;
}

const LOS_MEMORY_MANAGER_BOOTSTRAP_INFO *LosGetMemoryManagerBootstrapInfo(void)
{
    return &LosMemoryManagerBootstrapState()->Info;
}

const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LosGetMemoryManagerLaunchBlock(void)
{
    return LosMemoryManagerBootstrapState()->LaunchBlock;
}

BOOLEAN LosIsMemoryManagerBootstrapReady(void)
{
    return LosMemoryManagerBootstrapState()->Info.State == LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_READY;
}

LOS_MEMORY_MANAGER_REQUEST_MAILBOX *LosMemoryManagerBootstrapGetRequestMailbox(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    return (LOS_MEMORY_MANAGER_REQUEST_MAILBOX *)(UINTN)State->RequestMailboxVirtualAddress;
}

LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *LosMemoryManagerBootstrapGetResponseMailbox(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    return (LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *)(UINTN)State->ResponseMailboxVirtualAddress;
}

LOS_MEMORY_MANAGER_EVENT_MAILBOX *LosMemoryManagerBootstrapGetEventMailbox(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    return (LOS_MEMORY_MANAGER_EVENT_MAILBOX *)(UINTN)State->EventMailboxVirtualAddress;
}

void LosMemoryManagerBootstrapInitializeMailboxes(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    InitializeRequestMailbox(LosMemoryManagerBootstrapGetRequestMailbox(), State->Info.Endpoints.KernelToService);
    InitializeResponseMailbox(LosMemoryManagerBootstrapGetResponseMailbox(), State->Info.Endpoints.ServiceToKernel);
    InitializeEventMailbox(LosMemoryManagerBootstrapGetEventMailbox(), State->Info.Endpoints.ServiceEvents);
}

static BOOLEAN MapBootstrapObject(UINT64 PhysicalAddress, UINT64 Size, void **MappedOut)
{
    void *Mapped;

    Mapped = LosX64GetDirectMapVirtualAddress(PhysicalAddress, Size);
    if (Mapped == 0)
    {
        *MappedOut = 0;
        return 0;
    }

    *MappedOut = Mapped;
    return 1;
}

BOOLEAN LosMemoryManagerBootstrapStageTransport(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *KernelToServiceEndpointObject;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *ServiceToKernelEndpointObject;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *ServiceEventsEndpointObject;
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *ServiceAddressSpaceObject;
    LOS_MEMORY_MANAGER_TASK_OBJECT *ServiceTaskObject;
    LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    void *Mapped;

    State = LosMemoryManagerBootstrapState();

    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_REQUEST_MAILBOX_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.RequestMailboxPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_RESPONSE_MAILBOX_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ResponseMailboxPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_EVENT_MAILBOX_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.EventMailboxPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.KernelToServiceEndpointObjectPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ServiceToKernelEndpointObjectPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ServiceEventsEndpointObjectPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ServiceAddressSpaceObjectPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_TASK_OBJECT_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ServiceTaskObjectPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_LAUNCH_BLOCK_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.LaunchBlockPhysicalAddress))
    {
        return 0;
    }
    if (!ClaimBootstrapPages(LOS_MEMORY_MANAGER_SERVICE_STACK_PAGE_COUNT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED, &State->Info.ServiceStackPhysicalAddress))
    {
        return 0;
    }

    State->Info.RequestMailboxSize = LOS_MEMORY_MANAGER_REQUEST_MAILBOX_PAGE_COUNT * 0x1000ULL;
    State->Info.ResponseMailboxSize = LOS_MEMORY_MANAGER_RESPONSE_MAILBOX_PAGE_COUNT * 0x1000ULL;
    State->Info.EventMailboxSize = LOS_MEMORY_MANAGER_EVENT_MAILBOX_PAGE_COUNT * 0x1000ULL;
    State->Info.LaunchBlockSize = LOS_MEMORY_MANAGER_LAUNCH_BLOCK_PAGE_COUNT * 0x1000ULL;
    State->Info.ServiceStackPageCount = LOS_MEMORY_MANAGER_SERVICE_STACK_PAGE_COUNT;

    if (!MapBootstrapObject(State->Info.RequestMailboxPhysicalAddress, State->Info.RequestMailboxSize, &Mapped))
    {
        return 0;
    }
    State->RequestMailboxVirtualAddress = (UINT64)(UINTN)Mapped;

    if (!MapBootstrapObject(State->Info.ResponseMailboxPhysicalAddress, State->Info.ResponseMailboxSize, &Mapped))
    {
        return 0;
    }
    State->ResponseMailboxVirtualAddress = (UINT64)(UINTN)Mapped;

    if (!MapBootstrapObject(State->Info.EventMailboxPhysicalAddress, State->Info.EventMailboxSize, &Mapped))
    {
        return 0;
    }
    State->EventMailboxVirtualAddress = (UINT64)(UINTN)Mapped;

    if (!MapBootstrapObject(State->Info.KernelToServiceEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 0x1000ULL, &Mapped))
    {
        return 0;
    }
    KernelToServiceEndpointObject = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)Mapped;

    if (!MapBootstrapObject(State->Info.ServiceToKernelEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 0x1000ULL, &Mapped))
    {
        return 0;
    }
    ServiceToKernelEndpointObject = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)Mapped;

    if (!MapBootstrapObject(State->Info.ServiceEventsEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 0x1000ULL, &Mapped))
    {
        return 0;
    }
    ServiceEventsEndpointObject = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)Mapped;

    if (!MapBootstrapObject(State->Info.ServiceAddressSpaceObjectPhysicalAddress, LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT_PAGE_COUNT * 0x1000ULL, &Mapped))
    {
        return 0;
    }
    ServiceAddressSpaceObject = (LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *)Mapped;

    if (!MapBootstrapObject(State->Info.ServiceTaskObjectPhysicalAddress, LOS_MEMORY_MANAGER_TASK_OBJECT_PAGE_COUNT * 0x1000ULL, &Mapped))
    {
        return 0;
    }
    ServiceTaskObject = (LOS_MEMORY_MANAGER_TASK_OBJECT *)Mapped;

    if (!MapBootstrapObject(State->Info.LaunchBlockPhysicalAddress, State->Info.LaunchBlockSize, &Mapped))
    {
        return 0;
    }
    LaunchBlock = (LOS_MEMORY_MANAGER_LAUNCH_BLOCK *)Mapped;

    State->KernelToServiceEndpointObject = KernelToServiceEndpointObject;
    State->ServiceToKernelEndpointObject = ServiceToKernelEndpointObject;
    State->ServiceEventsEndpointObject = ServiceEventsEndpointObject;
    State->ServiceAddressSpaceObject = ServiceAddressSpaceObject;
    State->ServiceTaskObject = ServiceTaskObject;
    State->LaunchBlock = LaunchBlock;

    LosMemoryManagerBootstrapInitializeMailboxes();
    InitializeEndpointObject(
        State->KernelToServiceEndpointObject,
        State->Info.Endpoints.KernelToService,
        LOS_MEMORY_MANAGER_ENDPOINT_ROLE_RECEIVE,
        State->Info.RequestMailboxPhysicalAddress,
        State->Info.RequestMailboxSize,
        State->Info.Endpoints.ServiceToKernel);
    InitializeEndpointObject(
        State->ServiceToKernelEndpointObject,
        State->Info.Endpoints.ServiceToKernel,
        LOS_MEMORY_MANAGER_ENDPOINT_ROLE_REPLY,
        State->Info.ResponseMailboxPhysicalAddress,
        State->Info.ResponseMailboxSize,
        State->Info.Endpoints.KernelToService);
    InitializeEndpointObject(
        State->ServiceEventsEndpointObject,
        State->Info.Endpoints.ServiceEvents,
        LOS_MEMORY_MANAGER_ENDPOINT_ROLE_EVENT,
        State->Info.EventMailboxPhysicalAddress,
        State->Info.EventMailboxSize,
        State->Info.Endpoints.KernelToService);
    InitializeAddressSpaceObject(State->ServiceAddressSpaceObject, State->Info.ServiceImagePhysicalAddress);
    InitializeTaskObject(
        State->ServiceTaskObject,
        State->Info.ServiceAddressSpaceObjectPhysicalAddress,
        State->Info.ServiceEntryVirtualAddress,
        State->Info.ServiceStackPhysicalAddress + (State->Info.ServiceStackPageCount * 0x1000ULL),
        0ULL);
    ZeroMemory(State->LaunchBlock, sizeof(*State->LaunchBlock));
    State->LaunchBlock->Signature = LOS_MEMORY_MANAGER_LAUNCH_BLOCK_SIGNATURE;
    State->LaunchBlock->Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    State->LaunchBlock->Flags = State->Info.Flags;
    State->LaunchBlock->Endpoints = State->Info.Endpoints;
    State->LaunchBlock->KernelToServiceEndpointObjectPhysicalAddress = State->Info.KernelToServiceEndpointObjectPhysicalAddress;
    State->LaunchBlock->ServiceToKernelEndpointObjectPhysicalAddress = State->Info.ServiceToKernelEndpointObjectPhysicalAddress;
    State->LaunchBlock->ServiceEventsEndpointObjectPhysicalAddress = State->Info.ServiceEventsEndpointObjectPhysicalAddress;
    State->LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress = State->Info.ServiceAddressSpaceObjectPhysicalAddress;
    State->LaunchBlock->ServiceTaskObjectPhysicalAddress = State->Info.ServiceTaskObjectPhysicalAddress;
    State->LaunchBlock->ServicePageMapLevel4PhysicalAddress = State->Info.ServicePageMapLevel4PhysicalAddress;
    State->LaunchBlock->RequestMailboxPhysicalAddress = State->Info.RequestMailboxPhysicalAddress;
    State->LaunchBlock->RequestMailboxSize = State->Info.RequestMailboxSize;
    State->LaunchBlock->ResponseMailboxPhysicalAddress = State->Info.ResponseMailboxPhysicalAddress;
    State->LaunchBlock->ResponseMailboxSize = State->Info.ResponseMailboxSize;
    State->LaunchBlock->EventMailboxPhysicalAddress = State->Info.EventMailboxPhysicalAddress;
    State->LaunchBlock->EventMailboxSize = State->Info.EventMailboxSize;
    State->LaunchBlock->LaunchBlockPhysicalAddress = State->Info.LaunchBlockPhysicalAddress;
    State->LaunchBlock->ServiceStackPhysicalAddress = State->Info.ServiceStackPhysicalAddress;
    State->LaunchBlock->ServiceStackPageCount = State->Info.ServiceStackPageCount;
    State->LaunchBlock->ServiceStackTopPhysicalAddress = State->Info.ServiceStackPhysicalAddress + (State->Info.ServiceStackPageCount * 0x1000ULL);
    State->LaunchBlock->ServiceStackTopVirtualAddress = State->ServiceTaskObject->StackTopVirtualAddress;
    State->LaunchBlock->ServiceImagePhysicalAddress = State->Info.ServiceImagePhysicalAddress;
    State->LaunchBlock->ServiceImageSize = State->Info.ServiceImageSize;
    State->LaunchBlock->ServiceEntryVirtualAddress = State->Info.ServiceEntryVirtualAddress;
    CopyUtf16(State->LaunchBlock->ServicePath, LOS_MEMORY_MANAGER_SERVICE_PATH_CHARACTERS, State->Info.ServicePath);

    LosMemoryManagerBootstrapSetFlag(LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_TRANSPORT_READY);
    LosMemoryManagerBootstrapSetFlag(LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_LAUNCH_BLOCK_READY);
    LosMemoryManagerBootstrapSetFlag(LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_STACK_READY);
    LosMemoryManagerBootstrapUpdateState(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_STAGED);
    return 1;
}
