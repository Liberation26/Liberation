/*
 * File Name: SchedulerLifecycleSection06.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerLifecycle.c.
 */

BOOLEAN LosKernelSchedulerPrepareUserTransitionScaffold(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 ProcessId;
    UINT64 TaskId;
    UINT64 CriticalSectionFlags;

    State = LosKernelSchedulerState();
    if (State == 0)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldReady != 0U)
    {
        return 1;
    }

    if (LosIsMemoryManagerBootstrapReady() == 0U)
    {
        return 0;
    }

    ProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    TaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    if (!LosKernelSchedulerCreateProcess(
            "InitCommandProcess",
            LOS_KERNEL_SCHEDULER_PROCESS_FLAG_USER_TRANSITION |
            LOS_KERNEL_SCHEDULER_PROCESS_FLAG_REQUIRE_OWN_ADDRESS_SPACE,
            0ULL,
            LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS,
            &ProcessId))
    {
        return 0;
    }

    if (!LosKernelSchedulerCreateTask(
            "InitCommandTask",
            ProcessId,
            LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE |
            LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_SCAFFOLD,
            LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_PRIORITY,
            1U,
            0ULL,
            LosKernelSchedulerUserTransitionTrapThread,
            0,
            &TaskId))
    {
        (void)LosKernelSchedulerMarkProcessTerminated(ProcessId, 1ULL);
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(ProcessId);
    Task = FindTaskByIdMutable(TaskId);
    if (Process != 0)
    {
        Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_USER_TRANSITION;
        Process->UserEntryVirtualAddress = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_ENTRY_VIRTUAL_ADDRESS;
        Process->UserStackTopVirtualAddress = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_TOP_VIRTUAL_ADDRESS;
        Process->UserCodeSegmentSelector = 0ULL;
        Process->UserStackSegmentSelector = 0ULL;
        Process->UserRflags = 0ULL;
        Process->UserTransitionFrameStackPointer = 0ULL;
        Process->UserTransitionKernelEntryVirtualAddress = 0ULL;
        Process->UserTransitionBridgeVirtualAddress = 0ULL;
        Process->UserTransitionChainStackPointer = 0ULL;
        Process->UserTransitionContractSignature = 0ULL;
        Process->UserTransitionSealValue = 0ULL;
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_PREPARED;
    }
    if (Task != 0)
    {
        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED;
        Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION;
        Task->ReadySinceTick = 0ULL;
        Task->NextWakeTick = 0ULL;
        Task->RemainingQuantumTicks = Task->QuantumTicks;
        Task->Flags |= LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE | LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_SCAFFOLD;
        Task->UserInstructionPointer = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_ENTRY_VIRTUAL_ADDRESS;
        Task->UserStackPointer = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_TOP_VIRTUAL_ADDRESS;
        Task->UserCodeSegmentSelector = 0ULL;
        Task->UserStackSegmentSelector = 0ULL;
        Task->UserRflags = 0ULL;
        Task->UserTransitionFrameStackPointer = 0ULL;
        Task->UserTransitionKernelEntryVirtualAddress = 0ULL;
        Task->UserTransitionBridgeVirtualAddress = 0ULL;
        Task->UserTransitionChainStackPointer = 0ULL;
        Task->UserTransitionContractSignature = 0ULL;
        Task->UserTransitionSealValue = 0ULL;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_PREPARED;
    }
    State->UserTransitionScaffoldReady = 1U;
    State->UserTransitionPreparedCount += 1ULL;
    State->UserTransitionScaffoldProcessId = ProcessId;
    State->UserTransitionScaffoldTaskId = TaskId;
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    LosKernelTraceOk("Scheduler init command prepared.");
    LosKernelTraceHex64("Scheduler init command entry: ", LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_ENTRY_VIRTUAL_ADDRESS);
    LosKernelTraceHex64("Scheduler init command user-stack top: ", LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_TOP_VIRTUAL_ADDRESS);
    LosKernelSchedulerTraceProcess("Prepared scheduler init command process", Process);
    LosKernelSchedulerTraceTask("Prepared scheduler init command task", Task);
    return 1;
}

BOOLEAN LosKernelSchedulerValidateUserTransitionScaffold(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN Validated;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->AddressSpaceId == 0ULL ||
        Process->RootTablePhysicalAddress == LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS ||
        Process->UserEntryVirtualAddress == 0ULL ||
        Process->UserStackTopVirtualAddress == 0ULL ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Task->UserInstructionPointer == 0ULL ||
        Task->UserStackPointer == 0ULL)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED)
    {
        return 1;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Validated = 0U;
    if (Process != 0 && Task != 0 &&
        Process->AddressSpaceId != 0ULL &&
        Process->RootTablePhysicalAddress != LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS &&
        Process->UserEntryVirtualAddress != 0ULL &&
        Process->UserStackTopVirtualAddress != 0ULL &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->UserInstructionPointer != 0ULL &&
        Task->UserStackPointer != 0ULL)
    {
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED;
        State->UserTransitionValidatedCount += 1ULL;
        Validated = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Validated != 0U)
    {
        LosKernelTraceOk("Scheduler first user task validated.");
        LosKernelSchedulerTraceProcess("Validated scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Validated scheduler first user task task", Task);
        return 1;
    }

    return 0;
}

BOOLEAN LosKernelSchedulerArmUserTransitionScaffold(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN Armed;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionValidatedCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED)
    {
        return 1;
    }

    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Armed = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED;
        State->UserTransitionArmedCount += 1ULL;
        Armed = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Armed != 0U)
    {
        LosKernelTraceOk("Scheduler first user task armed.");
        LosKernelSchedulerTraceProcess("Armed scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Armed scheduler first user task task", Task);
        return 1;
    }

    return 0;
}

BOOLEAN LosKernelSchedulerRequestUserTransitionScaffold(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN Requested;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionArmedCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED)
    {
        return 1;
    }

    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Requested = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED;
        State->UserTransitionLaunchRequestCount += 1ULL;
        Requested = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Requested != 0U)
    {
        LosKernelTraceOk("Scheduler first user task launch requested.");
        LosKernelSchedulerTraceProcess("Launch-requested scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Launch-requested scheduler first user task task", Task);
        return 1;
    }

    return 0;
}
