#include "MonitorInternal.h"

static const CHAR16 *const KernelPathDataPartition = LOS_TEXT("\\LIBERATION\\KERNELX64.ELF");
static const CHAR16 *const KernelPathEspFallback = LOS_TEXT("\\EFI\\BOOT\\KERNELX64.ELF");
static const CHAR16 *const BootInfoPath = LOS_TEXT("\\EFI\\BOOT\\BOOTINFO.TXT");
static const CHAR16 *const DefaultBootInfoText = LOS_TEXT("Booting from an installed Liberation drive\r\n");
static const CHAR16 *const DataPartitionText = LOS_TEXT("Kernel partition: Partition 2 (Liberation Data)\r\n");
static const CHAR16 *const EspPartitionText = LOS_TEXT("Kernel partition: Partition 1 (EFI System Partition)\r\n");

EFI_STATUS EFIAPI LosRunKernelMonitor(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    LOS_MONITOR_ENTER(SystemTable);
    EFI_HANDLE ParentDeviceHandle;
    EFI_FILE_PROTOCOL *Root;
    EFI_STATUS Status;
    LOS_KERNEL_ENTRY KernelEntry;
    void *KernelBuffer;
    UINT64 KernelPhysicalBase;
    UINTN KernelSize;
    CHAR16 *BootInfoBuffer;
    const CHAR16 *BootInfoText;
    const CHAR16 *KernelPartitionText;
    LOS_BOOT_CONTEXT *BootContext;
    UINT64 BootContextAddress;
    UINTN BootContextPageCount;
    LOS_BOOT_CONTEXT_LOAD_SEGMENT KernelLoadSegments[LOS_BOOT_CONTEXT_MAX_LOAD_SEGMENTS];
    UINT64 KernelLoadSegmentCount;

    if (SystemTable == 0 || SystemTable->ConOut == 0 || SystemTable->BootServices == 0)
    {
        return EFI_SUCCESS;
    }

    LosMonitorStatusOk(SystemTable, LOS_TEXT("Kernel monitor started."));
    LosMonitorStatusOk(SystemTable, LOS_TEXT("Kernel monitor is a handoff stage only."));
    LosMonitorStatusOk(SystemTable, LOS_TEXT("Kernel monitor loading Liberation kernel."));

    ParentDeviceHandle = 0;
    Root = 0;
    KernelBuffer = 0;
    KernelPhysicalBase = 0ULL;
    KernelSize = 0U;
    BootInfoBuffer = 0;
    BootInfoText = DefaultBootInfoText;
    KernelPartitionText = LOS_TEXT("Kernel partition: unknown\r\n");
    BootContext = 0;
    BootContextAddress = 0ULL;
    BootContextPageCount = (sizeof(LOS_BOOT_CONTEXT) + (UINTN)(LOS_PAGE_SIZE - 1ULL)) / (UINTN)LOS_PAGE_SIZE;
    LosMonitorMemorySet((void *)&KernelLoadSegments[0], 0, sizeof(KernelLoadSegments));
    KernelLoadSegmentCount = 0ULL;

    Status = LosMonitorGetParentDeviceHandle(ImageHandle, SystemTable, &ParentDeviceHandle);
    if (EFI_ERROR(Status))
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("Resolve monitor boot device handle failed: "), Status);
        ParentDeviceHandle = 0;
    }
    else if (ParentDeviceHandle == 0)
    {
        LosMonitorTrace(SystemTable, LOS_TEXT("Monitor image has no device handle; using filesystem scan fallback."));
    }
    else
    {
        LosMonitorTraceHex64(SystemTable, LOS_TEXT("Monitor device handle: "), (UINT64)(UINTN)ParentDeviceHandle);
    }

    if (ParentDeviceHandle != 0)
    {
        Status = LosMonitorOpenRootForHandle(SystemTable, ParentDeviceHandle, &Root);
    }
    else
    {
        Status = EFI_NOT_FOUND;
        Root = 0;
    }

    if (!EFI_ERROR(Status) && Root != 0)
    {
        LosMonitorTrace(SystemTable, LOS_TEXT("Primary root opened on boot device."));
        Status = LosMonitorReadTextFileFromRoot(SystemTable, Root, BootInfoPath, &BootInfoBuffer);
        if (!EFI_ERROR(Status) && BootInfoBuffer != 0 && BootInfoBuffer[0] != 0)
        {
            BootInfoText = BootInfoBuffer;
            LosMonitorTrace(SystemTable, LOS_TEXT("Boot info text loaded from ESP."));
        }
        else
        {
            LosMonitorTraceStatus(SystemTable, LOS_TEXT("Boot info text load status: "), Status);
        }
        LosMonitorTracePath(SystemTable, LOS_TEXT("Trying kernel path on current root: "), KernelPathDataPartition);
        Status = LosMonitorLoadKernelFileFromRoot(SystemTable, Root, KernelPathDataPartition, &KernelBuffer, &KernelPhysicalBase, &KernelSize, &KernelLoadSegments[0], &KernelLoadSegmentCount);
        Root->Close(Root);
        Root = 0;
    }

    if (!EFI_ERROR(Status) && KernelBuffer != 0 && KernelSize != 0U)
    {
        KernelPartitionText = DataPartitionText;
        LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel image base from current root: "), KernelPhysicalBase);
        LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel entry from current root: "), (UINT64)(UINTN)KernelBuffer);
        LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel loaded image bytes: "), (UINT64)KernelSize);
    }
    else
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("Current-root kernel load status: "), Status);
        LosMonitorTracePath(SystemTable, LOS_TEXT("Trying sibling filesystem kernel path: "), KernelPathDataPartition);
        Status = LosMonitorLoadKernelFromSiblingFileSystemHandle(ParentDeviceHandle, SystemTable, KernelPathDataPartition, &KernelBuffer, &KernelPhysicalBase, &KernelSize, &KernelLoadSegments[0], &KernelLoadSegmentCount);
        if (!EFI_ERROR(Status) && KernelBuffer != 0 && KernelSize != 0U)
        {
            KernelPartitionText = DataPartitionText;
            LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel image base from sibling filesystem: "), KernelPhysicalBase);
            LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel entry from sibling filesystem: "), (UINT64)(UINTN)KernelBuffer);
            LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel loaded image bytes: "), (UINT64)KernelSize);
        }
        else
        {
            LosMonitorTraceStatus(SystemTable, LOS_TEXT("Sibling filesystem kernel load status: "), Status);
            Status = LosMonitorOpenRootForHandle(SystemTable, ParentDeviceHandle, &Root);
            if (!EFI_ERROR(Status) && Root != 0)
            {
                LosMonitorTrace(SystemTable, LOS_TEXT("Re-opened primary root for ESP fallback kernel path."));
                LosMonitorTracePath(SystemTable, LOS_TEXT("Trying fallback ESP kernel path: "), KernelPathEspFallback);
                Status = LosMonitorLoadKernelFileFromRoot(SystemTable, Root, KernelPathEspFallback, &KernelBuffer, &KernelPhysicalBase, &KernelSize, &KernelLoadSegments[0], &KernelLoadSegmentCount);
                Root->Close(Root);
                Root = 0;
                if (!EFI_ERROR(Status) && KernelBuffer != 0 && KernelSize != 0U)
                {
                    KernelPartitionText = EspPartitionText;
                    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel image base from ESP fallback: "), KernelPhysicalBase);
                    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel entry from ESP fallback: "), (UINT64)(UINTN)KernelBuffer);
                    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel loaded image bytes: "), (UINT64)KernelSize);
                }
                else
                {
                    LosMonitorTraceStatus(SystemTable, LOS_TEXT("ESP fallback kernel load status: "), Status);
                }
            }
        }
    }

    if (EFI_ERROR(Status) || KernelBuffer == 0 || KernelSize == 0U)
    {
        LosMonitorStatusFail(SystemTable, LOS_TEXT("Kernel monitor could not load the kernel."));
        LosMonitorHaltForever();
    }

    Status = SystemTable->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, BootContextPageCount, &BootContextAddress);
    if (EFI_ERROR(Status) || BootContextAddress == 0ULL)
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("AllocatePages for boot context failed: "), Status);
        LosMonitorStatusFail(SystemTable, LOS_TEXT("Kernel monitor could not allocate boot context."));
        LosMonitorHaltForever();
    }

    BootContext = (LOS_BOOT_CONTEXT *)(UINTN)BootContextAddress;
    LosMonitorInitializeBootContext(
        BootContext,
        BootContextAddress,
        (UINT64)(BootContextPageCount * (UINTN)LOS_PAGE_SIZE),
        KernelPhysicalBase,
        (UINT64)KernelSize,
        &KernelLoadSegments[0],
        KernelLoadSegmentCount,
        BootInfoText,
        KernelPartitionText);
    LosMonitorCaptureFramebufferInfo(SystemTable, BootContext);
    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Boot context address: "), BootContext->BootContextAddress);
    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Boot context bytes: "), BootContext->BootContextSize);

    LosMonitorStatusOk(SystemTable, LOS_TEXT("Kernel monitor preparing final handoff to the kernel."));
    LosMonitorStatusOk(SystemTable, LOS_TEXT("Kernel monitor exiting UEFI boot services."));
    Status = LosMonitorExitBootServicesWithMemoryMap(ImageHandle, SystemTable, BootContext);
    if (EFI_ERROR(Status))
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("ExitBootServices path failed: "), Status);
        LosMonitorStatusFail(SystemTable, LOS_TEXT("Kernel monitor failed to exit boot services."));
        LosMonitorHaltForever();
    }

    __asm__ __volatile__("cli" : : : "memory");
    KernelEntry = (LOS_KERNEL_ENTRY)(UINTN)KernelBuffer;
    KernelEntry(BootContext);

    LosMonitorHaltForever();
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    LOS_MONITOR_ENTER(SystemTable);
    return LosRunKernelMonitor(ImageHandle, SystemTable);
}
