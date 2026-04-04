#include "MemoryManagerBootstrapInternal.h"

void LosInitializeMemoryManagerBootstrap(const LOS_BOOT_CONTEXT *BootContext)
{
    LosMemoryManagerBootstrapReset(BootContext);
}

void LosLaunchMemoryManagerBootstrap(void)
{
    LosMemoryManagerBootstrapUpdateState(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_LAUNCHED);
    if (!LosMemoryManagerBootstrapValidateServiceImage())
    {
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager service ELF image validation failed.");
    }

    if (!LosMemoryManagerBootstrapStageTransport())
    {
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap transport staging failed.");
    }

    LosMemoryManagerBootstrapPublishLaunchReady();
    LosMemoryManagerBootstrapRunProbe();
}
