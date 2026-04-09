/*
 * File Name: SchedulerDiagnosticsSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerDiagnostics.c.
 */

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
    LosKernelSerialWriteText(" first-user-task-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldReady);
    LosKernelSerialWriteText(" first-user-task-prepared=");
    LosKernelSerialWriteUnsigned(State->UserTransitionPreparedCount);
    LosKernelSerialWriteText(" first-user-task-validated=");
    LosKernelSerialWriteUnsigned(State->UserTransitionValidatedCount);
    LosKernelSerialWriteText(" first-user-task-armed=");
    LosKernelSerialWriteUnsigned(State->UserTransitionArmedCount);
    LosKernelSerialWriteText(" first-user-task-requested=");
    LosKernelSerialWriteUnsigned(State->UserTransitionLaunchRequestCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-entry-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionEntryReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-descriptor-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionDescriptorReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-frame-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionFrameReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-trampoline-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionTrampolineReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-bridge-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionBridgeReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-chain-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionChainReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-contract-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionContractReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-seal-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionSealReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-handoff-ready=");
    LosKernelSerialWriteUnsigned(State->UserTransitionHandoffReadyCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-complete=");
    LosKernelSerialWriteUnsigned(State->UserTransitionCompleteCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-live=");
    LosKernelSerialWriteUnsigned(State->UserTransitionLiveCount != 0ULL ? 1ULL : 0ULL);
    LosKernelSerialWriteText(" first-user-task-live-gate-closed=");
    LosKernelSerialWriteUnsigned(State->UserTransitionLiveGateClosed);
    LosKernelSerialWriteText(" first-user-task-blocked=");
    LosKernelSerialWriteUnsigned(LosKernelSchedulerIsUserTransitionScaffoldBlocked());
    LosKernelSerialWriteText(" first-user-task-reblocked=");
    LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldReblockCount);
    LosKernelSerialWriteText(" user-dispatch-skip=");
    LosKernelSerialWriteUnsigned(State->UserTransitionDispatchSkipCount);
    LosKernelSerialWriteText(" first-user-task-proc=");
    LosKernelSerialWriteUnsigned(State->UserTransitionScaffoldProcessId);
    LosKernelSerialWriteText(" first-user-task-task=");
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
