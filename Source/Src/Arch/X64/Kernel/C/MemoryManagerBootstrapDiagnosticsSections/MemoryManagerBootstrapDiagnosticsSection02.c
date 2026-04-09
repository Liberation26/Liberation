/*
 * File Name: MemoryManagerBootstrapDiagnosticsSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapDiagnostics.c.
 */

static void BuildMemoryManagerKnowledgeLine3(char *Buffer, UINTN Capacity, const LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT *Result)
{
    UINTN Length;

    if (Buffer == 0 || Capacity == 0U || Result == 0)
    {
        return;
    }

    Buffer[0] = '\0';
    Length = 0U;
    AppendText(Buffer, Capacity, &Length, "MM desc ");
    AppendUnsigned(Buffer, Capacity, &Length, Result->InternalDescriptorCount);
    AppendText(Buffer, Capacity, &Length, " frame-ranges ");
    AppendUnsigned(Buffer, Capacity, &Length, Result->PageFrameDatabaseEntryCount);
}

static void BuildMemoryManagerKnowledgeLine4(char *Buffer, UINTN Capacity, const LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT *Result)
{
    UINTN Length;

    if (Buffer == 0 || Capacity == 0U || Result == 0)
    {
        return;
    }

    Buffer[0] = '\0';
    Length = 0U;
    AppendText(Buffer, Capacity, &Length, "MM heap meta ");
    AppendUnsigned(Buffer, Capacity, &Length, Result->HeapMetadataPages);
    AppendText(Buffer, Capacity, &Length, " heap ");
    AppendUnsigned(Buffer, Capacity, &Length, Result->HeapReservedPages);
    AppendText(Buffer, Capacity, &Length, " slabcap ");
    AppendUnsigned(Buffer, Capacity, &Length, Result->HeapSlabPageCapacity);
    AppendText(Buffer, Capacity, &Length, " largecap ");
    AppendUnsigned(Buffer, Capacity, &Length, Result->HeapLargeAllocationCapacity);
}

static void BuildBootstrapAddressSpaceLine(char *Buffer, UINTN Capacity, const LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State)
{
    UINTN Length;
    UINT64 AddressSpaceId;
    UINT64 RootTablePhysicalAddress;

    if (Buffer == 0 || Capacity == 0U || State == 0 || State->Info.ServiceAddressSpaceObjectPhysicalAddress == 0ULL)
    {
        return;
    }

    AddressSpaceId = 1ULL;
    RootTablePhysicalAddress = State->Info.ServicePageMapLevel4PhysicalAddress;

    Buffer[0] = '\0';
    Length = 0U;
    AppendText(Buffer, Capacity, &Length, "MM as ");
    AppendUnsigned(Buffer, Capacity, &Length, AddressSpaceId);
    AppendText(Buffer, Capacity, &Length, " obj ");
    AppendHex64(Buffer, Capacity, &Length, State->Info.ServiceAddressSpaceObjectPhysicalAddress);
    AppendText(Buffer, Capacity, &Length, " root ");
    AppendHex64(Buffer, Capacity, &Length, RootTablePhysicalAddress);
}

static void ReportBootstrapAddressSpaceCreated(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    UINT64 AddressSpaceId;
    UINT64 RootTablePhysicalAddress;
    char ScreenLine[96];

    State = LosMemoryManagerBootstrapState();
    if (State == 0 || State->Info.ServiceAddressSpaceObjectPhysicalAddress == 0ULL)
    {
        return;
    }

    AddressSpaceId = 1ULL;
    RootTablePhysicalAddress = State->Info.ServicePageMapLevel4PhysicalAddress;

    LosKernelTraceUnsigned("Memory-manager bootstrap address-space id: ", AddressSpaceId);
    LosKernelTraceHex64("Memory-manager bootstrap address-space object: ", State->Info.ServiceAddressSpaceObjectPhysicalAddress);
    LosKernelTraceHex64("Memory-manager bootstrap address-space root: ", RootTablePhysicalAddress);
    BuildBootstrapAddressSpaceLine(ScreenLine, sizeof(ScreenLine), State);
}

