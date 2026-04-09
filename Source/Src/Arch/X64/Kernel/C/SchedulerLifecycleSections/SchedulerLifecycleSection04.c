/*
 * File Name: SchedulerLifecycleSection04.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerLifecycle.c.
 */

static BOOLEAN DestroyOwnedProcessAddressSpace(LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_REQUEST Request;
    LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_RESULT Result;

    if (Process == 0)
    {
        return 0;
    }
    if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) == 0U ||
        Process->AddressSpaceObjectPhysicalAddress == 0ULL)
    {
        return 1;
    }
    if (IsMemoryManagerSchedulerTransportReady() == 0U)
    {
        LosKernelTraceFail("Kernel scheduler could not destroy a process address space because the memory manager bootstrap is not ready.");
        LosKernelTraceUnsigned("Kernel scheduler process awaiting address-space destroy: ", Process->ProcessId);
        return 0;
    }

    ZeroBytes(&Request, sizeof(Request));
    ZeroBytes(&Result, sizeof(Result));
    Request.AddressSpaceObjectPhysicalAddress = Process->AddressSpaceObjectPhysicalAddress;
    LosMemoryManagerSendDestroyAddressSpace(&Request, &Result);
    if (Result.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS)
    {
        LosKernelTraceFail("Kernel scheduler could not destroy a terminated process address space.");
        LosKernelTraceUnsigned("Kernel scheduler process address-space-destroy status: ", Result.Status);
        LosKernelTraceUnsigned("Kernel scheduler process awaiting destroy completion: ", Process->ProcessId);
        LosKernelTraceHex64("Kernel scheduler address-space object awaiting destroy: ", Process->AddressSpaceObjectPhysicalAddress);
        return 0;
    }

    Process->AddressSpaceObjectPhysicalAddress = 0ULL;
    Process->AddressSpaceId = 0ULL;
    Process->RootTablePhysicalAddress = GetKernelRootTablePhysicalAddress();
    Process->Flags &= ~LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE;
    Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT;
    return 1;
}

static UINT64 ResolveProcessRootTablePhysicalAddress(UINT64 RequestedRootTablePhysicalAddress, UINT32 *ResolvedFlags)
{
    const LOS_KERNEL_SCHEDULER_PROCESS *CurrentProcess;
    UINT64 ResolvedRoot;

    ResolvedRoot = RequestedRootTablePhysicalAddress;
    if (ResolvedRoot != LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS)
    {
        return ResolvedRoot;
    }

    CurrentProcess = LosKernelSchedulerGetCurrentProcess();
    if (CurrentProcess != 0 &&
        CurrentProcess->RootTablePhysicalAddress != LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS)
    {
        ResolvedRoot = CurrentProcess->RootTablePhysicalAddress;
    }
    else
    {
        ResolvedRoot = GetKernelRootTablePhysicalAddress();
    }

    if (ResolvedFlags != 0)
    {
        *ResolvedFlags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT;
    }

    return ResolvedRoot;
}

static UINT64 GetCreatingOwnerTaskId(void)
{
    const LOS_KERNEL_SCHEDULER_TASK *CurrentTask;

    CurrentTask = LosKernelSchedulerGetCurrentTask();
    if (CurrentTask == 0)
    {
        return 0ULL;
    }

    return CurrentTask->TaskId;
}

static UINT64 GetCreatingOwnerProcessId(void)
{
    const LOS_KERNEL_SCHEDULER_TASK *CurrentTask;

    CurrentTask = LosKernelSchedulerGetCurrentTask();
    if (CurrentTask == 0)
    {
        return 0ULL;
    }

    return CurrentTask->ProcessId;
}

static UINT64 GetKernelRootTablePhysicalAddress(void)
{
    return LosX64GetCurrentPageMapLevel4PhysicalAddress();
}

static void InitializeTaskStackContext(LOS_KERNEL_SCHEDULER_TASK *Task, void *StackBase, UINT64 StackPhysicalAddress)
{
    UINT64 InitialStackPointer;

    ZeroBytes(StackBase, (UINTN)LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES);
    Task->StackPhysicalAddress = StackPhysicalAddress;
    Task->StackBaseVirtualAddress = (UINT64)(UINTN)StackBase;
    Task->StackTopVirtualAddress = Task->StackBaseVirtualAddress + LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES;
    Task->StackSizeBytes = LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES;
    Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_NONE;

    InitialStackPointer = (Task->StackTopVirtualAddress - 16ULL) & ~0xFULL;
    WriteStackReturnAddress(InitialStackPointer, (UINT64)(UINTN)LosKernelSchedulerThreadTrampoline);
    Task->ExecutionContext.StackPointer = InitialStackPointer;
    Task->ExecutionContext.Rbx = 0ULL;
    Task->ExecutionContext.Rbp = 0ULL;
    Task->ExecutionContext.R12 = 0ULL;
    Task->ExecutionContext.R13 = 0ULL;
    Task->ExecutionContext.R14 = 0ULL;
    Task->ExecutionContext.R15 = 0ULL;
    Task->ExecutionContext.Rflags = 0x202ULL;
}

