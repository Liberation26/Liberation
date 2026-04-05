#include "SchedulerInternal.h"

void LosKernelSchedulerWakeDueTasks(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        LOS_KERNEL_SCHEDULER_TASK *Task;

        Task = &State->Tasks[Index];
        if (Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED)
        {
            continue;
        }
        if (Task->NextWakeTick == 0ULL || State->TickCount < Task->NextWakeTick)
        {
            continue;
        }

        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
        Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_NONE;
        Task->NextWakeTick = 0ULL;
    }
}

UINT32 LosKernelSchedulerSelectNextTaskIndex(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Offset;
    UINT32 BestIndex;
    UINT32 BestPriority;
    UINT32 StartIndex;

    State = LosKernelSchedulerState();
    BestIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
    BestPriority = 0U;
    StartIndex = State->LastSelectedIndex == LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX
        ? 0U
        : (UINT32)((State->LastSelectedIndex + 1U) % LOS_KERNEL_SCHEDULER_MAX_TASKS);

    for (Offset = 0U; Offset < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Offset)
    {
        UINT32 Index;
        const LOS_KERNEL_SCHEDULER_TASK *Task;

        Index = (UINT32)((StartIndex + Offset) % LOS_KERNEL_SCHEDULER_MAX_TASKS);
        Task = &State->Tasks[Index];
        if (Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_READY)
        {
            continue;
        }

        if (BestIndex == LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX || Task->Priority > BestPriority)
        {
            BestIndex = Index;
            BestPriority = Task->Priority;
        }
    }

    if (BestIndex == LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX)
    {
        for (Offset = 0U; Offset < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Offset)
        {
            const LOS_KERNEL_SCHEDULER_TASK *Task;

            Task = &State->Tasks[Offset];
            if ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_IDLE) != 0U &&
                Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED)
            {
                BestIndex = Offset;
                break;
            }
        }
    }

    return BestIndex;
}
