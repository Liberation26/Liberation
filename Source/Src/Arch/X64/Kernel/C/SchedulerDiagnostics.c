#include "SchedulerInternal.h"

#undef LosKernelTrace
#undef LosKernelTraceOk
#undef LosKernelTraceFail
#undef LosKernelTraceHex64
#undef LosKernelTraceUnsigned

BOOLEAN LosKernelSchedulerShouldEmitSerial(void)
{
    const LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerGetState();
    if (State == 0)
    {
        return 0U;
    }

    return (State->UserTransitionSealReadyCount != 0ULL ||
            State->UserTransitionCompleteCount != 0ULL ||
            State->UserTransitionLiveCount != 0ULL)
        ? 1U
        : 0U;
}

void LosKernelSchedulerEmitTrace(const char *Text)
{
    if (LosKernelSchedulerShouldEmitSerial() == 0U)
    {
        return;
    }

    LosKernelTrace(Text);
}

void LosKernelSchedulerEmitTraceOk(const char *Text)
{
    if (LosKernelSchedulerShouldEmitSerial() == 0U)
    {
        return;
    }

    LosKernelTraceOk(Text);
}

void LosKernelSchedulerEmitTraceFail(const char *Text)
{
    LosKernelTraceFail(Text);
}

void LosKernelSchedulerEmitTraceHex64(const char *Prefix, UINT64 Value)
{
    LosKernelTraceHex64(Prefix, Value);
}

void LosKernelSchedulerEmitTraceUnsigned(const char *Prefix, UINT64 Value)
{
    LosKernelTraceUnsigned(Prefix, Value);
}

static void WriteName(const char *Name)
{
    LosKernelSerialWriteText(Name != 0 ? Name : "<unnamed>");
}

void LosKernelSchedulerTraceProcess(const char *Prefix, const LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    if (Process == 0 || LosKernelSchedulerShouldEmitSerial() == 0U)
    {
        return;
    }

    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Prefix != 0 ? Prefix : "Scheduler process");
    LosKernelSerialWriteText(": id=");
    LosKernelSerialWriteUnsigned(Process->ProcessId);
    LosKernelSerialWriteText(" owner=");
    LosKernelSerialWriteUnsigned(Process->OwnerProcessId);
    LosKernelSerialWriteText(" generation=");
    LosKernelSerialWriteUnsigned(Process->Generation);
    LosKernelSerialWriteText(" name=");
    WriteName(Process->Name);
    LosKernelSerialWriteText(" state=");
    LosKernelSerialWriteUnsigned(Process->State);
    LosKernelSerialWriteText(" flags=");
    LosKernelSerialWriteUnsigned(Process->Flags);
    LosKernelSerialWriteText(" threads=");
    LosKernelSerialWriteUnsigned(Process->ThreadCount);
    LosKernelSerialWriteText(" address-space=");
    LosKernelSerialWriteUnsigned(Process->AddressSpaceId);
    LosKernelSerialWriteText(" address-space-object=");
    LosKernelSerialWriteHex64(Process->AddressSpaceObjectPhysicalAddress);
    LosKernelSerialWriteText(" dispatches=");
    LosKernelSerialWriteUnsigned(Process->DispatchCount);
    LosKernelSerialWriteText(" total-ticks=");
    LosKernelSerialWriteUnsigned(Process->TotalTicks);
    LosKernelSerialWriteText(" last-run=");
    LosKernelSerialWriteUnsigned(Process->LastRunTick);
    LosKernelSerialWriteText(" max-ready-delay=");
    LosKernelSerialWriteUnsigned(Process->MaxReadyDelayTicks);
    LosKernelSerialWriteText(" max-wake-delay=");
    LosKernelSerialWriteUnsigned(Process->MaxWakeDelayTicks);
    LosKernelSerialWriteText(" last-run-slice=");
    LosKernelSerialWriteUnsigned(Process->LastRunSliceTicks);
    LosKernelSerialWriteText(" max-run-slice=");
    LosKernelSerialWriteUnsigned(Process->MaxRunSliceTicks);
    LosKernelSerialWriteText(" user-entry=");
    LosKernelSerialWriteHex64(Process->UserEntryVirtualAddress);
    LosKernelSerialWriteText(" user-stack=");
    LosKernelSerialWriteHex64(Process->UserStackTopVirtualAddress);
    LosKernelSerialWriteText(" user-cs=");
    LosKernelSerialWriteHex64(Process->UserCodeSegmentSelector);
    LosKernelSerialWriteText(" user-ss=");
    LosKernelSerialWriteHex64(Process->UserStackSegmentSelector);
    LosKernelSerialWriteText(" user-rflags=");
    LosKernelSerialWriteHex64(Process->UserRflags);
    LosKernelSerialWriteText(" user-frame-sp=");
    LosKernelSerialWriteHex64(Process->UserTransitionFrameStackPointer);
    LosKernelSerialWriteText(" user-kentry=");
    LosKernelSerialWriteHex64(Process->UserTransitionKernelEntryVirtualAddress);
    LosKernelSerialWriteText(" user-bridge=");
    LosKernelSerialWriteHex64(Process->UserTransitionBridgeVirtualAddress);
    LosKernelSerialWriteText(" user-chain-sp=");
    LosKernelSerialWriteHex64(Process->UserTransitionChainStackPointer);
    LosKernelSerialWriteText(" user-contract=");
    LosKernelSerialWriteHex64(Process->UserTransitionContractSignature);
    LosKernelSerialWriteText(" user-seal=");
    LosKernelSerialWriteHex64(Process->UserTransitionSealValue);
    LosKernelSerialWriteText(" user-handoff-sp=");
    LosKernelSerialWriteHex64(Process->UserTransitionHandoffStackPointer);
    LosKernelSerialWriteText(" user-state=");
    LosKernelSerialWriteUnsigned(Process->UserTransitionState);
    LosKernelSerialWriteText(" root=");
    LosKernelSerialWriteHex64(Process->RootTablePhysicalAddress);
    LosKernelSerialWriteText(" inherited-root=");
    LosKernelSerialWriteUnsigned((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT) != 0U ? 1U : 0U);
    LosKernelSerialWriteText(" owns-address-space=");
    LosKernelSerialWriteUnsigned((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) != 0U ? 1U : 0U);
    LosKernelSerialWriteText(" bind-in-progress=");
    LosKernelSerialWriteUnsigned((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS) != 0U ? 1U : 0U);
    LosKernelSerialWriteText(" create-in-progress=");
    LosKernelSerialWriteUnsigned((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_CREATE_IN_PROGRESS) != 0U ? 1U : 0U);
    LosKernelSerialWriteText(" cleanup=");
    LosKernelSerialWriteUnsigned(Process->CleanupPending);
    LosKernelSerialWriteText(" exit=");
    LosKernelSerialWriteUnsigned(Process->ExitStatus);
    LosKernelSerialWriteText("\n");
}

