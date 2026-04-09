/*
 * File Name: MemoryManagerBootstrapDiagnosticsSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapDiagnostics.c.
 */

static const char *AttachStageName(UINT64 Stage)
{
    switch (Stage)
    {
        case 1ULL:
            return "launch-block";
        case 2ULL:
            return "direct-map-offset";
        case 3ULL:
            return "task-object-translation";
        case 4ULL:
            return "receive-endpoint";
        case 5ULL:
            return "reply-endpoint";
        case 6ULL:
            return "event-endpoint";
        case 7ULL:
            return "address-space-object";
        case 8ULL:
            return "task-object";
        case 100ULL:
            return "clone-root";
        case 101ULL:
            return "validate-elf";
        case 102ULL:
            return "map-segment";
        case 103ULL:
            return "map-stack";
        case 104ULL:
            return "publish-launch";
        case 105ULL:
            return "verify-context";
        default:
            return "none";
    }
}

static const char *AttachDetailName(UINT64 Detail)
{
    switch (Detail)
    {
        case 1ULL:
            return "null";
        case 2ULL:
            return "signature";
        case 3ULL:
            return "version";
        case 4ULL:
            return "endpoint-id";
        case 5ULL:
            return "role";
        case 6ULL:
            return "mailbox-physical";
        case 7ULL:
            return "mailbox-flag";
        case 8ULL:
            return "state";
        case 9ULL:
            return "service-image-physical";
        case 10ULL:
            return "root-table-physical";
        case 11ULL:
            return "kernel-root-physical";
        case 12ULL:
            return "address-space-object-physical";
        case 13ULL:
            return "entry-virtual";
        case 14ULL:
            return "stack-top-physical";
        case 15ULL:
            return "stack-top-virtual";
        case 16ULL:
            return "launch-block-physical";
        case 17ULL:
            return "request-mailbox-physical";
        case 18ULL:
            return "response-mailbox-physical";
        case 19ULL:
            return "event-mailbox-physical";
        case 20ULL:
            return "receive-endpoint-physical";
        case 21ULL:
            return "reply-endpoint-physical";
        case 22ULL:
            return "event-endpoint-physical";
        case 23ULL:
            return "address-space-object-pointer";
        case 24ULL:
            return "task-object-pointer";
        case 25ULL:
            return "service-root-physical";
        case 100ULL:
            return "begin";
        case 101ULL:
            return "claim-root-page";
        case 102ULL:
            return "direct-map-root";
        case 103ULL:
            return "copy-higher-half";
        case 104ULL:
            return "invalid-elf";
        case 105ULL:
            return "segment-frame-claim";
        case 106ULL:
            return "segment-map-status";
        case 107ULL:
            return "segment-direct-map";
        case 108ULL:
            return "stack-map-status";
        case 109ULL:
            return "stack-top-zero";
        case 110ULL:
            return "entry-zero";
        case 111ULL:
            return "root-zero";
        default:
            return "none";
    }
}

static void TraceAttachFailureDetails(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    if (State->ServiceTaskObject == 0)
    {
        return;
    }

    LosKernelTrace("Memory-manager attach failure diagnostics follow.");
    if (State->ServiceTaskObject->LastRequestId == 0ULL && State->ServiceTaskObject->Heartbeat == 0ULL)
    {
        LosKernelTrace("Memory-manager attach stage:");
        LosKernelTrace("unset");
        LosKernelTrace("Memory-manager attach detail:");
        LosKernelTrace("unset");
        LosKernelTrace("Memory-manager service entry did not publish attach diagnostics before the probe failed.");
    }
    else if (State->ServiceTaskObject->LastRequestId >= 0x1000ULL)
    {
        LosKernelTrace("Memory-manager attach stage:");
        LosKernelTrace("service-entry-breadcrumb");
        LosKernelTrace("Memory-manager attach detail:");
        LosKernelTrace("raw-value");
        LosKernelTraceHex64("Memory-manager service breadcrumb stage: ", State->ServiceTaskObject->LastRequestId);
        LosKernelTraceHex64("Memory-manager service breadcrumb value: ", State->ServiceTaskObject->Heartbeat);
    }
    else
    {
        LosKernelTrace("Memory-manager attach stage:");
        LosKernelTrace(AttachStageName(State->ServiceTaskObject->LastRequestId));
        LosKernelTrace("Memory-manager attach detail:");
        LosKernelTrace(AttachDetailName(State->ServiceTaskObject->Heartbeat));
    }
    LosKernelTraceUnsigned("Memory-manager attach stage code: ", State->ServiceTaskObject->LastRequestId);
    LosKernelTraceUnsigned("Memory-manager attach detail code: ", State->ServiceTaskObject->Heartbeat);
    if (State->KernelToServiceEndpointObject != 0)
    {
        LosKernelTraceUnsigned("Memory-manager receive endpoint state snapshot: ", State->KernelToServiceEndpointObject->State);
    }
    if (State->ServiceToKernelEndpointObject != 0)
    {
        LosKernelTraceUnsigned("Memory-manager reply endpoint state snapshot: ", State->ServiceToKernelEndpointObject->State);
    }
    if (State->ServiceEventsEndpointObject != 0)
    {
        LosKernelTraceUnsigned("Memory-manager event endpoint state snapshot: ", State->ServiceEventsEndpointObject->State);
    }
    if (State->ServiceAddressSpaceObject != 0)
    {
        LosKernelTraceUnsigned("Memory-manager address-space state snapshot: ", State->ServiceAddressSpaceObject->State);
        LosKernelTraceHex64("Memory-manager service root snapshot: ", State->ServiceAddressSpaceObject->RootTablePhysicalAddress);
    }
}

