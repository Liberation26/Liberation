#include "SchedulerInternal.h"

static void WriteName(const char *Name)
{
    LosKernelSerialWriteText(Name != 0 ? Name : "<unnamed>");
}

void LosKernelSchedulerTraceProcess(const char *Prefix, const LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    if (Process == 0)
    {
        return;
    }

    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Prefix != 0 ? Prefix : "Scheduler process");
    LosKernelSerialWriteText(": id=");
    LosKernelSerialWriteUnsigned(Process->ProcessId);
    LosKernelSerialWriteText(" owner=");
    LosKernelSerialWriteUnsigned(Process->OwnerProcessId);
    LosKernelSerialWriteText(" generation=");
    LosKernelSerialWriteUnsigned(Process->Generation);
    LosKernelSerialWriteText(" name=");
    WriteName(Process->Name);
    LosKernelSerialWriteText(" state=");
    LosKernelSerialWriteUnsigned(Process->State);
    LosKernelSerialWriteText(" flags=");
    LosKernelSerialWriteUnsigned(Process->Flags);
    LosKernelSerialWriteText(" threads=");
    LosKernelSerialWriteUnsigned(Process->ThreadCount);
    LosKernelSerialWriteText(" address-space=");
    LosKernelSerialWriteUnsigned(Process->AddressSpaceId);
    LosKernelSerialWriteText(" address-space-object=");
    LosKernelSerialWriteHex64(Process->AddressSpaceObjectPhysicalAddress);
    LosKernelSerialWriteText(" root=");
    LosKernelSerialWriteHex64(Process->RootTablePhysicalAddress);
    LosKernelSerialWriteText(" inherited-root=");
    LosKernelSerialWriteUnsigned((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT) != 0U ? 1U : 0U);
    LosKernelSerialWriteText(" owns-address-space=");
    LosKernelSerialWriteUnsigned((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) != 0U ? 1U : 0U);
    LosKernelSerialWriteText(" bind-in-progress=");
    LosKernelSerialWriteUnsigned((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS) != 0U ? 1U : 0U);
    LosKernelSerialWriteText(" create-in-progress=");
    LosKernelSerialWriteUnsigned((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_CREATE_IN_PROGRESS) != 0U ? 1U : 0U);
    LosKernelSerialWriteText(" cleanup=");
    LosKernelSerialWriteUnsigned(Process->CleanupPending);
    LosKernelSerialWriteText(" exit=");
    LosKernelSerialWriteUnsigned(Process->ExitStatus);
    LosKernelSerialWriteText("\n");
}

void LosKernelSchedulerTraceTask(const char *Prefix, const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    if (Task == 0)
    {
        return;
    }

    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Prefix != 0 ? Prefix : "Scheduler task");
    LosKernelSerialWriteText(": id=");
    LosKernelSerialWriteUnsigned(Task->TaskId);
    LosKernelSerialWriteText(" owner=");
    LosKernelSerialWriteUnsigned(Task->OwnerTaskId);
    LosKernelSerialWriteText(" process=");
    LosKernelSerialWriteUnsigned(Task->ProcessId);
    LosKernelSerialWriteText(" generation=");
    LosKernelSerialWriteUnsigned(Task->Generation);
    LosKernelSerialWriteText(" name=");
    WriteName(Task->Name);
    LosKernelSerialWriteText(" state=");
    LosKernelSerialWriteUnsigned(Task->State);
    LosKernelSerialWriteText(" priority=");
    LosKernelSerialWriteUnsigned(Task->Priority);
    LosKernelSerialWriteText(" flags=");
    LosKernelSerialWriteUnsigned(Task->Flags);
    LosKernelSerialWriteText(" stack-base=");
    LosKernelSerialWriteHex64(Task->StackBaseVirtualAddress);
    LosKernelSerialWriteText(" stack-top=");
    LosKernelSerialWriteHex64(Task->StackTopVirtualAddress);
    LosKernelSerialWriteText(" ready-since=");
    LosKernelSerialWriteUnsigned(Task->ReadySinceTick);
    LosKernelSerialWriteText(" last-wake=");
    LosKernelSerialWriteUnsigned(Task->LastWakeTick);
    LosKernelSerialWriteText(" preemptions=");
    LosKernelSerialWriteUnsigned(Task->PreemptionCount);
    LosKernelSerialWriteText(" wake-pending=");
    LosKernelSerialWriteUnsigned(Task->WakeDispatchPending);
    LosKernelSerialWriteText(" resume-boost=");
    LosKernelSerialWriteUnsigned(Task->ResumeBoostTicks);
    LosKernelSerialWriteText(" cleanup=");
    LosKernelSerialWriteUnsigned(Task->CleanupPending);
    LosKernelSerialWriteText(" exit=");
    LosKernelSerialWriteUnsigned(Task->ExitStatus);
    LosKernelSerialWriteText("\n");
}

