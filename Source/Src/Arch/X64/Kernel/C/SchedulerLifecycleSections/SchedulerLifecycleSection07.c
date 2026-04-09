/*
 * File Name: SchedulerLifecycleSection07.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerLifecycle.c.
 */

void LosKernelSchedulerCleanupTerminatedTasks(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;
    UINT64 CriticalSectionFlags;

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        LOS_KERNEL_SCHEDULER_TASK *Task;
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Task = &State->Tasks[Index];
        if (Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED || Task->CleanupPending == 0U)
        {
            continue;
        }

        LosKernelSchedulerTraceTask("Reclaimed scheduler task", Task);
        Process = FindProcessByIdMutable(Task->ProcessId);
        if (Process != 0 && Process->ThreadCount > 0U)
        {
            Process->ThreadCount -= 1U;
            if (Process->ThreadCount == 0U && (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) == 0U)
            {
                if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED)
                {
                    State->TerminatedProcessCount += 1ULL;
                }
                Process->State = LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED;
                Process->TerminatedTick = State->TickCount;
                Process->ExitStatus = Task->ExitStatus;
                Process->CleanupPending = 1U;
            }
        }

        if (Task->TaskId == State->UserTransitionScaffoldTaskId)
        {
            State->UserTransitionScaffoldTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
        }
        ReleaseTaskStackResources(Task);
        if (State->TaskCount > 0U)
        {
            State->TaskCount -= 1U;
        }
        State->ReapedTaskCount += 1ULL;
        ZeroBytes(Task, sizeof(*Task));
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
}

void LosKernelSchedulerCleanupTerminatedProcesses(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;
    UINT64 CriticalSectionFlags;

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED ||
            Process->CleanupPending == 0U ||
            Process->ThreadCount != 0U)
        {
            continue;
        }

        if (!DestroyOwnedProcessAddressSpace(Process))
        {
            continue;
        }

        if (Process->ProcessId == State->UserTransitionScaffoldProcessId)
        {
            State->UserTransitionScaffoldProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
        }
        LosKernelSchedulerTraceProcess("Reclaimed scheduler process", Process);
        if (State->ProcessCount > 0U)
        {
            State->ProcessCount -= 1U;
        }
        State->ReapedProcessCount += 1ULL;
        ZeroBytes(Process, sizeof(*Process));
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
}

BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldEntryReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN EntryReady;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionLaunchRequestCount == 0ULL)
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

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY)
    {
        return 1;
    }

    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) == 0U ||
        Process->AddressSpaceObjectPhysicalAddress == 0ULL ||
        Process->RootTablePhysicalAddress == 0ULL)
    {
        return 0;
    }

    if (EnsureFirstUserTaskMappings(Process, Task) == 0U)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0 ||
        Task->UserInstructionPointer == 0ULL ||
        Task->UserStackPointer == 0ULL ||
        Process->UserEntryVirtualAddress == 0ULL ||
        Process->UserStackTopVirtualAddress == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    EntryReady = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->UserInstructionPointer != 0ULL &&
        Task->UserStackPointer != 0ULL &&
        Process->UserEntryVirtualAddress != 0ULL &&
        Process->UserStackTopVirtualAddress != 0ULL)
    {
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY;
        State->UserTransitionEntryReadyCount += 1ULL;
        EntryReady = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (EntryReady != 0U)
    {
        LosKernelTraceOk("Scheduler first user task entry-ready.");
        LosKernelSchedulerTraceProcess("Entry-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Entry-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}

BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldDescriptorReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN DescriptorReady;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionEntryReadyCount == 0ULL)
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

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY)
    {
        return 1;
    }

    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Process->UserEntryVirtualAddress == 0ULL ||
        Process->UserStackTopVirtualAddress == 0ULL ||
        Task->UserInstructionPointer == 0ULL ||
        Task->UserStackPointer == 0ULL ||
        LOS_X64_USER_CODE_SELECTOR == 0U ||
        LOS_X64_USER_DATA_SELECTOR == 0U)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    DescriptorReady = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Process->UserEntryVirtualAddress != 0ULL &&
        Process->UserStackTopVirtualAddress != 0ULL &&
        Task->UserInstructionPointer != 0ULL &&
        Task->UserStackPointer != 0ULL)
    {
        Process->UserCodeSegmentSelector = (UINT64)LOS_X64_USER_CODE_SELECTOR;
        Process->UserStackSegmentSelector = (UINT64)LOS_X64_USER_DATA_SELECTOR;
        Process->UserRflags = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_RFLAGS;
        Task->UserCodeSegmentSelector = (UINT64)LOS_X64_USER_CODE_SELECTOR;
        Task->UserStackSegmentSelector = (UINT64)LOS_X64_USER_DATA_SELECTOR;
        Task->UserRflags = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_RFLAGS;
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY;
        State->UserTransitionDescriptorReadyCount += 1ULL;
        DescriptorReady = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (DescriptorReady != 0U)
    {
        LosKernelTraceOk("Scheduler first user task descriptor-ready.");
        LosKernelSchedulerTraceProcess("Descriptor-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Descriptor-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldFrameReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    UINT64 FrameStackPointer;
    BOOLEAN FrameReady;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionDescriptorReadyCount == 0ULL)
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

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY)
    {
        return 1;
    }

    FrameStackPointer = GetUserTransitionFrameStackPointer(Task);
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Task->UserInstructionPointer == 0ULL ||
        Task->UserStackPointer == 0ULL ||
        Process->UserEntryVirtualAddress == 0ULL ||
        Process->UserStackTopVirtualAddress == 0ULL ||
        Process->UserCodeSegmentSelector == 0ULL ||
        Process->UserStackSegmentSelector == 0ULL ||
        Process->UserRflags == 0ULL ||
        Task->UserCodeSegmentSelector == 0ULL ||
        Task->UserStackSegmentSelector == 0ULL ||
        Task->UserRflags == 0ULL ||
        FrameStackPointer == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    FrameReady = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->UserInstructionPointer != 0ULL &&
        Task->UserStackPointer != 0ULL &&
        Process->UserEntryVirtualAddress != 0ULL &&
        Process->UserStackTopVirtualAddress != 0ULL &&
        Process->UserCodeSegmentSelector != 0ULL &&
        Process->UserStackSegmentSelector != 0ULL &&
        Process->UserRflags != 0ULL &&
        Task->UserCodeSegmentSelector != 0ULL &&
        Task->UserStackSegmentSelector != 0ULL &&
        Task->UserRflags != 0ULL)
    {
        FrameStackPointer = GetUserTransitionFrameStackPointer(Task);
        if (FrameStackPointer != 0ULL)
        {
            WriteUserTransitionFrame(
                FrameStackPointer,
                Task->UserInstructionPointer,
                Task->UserCodeSegmentSelector,
                Task->UserRflags,
                Task->UserStackPointer,
                Task->UserStackSegmentSelector);
            Process->UserTransitionFrameStackPointer = FrameStackPointer;
            Task->UserTransitionFrameStackPointer = FrameStackPointer;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY;
            State->UserTransitionFrameReadyCount += 1ULL;
            FrameReady = 1U;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (FrameReady != 0U)
    {
        LosKernelTraceOk("Scheduler first user task frame-ready.");
        LosKernelSchedulerTraceProcess("Frame-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Frame-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}
