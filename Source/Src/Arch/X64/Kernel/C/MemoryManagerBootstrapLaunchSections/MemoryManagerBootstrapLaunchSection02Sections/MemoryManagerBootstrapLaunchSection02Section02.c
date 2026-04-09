/*
 * File Name: MemoryManagerBootstrapLaunchSection02Section02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapLaunchSection02.c.
 */

static BOOLEAN MapServiceImageIntoOwnAddressSpace(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header;
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders;
    const LOS_X64_VIRTUAL_MEMORY_LAYOUT *Layout;
    UINT64 ServiceRootPhysicalAddress;
    UINT64 ImageVirtualBase;
    UINT64 ImageMappedBytes;
    UINT64 ImagePageCount;
    UINT64 ImagePhysicalBase;
    UINT64 ServiceEntryVirtualAddress;
    UINT64 ServiceStackBaseVirtualAddress;

    State = LosMemoryManagerBootstrapState();
    if (State->ServiceImageVirtualAddress == 0ULL)
    {
        return 0;
    }

    if ((State->Info.Flags & LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_IMAGE_MAPPED) != 0ULL)
    {
        return 1;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_CLONE_ROOT, LOS_MEMORY_MANAGER_PREP_DETAIL_BEGIN);
    if (!CloneCurrentRootPageMap(&ServiceRootPhysicalAddress))
    {
        return 0;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VALIDATE_ELF, LOS_MEMORY_MANAGER_PREP_DETAIL_BEGIN);
    Header = (const LOS_MEMORY_MANAGER_ELF64_HEADER *)(UINTN)State->ServiceImageVirtualAddress;
    if (Header == 0)
    {
        return 0;
    }

    if (Header->Ident[0] != LOS_ELF_MAGIC_0 ||
        Header->Ident[1] != LOS_ELF_MAGIC_1 ||
        Header->Ident[2] != LOS_ELF_MAGIC_2 ||
        Header->Ident[3] != LOS_ELF_MAGIC_3 ||
        Header->Ident[4] != LOS_ELF_CLASS_64 ||
        Header->Ident[5] != LOS_ELF_DATA_LITTLE_ENDIAN ||
        Header->Machine != LOS_ELF_MACHINE_X86_64 ||
        Header->Type != LOS_ELF_TYPE_EXEC)
    {
        SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VALIDATE_ELF, LOS_MEMORY_MANAGER_PREP_DETAIL_INVALID_ELF);
        return 0;
    }

    ProgramHeaders = (const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *)((const UINT8 *)Header + Header->ProgramHeaderOffset);
    if (!StageServiceImageInPhysicalMemory(
            Header,
            ProgramHeaders,
            &ImageVirtualBase,
            &ImageMappedBytes,
            &ImagePageCount,
            &ImagePhysicalBase) ||
        !ResolveServiceEntryVirtualAddress(Header, ImageVirtualBase, ImageMappedBytes, &ServiceEntryVirtualAddress))
    {
        return 0;
    }

    if (!MapStagedServiceImageIntoAddressSpace(
            ServiceRootPhysicalAddress,
            Header,
            ProgramHeaders,
            ImageVirtualBase,
            ImagePageCount,
            ImagePhysicalBase))
    {
        return 0;
    }

    ServiceStackBaseVirtualAddress = SelectServiceStackBaseVirtualAddress(ImageVirtualBase, ImagePageCount);
    Layout = LosX64GetVirtualMemoryLayout();
    State->ServiceAddressSpaceObject->RootTablePhysicalAddress = ServiceRootPhysicalAddress;
    State->Info.ServicePageMapLevel4PhysicalAddress = ServiceRootPhysicalAddress;
    State->LaunchBlock->ServicePageMapLevel4PhysicalAddress = ServiceRootPhysicalAddress;
    State->ServiceAddressSpaceObject->DirectMapBase = Layout->HigherHalfDirectMapBase;
    State->ServiceAddressSpaceObject->DirectMapSize = Layout->HigherHalfDirectMapSize;
    State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress = ImagePhysicalBase;
    State->ServiceAddressSpaceObject->ServiceImageSize = ImageMappedBytes;
    State->LaunchBlock->ServiceImageSize = ImageMappedBytes;
    State->ServiceAddressSpaceObject->ServiceImageVirtualBase = ImageVirtualBase;
    State->ServiceAddressSpaceObject->EntryVirtualAddress = ServiceEntryVirtualAddress;
    State->ServiceAddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE;
    if (!MapServiceStackIntoAddressSpace(
            ServiceRootPhysicalAddress,
            ServiceStackBaseVirtualAddress,
            State->Info.ServiceStackPhysicalAddress,
            State->Info.ServiceStackPageCount,
            &State->ServiceTaskObject->StackTopVirtualAddress))
    {
        LosKernelTraceFail("Memory-manager service stack map into service root failed.");
        return 0;
    }
    if (State->ServiceTaskObject->StackTopVirtualAddress == 0ULL)
    {
        LosKernelTraceFail("Memory-manager service stack top virtual address remained zero after stack map.");
        return 0;
    }
    State->ServiceAddressSpaceObject->StackPhysicalAddress = State->Info.ServiceStackPhysicalAddress;
    State->ServiceAddressSpaceObject->StackPageCount = State->Info.ServiceStackPageCount;
    State->ServiceAddressSpaceObject->StackBaseVirtualAddress = ServiceStackBaseVirtualAddress;
    State->ServiceAddressSpaceObject->StackTopVirtualAddress = State->ServiceTaskObject->StackTopVirtualAddress;
    State->ServiceAddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK;
    State->ServiceAddressSpaceObject->ReservedVirtualRegionCount = 2U;
    State->ServiceAddressSpaceObject->ReservedVirtualRegions[0].BaseVirtualAddress = ImageVirtualBase;
    State->ServiceAddressSpaceObject->ReservedVirtualRegions[0].PageCount = ImagePageCount;
    State->ServiceAddressSpaceObject->ReservedVirtualRegions[0].Type = LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_IMAGE;
    State->ServiceAddressSpaceObject->ReservedVirtualRegions[0].Flags = 0U;
    State->ServiceAddressSpaceObject->ReservedVirtualRegions[0].BackingPhysicalAddress = ImagePhysicalBase;
    State->ServiceAddressSpaceObject->ReservedVirtualRegions[1].BaseVirtualAddress = ServiceStackBaseVirtualAddress;
    State->ServiceAddressSpaceObject->ReservedVirtualRegions[1].PageCount = State->Info.ServiceStackPageCount;
    State->ServiceAddressSpaceObject->ReservedVirtualRegions[1].Type = LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_STACK;
    State->ServiceAddressSpaceObject->ReservedVirtualRegions[1].Flags = 0U;
    State->ServiceAddressSpaceObject->ReservedVirtualRegions[1].BackingPhysicalAddress = State->Info.ServiceStackPhysicalAddress;
    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_PUBLISH_LAUNCH, LOS_MEMORY_MANAGER_PREP_DETAIL_BEGIN);
    State->Info.Flags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_IMAGE_MAPPED;
    State->LaunchBlock->Flags = State->Info.Flags;
    State->LaunchBlock->ServiceStackTopVirtualAddress = State->ServiceTaskObject->StackTopVirtualAddress;
    return 1;
}

