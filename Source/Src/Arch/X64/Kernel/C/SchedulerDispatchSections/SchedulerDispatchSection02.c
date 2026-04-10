/*
 * File Name: SchedulerDispatchSection02.c
 * File Version: 0.0.4
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-10T23:20:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerDispatch.c.
 */


static volatile UINT64 LosKernelSchedulerDeferredLiveUserInterruptPreemptionLogged = 0ULL;
static volatile UINT64 LosKernelSchedulerLiveUserDispatchTraceLogged = 0ULL;

static BOOLEAN IsHigherHalfKernelAddress(UINT64 Address)
{
    return Address >= 0xFFFFFFFF80000000ULL ? 1U : 0U;
}

static UINT64 ReadStackReturnAddress(UINT64 StackAddress)
{
    const UINT64 *Pointer;

    Pointer = (const UINT64 *)(UINTN)StackAddress;
    return *Pointer;
}

static BOOLEAN IsCanonicalUserAddress(UINT64 Address)
{
    return (Address != 0ULL && Address < 0x0000800000000000ULL) ? 1U : 0U;
}

static BOOLEAN IsAddressRangeWithinTaskKernelStack(const LOS_KERNEL_SCHEDULER_TASK *Task, UINT64 Address, UINT64 Size)
{
    UINT64 EndExclusive;

    if (Task == 0 ||
        Address == 0ULL ||
        Size == 0ULL ||
        Task->StackBaseVirtualAddress == 0ULL ||
        Task->StackTopVirtualAddress == 0ULL ||
        Task->StackTopVirtualAddress <= Task->StackBaseVirtualAddress)
    {
        return 0U;
    }

    EndExclusive = Address + Size;
    if (EndExclusive < Address)
    {
        return 0U;
    }

    return (Address >= Task->StackBaseVirtualAddress &&
            EndExclusive <= Task->StackTopVirtualAddress)
        ? 1U
        : 0U;
}

static void TraceUserTransitionFrameValues(const char *Prefix, const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    const LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *Frame;

    if (Task == 0 ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        IsAddressRangeWithinTaskKernelStack(Task,
                                            Task->UserTransitionFrameStackPointer,
                                            sizeof(LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME)) == 0U)
    {
        return;
    }

    Frame = (const LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *)(UINTN)Task->UserTransitionFrameStackPointer;
    if (Prefix != 0)
    {
        LosKernelTrace(Prefix);
    }
    LosKernelTraceHex64("Scheduler first-user-task frame stack pointer: ", Task->UserTransitionFrameStackPointer);
    LosKernelTraceHex64("Scheduler first-user-task frame rip: ", Frame->Rip);
    LosKernelTraceHex64("Scheduler first-user-task frame cs: ", Frame->Cs);
    LosKernelTraceHex64("Scheduler first-user-task frame rflags: ", Frame->Rflags);
    LosKernelTraceHex64("Scheduler first-user-task frame rsp: ", Frame->Rsp);
    LosKernelTraceHex64("Scheduler first-user-task frame ss: ", Frame->Ss);
}

static BOOLEAN ShouldDeferLiveUserTaskInterruptPreemption(const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    if (Task == 0)
    {
        return 0U;
    }

    return ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE) != 0U &&
            Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE &&
            Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING)
        ? 1U
        : 0U;
}

void LosKernelSchedulerPreemptIfNeededFromInterrupt(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_TASK *Task;

    State = LosKernelSchedulerState();
    if (State->Online == 0U || State->InScheduler != 0U || State->ReschedulePending == 0U)
    {
        return;
    }

    Task = GetCurrentTaskMutable();
    if (Task == 0)
    {
        return;
    }

    if (ShouldDeferLiveUserTaskInterruptPreemption(Task) != 0U)
    {
        if (LosKernelSchedulerDeferredLiveUserInterruptPreemptionLogged == 0ULL)
        {
            LosKernelSchedulerDeferredLiveUserInterruptPreemptionLogged = 1ULL;
            LosKernelTraceOk("Kernel scheduler deferred timer-driven preemption for the first live user task to preserve the iret frame.");
        }
        return;
    }

    if (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING)
    {
        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
        Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_PREEMPTED;
        Task->ReadySinceTick = State->TickCount;
        Task->PreemptionCount += 1ULL;
        State->InterruptPreemptionCount += 1ULL;
    }
    else if (Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED &&
             Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED)
    {
        return;
    }

    LosKernelSchedulerSwitchContext(&Task->ExecutionContext, &State->SchedulerContext);
}

