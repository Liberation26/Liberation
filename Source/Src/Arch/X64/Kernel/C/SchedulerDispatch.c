#include "SchedulerInternal.h"

static LOS_KERNEL_SCHEDULER_TASK *GetCurrentTaskMutable(void)
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

void LosKernelSchedulerRequestReschedule(void)
{
    LosKernelSchedulerState()->ReschedulePending = 1U;
}

void LosKernelSchedulerOnTimerTick(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_TASK *Task;

    State = LosKernelSchedulerState();
    State->TickCount += 1ULL;
    LosKernelSchedulerWakeDueTasks();

    if (State->InScheduler != 0U)
    {
        return;
    }

    Task = GetCurrentTaskMutable();
    if (Task == 0 || Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING)
    {
        return;
    }

    Task->TotalTicks += 1ULL;
    if (Task->RemainingQuantumTicks > 0U)
    {
        Task->RemainingQuantumTicks -= 1U;
    }
    if (Task->RemainingQuantumTicks == 0U)
    {
        State->ReschedulePending = 1U;
    }
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
    if (Task == 0 || Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING)
    {
        return;
    }

    Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
    Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_PREEMPTED;
    Task->PreemptionCount += 1ULL;
    State->InterruptPreemptionCount += 1ULL;
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

    Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED;
    Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_TERMINATED;
    if ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_IDLE) == 0U && State->TaskCount > 0U)
    {
        State->TaskCount -= 1U;
    }
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
        LosKernelSchedulerWakeDueTasks();
        SelectedIndex = LosKernelSchedulerSelectNextTaskIndex();
        if (SelectedIndex == LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX)
        {
            LosKernelTraceFail("Kernel scheduler found no dispatchable task.");
            LosKernelHaltForever();
        }

        Task = &State->Tasks[SelectedIndex];
        State->CurrentTaskIndex = SelectedIndex;
        State->CurrentTaskId = Task->TaskId;
        State->LastSelectedIndex = SelectedIndex;
        State->DispatchCount += 1ULL;
        State->ReschedulePending = 0U;

        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING;
        Task->DispatchCount += 1ULL;
        Task->LastRunTick = State->TickCount;
        Task->RemainingQuantumTicks = Task->QuantumTicks == 0U ? 1U : Task->QuantumTicks;
        State->InScheduler = 0U;
        LosKernelSchedulerSwitchContext(&State->SchedulerContext, &Task->ExecutionContext);

        State->InScheduler = 1U;
        State->CurrentTaskIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
        State->CurrentTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    }
}