void LosKernelSchedulerTraceTask(const char *Prefix, const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    if (Task == 0 || LosKernelSchedulerShouldEmitSerial() == 0U)
    {
        return;
    }

    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Prefix != 0 ? Prefix : "Scheduler task");
    LosKernelSerialWriteText(": id=");
    LosKernelSerialWriteUnsigned(Task->TaskId);
    LosKernelSerialWriteText(" owner=");
    LosKernelSerialWriteUnsigned(Task->OwnerTaskId);
    LosKernelSerialWriteText(" process=");
    LosKernelSerialWriteUnsigned(Task->ProcessId);
    LosKernelSerialWriteText(" generation=");
    LosKernelSerialWriteUnsigned(Task->Generation);
    LosKernelSerialWriteText(" name=");
    WriteName(Task->Name);
    LosKernelSerialWriteText(" state=");
    LosKernelSerialWriteUnsigned(Task->State);
    LosKernelSerialWriteText(" priority=");
    LosKernelSerialWriteUnsigned(Task->Priority);
    LosKernelSerialWriteText(" flags=");
    LosKernelSerialWriteUnsigned(Task->Flags);
    LosKernelSerialWriteText(" stack-base=");
    LosKernelSerialWriteHex64(Task->StackBaseVirtualAddress);
    LosKernelSerialWriteText(" stack-top=");
    LosKernelSerialWriteHex64(Task->StackTopVirtualAddress);
    LosKernelSerialWriteText(" dispatches=");
    LosKernelSerialWriteUnsigned(Task->DispatchCount);
    LosKernelSerialWriteText(" total-ticks=");
    LosKernelSerialWriteUnsigned(Task->TotalTicks);
    LosKernelSerialWriteText(" ready-since=");
    LosKernelSerialWriteUnsigned(Task->ReadySinceTick);
    LosKernelSerialWriteText(" max-ready-delay=");
    LosKernelSerialWriteUnsigned(Task->MaxReadyDelayTicks);
    LosKernelSerialWriteText(" max-wake-delay=");
    LosKernelSerialWriteUnsigned(Task->MaxWakeDelayTicks);
    LosKernelSerialWriteText(" last-run-slice=");
    LosKernelSerialWriteUnsigned(Task->LastRunSliceTicks);
    LosKernelSerialWriteText(" max-run-slice=");
    LosKernelSerialWriteUnsigned(Task->MaxRunSliceTicks);
    LosKernelSerialWriteText(" user-entry=");
    LosKernelSerialWriteHex64(Task->UserInstructionPointer);
    LosKernelSerialWriteText(" user-stack=");
    LosKernelSerialWriteHex64(Task->UserStackPointer);
    LosKernelSerialWriteText(" user-cs=");
    LosKernelSerialWriteHex64(Task->UserCodeSegmentSelector);
    LosKernelSerialWriteText(" user-ss=");
    LosKernelSerialWriteHex64(Task->UserStackSegmentSelector);
    LosKernelSerialWriteText(" user-rflags=");
    LosKernelSerialWriteHex64(Task->UserRflags);
    LosKernelSerialWriteText(" user-frame-sp=");
    LosKernelSerialWriteHex64(Task->UserTransitionFrameStackPointer);
    LosKernelSerialWriteText(" user-kentry=");
    LosKernelSerialWriteHex64(Task->UserTransitionKernelEntryVirtualAddress);
    LosKernelSerialWriteText(" user-bridge=");
    LosKernelSerialWriteHex64(Task->UserTransitionBridgeVirtualAddress);
    LosKernelSerialWriteText(" user-chain-sp=");
    LosKernelSerialWriteHex64(Task->UserTransitionChainStackPointer);
    LosKernelSerialWriteText(" user-contract=");
    LosKernelSerialWriteHex64(Task->UserTransitionContractSignature);
    LosKernelSerialWriteText(" user-seal=");
    LosKernelSerialWriteHex64(Task->UserTransitionSealValue);
    LosKernelSerialWriteText(" user-handoff-sp=");
    LosKernelSerialWriteHex64(Task->UserTransitionHandoffStackPointer);
    LosKernelSerialWriteText(" user-state=");
    LosKernelSerialWriteUnsigned(Task->UserTransitionState);
    LosKernelSerialWriteText(" last-wake=");
    LosKernelSerialWriteUnsigned(Task->LastWakeTick);
    LosKernelSerialWriteText(" preemptions=");
    LosKernelSerialWriteUnsigned(Task->PreemptionCount);
    LosKernelSerialWriteText(" wake-pending=");
    LosKernelSerialWriteUnsigned(Task->WakeDispatchPending);
    LosKernelSerialWriteText(" resume-boost=");
    LosKernelSerialWriteUnsigned(Task->ResumeBoostTicks);
    LosKernelSerialWriteText(" stack-source=");
    LosKernelSerialWriteUnsigned(Task->StackAllocationSource);
    LosKernelSerialWriteText(" cleanup=");
    LosKernelSerialWriteUnsigned(Task->CleanupPending);
    LosKernelSerialWriteText(" exit=");
    LosKernelSerialWriteUnsigned(Task->ExitStatus);
    LosKernelSerialWriteText("\n");
}