void LosKernelSchedulerYieldCurrent(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_TASK *Task;

    State = LosKernelSchedulerState();
    Task = GetCurrentTaskMutable();
    if (Task == 0)
    {
        return;
    }

    Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
    Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_YIELD;
    Task->ReadySinceTick = State->TickCount;
    State->ReschedulePending = 1U;
    LosKernelSchedulerSwitchContext(&Task->ExecutionContext, &State->SchedulerContext);
}

void LosKernelSchedulerSleepCurrent(UINT64 TickCount)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_TASK *Task;

    State = LosKernelSchedulerState();
    Task = GetCurrentTaskMutable();
    if (Task == 0)
    {
        return;
    }

    if (TickCount == 0ULL)
    {
        LosKernelSchedulerYieldCurrent();
        return;
    }

    Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED;
    Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_WAIT_PERIOD;
    Task->NextWakeTick = State->TickCount + TickCount;
    State->ReschedulePending = 1U;
    LosKernelSchedulerSwitchContext(&Task->ExecutionContext, &State->SchedulerContext);
}

void LosKernelSchedulerTerminateCurrent(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_TASK *Task;

    State = LosKernelSchedulerState();
    Task = GetCurrentTaskMutable();
    if (Task == 0)
    {
        LosKernelTraceFail("Kernel scheduler terminate requested without a current task.");
        LosKernelHaltForever();
    }
    if ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_IDLE) != 0U)
    {
        LosKernelTraceFail("Kernel scheduler idle task attempted to terminate.");
        LosKernelHaltForever();
    }

    Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED;
    Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_TERMINATED;
    Task->ExitStatus = 0ULL;
    Task->CleanupPending = 1U;
    State->TerminatedTaskCount += 1ULL;
    State->ReschedulePending = 1U;
    LosKernelSchedulerSwitchContext(&Task->ExecutionContext, &State->SchedulerContext);
    LosKernelTraceFail("Kernel scheduler terminate path returned unexpectedly.");
    LosKernelHaltForever();
}

void LosKernelSchedulerThreadTrampoline(void)
{
    LOS_KERNEL_SCHEDULER_TASK *Task;

    Task = GetCurrentTaskMutable();
    if (Task == 0 || Task->ThreadRoutine == 0)
    {
        LosKernelTraceFail("Kernel scheduler thread trampoline has no current task.");
        LosKernelHaltForever();
    }

    Task->ThreadRoutine(Task->Context);
    LosKernelTraceFail("Kernel scheduler thread returned unexpectedly.");
    LosKernelSchedulerTerminateCurrent();
    LosKernelHaltForever();
}

