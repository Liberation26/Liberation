#include "SchedulerInternal.h"
#include "MemoryManagerBootstrap.h"
#include "VirtualMemoryInternal.h"

static LOS_KERNEL_SCHEDULER_STATE LosKernelSchedulerGlobalState;
static UINT8 LosKernelSchedulerBootstrapStacks[LOS_KERNEL_SCHEDULER_MAX_TASKS][LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES] __attribute__((aligned(4096)));
static UINT8 LosKernelSchedulerBootstrapStackUsed[LOS_KERNEL_SCHEDULER_MAX_TASKS];

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

static void WriteStackReturnAddress(UINT64 StackAddress, UINT64 ReturnAddress)
{
    UINT64 *Pointer;

    Pointer = (UINT64 *)(UINTN)StackAddress;
    *Pointer = ReturnAddress;
}

static LOS_KERNEL_SCHEDULER_PROCESS *FindProcessByIdMutable(UINT64 ProcessId)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    if (ProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID)
    {
        return 0;
    }

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY)
        {
            continue;
        }
        if (Process->ProcessId == ProcessId)
        {
            return Process;
        }
    }

    return 0;
}

static const LOS_KERNEL_SCHEDULER_PROCESS *FindProcessById(UINT64 ProcessId)
{
    return (const LOS_KERNEL_SCHEDULER_PROCESS *)FindProcessByIdMutable(ProcessId);
}

BOOLEAN LosKernelSchedulerHasActiveTransientProcess(void)
{
    const LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        const LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_TRANSIENT) == 0U)
        {
            continue;
        }
        if (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY ||
            Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_INITIALIZING ||
            (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED && Process->CleanupPending != 0U))
        {
            return 1;
        }
    }

    return 0;
}


static void AbandonCreatedProcess(LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT64 CriticalSectionFlags;

    if (Process == 0)
    {
        return;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED &&
        Process->ThreadCount == 0U)
    {
        if ((Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY ||
             Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED) &&
            State->ProcessCount > 0U)
        {
            State->ProcessCount -= 1U;
        }
        ZeroBytes(Process, sizeof(*Process));
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
}

static UINT64 GetKernelRootTablePhysicalAddress(void);
static BOOLEAN BindOwnedProcessAddressSpace(LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_REQUEST Request;
    LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_RESULT Result;
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT64 CriticalSectionFlags;
    UINT64 ProcessId;

    if (Process == 0)
    {
        return 0;
    }

    State = LosKernelSchedulerState();
    CriticalSectionFlags = EnterSchedulerCriticalSection();
    if (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED ||
        Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED ||
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) != 0U ||
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) != 0U ||
        Process->AddressSpaceObjectPhysicalAddress != 0ULL)
    {
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        return 1;
    }
    if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS) != 0U)
    {
        State->AddressSpaceBindDeferredCount += 1ULL;
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        return 0;
    }

    Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS;
    ProcessId = Process->ProcessId;
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (LosIsMemoryManagerBootstrapReady() == 0U)
    {
        CriticalSectionFlags = EnterSchedulerCriticalSection();
        Process->Flags &= ~LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS;
        State->AddressSpaceBindDeferredCount += 1ULL;
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        (void)ProcessId;
        return 0;
    }

    ZeroBytes(&Request, sizeof(Request));
    ZeroBytes(&Result, sizeof(Result));
    LosMemoryManagerSendCreateAddressSpace(&Request, &Result);
    if (Result.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS ||
        Result.AddressSpaceObjectPhysicalAddress == 0ULL ||
        Result.RootTablePhysicalAddress == 0ULL ||
        Result.AddressSpaceId == 0ULL)
    {
        CriticalSectionFlags = EnterSchedulerCriticalSection();
        Process->Flags &= ~LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS;
        State->AddressSpaceBindDeferredCount += 1ULL;
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        LosKernelTraceFail("Kernel scheduler could not bind a distinct process address space.");
        LosKernelTraceUnsigned("Kernel scheduler process address-space-create status: ", Result.Status);
        LosKernelTraceUnsigned("Kernel scheduler process id awaiting root binding: ", ProcessId);
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    if (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED)
    {
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        return 0;
    }
    Process->AddressSpaceId = Result.AddressSpaceId;
    Process->AddressSpaceObjectPhysicalAddress = Result.AddressSpaceObjectPhysicalAddress;
    Process->RootTablePhysicalAddress = Result.RootTablePhysicalAddress;
    Process->Flags &= ~(LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT | LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS);
    Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE;
    State->AddressSpaceBindCount += 1ULL;
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
    return 1;
}

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
    if (LosIsMemoryManagerBootstrapReady() == 0U)
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
            LosKernelTraceOk("Kernel scheduler using bootstrap stack fallback.");
            return 1;
        }
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
    ZeroBytes(&ClaimRequest, sizeof(ClaimRequest));
    ZeroBytes(&ClaimResult, sizeof(ClaimResult));
    ClaimRequest.PageCount = LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES;
    ClaimRequest.AlignmentBytes = 4096ULL;
    ClaimRequest.Flags = LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS;
    ClaimRequest.Owner = LOS_X64_MEMORY_REGION_OWNER_CLAIMED;
    LosX64ClaimFrames(&ClaimRequest, &ClaimResult);
    if (ClaimResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS &&
        ClaimResult.BaseAddress != 0ULL &&
        ClaimResult.PageCount == LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES)
    {
        StackBase = LosX64GetDirectMapVirtualAddress(ClaimResult.BaseAddress, LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES);
        if (StackBase != 0)
        {
            InitializeTaskStackContext(Task, StackBase, ClaimResult.BaseAddress);
            return 1;
        }

        LosKernelTraceFail("Kernel scheduler could not direct-map claimed thread stack.");
    }
    else
    {
        LosKernelTraceFail("Kernel scheduler could not allocate thread stack frames.");
        LosKernelTraceUnsigned("Kernel scheduler stack-claim status: ", ClaimResult.Status);
        LosKernelTraceUnsigned("Kernel scheduler stack-claim pages returned: ", ClaimResult.PageCount);
    }

    return AllocateBootstrapKernelThreadStack(Task);
}