static BOOLEAN AllocateBootstrapKernelThreadStack(LOS_KERNEL_SCHEDULER_TASK *Task)
{
    UINT32 Index;
    UINT64 PhysicalAddress;
    void *StackBase;

    if (Task == 0)
    {
        return 0;
    }

    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        if (LosKernelSchedulerBootstrapStackUsed[Index] == 0U)
        {
            LosKernelSchedulerBootstrapStackUsed[Index] = 1U;
            StackBase = &LosKernelSchedulerBootstrapStacks[Index][0];
            PhysicalAddress = 0ULL;
            LosX64TryTranslateKernelVirtualToPhysical((UINT64)(UINTN)StackBase, &PhysicalAddress);
            Task->BootstrapStackSlot = Index;
            InitializeTaskStackContext(Task, StackBase, PhysicalAddress);
            Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_BOOTSTRAP;
            LosKernelTraceOk("Kernel scheduler using bootstrap stack fallback.");
            return 1;
        }
    }

    return 0;
}

static BOOLEAN AllocateDirectClaimKernelThreadStack(LOS_KERNEL_SCHEDULER_TASK *Task)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;
    UINT8 *StackBase;
    UINT64 StackPhysicalAddress;

    if (Task == 0)
    {
        return 0;
    }

    State = LosKernelSchedulerState();
    if (State == 0 || State->DirectClaimStackPoolReady == 0U ||
        LosKernelSchedulerDirectClaimStackPoolBase == 0)
    {
        return 0;
    }

    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        if (LosKernelSchedulerDirectClaimStackUsed[Index] != 0U)
        {
            continue;
        }

        LosKernelSchedulerDirectClaimStackUsed[Index] = 1U;
        if (State->DirectClaimStackSlotsInUse < LOS_KERNEL_SCHEDULER_MAX_TASKS)
        {
            State->DirectClaimStackSlotsInUse += 1U;
        }
        StackBase = &((UINT8 *)LosKernelSchedulerDirectClaimStackPoolBase)[Index * LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES];
        StackPhysicalAddress = State->DirectClaimStackPoolPhysicalAddress +
            ((UINT64)Index * LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES);
        Task->BootstrapStackSlot = Index;
        InitializeTaskStackContext(Task, StackBase, StackPhysicalAddress);
        Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_DIRECT_CLAIM;
        if (State->DirectClaimStackSlotsInUse == 1U)
        {
            LosKernelTraceOk("Kernel scheduler using direct-claim stack pool.");
        }
        return 1;
    }

    return 0;
}

static BOOLEAN AllocateKernelThreadStack(LOS_KERNEL_SCHEDULER_TASK *Task)
{
    LOS_X64_CLAIM_FRAMES_REQUEST ClaimRequest;
    LOS_X64_CLAIM_FRAMES_RESULT ClaimResult;
    void *StackBase;

    if (Task == 0)
    {
        return 0;
    }

    Task->BootstrapStackSlot = LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT;
    Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_NONE;

    if (LosKernelSchedulerIsOnline() != 0U &&
        IsMemoryManagerSchedulerTransportReady() != 0U &&
        SchedulerMayUseMemoryManagerBackedThreadStacks() != 0U)
    {
        ZeroBytes(&ClaimRequest, sizeof(ClaimRequest));
        ZeroBytes(&ClaimResult, sizeof(ClaimResult));
        ClaimRequest.PageCount = LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES;
        ClaimRequest.AlignmentBytes = 4096ULL;
        ClaimRequest.Flags = LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS;
        ClaimRequest.Owner = LOS_X64_MEMORY_REGION_OWNER_CLAIMED;
        LosMemoryManagerSendClaimFrames(&ClaimRequest, &ClaimResult);
        if (ClaimResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS &&
            ClaimResult.BaseAddress != 0ULL &&
            ClaimResult.PageCount == LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES)
        {
            StackBase = LosX64GetDirectMapVirtualAddress(ClaimResult.BaseAddress, LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES);
            if (StackBase != 0)
            {
                InitializeTaskStackContext(Task, StackBase, ClaimResult.BaseAddress);
                Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_MEMORY_MANAGER;
                return 1;
            }

            {
                LOS_X64_FREE_FRAMES_REQUEST FreeRequest;
                LOS_X64_FREE_FRAMES_RESULT FreeResult;

                ZeroBytes(&FreeRequest, sizeof(FreeRequest));
                ZeroBytes(&FreeResult, sizeof(FreeResult));
                FreeRequest.PhysicalAddress = ClaimResult.BaseAddress;
                FreeRequest.PageCount = ClaimResult.PageCount;
                LosMemoryManagerSendFreeFrames(&FreeRequest, &FreeResult);
            }

            LosKernelTraceFail("Kernel scheduler could not direct-map memory-manager-backed thread stack.");
        }
        else
        {
            LosKernelTraceFail("Kernel scheduler could not allocate memory-manager-backed thread stack frames.");
            LosKernelTraceUnsigned("Kernel scheduler stack-claim status: ", ClaimResult.Status);
            LosKernelTraceUnsigned("Kernel scheduler stack-claim pages returned: ", ClaimResult.PageCount);
        }
    }

    if (AllocateDirectClaimKernelThreadStack(Task) != 0U)
    {
        return 1;
    }

    if (LosKernelSchedulerIsOnline() != 0U &&
        IsMemoryManagerSchedulerTransportReady() != 0U)
    {
        LosKernelTrace("Kernel scheduler keeping hosted transient thread stack on bootstrap fallback until AllocateFrames replies are stable.");
    }

    return AllocateBootstrapKernelThreadStack(Task);
}