UINT64 LosKernelSchedulerPrepareUserTransitionIret(void)
{
    const LOS_KERNEL_SCHEDULER_TASK *Task;
    const LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *Frame;
    UINT64 BridgeReturnSlot;
    UINT64 KernelEntryReturnSlot;

    Task = LosKernelSchedulerGetCurrentTask();
    if (Task == 0 ||
        (Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE) == 0U ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Task->ExecutionContext.StackPointer == 0ULL ||
        Task->StackTopVirtualAddress == 0ULL)
    {
        LosKernelTraceFail("Kernel scheduler could not prepare a live first-user-task iret frame.");
        LosKernelHaltForever();
    }

    if (IsAddressRangeWithinTaskKernelStack(Task,
                                            Task->UserTransitionFrameStackPointer,
                                            sizeof(LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME)) == 0U ||
        IsAddressRangeWithinTaskKernelStack(Task, Task->ExecutionContext.StackPointer, 16ULL) == 0U)
    {
        LosKernelTraceFail("Kernel scheduler rejected the live first-user-task handoff because the prepared stack slots were outside the task kernel stack.");
        LosKernelTraceHex64("Scheduler first-user-task stack base: ", Task->StackBaseVirtualAddress);
        LosKernelTraceHex64("Scheduler first-user-task stack top: ", Task->StackTopVirtualAddress);
        LosKernelTraceHex64("Scheduler first-user-task handoff stack pointer: ", Task->ExecutionContext.StackPointer);
        LosKernelTraceHex64("Scheduler first-user-task frame stack pointer: ", Task->UserTransitionFrameStackPointer);
        LosKernelHaltForever();
    }

    if (IsHigherHalfKernelAddress(Task->UserTransitionBridgeVirtualAddress) == 0U ||
        IsHigherHalfKernelAddress(Task->UserTransitionKernelEntryVirtualAddress) == 0U ||
        IsCanonicalUserAddress(Task->UserInstructionPointer) == 0U ||
        IsCanonicalUserAddress(Task->UserStackPointer) == 0U)
    {
        LosKernelTraceFail("Kernel scheduler rejected the live first-user-task handoff because one of the prepared transition addresses was invalid.");
        LosKernelTraceHex64("Scheduler first-user-task user rip: ", Task->UserInstructionPointer);
        LosKernelTraceHex64("Scheduler first-user-task user rsp: ", Task->UserStackPointer);
        LosKernelTraceHex64("Scheduler first-user-task bridge address: ", Task->UserTransitionBridgeVirtualAddress);
        LosKernelTraceHex64("Scheduler first-user-task kernel-entry address: ", Task->UserTransitionKernelEntryVirtualAddress);
        LosKernelHaltForever();
    }

    BridgeReturnSlot = ReadStackReturnAddress(Task->ExecutionContext.StackPointer);
    KernelEntryReturnSlot = ReadStackReturnAddress(Task->ExecutionContext.StackPointer + 8ULL);
    if (BridgeReturnSlot != Task->UserTransitionBridgeVirtualAddress ||
        KernelEntryReturnSlot != Task->UserTransitionKernelEntryVirtualAddress)
    {
        LosKernelTraceFail("Kernel scheduler rejected the live first-user-task handoff because the chain stack did not contain the expected bridge contract.");
        LosKernelTraceHex64("Scheduler first-user-task handoff stack pointer: ", Task->ExecutionContext.StackPointer);
        LosKernelTraceHex64("Scheduler first-user-task observed bridge return slot: ", BridgeReturnSlot);
        LosKernelTraceHex64("Scheduler first-user-task observed kernel-entry return slot: ", KernelEntryReturnSlot);
        LosKernelTraceHex64("Scheduler first-user-task expected bridge address: ", Task->UserTransitionBridgeVirtualAddress);
        LosKernelTraceHex64("Scheduler first-user-task expected kernel-entry address: ", Task->UserTransitionKernelEntryVirtualAddress);
        LosKernelHaltForever();
    }

    Frame = (const LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *)(UINTN)Task->UserTransitionFrameStackPointer;
    if (Frame == 0 ||
        Frame->Rip != Task->UserInstructionPointer ||
        Frame->Cs != Task->UserCodeSegmentSelector ||
        Frame->Rflags != Task->UserRflags ||
        Frame->Rsp != Task->UserStackPointer ||
        Frame->Ss != Task->UserStackSegmentSelector)
    {
        LosKernelTraceFail("Kernel scheduler rejected the live first-user-task handoff because the iret frame contents did not match the recorded transition state.");
        TraceUserTransitionFrameValues("Scheduler first-user-task frame mismatch diagnostics follow.", Task);
        LosKernelSchedulerTraceTask("Rejected scheduler first-user-task task", Task);
        LosKernelHaltForever();
    }

    LosKernelTraceOk("Kernel scheduler prepared the first live CPL3 iretq dispatch.");
    LosKernelTraceHex64("Scheduler first-user-task handoff stack pointer: ", Task->ExecutionContext.StackPointer);
    LosKernelTraceHex64("Scheduler first-user-task bridge return slot: ", BridgeReturnSlot);
    LosKernelTraceHex64("Scheduler first-user-task kernel-entry return slot: ", KernelEntryReturnSlot);
    LosKernelTraceHex64("Scheduler first-user-task user rip: ", Task->UserInstructionPointer);
    LosKernelTraceHex64("Scheduler first-user-task user rsp: ", Task->UserStackPointer);
    LosKernelTraceHex64("Scheduler first-user-task user cs: ", Task->UserCodeSegmentSelector);
    LosKernelTraceHex64("Scheduler first-user-task user ss: ", Task->UserStackSegmentSelector);
    TraceUserTransitionFrameValues("Scheduler first-user-task prepared frame diagnostics follow.", Task);
    LosKernelSetInterruptStackTop(Task->StackTopVirtualAddress);
    return Task->UserTransitionFrameStackPointer;
}

