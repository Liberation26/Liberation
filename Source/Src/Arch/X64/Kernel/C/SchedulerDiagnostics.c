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
    LosKernelSerialWriteText(" tasks=");
    LosKernelSerialWriteUnsigned(State->TaskCount);
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
            LosKernelSerialWriteText("\n");
        }

        LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_HEARTBEAT_PERIOD_TICKS);
    }
}
