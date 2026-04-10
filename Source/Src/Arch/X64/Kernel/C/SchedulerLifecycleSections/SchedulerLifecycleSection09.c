/*
 * File Name: SchedulerLifecycleSection09.c
 * File Version: 0.0.3
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-10T18:15:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerLifecycle.c.
 */

BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldSealReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    UINT64 SealValue;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionContractReadyCount == 0ULL)
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

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY)
    {
        return 1;
    }

    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->ExecutionContext.StackPointer != 0ULL &&
        Task->UserInstructionPointer != 0ULL &&
        Task->UserStackPointer != 0ULL)
    {
        SealValue = 0x5345414C00000000ULL;
        SealValue ^= Process->ProcessId;
        SealValue ^= (Task->TaskId << 9U);
        SealValue ^= Task->ExecutionContext.StackPointer;
        SealValue ^= Task->UserInstructionPointer;
        SealValue ^= Task->UserStackPointer;
        if (SealValue == 0ULL)
        {
            SealValue = 0x5345414C56414C55ULL;
        }

        Process->UserTransitionSealValue = SealValue;
        Task->UserTransitionSealValue = SealValue;
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY;
        State->UserTransitionSealReadyCount += 1ULL;
        Ready = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task seal-ready.");
        LosKernelSchedulerTraceProcess("Seal-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Seal-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldHandoffReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    UINT64 HandoffStackPointer;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionSealReadyCount == 0ULL)
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

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY)
    {
        return 1;
    }

    HandoffStackPointer = Task->ExecutionContext.StackPointer;
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Process->UserTransitionChainStackPointer == 0ULL ||
        Task->UserTransitionChainStackPointer == 0ULL ||
        HandoffStackPointer == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        HandoffStackPointer = Task->ExecutionContext.StackPointer;
        if (HandoffStackPointer != 0ULL)
        {
            Process->UserTransitionHandoffStackPointer = HandoffStackPointer;
            Task->UserTransitionHandoffStackPointer = HandoffStackPointer;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY;
            State->UserTransitionHandoffReadyCount += 1ULL;
            Ready = 1U;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task handoff-ready.");
        LosKernelSchedulerTraceProcess("Handoff-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Handoff-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldComplete(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    /*
     * Compatibility shim for older lifecycle callers: do not mark the
     * first user task COMPLETE before it ever reaches the real CPL3 entry
     * path. Promote the handoff-ready scaffold to LIVE instead.
     */
    if (State->UserTransitionLiveCount == 0ULL &&
        State->UserTransitionCompleteCount == 0ULL &&
        State->UserTransitionHandoffReadyCount != 0ULL)
    {
        if (State->UserTransitionLiveGateClosed != 0U)
        {
            State->UserTransitionLiveGateClosed = 0U;
        }
        return LosKernelSchedulerMarkUserTransitionScaffoldLive();
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldLive(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN Live;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionHandoffReadyCount == 0ULL ||
        State->UserTransitionCompleteCount != 0ULL)
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

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE)
    {
        return 1;
    }

    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY ||
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
        Process->UserTransitionFrameStackPointer == 0ULL ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Process->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Task->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Process->UserTransitionBridgeVirtualAddress == 0ULL ||
        Task->UserTransitionBridgeVirtualAddress == 0ULL ||
        Process->UserTransitionChainStackPointer == 0ULL ||
        Task->UserTransitionChainStackPointer == 0ULL ||
        Process->UserTransitionContractSignature == 0ULL ||
        Task->UserTransitionContractSignature == 0ULL ||
        Process->UserTransitionSealValue == 0ULL ||
        Task->UserTransitionSealValue == 0ULL ||
        Process->UserTransitionHandoffStackPointer == 0ULL ||
        Task->UserTransitionHandoffStackPointer == 0ULL ||
        Process->UserTransitionHandoffStackPointer != Task->UserTransitionHandoffStackPointer ||
        Task->ExecutionContext.StackPointer != Task->UserTransitionHandoffStackPointer)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Live = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY &&
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
        Task->UserRflags != 0ULL &&
        Process->UserTransitionFrameStackPointer != 0ULL &&
        Task->UserTransitionFrameStackPointer != 0ULL &&
        Process->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Task->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Process->UserTransitionBridgeVirtualAddress != 0ULL &&
        Task->UserTransitionBridgeVirtualAddress != 0ULL &&
        Process->UserTransitionChainStackPointer != 0ULL &&
        Task->UserTransitionChainStackPointer != 0ULL &&
        Process->UserTransitionContractSignature != 0ULL &&
        Task->UserTransitionContractSignature != 0ULL &&
        Process->UserTransitionSealValue != 0ULL &&
        Task->UserTransitionSealValue != 0ULL &&
        Process->UserTransitionHandoffStackPointer != 0ULL &&
        Task->UserTransitionHandoffStackPointer != 0ULL &&
        Process->UserTransitionHandoffStackPointer == Task->UserTransitionHandoffStackPointer &&
        Task->ExecutionContext.StackPointer == Task->UserTransitionHandoffStackPointer)
    {
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE;
        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
        Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_NONE;
        Task->ReadySinceTick = State->TickCount;
        Task->LastWakeTick = State->TickCount;
        Task->WakeDispatchPending = 1U;
        Task->ResumeBoostTicks = 4U;
        Task->NextWakeTick = 0ULL;
        Task->RemainingQuantumTicks = Task->QuantumTicks;
        State->UserTransitionLiveCount += 1ULL;
        State->UserTransitionLiveGateClosed = 0U;
        State->UserTransitionCompleteCount = 0ULL;
        Live = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Live != 0U)
    {
        LosKernelTraceOk("Scheduler first user task marked live for real iretq dispatch.");
        LosKernelTraceHex64("Scheduler first user task live stack pointer: ", Task->ExecutionContext.StackPointer);
        LosKernelTraceHex64("Scheduler first user task live return slot: ",
                            ReadStackReturnAddress(Task->ExecutionContext.StackPointer));
        LosKernelTraceHex64("Scheduler first user task live next return slot: ",
                            ReadStackReturnAddress(Task->ExecutionContext.StackPointer + 8ULL));
        LosKernelSchedulerTraceProcess("Live scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Live scheduler first user task task", Task);
        return 1;
    }

    return 0;
}
