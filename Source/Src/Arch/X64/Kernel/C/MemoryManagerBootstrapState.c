#include "MemoryManagerBootstrapInternal.h"

#define LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE 0x5450424D4D534F4CULL
#define LOS_MEMORY_MANAGER_MAILBOX_SIGNATURE 0x58424D4D4D534F4CULL
#define LOS_MEMORY_MANAGER_LAUNCH_BLOCK_SIGNATURE 0x4B4C424D4D534F4CULL

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

static BOOLEAN ClaimBootstrapPages(UINT64 PageCount, UINT32 Owner, UINT64 *BaseAddress)
{
    LOS_X64_CLAIM_FRAMES_REQUEST Request;
    LOS_X64_CLAIM_FRAMES_RESULT Result;

    ZeroMemory(&Request, sizeof(Request));
    ZeroMemory(&Result, sizeof(Result));

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

LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *LosMemoryManagerBootstrapState(void)
{
    return &LosMemoryManagerBootstrapGlobalState;
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
    State->Info.SupportedOperations =
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES) |
        (1ULL << LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES);
    State->Info.ServiceImagePhysicalAddress = 0ULL;
    State->Info.ServiceImageSize = 0ULL;
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
    State->NextRequestId = 1ULL;
    CopyUtf16(State->Info.ServicePath, LOS_MEMORY_MANAGER_SERVICE_PATH_CHARACTERS, L"\\LIBERATION\\SERVICES\\MEMORYMGR.ELF");

    if (BootContext != 0)
    {
        (void)BootContext;
    }

    State->Info.ServiceImagePhysicalAddress = (UINT64)(UINTN)LosMemoryManagerServiceImageStart;
    State->Info.ServiceImageSize = (UINT64)((UINTN)LosMemoryManagerServiceImageEnd - (UINTN)LosMemoryManagerServiceImageStart);
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

void LosMemoryManagerBootstrapInitializeMailboxes(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    InitializeRequestMailbox(LosMemoryManagerBootstrapGetRequestMailbox(), State->Info.Endpoints.KernelToService);
    InitializeResponseMailbox(LosMemoryManagerBootstrapGetResponseMailbox(), State->Info.Endpoints.ServiceToKernel);
    InitializeResponseMailbox((LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *)(UINTN)State->EventMailboxVirtualAddress, State->Info.Endpoints.ServiceEvents);
}

BOOLEAN LosMemoryManagerBootstrapStageTransport(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
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

    Mapped = LosX64GetDirectMapVirtualAddress(State->Info.RequestMailboxPhysicalAddress, State->Info.RequestMailboxSize);
    State->RequestMailboxVirtualAddress = (UINT64)(UINTN)Mapped;
    Mapped = LosX64GetDirectMapVirtualAddress(State->Info.ResponseMailboxPhysicalAddress, State->Info.ResponseMailboxSize);
    State->ResponseMailboxVirtualAddress = (UINT64)(UINTN)Mapped;
    Mapped = LosX64GetDirectMapVirtualAddress(State->Info.EventMailboxPhysicalAddress, State->Info.EventMailboxSize);
    State->EventMailboxVirtualAddress = (UINT64)(UINTN)Mapped;
    Mapped = LosX64GetDirectMapVirtualAddress(State->Info.LaunchBlockPhysicalAddress, State->Info.LaunchBlockSize);
    State->LaunchBlock = (LOS_MEMORY_MANAGER_LAUNCH_BLOCK *)Mapped;

    if (State->RequestMailboxVirtualAddress == 0ULL ||
        State->ResponseMailboxVirtualAddress == 0ULL ||
        State->EventMailboxVirtualAddress == 0ULL ||
        State->LaunchBlock == 0)
    {
        return 0;
    }

    LosMemoryManagerBootstrapInitializeMailboxes();
    ZeroMemory(State->LaunchBlock, sizeof(*State->LaunchBlock));
    State->LaunchBlock->Signature = LOS_MEMORY_MANAGER_LAUNCH_BLOCK_SIGNATURE;
    State->LaunchBlock->Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    State->LaunchBlock->Flags = State->Info.Flags;
    State->LaunchBlock->Endpoints = State->Info.Endpoints;
    State->LaunchBlock->RequestMailboxPhysicalAddress = State->Info.RequestMailboxPhysicalAddress;
    State->LaunchBlock->RequestMailboxSize = State->Info.RequestMailboxSize;
    State->LaunchBlock->ResponseMailboxPhysicalAddress = State->Info.ResponseMailboxPhysicalAddress;
    State->LaunchBlock->ResponseMailboxSize = State->Info.ResponseMailboxSize;
    State->LaunchBlock->EventMailboxPhysicalAddress = State->Info.EventMailboxPhysicalAddress;
    State->LaunchBlock->EventMailboxSize = State->Info.EventMailboxSize;
    State->LaunchBlock->ServiceStackPhysicalAddress = State->Info.ServiceStackPhysicalAddress;
    State->LaunchBlock->ServiceStackPageCount = State->Info.ServiceStackPageCount;
    State->LaunchBlock->ServiceStackTopPhysicalAddress = State->Info.ServiceStackPhysicalAddress + (State->Info.ServiceStackPageCount * 0x1000ULL);
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