BOOLEAN LosKernelSchedulerHandleUserModeInterrupt(UINT64 Vector, UINT64 ErrorCode, UINT64 InstructionPointer, UINT64 StackPointer)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;

    State = LosKernelSchedulerState();
    Task = GetCurrentTaskMutable();
    if (State == 0 || Task == 0 ||
        (Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE) == 0U ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE)
    {
        return 0U;
    }

    if (Vector == LOS_X64_USER_TRANSITION_VECTOR)
    {
        Process = FindProcessByIdMutable(Task->ProcessId);
        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED;
        Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_TERMINATED;
        Task->ReadySinceTick = 0ULL;
        Task->NextWakeTick = 0ULL;
        Task->WakeDispatchPending = 0U;
        Task->ResumeBoostTicks = 0U;
        Task->RemainingQuantumTicks = 0U;
        Task->CleanupPending = 1U;
        Task->ExitStatus = Vector;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE;
        if (Process != 0)
        {
            Process->ExitStatus = Vector;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE;
        }
        State->TerminatedTaskCount += 1ULL;
        if (State->UserTransitionCompleteCount == 0ULL)
        {
            State->UserTransitionCompleteCount += 1ULL;
        }
        State->ReschedulePending = 1U;

        LosKernelTraceOk("First user task executed in ring 3 and returned through vector 128.");
        LosKernelTraceUnsigned("First user task return vector: ", Vector);
        LosKernelTraceHex64("First user task return rip: ", InstructionPointer);
        LosKernelTraceHex64("First user task return rsp: ", StackPointer);
        LosKernelTraceHex64("First user task return cs: ", Task->UserCodeSegmentSelector);
        LosKernelTraceHex64("First user task return ss: ", Task->UserStackSegmentSelector);
        LosKernelReportKnownDrives();
        if (Process != 0)
        {
            LosKernelSchedulerTraceProcess("Completed first user task process", Process);
        }
        LosKernelSchedulerTraceTask("Completed first user task task", Task);
        return 1U;
    }

    if (Vector < LOS_X64_EXCEPTION_VECTOR_COUNT)
    {
        Process = FindProcessByIdMutable(Task->ProcessId);
        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED;
        Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_TERMINATED;
        Task->ReadySinceTick = 0ULL;
        Task->NextWakeTick = 0ULL;
        Task->WakeDispatchPending = 0U;
        Task->ResumeBoostTicks = 0U;
        Task->RemainingQuantumTicks = 0U;
        Task->CleanupPending = 1U;
        Task->ExitStatus = Vector;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE;
        if (Process != 0)
        {
            Process->ExitStatus = Vector;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE;
        }
        State->TerminatedTaskCount += 1ULL;
        if (State->UserTransitionCompleteCount == 0ULL)
        {
            State->UserTransitionCompleteCount += 1ULL;
        }
        State->ReschedulePending = 1U;

        LosKernelTraceFail("First user task faulted while running in ring 3.");
        LosKernelTraceUnsigned("First user task fault vector: ", Vector);
        LosKernelTraceHex64("First user task fault error: ", ErrorCode);
        LosKernelTraceHex64("First user task fault rip: ", InstructionPointer);
        LosKernelTraceHex64("First user task fault rsp: ", StackPointer);
        LosKernelTraceHex64("First user task fault expected entry: ", Task->UserInstructionPointer);
        LosKernelTraceHex64("First user task fault expected user rsp: ", Task->UserStackPointer);
        LosKernelTraceHex64("First user task fault user cs: ", Task->UserCodeSegmentSelector);
        LosKernelTraceHex64("First user task fault user ss: ", Task->UserStackSegmentSelector);
        if (Process != 0)
        {
            LosKernelSchedulerTraceProcess("Faulted first user task process", Process);
        }
        LosKernelSchedulerTraceTask("Faulted first user task task", Task);
        return 1U;
    }

    return 0U;
}

void LosKernelSchedulerUserTransitionKernelEntry(void)
{
    const LOS_KERNEL_SCHEDULER_TASK *Task;

    Task = LosKernelSchedulerGetCurrentTask();
    if (Task != 0)
    {
        LosKernelSchedulerTraceTask("Scheduler first-user-task kernel-entry task", Task);
    }

    CompleteCurrentUserTransitionTask(LOS_X64_USER_TRANSITION_VECTOR,
                                      "First user task returned through the kernel-entry bridge after the first CPL3 dispatch.");
    LosKernelSchedulerSwitchContext(&GetCurrentTaskMutable()->ExecutionContext,
                                    &LosKernelSchedulerState()->SchedulerContext);
    LosKernelTraceFail("Kernel scheduler first-user-task kernel-entry path resumed unexpectedly.");
    LosKernelHaltForever();
}