static void ReleaseTaskStackResources(LOS_KERNEL_SCHEDULER_TASK *Task)
{
    if (Task == 0)
    {
        return;
    }

    if (Task->StackAllocationSource == LOS_KERNEL_SCHEDULER_STACK_SOURCE_BOOTSTRAP)
    {
        if (Task->BootstrapStackSlot != LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT &&
            Task->BootstrapStackSlot < LOS_KERNEL_SCHEDULER_MAX_TASKS)
        {
            LosKernelSchedulerBootstrapStackUsed[Task->BootstrapStackSlot] = 0U;
            Task->BootstrapStackSlot = LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT;
        }
        else
        {
            LosKernelTraceFail("Kernel scheduler lost track of a bootstrap fallback stack slot.");
        }
    }
    else if (Task->StackAllocationSource == LOS_KERNEL_SCHEDULER_STACK_SOURCE_DIRECT_CLAIM)
    {
        LOS_KERNEL_SCHEDULER_STATE *State;

        State = LosKernelSchedulerState();
        if (Task->BootstrapStackSlot != LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT &&
            Task->BootstrapStackSlot < LOS_KERNEL_SCHEDULER_MAX_TASKS)
        {
            LosKernelSchedulerDirectClaimStackUsed[Task->BootstrapStackSlot] = 0U;
            if (State != 0 && State->DirectClaimStackSlotsInUse > 0U)
            {
                State->DirectClaimStackSlotsInUse -= 1U;
            }
            Task->BootstrapStackSlot = LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT;
        }
        else
        {
            LosKernelTraceFail("Kernel scheduler lost track of a direct-claim stack-pool slot.");
            LosKernelTraceHex64("Kernel scheduler direct-claim stack base: ", Task->StackPhysicalAddress);
        }
    }
    else if (Task->StackPhysicalAddress != 0ULL && Task->StackSizeBytes != 0ULL)
    {
        if (Task->StackAllocationSource == LOS_KERNEL_SCHEDULER_STACK_SOURCE_MEMORY_MANAGER)
        {
            LOS_X64_FREE_FRAMES_REQUEST FreeRequest;
            LOS_X64_FREE_FRAMES_RESULT FreeResult;

            ZeroBytes(&FreeRequest, sizeof(FreeRequest));
            ZeroBytes(&FreeResult, sizeof(FreeResult));
            FreeRequest.PhysicalAddress = Task->StackPhysicalAddress;
            FreeRequest.PageCount = Task->StackSizeBytes / 4096ULL;

            if (IsMemoryManagerSchedulerTransportReady() != 0U)
            {
                LosMemoryManagerSendFreeFrames(&FreeRequest, &FreeResult);
                if (FreeResult.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS)
                {
                    LosKernelTraceFail("Kernel scheduler could not free terminated thread stack frames.");
                    LosKernelTraceUnsigned("Kernel scheduler stack-free status: ", FreeResult.Status);
                    LosKernelTraceHex64("Kernel scheduler stack-free base: ", FreeRequest.PhysicalAddress);
                }
            }
            else
            {
                LosKernelTraceFail("Kernel scheduler could not free terminated thread stack because the memory manager bootstrap is not ready.");
            }
        }
    }

    Task->StackPhysicalAddress = 0ULL;
    Task->StackBaseVirtualAddress = 0ULL;
    Task->StackTopVirtualAddress = 0ULL;
    Task->StackSizeBytes = 0ULL;
    Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_NONE;
    ZeroBytes(&Task->ExecutionContext, sizeof(Task->ExecutionContext));
}