void LosKernelSchedulerTraceState(const char *Prefix)
{
    const LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerGetState();
    if (State == 0)
    {
        return;
    }

    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Prefix != 0 ? Prefix : "Scheduler state");
    LosKernelSerialWriteText(": ticks=");
    LosKernelSerialWriteUnsigned(State->TickCount);
    LosKernelSerialWriteText(" dispatches=");
    LosKernelSerialWriteUnsigned(State->DispatchCount);
    LosKernelSerialWriteText(" preemptions=");
    LosKernelSerialWriteUnsigned(State->InterruptPreemptionCount);
    LosKernelSerialWriteText(" starvation-relief=");
    LosKernelSerialWriteUnsigned(State->StarvationReliefDispatchCount);
    LosKernelSerialWriteText(" tasks=");
    LosKernelSerialWriteUnsigned(State->TaskCount);
    LosKernelSerialWriteText(" processes=");
    LosKernelSerialWriteUnsigned(State->ProcessCount);
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
    LosKernelSerialWriteText(" active-root=");
    LosKernelSerialWriteHex64(State->ActiveRootTablePhysicalAddress);
    LosKernelSerialWriteText(" current=");
    LosKernelSerialWriteUnsigned(State->CurrentTaskId);
    LosKernelSerialWriteText(" current-proc=");
    LosKernelSerialWriteUnsigned(State->CurrentProcessId);
    LosKernelSerialWriteText("\n");
}

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
        if (State != 0)
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
    LosKernelSerialWriteText("[Kernel] Scheduler lifecycle manager entered.\n");

    for (;;)
    {
        UINT64 ProcessId;
        UINT64 TaskId;

        if (LosKernelSchedulerHasActiveTransientProcess() != 0U)
        {
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
            LosKernelSerialWriteText("[Kernel] Scheduler lifecycle could not create ephemeral process sequence=");
            LosKernelSerialWriteUnsigned(Sequence);
            LosKernelSerialWriteText("\n");
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
            LosKernelSerialWriteText("[Kernel] Scheduler lifecycle spawned ephemeral process id=");
            LosKernelSerialWriteUnsigned(ProcessId);
            LosKernelSerialWriteText(" task=");
            LosKernelSerialWriteUnsigned(TaskId);
            LosKernelSerialWriteText(" sequence=");
            LosKernelSerialWriteUnsigned(Sequence);
            LosKernelSerialWriteText("\n");
        }
        else
        {
            LosKernelSchedulerMarkProcessTerminated(ProcessId, 1ULL);
            LosKernelSerialWriteText("[Kernel] Scheduler lifecycle could not spawn ephemeral task for process id=");
            LosKernelSerialWriteUnsigned(ProcessId);
            LosKernelSerialWriteText(" sequence=");
            LosKernelSerialWriteUnsigned(Sequence);
            LosKernelSerialWriteText("\n");
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
    LosKernelSerialWriteText("[Kernel] Scheduler ephemeral task entered id=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
    LosKernelSerialWriteText(" owner=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->OwnerTaskId : 0ULL);
    LosKernelSerialWriteText(" process=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->ProcessId : 0ULL);
    LosKernelSerialWriteText(" sequence=");
    LosKernelSerialWriteUnsigned(Sequence);
    LosKernelSerialWriteText("\n");

    LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_EPHEMERAL_LIFETIME_TICKS);

    Task = LosKernelSchedulerGetCurrentTask();
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
    LosKernelSchedulerTerminateCurrent();
    LosKernelHaltForever();
}

void LosKernelSchedulerBusyThread(void *Context)
{
    UINT64 IterationCount;
    UINT64 LastReportTick;

    (void)Context;
    IterationCount = 0ULL;
    LastReportTick = 0ULL;
    LosKernelSerialWriteText("[Kernel] Scheduler busy worker entered.\n");

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
                LosKernelSerialWriteText("[Kernel] Scheduler busy worker iterations=");
                LosKernelSerialWriteUnsigned(IterationCount);
                LosKernelSerialWriteText(" ticks=");
                LosKernelSerialWriteUnsigned(TickCount);
                LosKernelSerialWriteText("\n");
                LastReportTick = TickCount;
            }
        }
    }
}
