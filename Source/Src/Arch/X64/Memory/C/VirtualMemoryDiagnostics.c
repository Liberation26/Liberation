/*
 * File Name: VirtualMemoryDiagnostics.c
 * File Version: 0.3.12
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-10T18:55:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "VirtualMemoryInternal.h"

static const char *LosX64GetEfiMemoryTypeName(UINT32 Type)
{
    switch (Type)
    {
        case 0U:
            return "Reserved";
        case 1U:
            return "LoaderCode";
        case 2U:
            return "LoaderData";
        case 3U:
            return "BootServicesCode";
        case 4U:
            return "BootServicesData";
        case 5U:
            return "RuntimeCode";
        case 6U:
            return "RuntimeData";
        case 7U:
            return "Conventional";
        case 8U:
            return "Unusable";
        case 9U:
            return "AcpiReclaim";
        case 10U:
            return "AcpiNvs";
        case 11U:
            return "Mmio";
        case 12U:
            return "MmioPort";
        case 13U:
            return "PalCode";
        case 14U:
            return "Persistent";
        default:
            return "Unknown";
    }
}

static const char *LosX64GetMemoryRegionTypeName(UINT32 Type)
{
    switch (Type)
    {
        case LOS_X64_MEMORY_REGION_TYPE_USABLE:
            return "Usable";
        case LOS_X64_MEMORY_REGION_TYPE_BOOT_RESERVED:
            return "BootReserved";
        case LOS_X64_MEMORY_REGION_TYPE_RUNTIME:
            return "Runtime";
        case LOS_X64_MEMORY_REGION_TYPE_MMIO:
            return "Mmio";
        case LOS_X64_MEMORY_REGION_TYPE_ACPI_NVS:
            return "AcpiNvs";
        case LOS_X64_MEMORY_REGION_TYPE_FIRMWARE_RESERVED:
            return "FirmwareReserved";
        case LOS_X64_MEMORY_REGION_TYPE_UNUSABLE:
            return "Unusable";
        default:
            return "Unknown";
    }
}

static const char *LosX64GetMemoryRegionSourceName(UINT32 Source)
{
    switch (Source)
    {
        case LOS_X64_MEMORY_REGION_SOURCE_EFI:
            return "Efi";
        case LOS_X64_MEMORY_REGION_SOURCE_BOOTSTRAP:
            return "Bootstrap";
        case LOS_X64_MEMORY_REGION_SOURCE_KERNEL:
            return "Kernel";
        case LOS_X64_MEMORY_REGION_SOURCE_RUNTIME:
            return "Runtime";
        default:
            return "Unknown";
    }
}

void LosX64DescribeVirtualMemoryLayout(void)
{
    LOS_KERNEL_ENTER();
    const LOS_X64_VIRTUAL_MEMORY_LAYOUT *Layout;

    Layout = LosX64GetVirtualMemoryLayout();
    LosKernelSerialWriteText("[Kernel] VM layout bootstrap identity: base=");
    LosKernelSerialWriteHex64(Layout->BootstrapIdentityBase);
    LosKernelSerialWriteText(" size=");
    LosKernelSerialWriteHex64(Layout->BootstrapIdentitySize);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] VM layout higher-half direct map: base=");
    LosKernelSerialWriteHex64(Layout->HigherHalfDirectMapBase);
    LosKernelSerialWriteText(" size=");
    LosKernelSerialWriteHex64(Layout->HigherHalfDirectMapSize);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Highest discovered physical address: ");
    LosKernelSerialWriteHex64(Layout->HighestDiscoveredPhysicalAddress);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Higher-half kernel text: virtual=");
    LosKernelSerialWriteHex64((UINT64)(UINTN)__LosKernelHigherHalfTextStart);
    LosKernelSerialWriteText("..");
    LosKernelSerialWriteHex64((UINT64)(UINTN)__LosKernelHigherHalfTextEnd);
    LosKernelSerialWriteText(" load=");
    LosKernelSerialWriteHex64((UINT64)(UINTN)__LosKernelHigherHalfTextLoadStart);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Higher-half kernel data+bss: virtual=");
    LosKernelSerialWriteHex64((UINT64)(UINTN)__LosKernelHigherHalfDataStart);
    LosKernelSerialWriteText("..");
    LosKernelSerialWriteHex64((UINT64)(UINTN)__LosKernelHigherHalfBssEnd);
    LosKernelSerialWriteText(" load=");
    LosKernelSerialWriteHex64((UINT64)(UINTN)__LosKernelHigherHalfDataLoadStart);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] VM layout kernel window: base=");
    LosKernelSerialWriteHex64(Layout->KernelWindowBase);
    LosKernelSerialWriteText(" size=");
    LosKernelSerialWriteHex64(Layout->KernelWindowSize);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] VM layout kernel stack: base=");
    LosKernelSerialWriteHex64(Layout->KernelStackBase);
    LosKernelSerialWriteText(" size=");
    LosKernelSerialWriteHex64(Layout->KernelStackSize);
    LosKernelSerialWriteText(" top=");
    LosKernelSerialWriteHex64(Layout->KernelStackTop);
    LosKernelSerialWriteText("\n");
}

void LosX64DescribeBootMemoryMap(const LOS_BOOT_CONTEXT *BootContext)
{
    const void *MappedBuffer;
    const UINT8 *DescriptorBytes;
    UINT64 DescriptorCount;
    UINT64 Index;

    LOS_KERNEL_ENTER();
    if (BootContext == 0 || BootContext->MemoryMapAddress == 0ULL || BootContext->MemoryMapSize == 0ULL || BootContext->MemoryMapDescriptorSize == 0ULL)
    {
        LosKernelSerialWriteText("[Kernel] EFI memory map unavailable.\n");
        return;
    }

    MappedBuffer = LosX64GetDirectMapVirtualAddress(BootContext->MemoryMapAddress, BootContext->MemoryMapSize);
    if (MappedBuffer == 0)
    {
        LosKernelSerialWriteText("[Kernel] EFI memory map could not be direct-mapped for reporting.\n");
        return;
    }

    DescriptorBytes = (const UINT8 *)MappedBuffer;
    DescriptorCount = BootContext->MemoryMapSize / BootContext->MemoryMapDescriptorSize;
    LosKernelSerialWriteText("[Kernel] EFI memory map begin. descriptors=");
    LosKernelSerialWriteUnsigned(DescriptorCount);
    LosKernelSerialWriteText(" descriptor-bytes=");
    LosKernelSerialWriteUnsigned(BootContext->MemoryMapDescriptorSize);
    LosKernelSerialWriteText(" version=");
    LosKernelSerialWriteUnsigned(BootContext->MemoryMapDescriptorVersion);
    LosKernelSerialWriteText("\n");

    for (Index = 0ULL; Index < DescriptorCount; ++Index)
    {
        const EFI_MEMORY_DESCRIPTOR *Descriptor;
        UINT64 ByteLength;

        Descriptor = (const EFI_MEMORY_DESCRIPTOR *)(const void *)(DescriptorBytes + (Index * BootContext->MemoryMapDescriptorSize));
        ByteLength = Descriptor->NumberOfPages * 4096ULL;

        LosKernelSerialWriteText("[Kernel] EFI[");
        LosKernelSerialWriteUnsigned(Index);
        LosKernelSerialWriteText("] type=");
        LosKernelSerialWriteText(LosX64GetEfiMemoryTypeName(Descriptor->Type));
        LosKernelSerialWriteText("(");
        LosKernelSerialWriteUnsigned((UINT64)Descriptor->Type);
        LosKernelSerialWriteText(") base=");
        LosKernelSerialWriteHex64(Descriptor->PhysicalStart);
        LosKernelSerialWriteText(" pages=");
        LosKernelSerialWriteUnsigned(Descriptor->NumberOfPages);
        LosKernelSerialWriteText(" bytes=");
        LosKernelSerialWriteHex64(ByteLength);
        LosKernelSerialWriteText(" attr=");
        LosKernelSerialWriteHex64(Descriptor->Attribute);
        LosKernelSerialWriteText("\n");
    }

    LosKernelSerialWriteText("[Kernel] EFI memory map end.\n");
}

void LosX64DescribePhysicalMemoryState(void)
{
    LOS_KERNEL_ENTER();
    const LOS_X64_MEMORY_MANAGER_HANDOFF *Handoff;
    UINTN Index;

    Handoff = LosX64GetMemoryManagerHandoff();
    LosKernelSerialWriteText("[Kernel] Total usable memory: ");
    LosKernelSerialWriteUnsigned(Handoff->TotalUsableBytes);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Total bootstrap-reserved memory: ");
    LosKernelSerialWriteUnsigned(Handoff->TotalBootstrapReservedBytes);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Total firmware-reserved memory: ");
    LosKernelSerialWriteUnsigned(Handoff->TotalFirmwareReservedBytes);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Total runtime memory: ");
    LosKernelSerialWriteUnsigned(Handoff->TotalRuntimeBytes);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Total MMIO memory: ");
    LosKernelSerialWriteUnsigned(Handoff->TotalMmioBytes);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Total ACPI/NVS memory: ");
    LosKernelSerialWriteUnsigned(Handoff->TotalAcpiBytes);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Total unusable memory: ");
    LosKernelSerialWriteUnsigned(Handoff->TotalUnusableBytes);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Total address-space gaps: ");
    LosKernelSerialWriteUnsigned(Handoff->TotalAddressSpaceGapBytes);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Highest usable physical address: ");
    LosKernelSerialWriteHex64(Handoff->HighestUsablePhysicalAddress);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Normalized memory map begin. regions=");
    LosKernelSerialWriteUnsigned(LosX64GetMemoryRegionCount());
    LosKernelSerialWriteText("\n");

    for (Index = 0U; Index < LosX64GetMemoryRegionCount(); ++Index)
    {
        const LOS_X64_MEMORY_REGION *Region;

        Region = LosX64GetMemoryRegion(Index);
        if (Region == 0)
        {
            continue;
        }

        LosKernelSerialWriteText("[Kernel] Region[");
        LosKernelSerialWriteUnsigned(Index);
        LosKernelSerialWriteText("] type=");
        LosKernelSerialWriteText(LosX64GetMemoryRegionTypeName(Region->Type));
        LosKernelSerialWriteText("(");
        LosKernelSerialWriteUnsigned((UINT64)Region->Type);
        LosKernelSerialWriteText(") base=");
        LosKernelSerialWriteHex64(Region->Base);
        LosKernelSerialWriteText(" bytes=");
        LosKernelSerialWriteHex64(Region->Length);
        LosKernelSerialWriteText(" flags=");
        LosKernelSerialWriteHex64((UINT64)Region->Flags);
        LosKernelSerialWriteText(" owner=");
        LosKernelSerialWriteUnsigned((UINT64)Region->Owner);
        LosKernelSerialWriteText(" source=");
        LosKernelSerialWriteText(LosX64GetMemoryRegionSourceName(Region->Source));
        LosKernelSerialWriteText("(");
        LosKernelSerialWriteUnsigned((UINT64)Region->Source);
        LosKernelSerialWriteText(")\n");
    }

    LosKernelSerialWriteText("[Kernel] Normalized memory map end.\n");
}

void LosX64DescribeMemoryManagerHandoff(void)
{
    LOS_KERNEL_ENTER();
    const LOS_X64_MEMORY_MANAGER_HANDOFF *Handoff;

    Handoff = LosX64GetMemoryManagerHandoff();
    LosKernelSerialWriteText("[Kernel] Memory-manager normalized region table: ");
    LosKernelSerialWriteHex64(Handoff->RegionDatabaseAddress);
    LosKernelSerialWriteText(" regions=");
    LosKernelSerialWriteUnsigned(Handoff->RegionCount);
    LosKernelSerialWriteText(" entry-bytes=");
    LosKernelSerialWriteUnsigned(Handoff->RegionEntrySize);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Memory-manager handoff flags: ");
    LosKernelSerialWriteHex64(Handoff->Flags);
    LosKernelSerialWriteText("\n");
}
