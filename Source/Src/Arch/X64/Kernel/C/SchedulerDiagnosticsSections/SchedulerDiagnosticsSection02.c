/*
 * File Name: SchedulerDiagnosticsSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerDiagnostics.c.
 */

void LosKernelSchedulerIdleThread(void *Context)
{
    (void)Context;

    for (;;)
    {
        __asm__ __volatile__("hlt" : : : "memory");
        if (LosKernelSchedulerState()->ReschedulePending != 0U)
        {
            LosKernelSchedulerYieldCurrent();
        }
    }
}

void LosKernelSchedulerHeartbeatThread(void *Context)
{
    const LOS_KERNEL_SCHEDULER_STATE *State;

    (void)Context;
    for (;;)
    {
        State = LosKernelSchedulerGetState();
        if (State != 0 && LosKernelSchedulerShouldEmitSerial() != 0U)
        {
            LosKernelSerialWriteText("[Kernel] Scheduler heartbeat ticks=");
            LosKernelSerialWriteUnsigned(State->TickCount);
            LosKernelSerialWriteText(" dispatches=");
            LosKernelSerialWriteUnsigned(State->DispatchCount);
            LosKernelSerialWriteText(" tasks=");
            LosKernelSerialWriteUnsigned(State->TaskCount);
            LosKernelSerialWriteText(" processes=");
            LosKernelSerialWriteUnsigned(State->ProcessCount);
            LosKernelSerialWriteText(" starvation-relief=");
            LosKernelSerialWriteUnsigned(State->StarvationReliefDispatchCount);
            LosKernelSerialWriteText(" created=");
            LosKernelSerialWriteUnsigned(State->CreatedTaskCount);
            LosKernelSerialWriteText(" terminated=");
            LosKernelSerialWriteUnsigned(State->TerminatedTaskCount);
            LosKernelSerialWriteText(" reaped=");
            LosKernelSerialWriteUnsigned(State->ReapedTaskCount);
            LosKernelSerialWriteText(" proc-created=");
            LosKernelSerialWriteUnsigned(State->CreatedProcessCount);
            LosKernelSerialWriteText(" proc-terminated=");
            LosKernelSerialWriteUnsigned(State->TerminatedProcessCount);
            LosKernelSerialWriteText(" proc-reaped=");
            LosKernelSerialWriteUnsigned(State->ReapedProcessCount);
            LosKernelSerialWriteText(" root-switches=");
            LosKernelSerialWriteUnsigned(State->AddressSpaceSwitchCount);
            LosKernelSerialWriteText(" root-reuse=");
            LosKernelSerialWriteUnsigned(State->AddressSpaceReuseCount);
            LosKernelSerialWriteText(" bind-count=");
            LosKernelSerialWriteUnsigned(State->AddressSpaceBindCount);
            LosKernelSerialWriteText(" bind-deferred=");
            LosKernelSerialWriteUnsigned(State->AddressSpaceBindDeferredCount);
            LosKernelSerialWriteText(" wakeups=");
            LosKernelSerialWriteUnsigned(State->WakeupCount);
            LosKernelSerialWriteText(" wake-dispatch=");
            LosKernelSerialWriteUnsigned(State->WakePriorityDispatchCount);
            LosKernelSerialWriteText(" resume-window=");
            LosKernelSerialWriteUnsigned(State->WakeResumeWindowDispatchCount);
            LosKernelSerialWriteText(" max-ready-delay=");
            LosKernelSerialWriteUnsigned(State->MaxReadyDelayTicks);
            LosKernelSerialWriteText(" max-wake-delay=");
            LosKernelSerialWriteUnsigned(State->MaxWakeDelayTicks);
            LosKernelSerialWriteText(" max-run-slice=");
            LosKernelSerialWriteUnsigned(State->MaxRunSliceTicks);
            LosKernelSerialWriteText(" idle-ticks=");
            LosKernelSerialWriteUnsigned(State->IdleTicks);
            LosKernelSerialWriteText(" busy-ticks=");
            LosKernelSerialWriteUnsigned(State->BusyTicks);
            LosKernelSerialWriteText(" first-user-task-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldReady);
            LosKernelSerialWriteText(" first-user-task-prepared=");
            LosKernelSerialWriteUnsigned(State->UserTransitionPreparedCount);
            LosKernelSerialWriteText(" first-user-task-validated=");
            LosKernelSerialWriteUnsigned(State->UserTransitionValidatedCount);
            LosKernelSerialWriteText(" first-user-task-armed=");
            LosKernelSerialWriteUnsigned(State->UserTransitionArmedCount);
            LosKernelSerialWriteText(" first-user-task-requested=");
            LosKernelSerialWriteUnsigned(State->UserTransitionLaunchRequestCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-entry-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionEntryReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-descriptor-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionDescriptorReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-frame-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionFrameReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-trampoline-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionTrampolineReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-bridge-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionBridgeReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-chain-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionChainReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-contract-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionContractReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-seal-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionSealReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-handoff-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionHandoffReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-complete=");
            LosKernelSerialWriteUnsigned(State->UserTransitionCompleteCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-live=");
            LosKernelSerialWriteUnsigned(State->UserTransitionLiveCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" first-user-task-live-gate-closed=");
            LosKernelSerialWriteUnsigned(State->UserTransitionLiveGateClosed);
            LosKernelSerialWriteText(" first-user-task-blocked=");
            LosKernelSerialWriteUnsigned(LosKernelSchedulerIsUserTransitionScaffoldBlocked());
            LosKernelSerialWriteText(" first-user-task-reblocked=");
            LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldReblockCount);
            LosKernelSerialWriteText(" user-dispatch-skip=");
            LosKernelSerialWriteUnsigned(State->UserTransitionDispatchSkipCount);
            LosKernelSerialWriteText(" first-user-task-proc=");
            LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldProcessId);
            LosKernelSerialWriteText(" first-user-task-task=");
            LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldTaskId);
            LosKernelSerialWriteText(" stack-pool-ready=");
            LosKernelSerialWriteUnsigned(State->DirectClaimStackPoolReady);
            LosKernelSerialWriteText(" stack-pool-used=");
            LosKernelSerialWriteUnsigned(State->DirectClaimStackSlotsInUse);
            LosKernelSerialWriteText("\n");
        }

        LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_HEARTBEAT_PERIOD_TICKS);
    }
}