static void TraceEndpoint(const char *Prefix, UINT64 Value)
{
    LosKernelTraceHex64(Prefix, Value);
}

static void TraceEndpointObject(const char *Prefix, const LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *Endpoint)
{
    if (Endpoint == 0)
    {
        return;
    }

    LosKernelTraceHex64(Prefix, Endpoint->EndpointId);
    LosKernelTraceUnsigned("Memory-manager endpoint role: ", Endpoint->Role);
    LosKernelTraceUnsigned("Memory-manager endpoint state: ", Endpoint->State);
    LosKernelTraceHex64("Memory-manager endpoint mailbox physical: ", Endpoint->MailboxPhysicalAddress);
}

static void AppendCharacter(char *Buffer, UINTN Capacity, UINTN *Length, char Character)
{
    if (Buffer == 0 || Length == 0 || Capacity == 0U)
    {
        return;
    }

    if (*Length + 1U >= Capacity)
    {
        return;
    }

    Buffer[*Length] = Character;
    *Length += 1U;
    Buffer[*Length] = '\0';
}

static void AppendText(char *Buffer, UINTN Capacity, UINTN *Length, const char *Text)
{
    UINTN Index;

    if (Text == 0)
    {
        return;
    }

    for (Index = 0U; Text[Index] != '\0'; ++Index)
    {
        AppendCharacter(Buffer, Capacity, Length, Text[Index]);
    }
}

static void AppendUnsigned(char *Buffer, UINTN Capacity, UINTN *Length, UINT64 Value)
{
    char Digits[32];
    UINTN DigitCount;

    if (Value == 0ULL)
    {
        AppendCharacter(Buffer, Capacity, Length, '0');
        return;
    }

    DigitCount = 0U;
    while (Value != 0ULL && DigitCount < (sizeof(Digits) / sizeof(Digits[0])))
    {
        Digits[DigitCount] = (char)('0' + (Value % 10ULL));
        Value /= 10ULL;
        ++DigitCount;
    }

    while (DigitCount > 0U)
    {
        --DigitCount;
        AppendCharacter(Buffer, Capacity, Length, Digits[DigitCount]);
    }
}

static void AppendHex64(char *Buffer, UINTN Capacity, UINTN *Length, UINT64 Value)
{
    UINTN Index;

    AppendText(Buffer, Capacity, Length, "0x");
    for (Index = 0U; Index < 16U; ++Index)
    {
        UINT8 Nibble;

        Nibble = (UINT8)((Value >> ((15U - Index) * 4U)) & 0x0FULL);
        AppendCharacter(Buffer, Capacity, Length, (char)(Nibble < 10U ? ('0' + Nibble) : ('A' + (Nibble - 10U))));
    }
}

static void AppendMiBValue(char *Buffer, UINTN Capacity, UINTN *Length, UINT64 Bytes)
{
    AppendUnsigned(Buffer, Capacity, Length, Bytes / (1024ULL * 1024ULL));
}

static void BuildMemoryManagerKnowledgeLine0(char *Buffer, UINTN Capacity, const LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT *Result)
{
    UINTN Length;

    if (Buffer == 0 || Capacity == 0U || Result == 0)
    {
        return;
    }

    Buffer[0] = '\0';
    Length = 0U;
    AppendText(Buffer, Capacity, &Length, "MM usable MiB ");
    AppendMiBValue(Buffer, Capacity, &Length, Result->TotalUsableBytes);
    AppendText(Buffer, Capacity, &Length, " boot ");
    AppendMiBValue(Buffer, Capacity, &Length, Result->TotalBootstrapReservedBytes);
    AppendText(Buffer, Capacity, &Length, " fw ");
    AppendMiBValue(Buffer, Capacity, &Length, Result->TotalFirmwareReservedBytes);
}

static void BuildMemoryManagerKnowledgeLine1(char *Buffer, UINTN Capacity, const LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT *Result)
{
    UINTN Length;

    if (Buffer == 0 || Capacity == 0U || Result == 0)
    {
        return;
    }

    Buffer[0] = '\0';
    Length = 0U;
    AppendText(Buffer, Capacity, &Length, "MM runtime ");
    AppendMiBValue(Buffer, Capacity, &Length, Result->TotalRuntimeBytes);
    AppendText(Buffer, Capacity, &Length, " mmio ");
    AppendMiBValue(Buffer, Capacity, &Length, Result->TotalMmioBytes);
    AppendText(Buffer, Capacity, &Length, " acpi ");
    AppendMiBValue(Buffer, Capacity, &Length, Result->TotalAcpiBytes);
    AppendText(Buffer, Capacity, &Length, " bad ");
    AppendMiBValue(Buffer, Capacity, &Length, Result->TotalUnusableBytes);
}

static void BuildMemoryManagerKnowledgeLine2(char *Buffer, UINTN Capacity, const LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT *Result)
{
    UINTN Length;

    if (Buffer == 0 || Capacity == 0U || Result == 0)
    {
        return;
    }

    Buffer[0] = '\0';
    Length = 0U;
    AppendText(Buffer, Capacity, &Length, "MM pages free ");
    AppendUnsigned(Buffer, Capacity, &Length, Result->FreePages);
    AppendText(Buffer, Capacity, &Length, " res ");
    AppendUnsigned(Buffer, Capacity, &Length, Result->ReservedPages);
    AppendText(Buffer, Capacity, &Length, " total ");
    AppendUnsigned(Buffer, Capacity, &Length, Result->TotalPages);
}
