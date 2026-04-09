/*
 * File Name: MemoryManagerBootstrapStateSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapState.c.
 */

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

static void ReportBootstrapAddressSpaceObjectCreated(UINT64 AddressSpaceId, UINT64 AddressSpaceObjectPhysicalAddress)
{
    LosKernelSerialWriteText("[Kernel] Bootstrap address-space object staged id=");
    LosKernelSerialWriteUnsigned(AddressSpaceId);
    LosKernelSerialWriteText(" object=");
    LosKernelSerialWriteHex64(AddressSpaceObjectPhysicalAddress);
    LosKernelSerialWriteText("\n");
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
    AddressSpace->ServiceImageSize = 0ULL;
    AddressSpace->ServiceImageVirtualBase = 0ULL;
    AddressSpace->EntryVirtualAddress = 0ULL;
    AddressSpace->StackPhysicalAddress = 0ULL;
    AddressSpace->StackPageCount = 0ULL;
    AddressSpace->StackBaseVirtualAddress = 0ULL;
    AddressSpace->StackTopVirtualAddress = 0ULL;
    AddressSpace->AddressSpaceId = 1ULL;
    AddressSpace->ReservedVirtualRegionCount = 0U;
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
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_PROTECT_PAGES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_QUERY_MAPPING) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES);
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
    State->Info.MemoryRegionTablePhysicalAddress = 0ULL;
    State->Info.MemoryRegionCount = 0ULL;
    State->Info.MemoryRegionEntrySize = 0ULL;
    State->Info.LaunchBlockPhysicalAddress = 0ULL;
    State->Info.LaunchBlockSize = 0ULL;
    State->ServiceImageVirtualAddress = 0ULL;
    State->NextRequestId = 1ULL;
    State->BootstrapAttachRequestId = 0ULL;
    State->BootstrapResultCode = LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_INVALID_REQUEST;
    State->StateTransitionCount = 0ULL;
    CopyUtf16(State->Info.ServicePath, LOS_MEMORY_MANAGER_SERVICE_PATH_CHARACTERS, L"\\LIBERATION\\SERVICES\\MEMORYMGR.ELF");

    if (BootContext != 0)
    {
        (void)BootContext;
    }

    State->ServiceImageVirtualAddress = (UINT64)(UINTN)LosMemoryManagerServiceImageStart;
    State->Info.ServiceImageSize = (UINT64)((UINTN)LosMemoryManagerServiceImageEnd - (UINTN)LosMemoryManagerServiceImageStart);
    State->Info.ServiceImagePhysicalAddress = ResolveServiceImagePhysicalAddress(BootContext, State->ServiceImageVirtualAddress, State->Info.ServiceImageSize);
    State->Info.MemoryRegionCount = (UINT64)LosX64GetMemoryRegionCount();
    State->Info.MemoryRegionEntrySize = (UINT64)sizeof(LOS_X64_MEMORY_REGION);
    if (State->Info.MemoryRegionCount != 0ULL)
    {
        UINT64 MemoryRegionTablePhysicalAddress;

        MemoryRegionTablePhysicalAddress = 0ULL;
        if (LosX64TryTranslateKernelVirtualToPhysical((UINT64)(UINTN)LosX64GetMemoryRegion(0U), &MemoryRegionTablePhysicalAddress))
        {
            State->Info.MemoryRegionTablePhysicalAddress = MemoryRegionTablePhysicalAddress;
        }
    }
}
