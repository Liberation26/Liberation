#include "SchedulerInternal.h"

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
    if (State->CurrentTaskIndex == LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX ||
        State->CurrentTaskIndex >= LOS_KERNEL_SCHEDULER_MAX_TASKS)
    {
        return;
    }

    Task = &State->Tasks[State->CurrentTaskIndex];
    if ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_IDLE) != 0U)
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

void LosKernelSchedulerEnter(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerState();
    State->Online = 1U;
    LosKernelTraceOk("Kernel scheduler entered.");
    LosKernelSchedulerTraceState("Scheduler online");

    for (;;)
    {
        UINT32 SelectedIndex;
        LOS_KERNEL_SCHEDULER_TASK *Task;

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
        Task->StepRoutine(Task->Context);

        if (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING)
        {
            if ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_IDLE) != 0U)
            {
                Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
            }
            else if ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_PERIODIC) != 0U)
            {
                Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED;
                Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_WAIT_PERIOD;
                Task->NextWakeTick = State->TickCount + Task->WakePeriodTicks;
            }
            else
            {
                Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
            }
        }

        State->CurrentTaskIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
        State->CurrentTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    }
}