static void ReleaseTaskStackResources(LOS_KERNEL_SCHEDULER_TASK *Task)
{
    if (Task == 0)
    {
        return;
    }

    if (Task->BootstrapStackSlot != LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT &&
        Task->BootstrapStackSlot < LOS_KERNEL_SCHEDULER_MAX_TASKS)
    {
        LosKernelSchedulerBootstrapStackUsed[Task->BootstrapStackSlot] = 0U;
        Task->BootstrapStackSlot = LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT;
    }
    else if (Task->StackPhysicalAddress != 0ULL && Task->StackSizeBytes != 0ULL)
    {
        LOS_X64_FREE_FRAMES_REQUEST FreeRequest;
        LOS_X64_FREE_FRAMES_RESULT FreeResult;

        ZeroBytes(&FreeRequest, sizeof(FreeRequest));
        ZeroBytes(&FreeResult, sizeof(FreeResult));
        FreeRequest.PhysicalAddress = Task->StackPhysicalAddress;
        FreeRequest.PageCount = Task->StackSizeBytes / 4096ULL;

        if (LosIsMemoryManagerBootstrapReady() != 0U)
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

    Task->StackPhysicalAddress = 0ULL;
    Task->StackBaseVirtualAddress = 0ULL;
    Task->StackTopVirtualAddress = 0ULL;
    Task->StackSizeBytes = 0ULL;
    ZeroBytes(&Task->ExecutionContext, sizeof(Task->ExecutionContext));
}


void LosKernelSchedulerBindPendingProcessAddressSpaces(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY)
        {
            continue;
        }
        if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) != 0U)
        {
            continue;
        }
        if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) != 0U)
        {
            continue;
        }
        if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT) == 0U)
        {
            continue;
        }
        if (Process->AddressSpaceId != 0ULL || Process->AddressSpaceObjectPhysicalAddress != 0ULL)
        {
            continue;
        }

        (void)BindOwnedProcessAddressSpace(Process);
    }
}

