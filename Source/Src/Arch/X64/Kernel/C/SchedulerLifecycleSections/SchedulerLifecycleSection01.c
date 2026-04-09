/*
 * File Name: SchedulerLifecycleSection01.c
 * File Version: 0.0.3
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T22:45:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerLifecycle.c.
 */

static LOS_KERNEL_SCHEDULER_STATE LosKernelSchedulerGlobalState;
static UINT8 LosKernelSchedulerBootstrapStacks[LOS_KERNEL_SCHEDULER_MAX_TASKS][LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES] __attribute__((aligned(4096)));
static UINT8 LosKernelSchedulerBootstrapStackUsed[LOS_KERNEL_SCHEDULER_MAX_TASKS];
static UINT8 LosKernelSchedulerDirectClaimStackUsed[LOS_KERNEL_SCHEDULER_MAX_TASKS];
static void *LosKernelSchedulerDirectClaimStackPoolBase;
static UINT64 LosKernelSchedulerStagedUserImagePhysicalAddress;
static UINT64 LosKernelSchedulerStagedUserImageBytes;

LOS_KERNEL_SCHEDULER_STATE *LosKernelSchedulerState(void)
{
    return &LosKernelSchedulerGlobalState;
}

static void ZeroBytes(void *Buffer, UINTN ByteCount)
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

static UINT64 EnterSchedulerCriticalSection(void)
{
    UINT64 Flags;

    __asm__ __volatile__("pushfq; popq %0" : "=r"(Flags) : : "memory");
    __asm__ __volatile__("cli" : : : "memory");
    return Flags;
}

static void LeaveSchedulerCriticalSection(UINT64 Flags)
{
    if ((Flags & 0x200ULL) != 0ULL)
    {
        __asm__ __volatile__("sti" : : : "memory");
    }
}


static BOOLEAN IsMemoryManagerSchedulerTransportReady(void)
{
    const LOS_MEMORY_MANAGER_BOOTSTRAP_INFO *Info;

    Info = LosGetMemoryManagerBootstrapInfo();
    if (Info == 0)
    {
        return 0;
    }

    if ((Info->Flags & LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_ATTACH_COMPLETE) == 0ULL)
    {
        return 0;
    }

    return 1;
}

static BOOLEAN SchedulerMayUseMemoryManagerBackedThreadStacks(void)
{
    /*
     * The hosted bootstrap path can still lose AllocateFrames replies after
     * transient process-root activation. Keep scheduler-owned thread stacks on
     * the bootstrap fallback pool until the memory manager is running through
     * the normal scheduled-service path rather than the hosted bootstrap pump.
     */
    return 0;
}

static BOOLEAN ReserveDirectClaimKernelThreadStackPool(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_X64_CLAIM_FRAMES_REQUEST ClaimRequest;
    LOS_X64_CLAIM_FRAMES_RESULT ClaimResult;
    UINT64 TotalPages;
    BOOLEAN ClaimedThroughMemoryManager;

    State = LosKernelSchedulerState();
    if (State == 0)
    {
        return 0;
    }

    if (State->DirectClaimStackPoolReady != 0U)
    {
        return 1;
    }

    TotalPages = LOS_KERNEL_SCHEDULER_MAX_TASKS * LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES;
    ZeroBytes(&ClaimRequest, sizeof(ClaimRequest));
    ZeroBytes(&ClaimResult, sizeof(ClaimResult));
    ClaimRequest.PageCount = TotalPages;
    ClaimRequest.AlignmentBytes = 4096ULL;
    ClaimRequest.Flags = LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS;
    ClaimRequest.Owner = LOS_X64_MEMORY_REGION_OWNER_CLAIMED;
    ClaimedThroughMemoryManager = 0;

    if (SchedulerMayUseMemoryManagerBackedThreadStacks() != 0U &&
        IsMemoryManagerSchedulerTransportReady() != 0U)
    {
        LosMemoryManagerSendClaimFrames(&ClaimRequest, &ClaimResult);
        if (ClaimResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS &&
            ClaimResult.BaseAddress != 0ULL &&
            ClaimResult.PageCount == TotalPages)
        {
            ClaimedThroughMemoryManager = 1;
        }
        else
        {
            LosKernelTrace("Kernel scheduler deferred the unstable memory-manager-backed stack-pool path and is using direct claims.");
            ZeroBytes(&ClaimResult, sizeof(ClaimResult));
        }
    }

    if (ClaimedThroughMemoryManager == 0)
    {
        LosX64ClaimFrames(&ClaimRequest, &ClaimResult);
    }

    if (ClaimResult.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS ||
        ClaimResult.BaseAddress == 0ULL ||
        ClaimResult.PageCount != TotalPages)
    {
        LosKernelTraceFail("Kernel scheduler could not reserve direct-claim stack pool frames.");
        LosKernelTraceUnsigned("Kernel scheduler direct-claim stack-pool status: ", ClaimResult.Status);
        LosKernelTraceUnsigned("Kernel scheduler direct-claim stack-pool pages returned: ", ClaimResult.PageCount);
        return 0;
    }

    LosKernelSchedulerDirectClaimStackPoolBase = LosX64GetDirectMapVirtualAddress(ClaimResult.BaseAddress, TotalPages * 4096ULL);
    if (LosKernelSchedulerDirectClaimStackPoolBase == 0)
    {
        LosKernelTraceFail("Kernel scheduler could not direct-map the direct-claim stack pool.");
        LosKernelTraceHex64("Kernel scheduler direct-claim stack-pool base: ", ClaimResult.BaseAddress);
        return 0;
    }

    State->DirectClaimStackPoolPhysicalAddress = ClaimResult.BaseAddress;
    State->DirectClaimStackPoolBytes = TotalPages * 4096ULL;
    State->DirectClaimStackPoolReady = 1U;
    State->DirectClaimStackSlotsInUse = 0U;
    ZeroBytes(&LosKernelSchedulerDirectClaimStackUsed[0], sizeof(LosKernelSchedulerDirectClaimStackUsed));
    if (ClaimedThroughMemoryManager != 0U)
    {
        LosKernelTraceOk("Kernel scheduler stack pool reserved through the memory manager.");
    }
    else
    {
        LosKernelTraceOk("Kernel scheduler direct-claim stack pool ready.");
    }
    LosKernelTraceHex64("Kernel scheduler direct-claim stack-pool base: ", ClaimResult.BaseAddress);
    LosKernelTraceUnsigned("Kernel scheduler direct-claim stack-pool pages: ", TotalPages);
    return 1;
}