void LosKernelSchedulerTraceState(const char *Prefix)
{
    const LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerGetState();
    if (State == 0 || LosKernelSchedulerShouldEmitSerial() == 0U)
    {
        return;
    }

    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Prefix != 0 ? Prefix : "Scheduler state");
    LosKernelSerialWriteText(": ticks=");
    LosKernelSerialWriteUnsigned(State->TickCount);
    LosKernelSerialWriteText(" dispatches=");
    LosKernelSerialWriteUnsigned(State->DispatchCount);
    LosKernelSerialWriteText(" preemptions=");
    LosKernelSerialWriteUnsigned(State->InterruptPreemptionCount);
    LosKernelSerialWriteText(" starvation-relief=");
    LosKernelSerialWriteUnsigned(State->StarvationReliefDispatchCount);
    LosKernelSerialWriteText(" tasks=");
    LosKernelSerialWriteUnsigned(State->TaskCount);
    LosKernelSerialWriteText(" processes=");
    LosKernelSerialWriteUnsigned(State->ProcessCount);
    LosKernelSerialWriteText(" created=");
    LosKernelSerialWriteUnsigned(State->CreatedTaskCount);
    LosKernelSerialWriteText(" terminated=");
    LosKernelSerialWriteUnsigned(State->TerminatedTaskCount);
    LosKernelSerialWriteText(" reaped=");
    LosKernelSerialWriteUnsigned(State->ReapedTaskCount);
    LosKernelSerialWriteText(" proc-created=");
    LosKernelSerialWriteUnsigned(State->CreatedProcessCount);
    LosKernelSerialWriteText(" proc-terminated=");
    LosKernelSerialWriteUnsigned(State->TerminatedProcessCount);
    LosKernelSerialWriteText(" proc-reaped=");
    LosKernelSerialWriteUnsigned(State->ReapedProcessCount);
    LosKernelSerialWriteText(" root-switches=");
    LosKernelSerialWriteUnsigned(State->AddressSpaceSwitchCount);
    LosKernelSerialWriteText(" root-reuse=");
    LosKernelSerialWriteUnsigned(State->AddressSpaceReuseCount);
    LosKernelSerialWriteText(" bind-count=");
    LosKernelSerialWriteUnsigned(State->AddressSpaceBindCount);
    LosKernelSerialWriteText(" bind-deferred=");
    LosKernelSerialWriteUnsigned(State->AddressSpaceBindDeferredCount);
    LosKernelSerialWriteText(" wakeups=");
    LosKernelSerialWriteUnsigned(State->WakeupCount);
    LosKernelSerialWriteText(" wake-dispatch=");
    LosKernelSerialWriteUnsigned(State->WakePriorityDispatchCount);
    LosKernelSerialWriteText(" resume-window=");
    LosKernelSerialWriteUnsigned(State->WakeResumeWindowDispatchCount);
    LosKernelSerialWriteText(" max-ready-delay=");
    LosKernelSerialWriteUnsigned(State->MaxReadyDelayTicks);
    LosKernelSerialWriteText(" max-wake-delay=");
    LosKernelSerialWriteUnsigned(State->MaxWakeDelayTicks);
    LosKernelSerialWriteText(" max-run-slice=");
    LosKernelSerialWriteUnsigned(State->MaxRunSliceTicks);
    LosKernelSerialWriteText(" idle-ticks=");
    LosKernelSerialWriteUnsigned(State->IdleTicks);
    LosKernelSerialWriteText(" busy-ticks=");
    LosKernelSerialWriteUnsigned(State->BusyTicks);
    LosKernelSerialWriteText(" user-scaffold-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldReady);
    LosKernelSerialWriteText(" user-scaffold-prepared=");
    LosKernelSerialWriteUnsigned(State->UserTransitionPreparedCount);
    LosKernelSerialWriteText(" user-scaffold-validated=");
    LosKernelSerialWriteUnsigned(State->UserTransitionValidatedCount);
    LosKernelSerialWriteText(" user-scaffold-armed=");
    LosKernelSerialWriteUnsigned(State->UserTransitionArmedCount);
    LosKernelSerialWriteText(" user-scaffold-requested=");
    LosKernelSerialWriteUnsigned(State->UserTransitionLaunchRequestCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-scaffold-entry-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionEntryReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-scaffold-descriptor-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionDescriptorReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-scaffold-frame-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionFrameReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-scaffold-trampoline-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionTrampolineReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-scaffold-bridge-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionBridgeReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-scaffold-chain-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionChainReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-scaffold-contract-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionContractReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-scaffold-seal-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionSealReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-scaffold-handoff-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionHandoffReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-scaffold-complete=");
    LosKernelSerialWriteUnsigned(State->UserTransitionCompleteCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-scaffold-live=");
    LosKernelSerialWriteUnsigned(State->UserTransitionLiveCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" user-live-gate-closed=");
    LosKernelSerialWriteUnsigned(State->UserTransitionLiveGateClosed);
    LosKernelSerialWriteText(" user-scaffold-blocked=");
    LosKernelSerialWriteUnsigned(LosKernelSchedulerIsUserTransitionScaffoldBlocked());
    LosKernelSerialWriteText(" user-scaffold-reblocked=");
    LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldReblockCount);
    LosKernelSerialWriteText(" user-dispatch-skip=");
    LosKernelSerialWriteUnsigned(State->UserTransitionDispatchSkipCount);
    LosKernelSerialWriteText(" user-scaffold-proc=");
    LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldProcessId);
    LosKernelSerialWriteText(" user-scaffold-task=");
    LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldTaskId);
    LosKernelSerialWriteText(" stack-pool-ready=");
    LosKernelSerialWriteUnsigned(State->DirectClaimStackPoolReady);
    LosKernelSerialWriteText(" stack-pool-used=");
    LosKernelSerialWriteUnsigned(State->DirectClaimStackSlotsInUse);
    LosKernelSerialWriteText(" active-root=");
    LosKernelSerialWriteHex64(State->ActiveRootTablePhysicalAddress);
    LosKernelSerialWriteText(" current=");
    LosKernelSerialWriteUnsigned(State->CurrentTaskId);
    LosKernelSerialWriteText(" current-proc=");
    LosKernelSerialWriteUnsigned(State->CurrentProcessId);
    LosKernelSerialWriteText("\n");
}