BOOLEAN LosKernelSchedulerCreateProcess(
    const char *Name,
    UINT32 Flags,
    UINT64 AddressSpaceId,
    UINT64 RootTablePhysicalAddress,
    UINT64 *ProcessId)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *CreatedProcess;
    UINT32 Index;
    UINT64 LocalProcessId;
    UINT64 CriticalSectionFlags;
    BOOLEAN NeedsOwnedAddressSpace;

    if (ProcessId != 0)
    {
        *ProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    }

    NeedsOwnedAddressSpace = ((Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) == 0U &&
                              RootTablePhysicalAddress == LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS &&
                              AddressSpaceId == 0ULL) ? 1U : 0U;
    if (NeedsOwnedAddressSpace != 0U &&
        (Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_REQUIRE_OWN_ADDRESS_SPACE) != 0U &&
        LosIsMemoryManagerBootstrapReady() == 0U)
    {
        State = LosKernelSchedulerState();
        CriticalSectionFlags = EnterSchedulerCriticalSection();
        State->AddressSpaceBindDeferredCount += 1ULL;
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        LosKernelTraceFail("Kernel scheduler rejected process creation because the memory manager bootstrap is not ready for a distinct process root.");
        return 0;
    }

    CreatedProcess = 0;
    LocalProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();

    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED)
        {
            continue;
        }

        ZeroBytes(Process, sizeof(*Process));
        Process->Signature = LOS_KERNEL_SCHEDULER_SIGNATURE;
        Process->Version = LOS_KERNEL_SCHEDULER_VERSION;
        Process->State = NeedsOwnedAddressSpace != 0U
            ? LOS_KERNEL_SCHEDULER_PROCESS_STATE_INITIALIZING
            : LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY;
        Process->ProcessId = State->NextProcessId;
        Process->OwnerProcessId = GetCreatingOwnerProcessId();
        Process->Generation = State->CreatedProcessCount + 1ULL;
        Process->Name = Name;
        Process->Flags = Flags;
        if (NeedsOwnedAddressSpace != 0U)
        {
            Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_CREATE_IN_PROGRESS;
        }
        Process->RootTablePhysicalAddress = ResolveProcessRootTablePhysicalAddress(RootTablePhysicalAddress, &Process->Flags);
        Process->ThreadCount = 0U;
        Process->AddressSpaceId = AddressSpaceId;
        Process->AddressSpaceObjectPhysicalAddress = 0ULL;
        Process->CreatedTick = State->TickCount;
        Process->TerminatedTick = 0ULL;
        Process->ExitStatus = 0ULL;
        Process->CleanupPending = 0U;
        Process->Reserved0 = 0U;

        State->NextProcessId += 1ULL;
        State->CreatedProcessCount += 1ULL;
        if (NeedsOwnedAddressSpace == 0U)
        {
            State->ProcessCount += 1U;
        }
        LocalProcessId = Process->ProcessId;
        CreatedProcess = Process;
        break;
    }

    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (CreatedProcess != 0)
    {
        if (NeedsOwnedAddressSpace != 0U)
        {
            if (!BindOwnedProcessAddressSpace(CreatedProcess))
            {
                if ((Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_REQUIRE_OWN_ADDRESS_SPACE) != 0U)
                {
                    LosKernelTraceFail("Kernel scheduler rejected process creation because a distinct address space was required but not available.");
                    LosKernelTraceUnsigned("Kernel scheduler rejected process id: ", LocalProcessId);
                }
                AbandonCreatedProcess(CreatedProcess);
                return 0;
            }

            CriticalSectionFlags = EnterSchedulerCriticalSection();
            if (CreatedProcess->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_INITIALIZING)
            {
                CreatedProcess->State = LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY;
                CreatedProcess->Flags &= ~LOS_KERNEL_SCHEDULER_PROCESS_FLAG_CREATE_IN_PROGRESS;
                State = LosKernelSchedulerState();
                State->ProcessCount += 1U;
            }
            LeaveSchedulerCriticalSection(CriticalSectionFlags);
        }

        if (ProcessId != 0)
        {
            *ProcessId = LocalProcessId;
        }
        LosKernelSchedulerTraceProcess("Registered scheduler process", CreatedProcess);
        return 1;
    }

    return 0;
}

