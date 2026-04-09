/*
 * File Name: SchedulerLifecycleSection10.c
 * File Version: 0.0.3
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T21:55:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerLifecycle.c.
 */

BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldLiveGateClosed(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    /*
     * Compatibility shim for older lifecycle callers: the scaffold should
     * be promoted to LIVE and dispatched through the real iretq bridge,
     * not parked behind the old non-live gate.
     */
    if (State->UserTransitionLiveCount == 0ULL &&
        State->UserTransitionHandoffReadyCount != 0ULL)
    {
        if (State->UserTransitionLiveGateClosed != 0U)
        {
            State->UserTransitionLiveGateClosed = 0U;
        }
        return LosKernelSchedulerMarkUserTransitionScaffoldLive();
    }

    return 0;
}


BOOLEAN LosKernelSchedulerIsUserTransitionScaffoldBlocked(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_TASK *Task;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0U;
    }

    if (State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0U;
    }

    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Task == 0)
    {
        return 0U;
    }

    return (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
            Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
        ? 1U
        : 0U;
}

BOOLEAN LosKernelSchedulerGuardUserTransitionScaffold(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN Reblocked;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0U;
    }

    if (State->UserTransitionLiveCount != 0ULL)
    {
        return 0U;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0U;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0U;
    }

    if (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING)
    {
        LosKernelTraceFail("Scheduler first user task reached RUNNING before the live handoff existed.");
        LosKernelSchedulerTraceProcess("Invalid running scheduler first user-task process", Process);
        LosKernelSchedulerTraceTask("Invalid running scheduler first user-task task", Task);
        LosKernelHaltForever();
    }

    if (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        return 1U;
    }

    if (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED ||
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_UNUSED)
    {
        return 0U;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Reblocked = 0U;
    if (Process != 0 && Task != 0 &&
        State->UserTransitionLiveCount == 0ULL &&
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING &&
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED &&
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_UNUSED &&
        (Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
         Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION))
    {
        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED;
        Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION;
        Task->ReadySinceTick = 0ULL;
        Task->NextWakeTick = 0ULL;
        Task->WakeDispatchPending = 0U;
        Task->ResumeBoostTicks = 0U;
        Task->RemainingQuantumTicks = Task->QuantumTicks;
        State->UserTransitionScaffoldReblockCount += 1ULL;
        Reblocked = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Reblocked != 0U)
    {
        LosKernelTraceOk("Scheduler first user-task guard re-blocked a non-live task before dispatch.");
        LosKernelSchedulerTraceProcess("Guarded scheduler first user-task process", Process);
        LosKernelSchedulerTraceTask("Guarded scheduler first user-task task", Task);
    }

    return 1U;
}


void LosKernelSchedulerInitialize(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT64 ProcessId;
    UINT64 TaskId;

    State = LosKernelSchedulerState();
    ZeroBytes(State, sizeof(*State));
    ZeroBytes(&LosKernelSchedulerBootstrapStackUsed[0], sizeof(LosKernelSchedulerBootstrapStackUsed));
    ZeroBytes(&LosKernelSchedulerDirectClaimStackUsed[0], sizeof(LosKernelSchedulerDirectClaimStackUsed));
    LosKernelSchedulerDirectClaimStackPoolBase = 0;
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
    State->WakeResumeWindowDispatchCount = 0ULL;
    State->MaxReadyDelayTicks = 0ULL;
    State->MaxWakeDelayTicks = 0ULL;
    State->MaxRunSliceTicks = 0ULL;
    State->IdleTicks = 0ULL;
    State->BusyTicks = 0ULL;
    State->UserTransitionPreparedCount = 0ULL;
    State->UserTransitionValidatedCount = 0ULL;
    State->UserTransitionArmedCount = 0ULL;
    State->UserTransitionLaunchRequestCount = 0ULL;
    State->UserTransitionEntryReadyCount = 0ULL;
    State->UserTransitionDescriptorReadyCount = 0ULL;
    State->UserTransitionFrameReadyCount = 0ULL;
    State->UserTransitionTrampolineReadyCount = 0ULL;
    State->UserTransitionBridgeReadyCount = 0ULL;
    State->UserTransitionChainReadyCount = 0ULL;
    State->UserTransitionContractReadyCount = 0ULL;
    State->UserTransitionSealReadyCount = 0ULL;
    State->UserTransitionHandoffReadyCount = 0ULL;
    State->UserTransitionCompleteCount = 0ULL;
    State->UserTransitionLiveCount = 0ULL;
    State->UserTransitionDispatchSkipCount = 0ULL;
    State->UserTransitionScaffoldReblockCount = 0ULL;
    State->UserTransitionScaffoldProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    State->UserTransitionScaffoldTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    State->DirectClaimStackPoolPhysicalAddress = 0ULL;
    State->DirectClaimStackPoolBytes = 0ULL;
    State->DirectClaimStackPoolReady = 0U;
    State->DirectClaimStackSlotsInUse = 0U;
    State->UserTransitionScaffoldReady = 0U;
    State->UserTransitionLiveGateClosed = 0U;
    ZeroBytes(&State->SchedulerContext, sizeof(State->SchedulerContext));
    State->SchedulerContext.Rflags = 0x202ULL;

    (void)ReserveDirectClaimKernelThreadStackPool();

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
    UINT64 LifecycleTaskId;
    UINT64 KernelProcessId;

    TaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    LifecycleTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
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
            "Lifecycle",
            KernelProcessId,
            LOS_KERNEL_SCHEDULER_TASK_FLAG_PERIODIC,
            LOS_KERNEL_SCHEDULER_LIFECYCLE_PRIORITY,
            1U,
            LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS,
            LosKernelSchedulerLifecycleThread,
            0,
            &LifecycleTaskId))
    {
        LosKernelTraceFail("Kernel scheduler could not create lifecycle task.");
        LosKernelHaltForever();
    }

    LosKernelTraceOk("Kernel scheduler bootstrap heartbeat registered.");
    LosKernelTraceOk("Kernel scheduler bootstrap lifecycle registered; the first user shell will launch after timer proof.");
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
