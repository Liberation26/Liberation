/*
 * File Name: SchedulerLifecycleSection03.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerLifecycle.c.
 */

static BOOLEAN EnsureFirstUserTaskMappings(LOS_KERNEL_SCHEDULER_PROCESS *Process, LOS_KERNEL_SCHEDULER_TASK *Task)
{
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_REQUEST AttachRequest;
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT AttachResult;
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_REQUEST StackRequest;
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT StackResult;
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
    UINT64 ImagePhysicalAddress;
    UINT64 StackBaseVirtualAddress;
    UINT64 MappedPhysicalAddress;
    UINT64 MappedPageFlags;
    UINT64 CriticalSectionFlags;
    UINT64 ResolvedEntryVirtualAddress;
    UINT64 ResolvedStackTopVirtualAddress;
    BOOLEAN ImageMapped;
    BOOLEAN StackMapped;

    if (Process == 0 || Task == 0 ||
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) == 0U ||
        Process->AddressSpaceObjectPhysicalAddress == 0ULL ||
        Process->RootTablePhysicalAddress == 0ULL)
    {
        return 0U;
    }

    AddressSpaceObject = ResolveSchedulerAddressSpaceObject(Process->AddressSpaceObjectPhysicalAddress);
    ResolvedEntryVirtualAddress = Process->UserEntryVirtualAddress != 0ULL ?
        Process->UserEntryVirtualAddress : LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_ENTRY_VIRTUAL_ADDRESS;
    ResolvedStackTopVirtualAddress = Process->UserStackTopVirtualAddress != 0ULL ?
        Process->UserStackTopVirtualAddress : LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_TOP_VIRTUAL_ADDRESS;
    if (AddressSpaceObject != 0)
    {
        if (AddressSpaceObject->EntryVirtualAddress != 0ULL)
        {
            ResolvedEntryVirtualAddress = AddressSpaceObject->EntryVirtualAddress;
        }
        if (AddressSpaceObject->StackTopVirtualAddress != 0ULL)
        {
            ResolvedStackTopVirtualAddress = AddressSpaceObject->StackTopVirtualAddress;
        }
    }

    ImageMapped = QueryAddressSpaceMapping(
        Process->AddressSpaceObjectPhysicalAddress,
        ResolvedEntryVirtualAddress,
        &MappedPhysicalAddress,
        &MappedPageFlags);
    StackMapped = QueryAddressSpaceMapping(
        Process->AddressSpaceObjectPhysicalAddress,
        ResolvedStackTopVirtualAddress - 0x1000ULL,
        &MappedPhysicalAddress,
        &MappedPageFlags);

    if (!ImageMapped)
    {
        ImagePhysicalAddress = 0ULL;
        if (!LosX64TryTranslateKernelVirtualToPhysical((UINT64)(UINTN)LosKernelSchedulerUserImage, &ImagePhysicalAddress) ||
            ImagePhysicalAddress == 0ULL)
        {
            return 0U;
        }

        ZeroBytes(&AttachRequest, sizeof(AttachRequest));
        ZeroBytes(&AttachResult, sizeof(AttachResult));
        AttachRequest.AddressSpaceObjectPhysicalAddress = Process->AddressSpaceObjectPhysicalAddress;
        AttachRequest.StagedImagePhysicalAddress = ImagePhysicalAddress;
        AttachRequest.StagedImageSize = LosKernelSchedulerUserImageSize;
        LosMemoryManagerSendAttachStagedImage(&AttachRequest, &AttachResult);
        if (AttachResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS ||
            AttachResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT)
        {
            AddressSpaceObject = ResolveSchedulerAddressSpaceObject(Process->AddressSpaceObjectPhysicalAddress);
            if (ApplyFirstUserTaskImageMetadata(&AttachResult, AddressSpaceObject, &ResolvedEntryVirtualAddress))
            {
                ImageMapped = 1U;
            }
            if (!ImageMapped && ResolvedEntryVirtualAddress != 0ULL)
            {
                ImageMapped = QueryAddressSpaceMapping(
                    Process->AddressSpaceObjectPhysicalAddress,
                    ResolvedEntryVirtualAddress,
                    &MappedPhysicalAddress,
                    &MappedPageFlags);
            }
        }

        if (!ImageMapped)
        {
            LosKernelTraceFail("Kernel scheduler could not attach the first user-task image into its owned address space.");
            LosKernelTraceUnsigned("Kernel scheduler user-image attach status: ", AttachResult.Status);
            return 0U;
        }
    }

    if (!StackMapped)
    {
        StackBaseVirtualAddress = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_TOP_VIRTUAL_ADDRESS -
            (LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_PAGES * 0x1000ULL);
        ZeroBytes(&StackRequest, sizeof(StackRequest));
        ZeroBytes(&StackResult, sizeof(StackResult));
        StackRequest.AddressSpaceObjectPhysicalAddress = Process->AddressSpaceObjectPhysicalAddress;
        StackRequest.DesiredStackBaseVirtualAddress = StackBaseVirtualAddress;
        StackRequest.PageCount = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_PAGES;
        LosMemoryManagerSendAllocateAddressSpaceStack(&StackRequest, &StackResult);
        if (StackResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS ||
            StackResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT)
        {
            AddressSpaceObject = ResolveSchedulerAddressSpaceObject(Process->AddressSpaceObjectPhysicalAddress);
            if (ApplyFirstUserTaskStackMetadata(&StackResult, AddressSpaceObject, &ResolvedStackTopVirtualAddress))
            {
                StackMapped = 1U;
            }
            if (!StackMapped && ResolvedStackTopVirtualAddress != 0ULL)
            {
                StackMapped = QueryAddressSpaceMapping(
                    Process->AddressSpaceObjectPhysicalAddress,
                    ResolvedStackTopVirtualAddress - 0x1000ULL,
                    &MappedPhysicalAddress,
                    &MappedPageFlags);
            }
        }

        if (!StackMapped)
        {
            LosKernelTraceFail("Kernel scheduler could not allocate the first user-task stack inside its owned address space.");
            LosKernelTraceUnsigned("Kernel scheduler user-stack allocate status: ", StackResult.Status);
            return 0U;
        }
    }

    if (!QueryAddressSpaceMapping(
            Process->AddressSpaceObjectPhysicalAddress,
            ResolvedEntryVirtualAddress,
            &MappedPhysicalAddress,
            &MappedPageFlags) ||
        !QueryAddressSpaceMapping(
            Process->AddressSpaceObjectPhysicalAddress,
            ResolvedStackTopVirtualAddress - 0x1000ULL,
            &MappedPhysicalAddress,
            &MappedPageFlags))
    {
        return 0U;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    Process->UserEntryVirtualAddress = ResolvedEntryVirtualAddress;
    Process->UserStackTopVirtualAddress = ResolvedStackTopVirtualAddress;
    Task->UserInstructionPointer = ResolvedEntryVirtualAddress;
    Task->UserStackPointer = ResolvedStackTopVirtualAddress;
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
    return 1U;
}