/* Dedicated assembly bootstrap task-transfer helper declared in MemoryManagerBootstrapInternal.h. */

BOOLEAN LosMemoryManagerBootstrapEnsureServiceEntryReady(void)
{
    return MapServiceImageIntoOwnAddressSpace();
}

BOOLEAN LosMemoryManagerBootstrapInvokeServiceEntry(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    UINT64 LaunchBlockDirectMapAddress;
    UINT64 ServiceStackTopVirtualAddress;
    UINT64 PreviousRootPhysicalAddress;

    State = LosMemoryManagerBootstrapState();
    if (!LosMemoryManagerBootstrapEnsureServiceEntryReady())
    {
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager service entry preparation failed.");
    }

    LaunchBlockDirectMapAddress = (UINT64)(UINTN)LosX64GetDirectMapVirtualAddress(State->Info.LaunchBlockPhysicalAddress, State->Info.LaunchBlockSize);
    if (LaunchBlockDirectMapAddress == 0ULL)
    {
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager launch block direct-map translation failed.");
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VERIFY_CONTEXT, LOS_MEMORY_MANAGER_PREP_DETAIL_BEGIN);
    ServiceStackTopVirtualAddress = State->ServiceTaskObject->StackTopVirtualAddress;
    if (ServiceStackTopVirtualAddress == 0ULL)
    {
        SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VERIFY_CONTEXT, LOS_MEMORY_MANAGER_PREP_DETAIL_STACK_TOP_ZERO);
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager service stack top virtual address is zero.");
    }

    PreviousRootPhysicalAddress = ReadCr3();
    TraceTransferContext(
        PreviousRootPhysicalAddress,
        State->ServiceAddressSpaceObject->RootTablePhysicalAddress,
        LaunchBlockDirectMapAddress,
        State->Info.ServiceEntryVirtualAddress,
        ServiceStackTopVirtualAddress);
    State->ServiceTaskObject->LastRequestId = 0x2001ULL;
    State->ServiceTaskObject->Heartbeat = LaunchBlockDirectMapAddress;
    State->ServiceAddressSpaceObject->State = LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_ACTIVE;
    State->ServiceTaskObject->State = LOS_MEMORY_MANAGER_TASK_STATE_RUNNING;
    State->Info.Flags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_CONTEXT_SWITCHED;
    State->LaunchBlock->Flags = State->Info.Flags;
    if (State->ServiceAddressSpaceObject->RootTablePhysicalAddress == 0ULL)
    {
        SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VERIFY_CONTEXT, LOS_MEMORY_MANAGER_PREP_DETAIL_ROOT_ZERO);
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager service root physical address is zero.");
    }
    if (State->Info.ServiceEntryVirtualAddress == 0ULL)
    {
        SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VERIFY_CONTEXT, LOS_MEMORY_MANAGER_PREP_DETAIL_ENTRY_ZERO);
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager service entry virtual address is zero.");
    }
    LosKernelTrace("Memory-manager task-transfer handoff executing now.");
    LosMemoryManagerBootstrapTransferToServiceTask(
        State->ServiceAddressSpaceObject->RootTablePhysicalAddress,
        State->Info.ServiceEntryVirtualAddress,
        ServiceStackTopVirtualAddress,
        LaunchBlockDirectMapAddress);
    LosKernelTrace("Memory-manager task-transfer helper returned to kernel context.");
    WriteCr3(PreviousRootPhysicalAddress);
    State->ServiceTaskObject->State = LOS_MEMORY_MANAGER_TASK_STATE_ONLINE;
    State->ServiceTaskObject->Flags &= ~LOS_MEMORY_MANAGER_TASK_FLAG_BOOTSTRAP_HOSTED;
    State->Info.Flags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_ENTRY_INVOKED;
    State->LaunchBlock->Flags = State->Info.Flags;
    LosKernelTrace("Memory-manager hosted bootstrap step completed and control returned to kernel.");
    return 1;
}
