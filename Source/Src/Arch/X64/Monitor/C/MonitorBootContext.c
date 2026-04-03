#include "MonitorInternal.h"

void LosMonitorInitializeBootContext(LOS_BOOT_CONTEXT *BootContext, UINT64 BootContextAddress, UINT64 BootContextSize, UINT64 KernelImagePhysicalAddress, UINT64 KernelImageSize, const LOS_BOOT_CONTEXT_LOAD_SEGMENT *KernelLoadSegments, UINT64 KernelLoadSegmentCount, const CHAR16 *BootSourceText, const CHAR16 *KernelPartitionText)
{
    if (BootContext == 0)
    {
        return;
    }

    LosMonitorMemorySet(BootContext, 0, sizeof(LOS_BOOT_CONTEXT));
    BootContext->Signature = LOS_BOOT_CONTEXT_SIGNATURE;
    BootContext->Version = LOS_BOOT_CONTEXT_VERSION;
    BootContext->Reserved = 0U;
    BootContext->Flags = LOS_BOOT_CONTEXT_FLAG_MONITOR_HANDOFF_ONLY;
    BootContext->BootContextAddress = BootContextAddress;
    BootContext->BootContextSize = BootContextSize;
    BootContext->KernelImagePhysicalAddress = KernelImagePhysicalAddress;
    BootContext->KernelImageSize = KernelImageSize;
    if (KernelLoadSegments != 0 && KernelLoadSegmentCount != 0ULL)
    {
        UINT64 Index;
        if (KernelLoadSegmentCount > LOS_BOOT_CONTEXT_MAX_LOAD_SEGMENTS)
        {
            KernelLoadSegmentCount = LOS_BOOT_CONTEXT_MAX_LOAD_SEGMENTS;
        }

        BootContext->Flags |= LOS_BOOT_CONTEXT_FLAG_KERNEL_SEGMENTS_VALID;
        BootContext->KernelLoadSegmentCount = KernelLoadSegmentCount;
        for (Index = 0ULL; Index < KernelLoadSegmentCount; ++Index)
        {
            BootContext->KernelLoadSegments[Index].VirtualAddress = KernelLoadSegments[Index].VirtualAddress;
            BootContext->KernelLoadSegments[Index].PhysicalAddress = KernelLoadSegments[Index].PhysicalAddress;
            BootContext->KernelLoadSegments[Index].FileSize = KernelLoadSegments[Index].FileSize;
            BootContext->KernelLoadSegments[Index].MemorySize = KernelLoadSegments[Index].MemorySize;
            BootContext->KernelLoadSegments[Index].Flags = KernelLoadSegments[Index].Flags;
            BootContext->KernelLoadSegments[Index].Reserved = KernelLoadSegments[Index].Reserved;
        }
    }
    LosMonitorUtf16Copy(BootContext->BootSourceText, LOS_BOOT_CONTEXT_TEXT_CHARACTERS, BootSourceText);
    LosMonitorUtf16Copy(BootContext->KernelPartitionText, LOS_BOOT_CONTEXT_TEXT_CHARACTERS, KernelPartitionText);
}

EFI_STATUS LosMonitorExitBootServicesWithMemoryMap(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, LOS_BOOT_CONTEXT *BootContext)
{
    LOS_MONITOR_ENTER(SystemTable);
    EFI_STATUS Status;
    UINTN MemoryMapSize;
    UINTN MemoryMapCapacity;
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;
    UINT64 MemoryMapAddress;
    UINTN PageCount;
    UINTN Attempt;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || BootContext == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    MemoryMapSize = 0U;
    MemoryMapCapacity = 0U;
    MapKey = 0U;
    DescriptorSize = 0U;
    DescriptorVersion = 0U;
    MemoryMapAddress = 0ULL;
    PageCount = 0U;

    Status = SystemTable->BootServices->GetMemoryMap(&MemoryMapSize, 0, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (Status != EFI_BUFFER_TOO_SMALL || DescriptorSize == 0U)
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("Initial GetMemoryMap failed: "), Status);
        return Status;
    }

    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Initial memory map bytes: "), (UINT64)MemoryMapSize);
    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Memory descriptor size: "), (UINT64)DescriptorSize);

    MemoryMapCapacity = MemoryMapSize + (DescriptorSize * 16U);
    PageCount = (MemoryMapCapacity + (UINTN)(LOS_PAGE_SIZE - 1ULL)) / (UINTN)LOS_PAGE_SIZE;
    Status = SystemTable->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, PageCount, &MemoryMapAddress);
    if (EFI_ERROR(Status) || MemoryMapAddress == 0ULL)
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("AllocatePages for memory map buffer failed: "), Status);
        return Status;
    }

    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Memory map buffer address: "), MemoryMapAddress);

    for (Attempt = 0U; Attempt < 8U; ++Attempt)
    {
        MemoryMapSize = MemoryMapCapacity;
        Status = SystemTable->BootServices->GetMemoryMap(&MemoryMapSize, (void *)(UINTN)MemoryMapAddress, &MapKey, &DescriptorSize, &DescriptorVersion);
        if (Status == EFI_BUFFER_TOO_SMALL)
        {
            UINT64 NewMemoryMapAddress;
            UINTN NewPageCount;

            MemoryMapCapacity = MemoryMapSize + (DescriptorSize * 16U);
            NewPageCount = (MemoryMapCapacity + (UINTN)(LOS_PAGE_SIZE - 1ULL)) / (UINTN)LOS_PAGE_SIZE;
            NewMemoryMapAddress = 0ULL;
            Status = SystemTable->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, NewPageCount, &NewMemoryMapAddress);
            if (EFI_ERROR(Status) || NewMemoryMapAddress == 0ULL)
            {
                LosMonitorTraceStatus(SystemTable, LOS_TEXT("AllocatePages for larger memory map buffer failed: "), Status);
                return Status;
            }

            MemoryMapAddress = NewMemoryMapAddress;
            PageCount = NewPageCount;
            LosMonitorTraceHex64(SystemTable, LOS_TEXT("Expanded memory map buffer address: "), MemoryMapAddress);
            continue;
        }

        if (EFI_ERROR(Status))
        {
            LosMonitorTraceStatus(SystemTable, LOS_TEXT("GetMemoryMap failed before ExitBootServices: "), Status);
            return Status;
        }

        Status = SystemTable->BootServices->ExitBootServices(ImageHandle, MapKey);
        if (!EFI_ERROR(Status))
        {
            BootContext->MemoryMapAddress = MemoryMapAddress;
            BootContext->MemoryMapSize = (UINT64)MemoryMapSize;
            BootContext->MemoryMapBufferSize = (UINT64)(PageCount * (UINTN)LOS_PAGE_SIZE);
            BootContext->MemoryMapDescriptorSize = (UINT64)DescriptorSize;
            BootContext->MemoryMapDescriptorVersion = (UINT64)DescriptorVersion;
            BootContext->MemoryRegionCount = DescriptorSize == 0U ? 0ULL : ((UINT64)MemoryMapSize / (UINT64)DescriptorSize);
            return EFI_SUCCESS;
        }

        if (Status != EFI_INVALID_PARAMETER)
        {
            LosMonitorTraceStatus(SystemTable, LOS_TEXT("ExitBootServices failed: "), Status);
            return Status;
        }

        LosMonitorTrace(SystemTable, LOS_TEXT("ExitBootServices returned EFI_INVALID_PARAMETER. Retrying with refreshed map."));
    }

    LosMonitorTraceStatus(SystemTable, LOS_TEXT("ExitBootServices path failed: "), Status);
    return Status;
}
