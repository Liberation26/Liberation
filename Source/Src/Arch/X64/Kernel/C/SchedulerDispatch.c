#include "SchedulerInternal.h"

static void LoadPageMapLevel4PhysicalAddress(UINT64 RootTablePhysicalAddress)
{
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(RootTablePhysicalAddress) : "memory");
}


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

static const LOS_KERNEL_SCHEDULER_PROCESS *FindProcessForDispatch(UINT64 ProcessId)
{
    const LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    State = LosKernelSchedulerGetState();
    if (State == 0 || ProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID)
    {
        return 0;
    }

    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        const LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED)
        {
            continue;
        }
        if (Process->ProcessId == ProcessId)
        {
            return Process;
        }
    }

    return 0;
}

void LosKernelSchedulerActivateProcessAddressSpace(const LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT64 TargetRoot;

    State = LosKernelSchedulerState();
    if (State == 0 || Process == 0)
    {
        return;
    }

    TargetRoot = Process->RootTablePhysicalAddress;
    if (TargetRoot == LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS)
    {
        LosKernelTraceFail("Kernel scheduler found no root table for dispatchable process.");
        LosKernelHaltForever();
    }

    if (State->ActiveRootTablePhysicalAddress == TargetRoot)
    {
        State->AddressSpaceReuseCount += 1ULL;
        return;
    }

    LoadPageMapLevel4PhysicalAddress(TargetRoot);
    State->ActiveRootTablePhysicalAddress = TargetRoot;
    State->AddressSpaceSwitchCount += 1ULL;
}

void LosKernelSchedulerRestoreKernelAddressSpace(void)
{
    const LOS_KERNEL_SCHEDULER_PROCESS *KernelProcess;

    KernelProcess = FindProcessForDispatch(LosKernelSchedulerState()->KernelProcessId);
    if (KernelProcess != 0)
    {
        LosKernelSchedulerActivateProcessAddressSpace(KernelProcess);
    }
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
    Task->ReadySinceTick = State->TickCount;
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
    Task->ReadySinceTick = State->TickCount;
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
    if ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_IDLE) != 0U)
    {
        LosKernelTraceFail("Kernel scheduler idle task attempted to terminate.");
        LosKernelHaltForever();
    }

    Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED;
    Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_TERMINATED;
    Task->ExitStatus = 0ULL;
    Task->CleanupPending = 1U;
    State->TerminatedTaskCount += 1ULL;
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
        LosKernelSchedulerCleanupTerminatedTasks();
        LosKernelSchedulerCleanupTerminatedProcesses();
        LosKernelSchedulerBindPendingProcessAddressSpaces();
        LosKernelSchedulerWakeDueTasks();
        SelectedIndex = LosKernelSchedulerSelectNextTaskIndex();
        if (SelectedIndex == LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX)
        {
            LosKernelTraceFail("Kernel scheduler found no dispatchable task.");
            LosKernelHaltForever();
        }

        Task = &State->Tasks[SelectedIndex];
        LosKernelSchedulerActivateProcessAddressSpace(FindProcessForDispatch(Task->ProcessId));
        State->CurrentTaskIndex = SelectedIndex;
        State->CurrentTaskId = Task->TaskId;
        State->CurrentProcessId = Task->ProcessId;
        State->LastSelectedIndex = SelectedIndex;
        State->DispatchCount += 1ULL;
        State->ReschedulePending = 0U;

        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING;
        Task->ReadySinceTick = 0ULL;
        Task->LastWakeTick = 0ULL;
        Task->DispatchCount += 1ULL;
        Task->LastRunTick = State->TickCount;
        Task->RemainingQuantumTicks = Task->QuantumTicks == 0U ? 1U : Task->QuantumTicks;
        State->InScheduler = 0U;
        LosKernelSchedulerSwitchContext(&State->SchedulerContext, &Task->ExecutionContext);

        State->InScheduler = 1U;
        LosKernelSchedulerRestoreKernelAddressSpace();
        State->CurrentTaskIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
        State->CurrentTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
        State->CurrentProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    }
}
