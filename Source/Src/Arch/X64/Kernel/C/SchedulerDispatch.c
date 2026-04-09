/*
 * File Name: SchedulerDispatch.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T10:02:19Z
 * Last Update Timestamp: 2026-04-07T12:35:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

#include "SchedulerInternal.h"
#include "InterruptsInternal.h"

const LOS_BOOT_CONTEXT *LosKernelGetBootContext(void);

static LOS_KERNEL_SCHEDULER_TASK *GetCurrentTaskMutable(void);
static LOS_KERNEL_SCHEDULER_PROCESS *FindProcessByIdMutable(UINT64 ProcessId);

static void LoadPageMapLevel4PhysicalAddress(UINT64 RootTablePhysicalAddress)
{
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(RootTablePhysicalAddress) : "memory");
}

static void CompleteCurrentUserTransitionTask(UINT64 ExitStatus,
                                              const char *ReasonText)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;

    State = LosKernelSchedulerState();
    Task = GetCurrentTaskMutable();
    if (State == 0 || Task == 0 ||
        (Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE) == 0U)
    {
        LosKernelTraceFail("Kernel scheduler could not complete a non-user task from the first-user-task return path.");
        LosKernelHaltForever();
    }

    Process = FindProcessByIdMutable(Task->ProcessId);
    Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED;
    Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_TERMINATED;
    Task->ReadySinceTick = 0ULL;
    Task->NextWakeTick = 0ULL;
    Task->WakeDispatchPending = 0U;
    Task->ResumeBoostTicks = 0U;
    Task->RemainingQuantumTicks = 0U;
    Task->CleanupPending = 1U;
    Task->ExitStatus = ExitStatus;
    Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE;
    if (Process != 0)
    {
        Process->ExitStatus = ExitStatus;
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE;
    }
    State->TerminatedTaskCount += 1ULL;
    if (State->UserTransitionCompleteCount == 0ULL)
    {
        State->UserTransitionCompleteCount += 1ULL;
    }
    State->ReschedulePending = 1U;

    if (ReasonText != 0)
    {
        LosKernelTraceOk(ReasonText);
    }
    if (Process != 0)
    {
        LosKernelSchedulerTraceProcess("Completed first user task process", Process);
    }
    LosKernelSchedulerTraceTask("Completed first user task task", Task);
}


static void LosKernelReportKnownDrives(void)
{
    const LOS_BOOT_CONTEXT *BootContext;

    BootContext = LosKernelGetBootContext();

    LosKernelSerialWriteText("[InitCmd] Known drives and partitions after init command return:\n");
    LosKernelSerialWriteText("[InitCmd] Communication model: send, receive, send-event, receive-event.\n[InitCmd] ABI version: 7\n");
    LosKernelSerialWriteText("[InitCmd] Drive 0: system boot drive\n");
    if (BootContext != 0)
    {
        LosKernelSerialWriteText("[InitCmd]   Boot source: ");
        LosKernelSerialWriteUtf16(BootContext->BootSourceText);
        LosKernelSerialWriteText("\n");
    }
    LosKernelSerialWriteText("[InitCmd]   Partition 1: EFI System Partition\n");
    LosKernelSerialWriteText("[InitCmd]   Partition 2: Liberation Data\n");
    if (BootContext != 0)
    {
        LosKernelSerialWriteText("[InitCmd]   Active kernel partition: ");
        LosKernelSerialWriteUtf16(BootContext->KernelPartitionText);
        LosKernelSerialWriteText("\n");
    }
}

static void UpdateDispatchLatencyAccounting(LOS_KERNEL_SCHEDULER_STATE *State,
                                          LOS_KERNEL_SCHEDULER_TASK *Task,
                                          LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    UINT64 ReadyDelayTicks;
    UINT64 WakeDelayTicks;

    if (State == 0 || Task == 0)
    {
        return;
    }

    ReadyDelayTicks = 0ULL;
    if (Task->ReadySinceTick != 0ULL && State->TickCount >= Task->ReadySinceTick)
    {
        ReadyDelayTicks = State->TickCount - Task->ReadySinceTick;
    }

    WakeDelayTicks = 0ULL;
    if (Task->LastWakeTick != 0ULL && State->TickCount >= Task->LastWakeTick)
    {
        WakeDelayTicks = State->TickCount - Task->LastWakeTick;
    }

    if (ReadyDelayTicks > Task->MaxReadyDelayTicks)
    {
        Task->MaxReadyDelayTicks = ReadyDelayTicks;
    }
    if (WakeDelayTicks > Task->MaxWakeDelayTicks)
    {
        Task->MaxWakeDelayTicks = WakeDelayTicks;
    }
    if (ReadyDelayTicks > State->MaxReadyDelayTicks)
    {
        State->MaxReadyDelayTicks = ReadyDelayTicks;
    }
    if (WakeDelayTicks > State->MaxWakeDelayTicks)
    {
        State->MaxWakeDelayTicks = WakeDelayTicks;
    }

    if (Process != 0)
    {
        if (ReadyDelayTicks > Process->MaxReadyDelayTicks)
        {
            Process->MaxReadyDelayTicks = ReadyDelayTicks;
        }
        if (WakeDelayTicks > Process->MaxWakeDelayTicks)
        {
            Process->MaxWakeDelayTicks = WakeDelayTicks;
        }
    }
}

static void FinalizeRunSliceAccounting(LOS_KERNEL_SCHEDULER_STATE *State,
                                      LOS_KERNEL_SCHEDULER_TASK *Task,
                                      LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    UINT64 RunSliceTicks;

    if (State == 0 || Task == 0)
    {
        return;
    }

    RunSliceTicks = 0ULL;
    if (Task->LastRunTick != 0ULL && State->TickCount >= Task->LastRunTick)
    {
        RunSliceTicks = State->TickCount - Task->LastRunTick;
    }

    Task->LastRunSliceTicks = RunSliceTicks;
    if (RunSliceTicks > Task->MaxRunSliceTicks)
    {
        Task->MaxRunSliceTicks = RunSliceTicks;
    }
    if (RunSliceTicks > State->MaxRunSliceTicks)
    {
        State->MaxRunSliceTicks = RunSliceTicks;
    }

    if (Process != 0)
    {
        Process->LastRunSliceTicks = RunSliceTicks;
        if (RunSliceTicks > Process->MaxRunSliceTicks)
        {
            Process->MaxRunSliceTicks = RunSliceTicks;
        }
    }
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

static LOS_KERNEL_SCHEDULER_PROCESS *FindProcessByIdMutable(UINT64 ProcessId)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    State = LosKernelSchedulerState();
    if (State == 0 || ProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID)
    {
        return 0;
    }

    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

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
    LOS_KERNEL_SCHEDULER_PROCESS *Process;

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
    Process = FindProcessByIdMutable(Task->ProcessId);
    if (Process != 0)
    {
        Process->TotalTicks += 1ULL;
    }
    if ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_IDLE) != 0U)
    {
        State->IdleTicks += 1ULL;
    }
    else
    {
        State->BusyTicks += 1ULL;
    }
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
    if (Task == 0)
    {
        return;
    }

    if (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING)
    {
        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
        Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_PREEMPTED;
        Task->ReadySinceTick = State->TickCount;
        Task->PreemptionCount += 1ULL;
        State->InterruptPreemptionCount += 1ULL;
    }
    else if (Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED &&
             Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED)
    {
        return;
    }

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

UINT64 LosKernelSchedulerPrepareUserTransitionIret(void)
{
    const LOS_KERNEL_SCHEDULER_TASK *Task;

    Task = LosKernelSchedulerGetCurrentTask();
    if (Task == 0 ||
        (Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE) == 0U ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Task->StackTopVirtualAddress == 0ULL)
    {
        LosKernelTraceFail("Kernel scheduler could not prepare a live first-user-task iret frame.");
        LosKernelHaltForever();
    }

    LosKernelTraceOk("Kernel scheduler prepared the first live CPL3 iretq dispatch.");
    LosKernelTraceHex64("Scheduler first-user-task iret frame rsp: ", Task->UserTransitionFrameStackPointer);
    LosKernelTraceHex64("Scheduler first-user-task user rip: ", Task->UserInstructionPointer);
    LosKernelTraceHex64("Scheduler first-user-task user rsp: ", Task->UserStackPointer);
    LosKernelTraceHex64("Scheduler first-user-task user cs: ", Task->UserCodeSegmentSelector);
    LosKernelTraceHex64("Scheduler first-user-task user ss: ", Task->UserStackSegmentSelector);
    LosKernelSetInterruptStackTop(Task->StackTopVirtualAddress);
    return Task->UserTransitionFrameStackPointer;
}

BOOLEAN LosKernelSchedulerHandleUserModeInterrupt(UINT64 Vector, UINT64 ErrorCode, UINT64 InstructionPointer, UINT64 StackPointer)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;

    (void)ErrorCode;

    State = LosKernelSchedulerState();
    Task = GetCurrentTaskMutable();
    if (State == 0 || Task == 0 ||
        (Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE) == 0U ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE)
    {
        return 0U;
    }

    Process = FindProcessByIdMutable(Task->ProcessId);
    Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED;
    Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_TERMINATED;
    Task->ReadySinceTick = 0ULL;
    Task->NextWakeTick = 0ULL;
    Task->WakeDispatchPending = 0U;
    Task->ResumeBoostTicks = 0U;
    Task->RemainingQuantumTicks = 0U;
    Task->CleanupPending = 1U;
    Task->ExitStatus = Vector;
    Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE;
    if (Process != 0)
    {
        Process->ExitStatus = Vector;
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE;
    }
    State->TerminatedTaskCount += 1ULL;
    if (State->UserTransitionCompleteCount == 0ULL)
    {
        State->UserTransitionCompleteCount += 1ULL;
    }
    State->ReschedulePending = 1U;

    if (Vector == LOS_X64_USER_TRANSITION_VECTOR)
    {
        LosKernelTraceOk("Init command executed in ring 3 and trapped back into the kernel.");
        LosKernelReportKnownDrives();
    }
    else
    {
        LosKernelTraceFail("First user-mode task faulted while running in ring 3.");
    }
    LosKernelTraceUnsigned("Scheduler drive-command return vector: ", Vector);
    LosKernelTraceHex64("Scheduler drive-command return rip: ", InstructionPointer);
    LosKernelTraceHex64("Scheduler drive-command return rsp: ", StackPointer);
    LosKernelTraceHex64("Scheduler drive-command return cs: ", Task->UserCodeSegmentSelector);
    LosKernelTraceHex64("Scheduler drive-command return ss: ", Task->UserStackSegmentSelector);
    if (Process != 0)
    {
        LosKernelSchedulerTraceProcess("Returned init command process", Process);
    }
    LosKernelSchedulerTraceTask("Returned init command task", Task);
    return 1U;
}

void LosKernelSchedulerUserTransitionKernelEntry(void)
{
    const LOS_KERNEL_SCHEDULER_TASK *Task;

    Task = LosKernelSchedulerGetCurrentTask();
    if (Task != 0)
    {
        LosKernelSchedulerTraceTask("Scheduler drive-command kernel-entry task", Task);
    }

    CompleteCurrentUserTransitionTask(LOS_X64_USER_TRANSITION_VECTOR,
                                      "Drive command returned through the kernel-entry bridge after the first CPL3 dispatch.");
    LosKernelSchedulerSwitchContext(&GetCurrentTaskMutable()->ExecutionContext,
                                    &LosKernelSchedulerState()->SchedulerContext);
    LosKernelTraceFail("Kernel scheduler drive-command kernel-entry path resumed unexpectedly.");
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

        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Task = &State->Tasks[SelectedIndex];
        if ((Task->Flags & LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE) != 0U &&
            Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE)
        {
            Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED;
            Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION;
            Task->ReadySinceTick = 0ULL;
            Task->WakeDispatchPending = 0U;
            Task->ResumeBoostTicks = 0U;
            State->UserTransitionDispatchSkipCount += 1ULL;
            continue;
        }
        Process = FindProcessByIdMutable(Task->ProcessId);
        LosKernelSchedulerActivateProcessAddressSpace(Process);
        State->CurrentTaskIndex = SelectedIndex;
        State->CurrentTaskId = Task->TaskId;
        State->CurrentProcessId = Task->ProcessId;
        State->LastSelectedIndex = SelectedIndex;
        State->DispatchCount += 1ULL;
        State->ReschedulePending = 0U;

        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING;
        UpdateDispatchLatencyAccounting(State, Task, Process);
        Task->ReadySinceTick = 0ULL;
        Task->DispatchCount += 1ULL;
        Task->LastRunTick = State->TickCount;
        if (Process != 0)
        {
            Process->DispatchCount += 1ULL;
            Process->LastRunTick = State->TickCount;
        }
        Task->RemainingQuantumTicks = Task->QuantumTicks == 0U ? 1U : Task->QuantumTicks;
        if (Task->WakeDispatchPending != 0U)
        {
            UINT32 ResumeQuantumTicks;

            ResumeQuantumTicks = Task->ResumeBoostTicks == 0U ? Task->RemainingQuantumTicks : Task->ResumeBoostTicks;
            if (ResumeQuantumTicks > Task->RemainingQuantumTicks)
            {
                Task->RemainingQuantumTicks = ResumeQuantumTicks;
            }
            Task->WakeDispatchPending = 0U;
            Task->ResumeBoostTicks = 0U;
            State->WakeResumeWindowDispatchCount += 1ULL;
        }
        Task->LastWakeTick = 0ULL;
        State->InScheduler = 0U;
        LosKernelSchedulerSwitchContext(&State->SchedulerContext, &Task->ExecutionContext);

        State->InScheduler = 1U;
        FinalizeRunSliceAccounting(State, Task, Process);
        LosKernelSchedulerRestoreKernelAddressSpace();
        State->CurrentTaskIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
        State->CurrentTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
        State->CurrentProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    }
}