void LosKernelSchedulerIdleThread(void *Context)
{
    (void)Context;

    for (;;)
    {
        __asm__ __volatile__("hlt" : : : "memory");
        if (LosKernelSchedulerState()->ReschedulePending != 0U)
        {
            LosKernelSchedulerYieldCurrent();
        }
    }
}

void LosKernelSchedulerHeartbeatThread(void *Context)
{
    const LOS_KERNEL_SCHEDULER_STATE *State;

    (void)Context;
    for (;;)
    {
        State = LosKernelSchedulerGetState();
        if (State != 0 && LosKernelSchedulerShouldEmitSerial() != 0U)
        {
            LosKernelSerialWriteText("[Kernel] Scheduler heartbeat ticks=");
            LosKernelSerialWriteUnsigned(State->TickCount);
            LosKernelSerialWriteText(" dispatches=");
            LosKernelSerialWriteUnsigned(State->DispatchCount);
            LosKernelSerialWriteText(" tasks=");
            LosKernelSerialWriteUnsigned(State->TaskCount);
            LosKernelSerialWriteText(" processes=");
            LosKernelSerialWriteUnsigned(State->ProcessCount);
            LosKernelSerialWriteText(" starvation-relief=");
            LosKernelSerialWriteUnsigned(State->StarvationReliefDispatchCount);
            LosKernelSerialWriteText(" created=");
            LosKernelSerialWriteUnsigned(State->CreatedTaskCount);
            LosKernelSerialWriteText(" terminated=");
            LosKernelSerialWriteUnsigned(State->TerminatedTaskCount);
            LosKernelSerialWriteText(" reaped=");
            LosKernelSerialWriteUnsigned(State->ReapedTaskCount);
            LosKernelSerialWriteText(" proc-created=");
            LosKernelSerialWriteUnsigned(State->CreatedProcessCount);
            LosKernelSerialWriteText(" proc-terminated=");
            LosKernelSerialWriteUnsigned(State->TerminatedProcessCount);
            LosKernelSerialWriteText(" proc-reaped=");
            LosKernelSerialWriteUnsigned(State->ReapedProcessCount);
            LosKernelSerialWriteText(" root-switches=");
            LosKernelSerialWriteUnsigned(State->AddressSpaceSwitchCount);
            LosKernelSerialWriteText(" root-reuse=");
            LosKernelSerialWriteUnsigned(State->AddressSpaceReuseCount);
            LosKernelSerialWriteText(" bind-count=");
            LosKernelSerialWriteUnsigned(State->AddressSpaceBindCount);
            LosKernelSerialWriteText(" bind-deferred=");
            LosKernelSerialWriteUnsigned(State->AddressSpaceBindDeferredCount);
            LosKernelSerialWriteText(" wakeups=");
            LosKernelSerialWriteUnsigned(State->WakeupCount);
            LosKernelSerialWriteText(" wake-dispatch=");
            LosKernelSerialWriteUnsigned(State->WakePriorityDispatchCount);
            LosKernelSerialWriteText(" resume-window=");
            LosKernelSerialWriteUnsigned(State->WakeResumeWindowDispatchCount);
            LosKernelSerialWriteText(" max-ready-delay=");
            LosKernelSerialWriteUnsigned(State->MaxReadyDelayTicks);
            LosKernelSerialWriteText(" max-wake-delay=");
            LosKernelSerialWriteUnsigned(State->MaxWakeDelayTicks);
            LosKernelSerialWriteText(" max-run-slice=");
            LosKernelSerialWriteUnsigned(State->MaxRunSliceTicks);
            LosKernelSerialWriteText(" idle-ticks=");
            LosKernelSerialWriteUnsigned(State->IdleTicks);
            LosKernelSerialWriteText(" busy-ticks=");
            LosKernelSerialWriteUnsigned(State->BusyTicks);
            LosKernelSerialWriteText(" user-scaffold-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldReady);
            LosKernelSerialWriteText(" user-scaffold-prepared=");
            LosKernelSerialWriteUnsigned(State->UserTransitionPreparedCount);
            LosKernelSerialWriteText(" user-scaffold-validated=");
            LosKernelSerialWriteUnsigned(State->UserTransitionValidatedCount);
            LosKernelSerialWriteText(" user-scaffold-armed=");
            LosKernelSerialWriteUnsigned(State->UserTransitionArmedCount);
            LosKernelSerialWriteText(" user-scaffold-requested=");
            LosKernelSerialWriteUnsigned(State->UserTransitionLaunchRequestCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-scaffold-entry-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionEntryReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-scaffold-descriptor-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionDescriptorReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-scaffold-frame-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionFrameReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-scaffold-trampoline-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionTrampolineReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-scaffold-bridge-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionBridgeReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-scaffold-chain-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionChainReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-scaffold-contract-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionContractReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-scaffold-seal-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionSealReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-scaffold-handoff-ready=");
            LosKernelSerialWriteUnsigned(State->UserTransitionHandoffReadyCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-scaffold-complete=");
            LosKernelSerialWriteUnsigned(State->UserTransitionCompleteCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-scaffold-live=");
            LosKernelSerialWriteUnsigned(State->UserTransitionLiveCount != 0ULL ? 1ULL : 0ULL);
            LosKernelSerialWriteText(" user-live-gate-closed=");
            LosKernelSerialWriteUnsigned(State->UserTransitionLiveGateClosed);
            LosKernelSerialWriteText(" user-scaffold-blocked=");
            LosKernelSerialWriteUnsigned(LosKernelSchedulerIsUserTransitionScaffoldBlocked());
            LosKernelSerialWriteText(" user-scaffold-reblocked=");
            LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldReblockCount);
            LosKernelSerialWriteText(" user-dispatch-skip=");
            LosKernelSerialWriteUnsigned(State->UserTransitionDispatchSkipCount);
            LosKernelSerialWriteText(" user-scaffold-proc=");
            LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldProcessId);
            LosKernelSerialWriteText(" user-scaffold-task=");
            LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldTaskId);
            LosKernelSerialWriteText(" stack-pool-ready=");
            LosKernelSerialWriteUnsigned(State->DirectClaimStackPoolReady);
            LosKernelSerialWriteText(" stack-pool-used=");
            LosKernelSerialWriteUnsigned(State->DirectClaimStackSlotsInUse);
            LosKernelSerialWriteText("\n");
        }

        LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_HEARTBEAT_PERIOD_TICKS);
    }
}