static LOS_KERNEL_SCHEDULER_PROCESS *FindProcessByIdMutable(UINT64 ProcessId)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    if (ProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID)
    {
        return 0;
    }

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY)
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

static const LOS_KERNEL_SCHEDULER_PROCESS *FindProcessById(UINT64 ProcessId)
{
    return (const LOS_KERNEL_SCHEDULER_PROCESS *)FindProcessByIdMutable(ProcessId);
}

static LOS_KERNEL_SCHEDULER_TASK *FindTaskByIdMutable(UINT64 TaskId)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    if (TaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        LOS_KERNEL_SCHEDULER_TASK *Task;

        Task = &State->Tasks[Index];
        if (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_UNUSED)
        {
            continue;
        }
        if (Task->TaskId == TaskId)
        {
            return Task;
        }
    }

    return 0;
}

BOOLEAN LosKernelSchedulerHasActiveTransientProcess(void)
{
    const LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        const LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_TRANSIENT) == 0U)
        {
            continue;
        }
        if (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY ||
            Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_INITIALIZING ||
            (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED && Process->CleanupPending != 0U))
        {
            return 1;
        }
    }

    return 0;
}


static void AbandonCreatedProcess(LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT64 CriticalSectionFlags;

    if (Process == 0)
    {
        return;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED &&
        Process->ThreadCount == 0U)
    {
        if ((Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY ||
             Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED) &&
            State->ProcessCount > 0U)
        {
            State->ProcessCount -= 1U;
        }
        ZeroBytes(Process, sizeof(*Process));
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
}

static UINT64 GetKernelRootTablePhysicalAddress(void);
static BOOLEAN BindOwnedProcessAddressSpace(LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_REQUEST Request;
    LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_RESULT Result;
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT64 CriticalSectionFlags;
    UINT64 ProcessId;

    if (Process == 0)
    {
        return 0;
    }

    State = LosKernelSchedulerState();
    CriticalSectionFlags = EnterSchedulerCriticalSection();
    if (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED ||
        Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED ||
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) != 0U ||
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) != 0U ||
        Process->AddressSpaceObjectPhysicalAddress != 0ULL)
    {
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        return 1;
    }
    if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS) != 0U)
    {
        State->AddressSpaceBindDeferredCount += 1ULL;
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        return 0;
    }

    Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS;
    ProcessId = Process->ProcessId;
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (IsMemoryManagerSchedulerTransportReady() == 0U)
    {
        CriticalSectionFlags = EnterSchedulerCriticalSection();
        Process->Flags &= ~LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS;
        State->AddressSpaceBindDeferredCount += 1ULL;
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        (void)ProcessId;
        return 0;
    }

    ZeroBytes(&Request, sizeof(Request));
    ZeroBytes(&Result, sizeof(Result));
    LosMemoryManagerSendCreateAddressSpace(&Request, &Result);
    if (Result.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS ||
        Result.AddressSpaceObjectPhysicalAddress == 0ULL ||
        Result.RootTablePhysicalAddress == 0ULL ||
        Result.AddressSpaceId == 0ULL)
    {
        CriticalSectionFlags = EnterSchedulerCriticalSection();
        Process->Flags &= ~LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS;
        State->AddressSpaceBindDeferredCount += 1ULL;
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        LosKernelTraceFail("Kernel scheduler could not bind a distinct process address space.");
        LosKernelTraceUnsigned("Kernel scheduler process address-space-create status: ", Result.Status);
        LosKernelTraceUnsigned("Kernel scheduler process id awaiting root binding: ", ProcessId);
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    if (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED)
    {
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        return 0;
    }
    Process->AddressSpaceId = Result.AddressSpaceId;
    Process->AddressSpaceObjectPhysicalAddress = Result.AddressSpaceObjectPhysicalAddress;
    Process->RootTablePhysicalAddress = Result.RootTablePhysicalAddress;
    Process->Flags &= ~(LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT | LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS);
    Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE;
    State->AddressSpaceBindCount += 1ULL;
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
    return 1;
}
