#include "MemoryManagerBootstrapInternal.h"

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

void LosMemoryManagerBootstrapRunProbe(void)
{
    LOS_X64_QUERY_MEMORY_REGIONS_RESULT Result;

    LosMemoryManagerSendQueryMemoryRegions(0, 0U, &Result);
    if (Result.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS)
    {
        LosMemoryManagerBootstrapSetFlag(LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_PROBE_COMPLETE);
        LosKernelTraceOk("Memory-manager endpoint bootstrap probe succeeded.");
        LosKernelTraceUnsigned("Memory-manager published regions: ", Result.TotalRegionCount);
        LosMemoryManagerBootstrapUpdateState(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_READY);
    }
    else
    {
        LosKernelTraceUnsigned("Memory-manager bootstrap status: ", Result.Status);
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager endpoint bootstrap probe failed.");
    }
}