void LosKernelSchedulerLifecycleThread(void *Context)
{
    UINT64 Sequence;

    (void)Context;
    Sequence = 0ULL;
    if (LosKernelSchedulerShouldEmitSerial() != 0U)
    {
        LosKernelSerialWriteText("[Kernel] Scheduler lifecycle manager entered.\n");
    }

    for (;;)
    {
        UINT64 ProcessId;
        UINT64 TaskId;

        if (LosKernelSchedulerState()->UserTransitionScaffoldReady == 0U)
        {
            (void)LosKernelSchedulerPrepareUserTransitionScaffold();
        }
        else if (LosKernelSchedulerState()->UserTransitionValidatedCount == 0ULL)
        {
            (void)LosKernelSchedulerValidateUserTransitionScaffold();
        }
        else if (LosKernelSchedulerState()->UserTransitionArmedCount == 0ULL)
        {
            (void)LosKernelSchedulerArmUserTransitionScaffold();
        }
        else if (LosKernelSchedulerState()->UserTransitionLaunchRequestCount == 0ULL)
        {
            (void)LosKernelSchedulerRequestUserTransitionScaffold();
        }
        else if (LosKernelSchedulerState()->UserTransitionEntryReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldEntryReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionDescriptorReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldDescriptorReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionFrameReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldFrameReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionTrampolineReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldTrampolineReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionBridgeReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldBridgeReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionChainReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldChainReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionContractReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldContractReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionSealReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldSealReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionHandoffReadyCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldHandoffReady();
        }
        else if (LosKernelSchedulerState()->UserTransitionLiveGateClosed == 0U &&
                 LosKernelSchedulerState()->UserTransitionLiveCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldLiveGateClosed();
        }
        else if (LosKernelSchedulerState()->UserTransitionCompleteCount == 0ULL &&
                 LosKernelSchedulerState()->UserTransitionLiveGateClosed != 0U &&
                 LosKernelSchedulerState()->UserTransitionLiveCount == 0ULL)
        {
            (void)LosKernelSchedulerMarkUserTransitionScaffoldComplete();
        }

        if (LosKernelSchedulerState()->UserTransitionScaffoldReady != 0U &&
            LosKernelSchedulerState()->UserTransitionLiveCount == 0ULL)
        {
            (void)LosKernelSchedulerGuardUserTransitionScaffold();
        }

        if (LosKernelSchedulerHasActiveTransientProcess() != 0U)
        {
            LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS);
            continue;
        }

        Sequence += 1ULL;
        ProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
        TaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;

        if (!LosKernelSchedulerCreateProcess(
                "EphemeralProcess",
                LOS_KERNEL_SCHEDULER_PROCESS_FLAG_TRANSIENT | LOS_KERNEL_SCHEDULER_PROCESS_FLAG_REQUIRE_OWN_ADDRESS_SPACE,
                0ULL,
                LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS,
                &ProcessId))
        {
            if (LosKernelSchedulerShouldEmitSerial() != 0U)
            {
                LosKernelSerialWriteText("[Kernel] Scheduler lifecycle could not create ephemeral process sequence=");
                LosKernelSerialWriteUnsigned(Sequence);
                LosKernelSerialWriteText("\n");
            }
            LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS);
            continue;
        }

        if (LosKernelSchedulerCreateTask(
                "EphemeralWorker",
                ProcessId,
                0U,
                LOS_KERNEL_SCHEDULER_EPHEMERAL_PRIORITY,
                1U,
                0ULL,
                LosKernelSchedulerEphemeralThread,
                (void *)(UINTN)Sequence,
                &TaskId))
        {
            if (LosKernelSchedulerShouldEmitSerial() != 0U)
            {
                LosKernelSerialWriteText("[Kernel] Scheduler lifecycle spawned ephemeral process id=");
                LosKernelSerialWriteUnsigned(ProcessId);
                LosKernelSerialWriteText(" task=");
                LosKernelSerialWriteUnsigned(TaskId);
                LosKernelSerialWriteText(" sequence=");
                LosKernelSerialWriteUnsigned(Sequence);
                LosKernelSerialWriteText("\n");
            }
        }
        else
        {
            LosKernelSchedulerMarkProcessTerminated(ProcessId, 1ULL);
            if (LosKernelSchedulerShouldEmitSerial() != 0U)
            {
                LosKernelSerialWriteText("[Kernel] Scheduler lifecycle could not spawn ephemeral task for process id=");
                LosKernelSerialWriteUnsigned(ProcessId);
                LosKernelSerialWriteText(" sequence=");
                LosKernelSerialWriteUnsigned(Sequence);
                LosKernelSerialWriteText("\n");
            }
        }

        LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS);
    }
}

