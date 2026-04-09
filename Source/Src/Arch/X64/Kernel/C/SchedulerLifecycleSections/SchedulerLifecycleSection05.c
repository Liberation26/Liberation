/*
 * File Name: SchedulerLifecycleSection05.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerLifecycle.c.
 */

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
        Process->DispatchCount = 0ULL;
        Process->TotalTicks = 0ULL;
        Process->LastRunTick = 0ULL;
        Process->UserEntryVirtualAddress = 0ULL;
        Process->UserStackTopVirtualAddress = 0ULL;
        Process->UserCodeSegmentSelector = 0ULL;
        Process->UserStackSegmentSelector = 0ULL;
        Process->UserRflags = 0ULL;
        Process->UserTransitionFrameStackPointer = 0ULL;
        Process->UserTransitionKernelEntryVirtualAddress = 0ULL;
        Process->UserTransitionBridgeVirtualAddress = 0ULL;
        Process->UserTransitionChainStackPointer = 0ULL;
        Process->ExitStatus = 0ULL;
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_NONE;
        Process->CleanupPending = 0U;
        Process->Reserved0 = 0U;
        Process->Reserved1 = 0U;

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
            Task->UserInstructionPointer = 0ULL;
            Task->UserStackPointer = 0ULL;
            Task->UserCodeSegmentSelector = 0ULL;
            Task->UserStackSegmentSelector = 0ULL;
            Task->UserRflags = 0ULL;
            Task->UserTransitionFrameStackPointer = 0ULL;
            Task->UserTransitionKernelEntryVirtualAddress = 0ULL;
            Task->UserTransitionBridgeVirtualAddress = 0ULL;
            Task->UserTransitionChainStackPointer = 0ULL;
            Task->PreemptionCount = 0ULL;
            Task->ExitStatus = 0ULL;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_NONE;
            Task->CleanupPending = 0U;
            Task->BootstrapStackSlot = LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT;
            Task->WakeDispatchPending = 0U;
            Task->ResumeBoostTicks = 0U;
            Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_NONE;
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
