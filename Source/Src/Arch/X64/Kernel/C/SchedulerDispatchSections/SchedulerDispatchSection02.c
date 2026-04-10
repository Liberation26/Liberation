/*
 * File Name: SchedulerDispatchSection02.c
 * File Version: 0.0.3
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-10T20:05:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerDispatch.c.
 */


static volatile UINT64 LosKernelSchedulerDeferredLiveUserInterruptPreemptionLogged = 0ULL;

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

    Task = LosKernelSchedulerGetCurrentTask();
    if (Task == 0 ||
        (Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE) == 0U ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Task->StackTopVirtualAddress == 0ULL)
    {
        LosKernelTraceFail("Kernel scheduler could not prepare a live first-user-task iret frame.");
        LosKernelHaltForever();
    }

    LosKernelTraceOk("Kernel scheduler prepared the first live CPL3 iretq dispatch.");
    LosKernelTraceHex64("Scheduler first-user-task iret frame rsp: ", Task->UserTransitionFrameStackPointer);
    LosKernelTraceHex64("Scheduler first-user-task user rip: ", Task->UserInstructionPointer);
    LosKernelTraceHex64("Scheduler first-user-task user rsp: ", Task->UserStackPointer);
    LosKernelTraceHex64("Scheduler first-user-task user cs: ", Task->UserCodeSegmentSelector);
    LosKernelTraceHex64("Scheduler first-user-task user ss: ", Task->UserStackSegmentSelector);
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