void LosKernelSchedulerEphemeralThread(void *Context)
{
    UINT64 Sequence;
    const LOS_KERNEL_SCHEDULER_TASK *Task;

    Sequence = (UINT64)(UINTN)Context;
    Task = LosKernelSchedulerGetCurrentTask();
    if (LosKernelSchedulerShouldEmitSerial() != 0U)
    {
        LosKernelSerialWriteText("[Kernel] Scheduler ephemeral task entered id=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
        LosKernelSerialWriteText(" owner=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->OwnerTaskId : 0ULL);
        LosKernelSerialWriteText(" process=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->ProcessId : 0ULL);
        LosKernelSerialWriteText(" sequence=");
        LosKernelSerialWriteUnsigned(Sequence);
        LosKernelSerialWriteText("\n");
    }

    LosKernelSchedulerSleepCurrent(LOS_KERNEL_SCHEDULER_EPHEMERAL_LIFETIME_TICKS);

    Task = LosKernelSchedulerGetCurrentTask();
    if (LosKernelSchedulerShouldEmitSerial() != 0U)
    {
        LosKernelSerialWriteText("[Kernel] Scheduler ephemeral task resumed id=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
        LosKernelSerialWriteText(" process=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->ProcessId : 0ULL);
        LosKernelSerialWriteText(" sequence=");
        LosKernelSerialWriteUnsigned(Sequence);
        LosKernelSerialWriteText(" ticks=");
        LosKernelSerialWriteUnsigned(LosKernelSchedulerGetTickCount());
        LosKernelSerialWriteText("\n");

        LosKernelSerialWriteText("[Kernel] Scheduler ephemeral task exiting id=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
        LosKernelSerialWriteText(" process=");
        LosKernelSerialWriteUnsigned(Task != 0 ? Task->ProcessId : 0ULL);
        LosKernelSerialWriteText(" sequence=");
        LosKernelSerialWriteUnsigned(Sequence);
        LosKernelSerialWriteText(" ticks=");
        LosKernelSerialWriteUnsigned(LosKernelSchedulerGetTickCount());
        LosKernelSerialWriteText("\n");
    }
    LosKernelSchedulerTerminateCurrent();
    LosKernelHaltForever();
}

void LosKernelSchedulerUserTransitionTrapThread(void *Context)
{
    const LOS_KERNEL_SCHEDULER_TASK *Task;

    (void)Context;
    Task = LosKernelSchedulerGetCurrentTask();
    LosKernelTraceFail("Scheduler user-transition scaffold task was dispatched before a real ring transition path existed.");
    LosKernelSerialWriteText("[Kernel] User-transition scaffold dispatch task=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
    LosKernelSerialWriteText(" process=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->ProcessId : 0ULL);
    LosKernelSerialWriteText("\n");
    LosKernelSchedulerTerminateCurrent();
    LosKernelHaltForever();
}


void LosKernelSchedulerUserTransitionBridgeTrap(UINT64 KernelStackPointer)
{
    const LOS_KERNEL_SCHEDULER_TASK *Task;

    Task = LosKernelSchedulerGetCurrentTask();
    LosKernelTraceFail("Scheduler user-transition bridge ran before the real ring-transition entry path existed.");
    LosKernelSerialWriteText("[Kernel] User-transition bridge task=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->TaskId : 0ULL);
    LosKernelSerialWriteText(" process=");
    LosKernelSerialWriteUnsigned(Task != 0 ? Task->ProcessId : 0ULL);
    LosKernelSerialWriteText(" rsp=");
    LosKernelSerialWriteHex64(KernelStackPointer);
    LosKernelSerialWriteText(" seal=");
    LosKernelSerialWriteHex64(KernelStackPointer != 0ULL ? *((const UINT64 *)(UINTN)(KernelStackPointer + 16ULL)) : 0ULL);
    LosKernelSerialWriteText(" handoff=");
    LosKernelSerialWriteHex64(Task != 0 ? Task->UserTransitionHandoffStackPointer : 0ULL);
    LosKernelSerialWriteText("\n");
    LosKernelHaltForever();
}

void LosKernelSchedulerBusyThread(void *Context)
{
    UINT64 IterationCount;
    UINT64 LastReportTick;

    (void)Context;
    IterationCount = 0ULL;
    LastReportTick = 0ULL;
    if (LosKernelSchedulerShouldEmitSerial() != 0U)
    {
        LosKernelSerialWriteText("[Kernel] Scheduler busy worker entered.\n");
    }

    for (;;)
    {
        IterationCount += 1ULL;
        __asm__ __volatile__("pause" : : : "memory");
        if ((IterationCount & 0x3FFFFULL) == 0ULL)
        {
            UINT64 TickCount;

            TickCount = LosKernelSchedulerGetTickCount();
            if ((TickCount - LastReportTick) >= LOS_KERNEL_SCHEDULER_BUSY_REPORT_PERIOD_TICKS)
            {
                if (LosKernelSchedulerShouldEmitSerial() != 0U)
                {
                    LosKernelSerialWriteText("[Kernel] Scheduler busy worker iterations=");
                    LosKernelSerialWriteUnsigned(IterationCount);
                    LosKernelSerialWriteText(" ticks=");
                    LosKernelSerialWriteUnsigned(TickCount);
                    LosKernelSerialWriteText("\n");
                }
                LastReportTick = TickCount;
            }
        }
    }
}