void LosKernelSchedulerLifecycleThread(void *Context)
{
    UINT64 Sequence;

    (void)Context;
    Sequence = 0ULL;
    if (LosKernelSchedulerShouldEmitSerial() != 0U)
    {
        LosKernelSerialWriteText("[Kernel] Scheduler lifecycle manager entered.\n");
    }

    for (;;)
    {
        UINT64 ProcessId;
        UINT64 TaskId;

        if (LosKernelSchedulerState()->UserTransitionScaffoldReady == 0U)
        {
            (void)LosKernelSchedulerPrepareUserTransitionScaffold();
        }
        else if (LosKernelSchedulerState()->UserTransitionValidatedCount == 0ULL)
        {
            (void)LosKernelSchedulerValidateUserTransitionScaffold();
        }
        else if (LosKernelSchedulerState()->UserTransitionArmedCount == 0ULL)
        {
            (void)LosKernelSchedulerArmUserTransitionScaffold();
        }
        else if (LosKernelSchedulerState()->UserTransitionLaunchRequestCount == 0ULL)
        {
            (void)LosKernelSchedulerRequestUserTransitionScaffold();
        }
        else if (LosKernelSchedulerState()->UserTransitionEntryReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldEntryReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionDescriptorReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldDescriptorReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionFrameReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldFrameReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionTrampolineReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldTrampolineReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionBridgeReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldBridgeReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionChainReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldChainReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionContractReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldContractReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionSealReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldSealReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionHandoffReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldHandoffReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionLiveCount == 0ULL &&
                 LosKernelSchedulerState()->UserTransitionHandoffReadyCount != 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldLive();
        }

        if (LosKernelSchedulerState()->UserTransitionScaffoldReady != 0U &&
            LosKernelSchedulerState()->UserTransitionLiveCount == 0ULL)
        {
            /*
             * Keep the prepared first user task blocked until the real live
             * promotion succeeds, but do not route it back through the old
             * COMPLETE/live-gate-closed scaffold stop.
             */
            (void)LosKernelSchedulerGuardUserTransitionScaffold();
            if (LosKernelSchedulerState()->UserTransitionHandoffReadyCount != 0ULL)
            {
                (void)LosKernelSchedulerMarkUserTransitionScaffoldLive();
            }
        }

        if (LosKernelSchedulerHasActiveTransientProcess() != 0U)
        {
            LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS);
            continue;
        }

        if (LosKernelSchedulerState()->UserTransitionLiveCount != 0ULL &&
            LosKernelSchedulerState()->UserTransitionCompleteCount == 0ULL)
        {
            LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS);
            continue;
        }

        if (LosKernelSchedulerState()->UserTransitionCompleteCount != 0ULL)
        {
            /*
             * The transient-process churn below is only a scheduler stress path.
             * Once the first real ring-3 proof has returned successfully, keep
             * the lifecycle manager quiet so the console does not flood and the
             * system can remain parked in the stable post-proof state.
             */
            LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS);
            continue;
        }

        Sequence += 1ULL;
        ProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
        TaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;

        if (!LosKernelSchedulerCreateProcess(
                "EphemeralProcess",
                LOS_KERNEL_SCHEDULER_PROCESS_FLAG_TRANSIENT | LOS_KERNEL_SCHEDULER_PROCESS_FLAG_REQUIRE_OWN_ADDRESS_SPACE,
                0ULL,
                LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS,
                &ProcessId))
        {
            if (LosKernelSchedulerShouldEmitSerial() != 0U)
            {
                LosKernelSerialWriteText("[Kernel] Scheduler lifecycle could not create ephemeral process sequence=");
                LosKernelSerialWriteUnsigned(Sequence);
                LosKernelSerialWriteText("\n");
            }
            LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS);
            continue;
        }

        if (LosKernelSchedulerCreateTask(
                "EphemeralWorker",
                ProcessId,
                0U,
                LOS_KERNEL_SCHEDULER_EPHEMERAL_PRIORITY,
                1U,
                0ULL,
                LosKernelSchedulerEphemeralThread,
                (void *)(UINTN)Sequence,
                &TaskId))
        {
            if (LosKernelSchedulerShouldEmitSerial() != 0U)
            {
                LosKernelSerialWriteText("[Kernel] Scheduler lifecycle spawned ephemeral process id=");
                LosKernelSerialWriteUnsigned(ProcessId);
                LosKernelSerialWriteText(" task=");
                LosKernelSerialWriteUnsigned(TaskId);
                LosKernelSerialWriteText(" sequence=");
                LosKernelSerialWriteUnsigned(Sequence);
                LosKernelSerialWriteText("\n");
            }
        }
        else
        {
            LosKernelSchedulerMarkProcessTerminated(ProcessId, 1ULL);
            if (LosKernelSchedulerShouldEmitSerial() != 0U)
            {
                LosKernelSerialWriteText("[Kernel] Scheduler lifecycle could not spawn ephemeral task for process id=");
                LosKernelSerialWriteUnsigned(ProcessId);
                LosKernelSerialWriteText(" sequence=");
                LosKernelSerialWriteUnsigned(Sequence);
                LosKernelSerialWriteText("\n");
            }
        }

        LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS);
    }
}