BOOLEAN LosKernelSchedulerMarkProcessTerminated(
    UINT64 ProcessId,
    UINT64 ExitStatus)
{
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    UINT64 CriticalSectionFlags;
    BOOLEAN Success;

    Success = 0;
    if (ProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    Process = FindProcessByIdMutable(ProcessId);
    if (Process != 0 &&
        Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED &&
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) == 0U)
    {
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED)
        {
            LosKernelSchedulerState()->TerminatedProcessCount += 1ULL;
        }
        Process->State = LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED;
        Process->TerminatedTick = LosKernelSchedulerState()->TickCount;
        Process->ExitStatus = ExitStatus;
        Process->CleanupPending = 1U;
        Success = 1;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    return Success;
}

BOOLEAN LosKernelSchedulerCreateTask(
    const char *Name,
    UINT64 ProcessId,
    UINT32 Flags,
    UINT32 Priority,
    UINT32 QuantumTicks,
    UINT64 WakePeriodTicks,
    LOS_KERNEL_SCHEDULER_THREAD_ROUTINE ThreadRoutine,
    void *Context,
    UINT64 *TaskId)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    UINT32 Index;
    LOS_KERNEL_SCHEDULER_TASK *CreatedTask;
    UINT64 CriticalSectionFlags;
    UINT64 LocalTaskId;

    if (TaskId != 0)
    {
        *TaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    }
    if (ThreadRoutine == 0)
    {
        return 0;
    }

    CreatedTask = 0;
    LocalTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(ProcessId);
    if (Process == 0 || Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY)
    {
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        return 0;
    }

    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        if (State->Tasks[Index].State == LOS_KERNEL_SCHEDULER_TASK_STATE_UNUSED)
        {
            LOS_KERNEL_SCHEDULER_TASK *Task;

            Task = &State->Tasks[Index];
            ZeroBytes(Task, sizeof(*Task));
            Task->Signature = LOS_KERNEL_SCHEDULER_SIGNATURE;
            Task->Version = LOS_KERNEL_SCHEDULER_VERSION;
            Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
            Task->TaskId = State->NextTaskId;
            Task->OwnerTaskId = GetCreatingOwnerTaskId();
            Task->ProcessId = ProcessId;
            Task->Generation = State->CreatedTaskCount + 1ULL;
            Task->Name = Name;
            Task->Flags = Flags;
            Task->Priority = Priority;
            Task->QuantumTicks = QuantumTicks == 0U ? LOS_KERNEL_SCHEDULER_DEFAULT_QUANTUM_TICKS : QuantumTicks;
            Task->RemainingQuantumTicks = Task->QuantumTicks;
            Task->WakePeriodTicks = WakePeriodTicks;
            Task->NextWakeTick = 0ULL;
            Task->DispatchCount = 0ULL;
            Task->TotalTicks = 0ULL;
            Task->LastRunTick = 0ULL;
            Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_NONE;
            Task->LastWakeTick = 0ULL;
            Task->ReadySinceTick = State->TickCount;
            Task->PreemptionCount = 0ULL;
            Task->ExitStatus = 0ULL;
            Task->CleanupPending = 0U;
            Task->BootstrapStackSlot = LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT;
            Task->ThreadRoutine = ThreadRoutine;
            Task->Context = Context;

            if (!AllocateKernelThreadStack(Task))
            {
                ZeroBytes(Task, sizeof(*Task));
                break;
            }

            Process->ThreadCount += 1U;
            State->TaskCount += 1U;
            State->NextTaskId += 1ULL;
            State->CreatedTaskCount += 1ULL;
            LocalTaskId = Task->TaskId;
            CreatedTask = Task;
            break;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (CreatedTask != 0)
    {
        if (TaskId != 0)
        {
            *TaskId = LocalTaskId;
        }
        LosKernelSchedulerTraceTask("Registered scheduler task", CreatedTask);
        return 1;
    }

    return 0;
}

