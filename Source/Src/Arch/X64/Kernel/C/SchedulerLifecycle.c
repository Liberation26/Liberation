#include "SchedulerInternal.h"

static LOS_KERNEL_SCHEDULER_STATE LosKernelSchedulerGlobalState;

LOS_KERNEL_SCHEDULER_STATE *LosKernelSchedulerState(void)
{
    return &LosKernelSchedulerGlobalState;
}

static void ZeroBytes(void *Buffer, UINTN ByteCount)
{
    UINT8 *Bytes;
    UINTN Index;

    if (Buffer == 0)
    {
        return;
    }

    Bytes = (UINT8 *)Buffer;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        Bytes[Index] = 0U;
    }
}

BOOLEAN LosKernelSchedulerCreateTask(
    const char *Name,
    UINT32 Flags,
    UINT32 Priority,
    UINT32 QuantumTicks,
    UINT64 WakePeriodTicks,
    LOS_KERNEL_SCHEDULER_STEP_ROUTINE StepRoutine,
    void *Context,
    UINT64 *TaskId)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;
    LOS_KERNEL_SCHEDULER_TASK *Task;

    if (TaskId != 0)
    {
        *TaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    }
    if (StepRoutine == 0)
    {
        return 0;
    }

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        if (State->Tasks[Index].State == LOS_KERNEL_SCHEDULER_TASK_STATE_UNUSED)
        {
            Task = &State->Tasks[Index];
            ZeroBytes(Task, sizeof(*Task));
            Task->Signature = LOS_KERNEL_SCHEDULER_SIGNATURE;
            Task->Version = LOS_KERNEL_SCHEDULER_VERSION;
            Task->State = ((Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_PERIODIC) != 0U)
                ? LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED
                : LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
            Task->TaskId = State->NextTaskId;
            Task->Name = Name;
            Task->Flags = Flags;
            Task->Priority = Priority;
            Task->QuantumTicks = QuantumTicks == 0U ? LOS_KERNEL_SCHEDULER_DEFAULT_QUANTUM_TICKS : QuantumTicks;
            Task->RemainingQuantumTicks = Task->QuantumTicks;
            Task->WakePeriodTicks = WakePeriodTicks;
            Task->NextWakeTick = WakePeriodTicks == 0ULL ? 0ULL : (State->TickCount + WakePeriodTicks);
            Task->DispatchCount = 0ULL;
            Task->TotalTicks = 0ULL;
            Task->LastRunTick = 0ULL;
            Task->LastBlockReason = ((Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_PERIODIC) != 0U)
                ? LOS_KERNEL_SCHEDULER_BLOCK_REASON_WAIT_PERIOD
                : LOS_KERNEL_SCHEDULER_BLOCK_REASON_NONE;
            Task->StepRoutine = StepRoutine;
            Task->Context = Context;

            State->TaskCount += 1U;
            State->NextTaskId += 1ULL;
            if (TaskId != 0)
            {
                *TaskId = Task->TaskId;
            }
            LosKernelSchedulerTraceTask("Registered scheduler task", Task);
            return 1;
        }
    }

    return 0;
}

void LosKernelSchedulerInitialize(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT64 TaskId;

    State = LosKernelSchedulerState();
    ZeroBytes(State, sizeof(*State));
    State->Signature = LOS_KERNEL_SCHEDULER_SIGNATURE;
    State->Version = LOS_KERNEL_SCHEDULER_VERSION;
    State->Online = 0U;
    State->TickCount = 0ULL;
    State->DispatchCount = 0ULL;
    State->CurrentTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    State->NextTaskId = 1ULL;
    State->TaskCount = 0U;
    State->CurrentTaskIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
    State->LastSelectedIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
    State->ReschedulePending = 0U;

    if (!LosKernelSchedulerCreateTask(
            "Idle",
            LOS_KERNEL_SCHEDULER_TASK_FLAG_IDLE,
            LOS_KERNEL_SCHEDULER_IDLE_PRIORITY,
            1U,
            0ULL,
            LosKernelSchedulerIdleStep,
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

    if (!LosKernelSchedulerCreateTask(
            "Heartbeat",
            LOS_KERNEL_SCHEDULER_TASK_FLAG_PERIODIC,
            LOS_KERNEL_SCHEDULER_HEARTBEAT_PRIORITY,
            1U,
            LOS_KERNEL_SCHEDULER_HEARTBEAT_PERIOD_TICKS,
            LosKernelSchedulerHeartbeatStep,
            0,
            &TaskId))
    {
        LosKernelTraceFail("Kernel scheduler could not create heartbeat task.");
        LosKernelHaltForever();
    }

    LosKernelTraceOk("Kernel scheduler bootstrap tasks registered.");
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

const LOS_KERNEL_SCHEDULER_STATE *LosKernelSchedulerGetState(void)
{
    return LosKernelSchedulerState();
}
