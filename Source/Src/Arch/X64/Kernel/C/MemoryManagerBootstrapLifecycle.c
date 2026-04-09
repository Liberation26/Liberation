/*
 * File Name: MemoryManagerBootstrapLifecycle.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

#include "MemoryManagerBootstrapInternal.h"

void LosInitializeMemoryManagerBootstrap(const LOS_BOOT_CONTEXT *BootContext)
{
    LosMemoryManagerBootstrapReset(BootContext);
}

void LosLaunchMemoryManagerBootstrap(void)
{
    LosMemoryManagerBootstrapTransitionTo(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_LAUNCHED);
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