void LosKernelSchedulerEphemeralThread(void *Context)
{
    UINT64 Sequence;
    const LOS_KERNEL_SCHEDULER_TASK *Task;

    Sequence = (UINT64)(UINTN)Context;
    Task = LosKernelSchedulerGetCurrentTask();
    if (LosKernelSchedulerShouldEmitSerial() != 0U)
    {
        LosKernelSerialWriteText("[Kernel] Scheduler ephemeral task entered id=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
        LosKernelSerialWriteText(" owner=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->OwnerTaskId : 0ULL);
        LosKernelSerialWriteText(" process=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->ProcessId : 0ULL);
        LosKernelSerialWriteText(" sequence=");
        LosKernelSerialWriteUnsigned(Sequence);
        LosKernelSerialWriteText("\n");
    }

    LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_EPHEMERAL_LIFETIME_TICKS);

    Task = LosKernelSchedulerGetCurrentTask();
    if (LosKernelSchedulerShouldEmitSerial() != 0U)
    {
        LosKernelSerialWriteText("[Kernel] Scheduler ephemeral task resumed id=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
        LosKernelSerialWriteText(" process=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->ProcessId : 0ULL);
        LosKernelSerialWriteText(" sequence=");
        LosKernelSerialWriteUnsigned(Sequence);
        LosKernelSerialWriteText(" ticks=");
        LosKernelSerialWriteUnsigned(LosKernelSchedulerGetTickCount());
        LosKernelSerialWriteText("\n");

        LosKernelSerialWriteText("[Kernel] Scheduler ephemeral task exiting id=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
        LosKernelSerialWriteText(" process=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->ProcessId : 0ULL);
        LosKernelSerialWriteText(" sequence=");
        LosKernelSerialWriteUnsigned(Sequence);
        LosKernelSerialWriteText(" ticks=");
        LosKernelSerialWriteUnsigned(LosKernelSchedulerGetTickCount());
        LosKernelSerialWriteText("\n");
    }
    LosKernelSchedulerTerminateCurrent();
    LosKernelHaltForever();
}

void LosKernelSchedulerUserTransitionTrapThread(void *Context)
{
    const LOS_KERNEL_SCHEDULER_TASK *Task;

    (void)Context;
    Task = LosKernelSchedulerGetCurrentTask();
    LosKernelTraceFail("Scheduler first user task was dispatched before a real ring transition path existed.");
    LosKernelSerialWriteText("[Kernel] First-user-task dispatch task=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
    LosKernelSerialWriteText(" process=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->ProcessId : 0ULL);
    LosKernelSerialWriteText("\n");
    LosKernelSchedulerTerminateCurrent();
    LosKernelHaltForever();
}


void LosKernelSchedulerUserTransitionBridgeTrap(UINT64 KernelStackPointer)
{
    const LOS_KERNEL_SCHEDULER_TASK *Task;

    Task = LosKernelSchedulerGetCurrentTask();
    LosKernelTraceFail("Scheduler first user task bridge ran before the real ring-transition entry path existed.");
    LosKernelSerialWriteText("[Kernel] First-user-task bridge task=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
    LosKernelSerialWriteText(" process=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->ProcessId : 0ULL);
    LosKernelSerialWriteText(" rsp=");
    LosKernelSerialWriteHex64(KernelStackPointer);
    LosKernelSerialWriteText(" seal=");
    LosKernelSerialWriteHex64(KernelStackPointer != 0ULL ? *((const UINT64 *)(UINTN)(KernelStackPointer + 16ULL)) : 0ULL);
    LosKernelSerialWriteText(" handoff=");
    LosKernelSerialWriteHex64(Task != 0 ? Task->UserTransitionHandoffStackPointer : 0ULL);
    LosKernelSerialWriteText("\n");
    LosKernelHaltForever();
}

void LosKernelSchedulerBusyThread(void *Context)
{
    UINT64 IterationCount;
    UINT64 LastReportTick;

    (void)Context;
    IterationCount = 0ULL;
    LastReportTick = 0ULL;
    if (LosKernelSchedulerShouldEmitSerial() != 0U)
    {
        LosKernelSerialWriteText("[Kernel] Scheduler busy worker entered.\n");
    }

    for (;;)
    {
        IterationCount += 1ULL;
        __asm__ __volatile__("pause" : : : "memory");
        if ((IterationCount & 0x3FFFFULL) == 0ULL)
        {
            UINT64 TickCount;

            TickCount = LosKernelSchedulerGetTickCount();
            if ((TickCount - LastReportTick) >= LOS_KERNEL_SCHEDULER_BUSY_REPORT_PERIOD_TICKS)
            {
                if (LosKernelSchedulerShouldEmitSerial() != 0U)
                {
                    LosKernelSerialWriteText("[Kernel] Scheduler busy worker iterations=");
                    LosKernelSerialWriteUnsigned(IterationCount);
                    LosKernelSerialWriteText(" ticks=");
                    LosKernelSerialWriteUnsigned(TickCount);
                    LosKernelSerialWriteText("\n");
                }
                LastReportTick = TickCount;
            }
        }
    }
}