static void ReportMemoryManagerKnowledge(const LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT *Result)
{
    char ScreenLine[96];

    if (Result == 0)
    {
        return;
    }

    LosKernelTraceOk("Memory now displayed from the Memory Manager's own knowledge.");
    LosKernelTraceUnsigned("Memory-manager total usable bytes: ", Result->TotalUsableBytes);
    LosKernelTraceUnsigned("Memory-manager bootstrap-reserved bytes: ", Result->TotalBootstrapReservedBytes);
    LosKernelTraceUnsigned("Memory-manager firmware-reserved bytes: ", Result->TotalFirmwareReservedBytes);
    LosKernelTraceUnsigned("Memory-manager runtime bytes: ", Result->TotalRuntimeBytes);
    LosKernelTraceUnsigned("Memory-manager MMIO bytes: ", Result->TotalMmioBytes);
    LosKernelTraceUnsigned("Memory-manager ACPI/NVS bytes: ", Result->TotalAcpiBytes);
    LosKernelTraceUnsigned("Memory-manager unusable bytes: ", Result->TotalUnusableBytes);
    LosKernelTraceUnsigned("Memory-manager total pages: ", Result->TotalPages);
    LosKernelTraceUnsigned("Memory-manager free pages: ", Result->FreePages);
    LosKernelTraceUnsigned("Memory-manager reserved pages: ", Result->ReservedPages);
    LosKernelTraceUnsigned("Memory-manager runtime pages: ", Result->RuntimePages);
    LosKernelTraceUnsigned("Memory-manager MMIO pages: ", Result->MmioPages);
    LosKernelTraceUnsigned("Memory-manager descriptor count: ", Result->InternalDescriptorCount);
    LosKernelTraceUnsigned("Memory-manager frame-database ranges: ", Result->PageFrameDatabaseEntryCount);
    LosKernelTraceUnsigned("Memory-manager heap metadata pages: ", Result->HeapMetadataPages);
    LosKernelTraceUnsigned("Memory-manager heap reserved pages: ", Result->HeapReservedPages);
    LosKernelTraceUnsigned("Memory-manager heap slab capacity: ", Result->HeapSlabPageCapacity);
    LosKernelTraceUnsigned("Memory-manager heap large-allocation capacity: ", Result->HeapLargeAllocationCapacity);

    LosKernelTraceOk("Frame allocator ready.");
    LosKernelStatusScreenWriteOk("Frame allocator ready.");
    LosKernelTraceOk("Heap subsystem ready.");
    LosKernelStatusScreenWriteOk("Heap subsystem ready.");
    LosKernelStatusScreenWriteOk("Memory view from Memory Manager ready.");
    BuildMemoryManagerKnowledgeLine0(ScreenLine, sizeof(ScreenLine), Result);
    LosKernelStatusScreenWriteOk(ScreenLine);
    BuildMemoryManagerKnowledgeLine1(ScreenLine, sizeof(ScreenLine), Result);
    LosKernelStatusScreenWriteOk(ScreenLine);
    BuildMemoryManagerKnowledgeLine2(ScreenLine, sizeof(ScreenLine), Result);
    LosKernelStatusScreenWriteOk(ScreenLine);
    BuildMemoryManagerKnowledgeLine3(ScreenLine, sizeof(ScreenLine), Result);
    LosKernelStatusScreenWriteOk(ScreenLine);
    BuildMemoryManagerKnowledgeLine4(ScreenLine, sizeof(ScreenLine), Result);
    LosKernelStatusScreenWriteOk(ScreenLine);
}

void LosMemoryManagerBootstrapDescribeState(void)
{
    const LOS_MEMORY_MANAGER_BOOTSTRAP_INFO *Info;
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;

    Info = LosGetMemoryManagerBootstrapInfo();
    State = LosMemoryManagerBootstrapState();
    LaunchBlock = LosGetMemoryManagerLaunchBlock();

    LosKernelTraceOk("Memory-manager bootstrap contract defined.");
    TraceEndpoint("Memory-manager endpoint kernel->service: ", Info->Endpoints.KernelToService);
    TraceEndpoint("Memory-manager endpoint service->kernel: ", Info->Endpoints.ServiceToKernel);
    TraceEndpoint("Memory-manager endpoint service-events: ", Info->Endpoints.ServiceEvents);
    LosKernelTraceUnsigned("Memory-manager bootstrap state: ", Info->State);
    LosKernelTraceUnsigned("Memory-manager bootstrap transition count: ", State->StateTransitionCount);
    LosKernelTraceUnsigned("Memory-manager bootstrap result code: ", State->BootstrapResultCode);
    LosKernelTraceHex64("Memory-manager request mailbox physical: ", Info->RequestMailboxPhysicalAddress);
    LosKernelTraceHex64("Memory-manager response mailbox physical: ", Info->ResponseMailboxPhysicalAddress);
    LosKernelTraceHex64("Memory-manager event mailbox physical: ", Info->EventMailboxPhysicalAddress);
    LosKernelTraceHex64("Memory-manager receive endpoint object physical: ", Info->KernelToServiceEndpointObjectPhysicalAddress);
    LosKernelTraceHex64("Memory-manager reply endpoint object physical: ", Info->ServiceToKernelEndpointObjectPhysicalAddress);
    LosKernelTraceHex64("Memory-manager event endpoint object physical: ", Info->ServiceEventsEndpointObjectPhysicalAddress);
    LosKernelTraceHex64("Memory-manager launch block physical: ", Info->LaunchBlockPhysicalAddress);
    LosKernelTraceHex64("Memory-manager service stack physical: ", Info->ServiceStackPhysicalAddress);
    LosKernelTraceHex64("Memory-manager service image physical: ", Info->ServiceImagePhysicalAddress);
    LosKernelTraceHex64("Memory-manager service image bytes: ", Info->ServiceImageSize);
    LosKernelTraceHex64("Memory-manager service ELF entry: ", Info->ServiceEntryVirtualAddress);
    LosKernelTraceUnsigned("Memory-manager bootstrap messages sent: ", State->MessagesSent);
    LosKernelTraceUnsigned("Memory-manager bootstrap messages completed: ", State->MessagesCompleted);
    TraceEndpointObject("Memory-manager receive endpoint id: ", LosMemoryManagerBootstrapGetKernelToServiceEndpointObject());
    TraceEndpointObject("Memory-manager reply endpoint id: ", LosMemoryManagerBootstrapGetServiceToKernelEndpointObject());
    TraceEndpointObject("Memory-manager event endpoint id: ", LosMemoryManagerBootstrapGetServiceEventsEndpointObject());
    if (LaunchBlock != 0)
    {
        LosKernelTraceHex64("Memory-manager launch block stack top physical: ", LaunchBlock->ServiceStackTopPhysicalAddress);
        LosKernelTraceHex64("Memory-manager launch block stack top virtual: ", LaunchBlock->ServiceStackTopVirtualAddress);
        LosKernelTraceHex64("Memory-manager launch block entry: ", LaunchBlock->ServiceEntryVirtualAddress);
    }
}