static void WriteStackReturnAddress(UINT64 StackAddress, UINT64 ReturnAddress)
{
    UINT64 *Pointer;

    Pointer = (UINT64 *)(UINTN)StackAddress;
    *Pointer = ReturnAddress;
}

static UINT64 ReadStackReturnAddress(UINT64 StackAddress)
{
    const UINT64 *Pointer;

    Pointer = (const UINT64 *)(UINTN)StackAddress;
    return *Pointer;
}

static UINT64 GetUserTransitionFrameStackPointer(const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    UINT64 MinimumFrameAddress;

    if (Task == 0 ||
        Task->StackBaseVirtualAddress == 0ULL ||
        Task->StackTopVirtualAddress == 0ULL ||
        Task->StackTopVirtualAddress <= Task->StackBaseVirtualAddress)
    {
        return 0ULL;
    }

    MinimumFrameAddress = Task->StackBaseVirtualAddress + sizeof(LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME);
    if (Task->StackTopVirtualAddress < MinimumFrameAddress + LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME_STACK_OFFSET_BYTES)
    {
        return 0ULL;
    }

    return (Task->StackTopVirtualAddress - LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME_STACK_OFFSET_BYTES) & ~0xFULL;
}

static UINT64 GetUserTransitionChainStackPointer(const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    UINT64 MinimumChainAddress;

    if (Task == 0 ||
        Task->StackBaseVirtualAddress == 0ULL ||
        Task->StackTopVirtualAddress == 0ULL ||
        Task->StackTopVirtualAddress <= Task->StackBaseVirtualAddress)
    {
        return 0ULL;
    }

    MinimumChainAddress = Task->StackBaseVirtualAddress + 16ULL;
    if (Task->StackTopVirtualAddress < MinimumChainAddress + LOS_KERNEL_SCHEDULER_USER_TRANSITION_CHAIN_STACK_OFFSET_BYTES)
    {
        return 0ULL;
    }

    return (Task->StackTopVirtualAddress - LOS_KERNEL_SCHEDULER_USER_TRANSITION_CHAIN_STACK_OFFSET_BYTES) & ~0xFULL;
}

static UINT64 RotateLeft64(UINT64 Value, UINT32 Shift)
{
    if (Shift == 0U)
    {
        return Value;
    }

    return (Value << Shift) | (Value >> (64U - Shift));
}