void LosKernelSchedulerCleanupTerminatedTasks(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;
    UINT64 CriticalSectionFlags;

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        LOS_KERNEL_SCHEDULER_TASK *Task;
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Task = &State->Tasks[Index];
        if (Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED || Task->CleanupPending == 0U)
        {
            continue;
        }

        LosKernelSchedulerTraceTask("Reclaimed scheduler task", Task);
        Process = FindProcessByIdMutable(Task->ProcessId);
        if (Process != 0 && Process->ThreadCount > 0U)
        {
            Process->ThreadCount -= 1U;
            if (Process->ThreadCount == 0U && (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) == 0U)
            {
                if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED)
                {
                    State->TerminatedProcessCount += 1ULL;
                }
                Process->State = LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED;
                Process->TerminatedTick = State->TickCount;
                Process->ExitStatus = Task->ExitStatus;
                Process->CleanupPending = 1U;
            }
        }

        ReleaseTaskStackResources(Task);
        if (State->TaskCount > 0U)
        {
            State->TaskCount -= 1U;
        }
        State->ReapedTaskCount += 1ULL;
        ZeroBytes(Task, sizeof(*Task));
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
}

void LosKernelSchedulerCleanupTerminatedProcesses(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;
    UINT64 CriticalSectionFlags;

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED ||
            Process->CleanupPending == 0U ||
            Process->ThreadCount != 0U)
        {
            continue;
        }

        if (!DestroyOwnedProcessAddressSpace(Process))
        {
            continue;
        }

        LosKernelSchedulerTraceProcess("Reclaimed scheduler process", Process);
        if (State->ProcessCount > 0U)
        {
            State->ProcessCount -= 1U;
        }
        State->ReapedProcessCount += 1ULL;
        ZeroBytes(Process, sizeof(*Process));
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
}

void LosKernelSchedulerInitialize(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT64 ProcessId;
    UINT64 TaskId;

    State = LosKernelSchedulerState();
    ZeroBytes(State, sizeof(*State));
    ZeroBytes(&LosKernelSchedulerBootstrapStackUsed[0], sizeof(LosKernelSchedulerBootstrapStackUsed));
    State->Signature = LOS_KERNEL_SCHEDULER_SIGNATURE;
    State->Version = LOS_KERNEL_SCHEDULER_VERSION;
    State->Online = 0U;
    State->TickCount = 0ULL;
    State->DispatchCount = 0ULL;
    State->CurrentTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    State->CurrentProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    State->NextTaskId = 1ULL;
    State->NextProcessId = 1ULL;
    State->KernelProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    State->TaskCount = 0U;
    State->ProcessCount = 0U;
    State->CurrentTaskIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
    State->LastSelectedIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
    State->ReschedulePending = 0U;
    State->InScheduler = 0U;
    State->Reserved0 = 0U;
    State->InterruptPreemptionCount = 0ULL;
    State->StarvationReliefDispatchCount = 0ULL;
    State->CreatedTaskCount = 0ULL;
    State->TerminatedTaskCount = 0ULL;
    State->ReapedTaskCount = 0ULL;
    State->CreatedProcessCount = 0ULL;
    State->TerminatedProcessCount = 0ULL;
    State->ReapedProcessCount = 0ULL;
    State->ActiveRootTablePhysicalAddress = GetKernelRootTablePhysicalAddress();
    State->AddressSpaceSwitchCount = 0ULL;
    State->AddressSpaceReuseCount = 0ULL;
    State->AddressSpaceBindCount = 0ULL;
    State->AddressSpaceBindDeferredCount = 0ULL;
    State->WakeupCount = 0ULL;
    State->WakePriorityDispatchCount = 0ULL;
    ZeroBytes(&State->SchedulerContext, sizeof(State->SchedulerContext));
    State->SchedulerContext.Rflags = 0x202ULL;

    if (!LosKernelSchedulerCreateProcess(
            "KernelProcess",
            LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL,
            0ULL,
            GetKernelRootTablePhysicalAddress(),
            &ProcessId))
    {
        LosKernelTraceFail("Kernel scheduler could not create kernel process.");
        LosKernelHaltForever();
    }
    State->KernelProcessId = ProcessId;

    if (!LosKernelSchedulerCreateTask(
            "Idle",
            State->KernelProcessId,
            LOS_KERNEL_SCHEDULER_TASK_FLAG_IDLE,
            LOS_KERNEL_SCHEDULER_IDLE_PRIORITY,
            1U,
            0ULL,
            LosKernelSchedulerIdleThread,
            0,
            &TaskId))
    {
        LosKernelTraceFail("Kernel scheduler could not create idle task.");
        LosKernelHaltForever();
    }

    LosKernelTraceOk("Kernel scheduler initialized.");
}