void LosKernelSchedulerEnter(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerState();
    State->Online = 1U;
    State->InScheduler = 1U;
    LosKernelTraceOk("Kernel scheduler entered.");
    LosKernelSchedulerTraceState("Scheduler online");

    for (;;)
    {
        UINT32 SelectedIndex;
        LOS_KERNEL_SCHEDULER_TASK *Task;

        State->InScheduler = 1U;
        LosKernelSchedulerCleanupTerminatedTasks();
        LosKernelSchedulerCleanupTerminatedProcesses();
        LosKernelSchedulerBindPendingProcessAddressSpaces();
        LosKernelSchedulerWakeDueTasks();
        SelectedIndex = LosKernelSchedulerSelectNextTaskIndex();
        if (SelectedIndex == LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX)
        {
            LosKernelTraceFail("Kernel scheduler found no dispatchable task.");
            LosKernelHaltForever();
        }

        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Task = &State->Tasks[SelectedIndex];
        if ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE) != 0U &&
            Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE)
        {
            Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED;
            Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION;
            Task->ReadySinceTick = 0ULL;
            Task->WakeDispatchPending = 0U;
            Task->ResumeBoostTicks = 0U;
            State->UserTransitionDispatchSkipCount += 1ULL;
            continue;
        }
        Process = FindProcessByIdMutable(Task->ProcessId);
        LosKernelSchedulerActivateProcessAddressSpace(Process);
        State->CurrentTaskIndex = SelectedIndex;
        State->CurrentTaskId = Task->TaskId;
        State->CurrentProcessId = Task->ProcessId;
        State->LastSelectedIndex = SelectedIndex;
        State->DispatchCount += 1ULL;
        State->ReschedulePending = 0U;

        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING;
        UpdateDispatchLatencyAccounting(State, Task, Process);
        Task->ReadySinceTick = 0ULL;
        Task->DispatchCount += 1ULL;
        Task->LastRunTick = State->TickCount;
        if (Process != 0)
        {
            Process->DispatchCount += 1ULL;
            Process->LastRunTick = State->TickCount;
        }
        Task->RemainingQuantumTicks = Task->QuantumTicks == 0U ? 1U : Task->QuantumTicks;
        if (Task->WakeDispatchPending != 0U)
        {
            UINT32 ResumeQuantumTicks;

            ResumeQuantumTicks = Task->ResumeBoostTicks == 0U ? Task->RemainingQuantumTicks : Task->ResumeBoostTicks;
            if (ResumeQuantumTicks > Task->RemainingQuantumTicks)
            {
                Task->RemainingQuantumTicks = ResumeQuantumTicks;
            }
            Task->WakeDispatchPending = 0U;
            Task->ResumeBoostTicks = 0U;
            State->WakeResumeWindowDispatchCount += 1ULL;
        }
        Task->LastWakeTick = 0ULL;
        if ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE) != 0U &&
            Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE &&
            LosKernelSchedulerLiveUserDispatchTraceLogged == 0ULL)
        {
            LosKernelSchedulerLiveUserDispatchTraceLogged = 1ULL;
            LosKernelTraceOk("Kernel scheduler is dispatching the first live user task through the real bridge path.");
            LosKernelTraceHex64("Scheduler live user dispatch stack pointer: ", Task->ExecutionContext.StackPointer);
            LosKernelTraceHex64("Scheduler live user dispatch bridge return slot: ",
                                ReadStackReturnAddress(Task->ExecutionContext.StackPointer));
            LosKernelTraceHex64("Scheduler live user dispatch kernel-entry return slot: ",
                                ReadStackReturnAddress(Task->ExecutionContext.StackPointer + 8ULL));
            TraceUserTransitionFrameValues("Scheduler live user dispatch frame diagnostics follow.", Task);
        }
        State->InScheduler = 0U;
        LosKernelSchedulerSwitchContext(&State->SchedulerContext, &Task->ExecutionContext);

        State->InScheduler = 1U;
        FinalizeRunSliceAccounting(State, Task, Process);
        LosKernelSchedulerRestoreKernelAddressSpace();
        State->CurrentTaskIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
        State->CurrentTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
        State->CurrentProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    }
}