static UINT64 ComputeUserTransitionContractSignature(
    const LOS_KERNEL_SCHEDULER_PROCESS *Process,
    const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    const LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *Frame;
    UINT64 Signature;

    if (Process == 0 || Task == 0)
    {
        return 0ULL;
    }

    if (Process->UserEntryVirtualAddress == 0ULL ||
        Process->UserStackTopVirtualAddress == 0ULL ||
        Process->UserCodeSegmentSelector == 0ULL ||
        Process->UserStackSegmentSelector == 0ULL ||
        Process->UserRflags == 0ULL ||
        Process->UserTransitionFrameStackPointer == 0ULL ||
        Process->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Process->UserTransitionBridgeVirtualAddress == 0ULL ||
        Process->UserTransitionChainStackPointer == 0ULL ||
        Task->UserInstructionPointer == 0ULL ||
        Task->UserStackPointer == 0ULL ||
        Task->UserCodeSegmentSelector == 0ULL ||
        Task->UserStackSegmentSelector == 0ULL ||
        Task->UserRflags == 0ULL ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Task->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Task->UserTransitionBridgeVirtualAddress == 0ULL ||
        Task->UserTransitionChainStackPointer == 0ULL)
    {
        return 0ULL;
    }

    if (Process->UserEntryVirtualAddress != Task->UserInstructionPointer ||
        Process->UserStackTopVirtualAddress != Task->UserStackPointer ||
        Process->UserCodeSegmentSelector != Task->UserCodeSegmentSelector ||
        Process->UserStackSegmentSelector != Task->UserStackSegmentSelector ||
        Process->UserRflags != Task->UserRflags ||
        Process->UserTransitionFrameStackPointer != Task->UserTransitionFrameStackPointer ||
        Process->UserTransitionKernelEntryVirtualAddress != Task->UserTransitionKernelEntryVirtualAddress ||
        Process->UserTransitionBridgeVirtualAddress != Task->UserTransitionBridgeVirtualAddress ||
        Process->UserTransitionChainStackPointer != Task->UserTransitionChainStackPointer)
    {
        return 0ULL;
    }

    Frame = (const LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *)(UINTN)Task->UserTransitionFrameStackPointer;
    if (Frame == 0 ||
        Frame->Rip != Task->UserInstructionPointer ||
        Frame->Cs != Task->UserCodeSegmentSelector ||
        Frame->Rflags != Task->UserRflags ||
        Frame->Rsp != Task->UserStackPointer ||
        Frame->Ss != Task->UserStackSegmentSelector)
    {
        return 0ULL;
    }

    if (ReadStackReturnAddress(Task->UserTransitionChainStackPointer) != Task->UserTransitionBridgeVirtualAddress ||
        ReadStackReturnAddress(Task->UserTransitionChainStackPointer + 8ULL) != Task->UserTransitionKernelEntryVirtualAddress)
    {
        return 0ULL;
    }

    Signature = 0x555452414E534954ULL;
    Signature ^= Process->ProcessId;
    Signature = RotateLeft64(Signature, 7U) ^ Process->UserEntryVirtualAddress;
    Signature = RotateLeft64(Signature, 11U) ^ Process->UserStackTopVirtualAddress;
    Signature = RotateLeft64(Signature, 13U) ^ Process->UserCodeSegmentSelector;
    Signature = RotateLeft64(Signature, 17U) ^ Process->UserStackSegmentSelector;
    Signature = RotateLeft64(Signature, 19U) ^ Process->UserRflags;
    Signature = RotateLeft64(Signature, 23U) ^ Process->UserTransitionFrameStackPointer;
    Signature = RotateLeft64(Signature, 29U) ^ Process->UserTransitionKernelEntryVirtualAddress;
    Signature = RotateLeft64(Signature, 31U) ^ Process->UserTransitionBridgeVirtualAddress;
    Signature = RotateLeft64(Signature, 37U) ^ Process->UserTransitionChainStackPointer;
    Signature = RotateLeft64(Signature, 41U) ^ Frame->Rip;
    Signature = RotateLeft64(Signature, 43U) ^ Frame->Rsp;
    Signature = RotateLeft64(Signature, 47U) ^ Task->TaskId;
    if (Signature == 0ULL)
    {
        Signature = 0x434F4E5452414354ULL;
    }

    return Signature;
}


static UINT64 ComputeUserTransitionSealValue(
    const LOS_KERNEL_SCHEDULER_PROCESS *Process,
    const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    UINT64 SealValue;

    if (Process == 0 || Task == 0)
    {
        return 0ULL;
    }

    if (Process->UserTransitionContractSignature == 0ULL ||
        Task->UserTransitionContractSignature == 0ULL ||
        Process->UserTransitionContractSignature != Task->UserTransitionContractSignature ||
        Process->UserTransitionFrameStackPointer == 0ULL ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Process->UserTransitionChainStackPointer == 0ULL ||
        Task->UserTransitionChainStackPointer == 0ULL)
    {
        return 0ULL;
    }

    if (ReadStackReturnAddress(Task->UserTransitionChainStackPointer) != Task->UserTransitionBridgeVirtualAddress ||
        ReadStackReturnAddress(Task->UserTransitionChainStackPointer + 8ULL) != Task->UserTransitionKernelEntryVirtualAddress)
    {
        return 0ULL;
    }

    SealValue = 0x5345414C55534552ULL;
    SealValue ^= Process->UserTransitionContractSignature;
    SealValue = RotateLeft64(SealValue, 5U) ^ Process->UserTransitionFrameStackPointer;
    SealValue = RotateLeft64(SealValue, 11U) ^ Process->UserTransitionChainStackPointer;
    SealValue = RotateLeft64(SealValue, 17U) ^ Process->UserTransitionKernelEntryVirtualAddress;
    SealValue = RotateLeft64(SealValue, 23U) ^ Process->UserTransitionBridgeVirtualAddress;
    SealValue = RotateLeft64(SealValue, 29U) ^ Task->TaskId;
    if (SealValue == 0ULL)
    {
        SealValue = 0x5345414C4F4B0001ULL;
    }

    return SealValue;
}
