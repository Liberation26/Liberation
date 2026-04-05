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
        Task->ReadySinceTick = State->TickCount;
        Task->NextWakeTick = 0ULL;
    }
}

static UINT32 GetTaskEffectivePriority(const LOS_KERNEL_SCHEDULER_STATE *State, const LOS_KERNEL_SCHEDULER_TASK *Task, UINT64 *WaitTicks)
{
    UINT64 LocalWaitTicks;
    UINT64 AgeBoost;

    LocalWaitTicks = 0ULL;
    if (State != 0 && Task != 0 && State->TickCount > Task->ReadySinceTick)
    {
        LocalWaitTicks = State->TickCount - Task->ReadySinceTick;
    }
    if (WaitTicks != 0)
    {
        *WaitTicks = LocalWaitTicks;
    }

    AgeBoost = LocalWaitTicks / LOS_KERNEL_SCHEDULER_AGING_INTERVAL_TICKS;
    if (AgeBoost > LOS_KERNEL_SCHEDULER_MAX_AGING_BOOST)
    {
        AgeBoost = LOS_KERNEL_SCHEDULER_MAX_AGING_BOOST;
    }

    return Task->Priority + (UINT32)AgeBoost;
}

UINT32 LosKernelSchedulerSelectNextTaskIndex(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Offset;
    UINT32 BestIndex;
    UINT32 BestPriority;
    UINT32 StartIndex;
    UINT64 BestWaitTicks;
    BOOLEAN SelectedByAging;

    State = LosKernelSchedulerState();
    BestIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
    BestPriority = 0U;
    BestWaitTicks = 0ULL;
    SelectedByAging = 0U;
    StartIndex = State->LastSelectedIndex == LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX
        ? 0U
        : (UINT32)((State->LastSelectedIndex + 1U) % LOS_KERNEL_SCHEDULER_MAX_TASKS);

    for (Offset = 0U; Offset < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Offset)
    {
        UINT32 Index;
        const LOS_KERNEL_SCHEDULER_TASK *Task;
        UINT32 EffectivePriority;
        UINT64 WaitTicks;

        Index = (UINT32)((StartIndex + Offset) % LOS_KERNEL_SCHEDULER_MAX_TASKS);
        Task = &State->Tasks[Index];
        if (Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_READY)
        {
            continue;
        }

        EffectivePriority = GetTaskEffectivePriority(State, Task, &WaitTicks);
        if (BestIndex == LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX ||
            EffectivePriority > BestPriority ||
            (EffectivePriority == BestPriority && WaitTicks > BestWaitTicks))
        {
            BestIndex = Index;
            BestPriority = EffectivePriority;
            BestWaitTicks = WaitTicks;
            SelectedByAging = EffectivePriority > Task->Priority ? 1U : 0U;
        }
    }

    if (BestIndex != LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX && SelectedByAging != 0U)
    {
        State->StarvationReliefDispatchCount += 1ULL;
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
