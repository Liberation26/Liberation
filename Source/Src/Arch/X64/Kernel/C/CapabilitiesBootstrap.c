/*
 * File Name: CapabilitiesBootstrap.c
 * File Version: 0.3.17
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T10:54:28Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

#include "KernelMain.h"

extern const UINT8 LosCapabilitiesServiceImageStart[];
extern const UINT8 LosCapabilitiesServiceImageEnd[];
extern const UINT8 LosCapabilitiesServiceImageSize[];
extern void LosCapabilitiesServiceBootstrapEntryWithContext(const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context);
extern void *LosCapabilitiesServiceState(void);

static void LosKernelZeroCharacters(char *Buffer, UINT32 Length)
{
    UINT32 Index;

    if (Buffer == 0)
    {
        return;
    }

    for (Index = 0U; Index < Length; ++Index)
    {
        Buffer[Index] = 0;
    }
}

static void LosKernelCopyAscii(char *Destination, UINT32 Capacity, const char *Source)
{
    UINT32 Index;

    if (Destination == 0 || Capacity == 0U)
    {
        return;
    }

    LosKernelZeroCharacters(Destination, Capacity);
    if (Source == 0)
    {
        return;
    }

    for (Index = 0U; Index + 1U < Capacity && Source[Index] != 0; ++Index)
    {
        Destination[Index] = Source[Index];
    }
}

const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *LosKernelGetBootstrapCapabilities(void)
{
    const LOS_BOOT_CONTEXT *BootContext;

    BootContext = LosKernelGetBootContext();
    if (BootContext == 0 || (BootContext->Flags & LOS_BOOT_CONTEXT_FLAG_CAPABILITIES_VALID) == 0ULL)
    {
        return 0;
    }

    return &BootContext->Capabilities;
}

BOOLEAN LosKernelBuildCapabilitiesServiceBootstrapContext(LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Context)
{
    const LOS_CAPABILITIES_BOOTSTRAP_CONTEXT *Source;

    if (Context == 0)
    {
        return 0;
    }

    Source = LosKernelGetBootstrapCapabilities();
    if (Source == 0)
    {
        Context->Version = LOS_CAPABILITIES_SERVICE_VERSION;
        Context->Flags = LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP;
        Context->BlockCount = 0U;
        Context->Capacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_BLOCKS;
        Context->AssignmentCount = 0U;
        Context->AssignmentCapacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_ASSIGNMENTS;
        Context->EventCount = 0U;
        Context->EventCapacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_EVENTS;
        return 0;
    }

    *Context = *Source;
    Context->Capacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_BLOCKS;
    Context->AssignmentCapacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_ASSIGNMENTS;
    Context->EventCapacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_EVENTS;
    return 1;
}

BOOLEAN LosKernelBuildCapabilitiesServiceRequest(LOS_INIT_COMMAND_SERVICE_REQUEST *Request)
{
    if (Request == 0)
    {
        return 0;
    }

    Request->Action = LOS_INIT_COMMAND_SERVICE_ACTION_LOAD_AND_RUN;
    Request->Flags = LOS_INIT_COMMAND_SERVICE_FLAG_WAIT_UNTIL_ONLINE |
                     LOS_INIT_COMMAND_SERVICE_FLAG_CAPABILITIES_IMPORT_REQUIRED |
                     LOS_INIT_COMMAND_SERVICE_FLAG_BOOTSTRAP_IMAGE_EMBEDDED;
    Request->RequestId = 1ULL;
    LosKernelCopyAscii(Request->ServicePath,
                       LOS_INIT_COMMAND_SERVICE_PATH_LENGTH,
                       "\\LIBERATION\\SERVICES\\CAPSMGR.ELF");

    Request->ServiceImage.Version = LOS_INIT_COMMAND_VERSION;
    Request->ServiceImage.ImageFormat = LOS_INIT_COMMAND_SERVICE_IMAGE_FORMAT_ELF64;
    Request->ServiceImage.Flags = Request->Flags;
    Request->ServiceImage.Reserved0 = 0U;
    Request->ServiceImage.Signature = LOS_INIT_COMMAND_SERVICE_IMAGE_SIGNATURE;
    Request->ServiceImage.RequestId = Request->RequestId;
    Request->ServiceImage.ImageAddress = (UINT64)(UINTN)LosCapabilitiesServiceImageStart;
    Request->ServiceImage.ImageSize = (UINT64)(UINTN)LosCapabilitiesServiceImageSize;
    Request->ServiceImage.EntryVirtualAddress = 0ULL;
    Request->ServiceImage.BootstrapCallableEntryAddress = (UINT64)(UINTN)LosCapabilitiesServiceBootstrapEntryWithContext;
    Request->ServiceImage.BootstrapContextAddress = (UINT64)(UINTN)LosKernelGetBootstrapCapabilities();
    Request->ServiceImage.BootstrapContextSize = (UINT64)sizeof(LOS_CAPABILITIES_BOOTSTRAP_CONTEXT);
    Request->ServiceImage.BootstrapStateAddress = (UINT64)(UINTN)LosCapabilitiesServiceState();
    Request->ServiceImage.BootstrapStateSize = 0ULL;
    LosKernelCopyAscii(Request->ServiceImage.ServicePath,
                       LOS_INIT_COMMAND_SERVICE_PATH_LENGTH,
                       "\\LIBERATION\\SERVICES\\CAPSMGR.ELF");

    return (Request->ServiceImage.ImageAddress != 0ULL && Request->ServiceImage.ImageSize != 0ULL) ? 1 : 0;
}
