#include "SchedulerInternal.h"

static void WriteTaskName(const char *Name)
{
    LosKernelSerialWriteText(Name != 0 ? Name : "<unnamed>");
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
    LosKernelSerialWriteText(" generation=");
    LosKernelSerialWriteUnsigned(Task->Generation);
    LosKernelSerialWriteText(" name=");
    WriteTaskName(Task->Name);
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
    LosKernelSerialWriteText(" preemptions=");
    LosKernelSerialWriteUnsigned(Task->PreemptionCount);
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
    LosKernelSerialWriteText(" tasks=");
    LosKernelSerialWriteUnsigned(State->TaskCount);
    LosKernelSerialWriteText(" created=");
    LosKernelSerialWriteUnsigned(State->CreatedTaskCount);
    LosKernelSerialWriteText(" terminated=");
    LosKernelSerialWriteUnsigned(State->TerminatedTaskCount);
    LosKernelSerialWriteText(" reaped=");
    LosKernelSerialWriteUnsigned(State->ReapedTaskCount);
    LosKernelSerialWriteText(" current=");
    LosKernelSerialWriteUnsigned(State->CurrentTaskId);
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
            LosKernelSerialWriteText(" created=");
            LosKernelSerialWriteUnsigned(State->CreatedTaskCount);
            LosKernelSerialWriteText(" terminated=");
            LosKernelSerialWriteUnsigned(State->TerminatedTaskCount);
            LosKernelSerialWriteText(" reaped=");
            LosKernelSerialWriteUnsigned(State->ReapedTaskCount);
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
        UINT64 TaskId;

        Sequence += 1ULL;
        if (LosKernelSchedulerCreateTask(
                "EphemeralWorker",
                0U,
                LOS_KERNEL_SCHEDULER_EPHEMERAL_PRIORITY,
                1U,
                0ULL,
                LosKernelSchedulerEphemeralThread,
                (void *)(UINTN)Sequence,
                &TaskId))
        {
            LosKernelSerialWriteText("[Kernel] Scheduler lifecycle spawned ephemeral task id=");
            LosKernelSerialWriteUnsigned(TaskId);
            LosKernelSerialWriteText(" sequence=");
            LosKernelSerialWriteUnsigned(Sequence);
            LosKernelSerialWriteText("\n");
        }
        else
        {
            LosKernelSerialWriteText("[Kernel] Scheduler lifecycle could not spawn ephemeral task sequence=");
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
    LosKernelSerialWriteText(" sequence=");
    LosKernelSerialWriteUnsigned(Sequence);
    LosKernelSerialWriteText("\n");

    LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_EPHEMERAL_LIFETIME_TICKS);

    LosKernelSerialWriteText("[Kernel] Scheduler ephemeral task exiting id=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
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
