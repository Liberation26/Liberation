#include "MemoryManagerBootstrapInternal.h"

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
        LosKernelTraceHex64("Memory-manager launch block stack top: ", LaunchBlock->ServiceStackTopPhysicalAddress);
        LosKernelTraceHex64("Memory-manager launch block entry: ", LaunchBlock->ServiceEntryVirtualAddress);
    }
}

void LosDescribeMemoryManagerBootstrap(void)
{
    LosMemoryManagerBootstrapDescribeState();
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
        LosKernelTraceFail("Memory-manager endpoint bootstrap probe failed.");
        LosKernelTraceUnsigned("Memory-manager bootstrap status: ", Result.Status);
    }
}