void LosKernelSchedulerRegisterBootstrapTasks(void)
{
    UINT64 TaskId;
    UINT64 KernelProcessId;

    KernelProcessId = LosKernelSchedulerState()->KernelProcessId;
    if (!LosKernelSchedulerCreateTask(
            "Heartbeat",
            KernelProcessId,
            LOS_KERNEL_SCHEDULER_TASK_FLAG_PERIODIC,
            LOS_KERNEL_SCHEDULER_HEARTBEAT_PRIORITY,
            1U,
            LOS_KERNEL_SCHEDULER_HEARTBEAT_PERIOD_TICKS,
            LosKernelSchedulerHeartbeatThread,
            0,
            &TaskId))
    {
        LosKernelTraceFail("Kernel scheduler could not create heartbeat task.");
        LosKernelHaltForever();
    }

    if (!LosKernelSchedulerCreateTask(
            "LifecycleManager",
            KernelProcessId,
            LOS_KERNEL_SCHEDULER_TASK_FLAG_PERIODIC,
            LOS_KERNEL_SCHEDULER_LIFECYCLE_PRIORITY,
            1U,
            LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS,
            LosKernelSchedulerLifecycleThread,
            0,
            &TaskId))
    {
        LosKernelTraceFail("Kernel scheduler could not create lifecycle manager task.");
        LosKernelHaltForever();
    }

    if (!LosKernelSchedulerCreateTask(
            "BusyWorker",
            KernelProcessId,
            0U,
            LOS_KERNEL_SCHEDULER_BUSY_PRIORITY,
            1U,
            0ULL,
            LosKernelSchedulerBusyThread,
            0,
            &TaskId))
    {
        LosKernelTraceFail("Kernel scheduler could not create busy worker task.");
        LosKernelHaltForever();
    }

    LosKernelTraceOk("Kernel scheduler bootstrap tasks registered.");
}

BOOLEAN LosKernelSchedulerIsOnline(void)
{
    return LosKernelSchedulerState()->Online != 0U ? 1U : 0U;
}

UINT64 LosKernelSchedulerGetTickCount(void)
{
    return LosKernelSchedulerState()->TickCount;
}

const LOS_KERNEL_SCHEDULER_TASK *LosKernelSchedulerGetCurrentTask(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerState();
    if (State->CurrentTaskIndex == LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX ||
        State->CurrentTaskIndex >= LOS_KERNEL_SCHEDULER_MAX_TASKS)
    {
        return 0;
    }

    return &State->Tasks[State->CurrentTaskIndex];
}

const LOS_KERNEL_SCHEDULER_PROCESS *LosKernelSchedulerGetCurrentProcess(void)
{
    const LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerGetState();
    if (State == 0 || State->CurrentProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID)
    {
        return 0;
    }

    return FindProcessById(State->CurrentProcessId);
}

const LOS_KERNEL_SCHEDULER_STATE *LosKernelSchedulerGetState(void)
{
    return LosKernelSchedulerState();
}