void LosDescribeMemoryManagerBootstrap(void)
{
    LosMemoryManagerBootstrapDescribeState();
}


void LosMemoryManagerBootstrapReportFailureAndHalt(const char *Reason)
{
    if (Reason != 0)
    {
        LosKernelTraceFail(Reason);
    }
    TraceAttachFailureDetails();
    LosMemoryManagerBootstrapDescribeState();
    LosKernelTraceFail("Memory-manager bootstrap entered fatal halt.");
    LosKernelHaltForever();
}

void LosMemoryManagerBootstrapReportFailureValueAndHalt(const char *Reason, const char *Prefix, UINT64 Value)
{
    if (Prefix != 0)
    {
        LosKernelTraceHex64(Prefix, Value);
    }
    LosMemoryManagerBootstrapReportFailureAndHalt(Reason);
}

static const char *LosMemoryManagerBootstrapAttachResultName(UINT32 Result)
{
    switch (Result)
    {
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY:
            return "ready";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ALREADY_ATTACHED:
            return "already-attached";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_INVALID_REQUEST:
            return "invalid-request";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_VERSION_MISMATCH:
            return "version-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_LAUNCH_BLOCK_MISMATCH:
            return "launch-block-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ENDPOINT_MISMATCH:
            return "endpoint-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_MAILBOX_MISMATCH:
            return "mailbox-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_ROOT_MISMATCH:
            return "service-root-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_IMAGE_MISMATCH:
            return "service-image-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_STATE_INVALID:
            return "service-state-invalid";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_OPERATION_SET_INVALID:
            return "operation-set-invalid";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ENTRY_MISMATCH:
            return "entry-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_STACK_TOP_MISMATCH:
            return "stack-top-mismatch";
        default:
            return "unknown";
    }
}

void LosMemoryManagerBootstrapRunProbe(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT Result;

    LosMemoryManagerSendBootstrapAttach(&Result);
    if (Result.BootstrapResult == LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY)
    {
        LosMemoryManagerBootstrapSetFlag(LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_PROBE_COMPLETE);
        LosMemoryManagerBootstrapSetFlag(LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_ATTACH_COMPLETE);
        LosKernelTraceOk("Memory-manager bootstrap attach succeeded.");
        LosKernelTraceUnsigned("Memory-manager bootstrap result: ", Result.BootstrapResult);
        LosKernelTraceUnsigned("Memory-manager bootstrap reply state: ", Result.BootstrapState);
        ReportBootstrapAddressSpaceCreated();
        ReportMemoryManagerKnowledge(&Result);
        LosMemoryManagerBootstrapTransitionTo(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_READY);
    }
    else
    {
        LosKernelTraceUnsigned("Memory-manager bootstrap result: ", Result.BootstrapResult);
        LosKernelTrace("Memory-manager bootstrap result name: ");
        LosKernelTrace(LosMemoryManagerBootstrapAttachResultName(Result.BootstrapResult));
        LosKernelTraceUnsigned("Memory-manager bootstrap reply state: ", Result.BootstrapState);
        LosKernelTraceHex64("Memory-manager bootstrap negotiated operations: ", Result.NegotiatedOperations);
        LosKernelTraceHex64("Memory-manager bootstrap active service root: ", Result.ActiveRootTablePhysicalAddress);
        LosKernelTraceHex64("Memory-manager bootstrap kernel root: ", Result.KernelRootTablePhysicalAddress);
        LosKernelTraceHex64("Memory-manager bootstrap reply flags: ", Result.BootstrapFlags);
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap attach failed.");
    }
}

