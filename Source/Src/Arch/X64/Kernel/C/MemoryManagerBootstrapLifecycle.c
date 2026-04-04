#include "MemoryManagerBootstrapInternal.h"

void LosInitializeMemoryManagerBootstrap(const LOS_BOOT_CONTEXT *BootContext)
{
    LosMemoryManagerBootstrapReset(BootContext);
    LosKernelTraceOk("Memory-manager service bootstrap defined.");
}

void LosLaunchMemoryManagerBootstrap(void)
{
    LosMemoryManagerBootstrapUpdateState(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_LAUNCHED);
    LosKernelTraceOk("Memory-manager service launch path entered.");
    LosKernelTrace("Kernel keeps only the lowest-level physical-frame and page-table operations.");

    if (!LosMemoryManagerBootstrapValidateServiceImage())
    {
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager service ELF image validation failed.");
    }

    LosKernelTraceOk("Memory-manager service ELF image validated for first userland launch.");

    if (!LosMemoryManagerBootstrapStageTransport())
    {
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap transport staging failed.");
    }

    LosKernelTraceOk("Memory-manager transport mailboxes and launch block staged.");
    LosMemoryManagerBootstrapPublishLaunchReady();
    LosMemoryManagerBootstrapRunProbe();
}
