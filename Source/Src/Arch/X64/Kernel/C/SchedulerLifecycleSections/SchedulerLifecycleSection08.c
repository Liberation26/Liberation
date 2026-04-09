/*
 * File Name: SchedulerLifecycleSection08.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerLifecycle.c.
 */

BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldTrampolineReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    UINT64 KernelEntryAddress;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionFrameReadyCount == 0ULL)
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

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY)
    {
        return 1;
    }

    KernelEntryAddress = (UINT64)(UINTN)LosKernelSchedulerUserTransitionKernelEntry;
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Task->ExecutionContext.StackPointer == 0ULL ||
        Process->UserTransitionFrameStackPointer == 0ULL ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        KernelEntryAddress == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->ExecutionContext.StackPointer != 0ULL &&
        Process->UserTransitionFrameStackPointer != 0ULL &&
        Task->UserTransitionFrameStackPointer != 0ULL &&
        KernelEntryAddress != 0ULL)
    {
        WriteStackReturnAddress(Task->ExecutionContext.StackPointer, KernelEntryAddress);
        if (ReadStackReturnAddress(Task->ExecutionContext.StackPointer) == KernelEntryAddress)
        {
            Process->UserTransitionKernelEntryVirtualAddress = KernelEntryAddress;
            Task->UserTransitionKernelEntryVirtualAddress = KernelEntryAddress;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY;
            State->UserTransitionTrampolineReadyCount += 1ULL;
            Ready = 1U;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task trampoline-ready.");
        LosKernelSchedulerTraceProcess("Trampoline-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Trampoline-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}

BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldBridgeReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 BridgeAddress;
    UINT64 CriticalSectionFlags;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionTrampolineReadyCount == 0ULL)
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

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY)
    {
        return 1;
    }

    BridgeAddress = (UINT64)(UINTN)LosKernelSchedulerUserTransitionDispatchBridge;
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Task->ExecutionContext.StackPointer == 0ULL ||
        Process->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Task->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        BridgeAddress == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->ExecutionContext.StackPointer != 0ULL &&
        Process->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Task->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        BridgeAddress != 0ULL)
    {
        WriteStackReturnAddress(Task->ExecutionContext.StackPointer, BridgeAddress);
        if (ReadStackReturnAddress(Task->ExecutionContext.StackPointer) == BridgeAddress)
        {
            Process->UserTransitionBridgeVirtualAddress = BridgeAddress;
            Task->UserTransitionBridgeVirtualAddress = BridgeAddress;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY;
            State->UserTransitionBridgeReadyCount += 1ULL;
            Ready = 1U;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task bridge-ready.");
        LosKernelSchedulerTraceProcess("Bridge-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Bridge-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldChainReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 ChainStackPointer;
    UINT64 CriticalSectionFlags;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionBridgeReadyCount == 0ULL)
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

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY)
    {
        return 1;
    }

    ChainStackPointer = GetUserTransitionChainStackPointer(Task);
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Process->UserTransitionFrameStackPointer == 0ULL ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Process->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Task->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Process->UserTransitionBridgeVirtualAddress == 0ULL ||
        Task->UserTransitionBridgeVirtualAddress == 0ULL ||
        ChainStackPointer == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Process->UserTransitionFrameStackPointer != 0ULL &&
        Task->UserTransitionFrameStackPointer != 0ULL &&
        Process->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Task->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Process->UserTransitionBridgeVirtualAddress != 0ULL &&
        Task->UserTransitionBridgeVirtualAddress != 0ULL)
    {
        ChainStackPointer = GetUserTransitionChainStackPointer(Task);
        if (ChainStackPointer != 0ULL)
        {
            WriteStackReturnAddress(ChainStackPointer, Task->UserTransitionBridgeVirtualAddress);
            WriteStackReturnAddress(ChainStackPointer + 8ULL, Task->UserTransitionKernelEntryVirtualAddress);
            if (ReadStackReturnAddress(ChainStackPointer) == Task->UserTransitionBridgeVirtualAddress &&
                ReadStackReturnAddress(ChainStackPointer + 8ULL) == Task->UserTransitionKernelEntryVirtualAddress)
            {
                Task->ExecutionContext.StackPointer = ChainStackPointer;
                Process->UserTransitionChainStackPointer = ChainStackPointer;
                Task->UserTransitionChainStackPointer = ChainStackPointer;
                Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY;
                Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY;
                State->UserTransitionChainReadyCount += 1ULL;
                Ready = 1U;
            }
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task chain-ready.");
        LosKernelSchedulerTraceProcess("Chain-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Chain-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldContractReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    UINT64 ContractSignature;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionChainReadyCount == 0ULL)
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

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY)
    {
        return 1;
    }

    ContractSignature = ComputeUserTransitionContractSignature(Process, Task);
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        ContractSignature == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        ContractSignature = ComputeUserTransitionContractSignature(Process, Task);
        if (ContractSignature != 0ULL)
        {
            Process->UserTransitionContractSignature = ContractSignature;
            Task->UserTransitionContractSignature = ContractSignature;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY;
            State->UserTransitionContractReadyCount += 1ULL;
            Ready = 1U;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task contract-ready.");
        LosKernelSchedulerTraceProcess("Contract-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Contract-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}
