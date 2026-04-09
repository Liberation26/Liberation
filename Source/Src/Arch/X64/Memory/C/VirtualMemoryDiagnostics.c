/*
 * File Name: VirtualMemoryDiagnostics.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "VirtualMemoryInternal.h"

#define LOS_X64_VERBOSE_PHYSICAL_MEMORY_DUMP 0U

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

void LosX64DescribePhysicalMemoryState(void)
{
    LOS_KERNEL_ENTER();
    const LOS_X64_MEMORY_MANAGER_HANDOFF *Handoff;

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

#if LOS_X64_VERBOSE_PHYSICAL_MEMORY_DUMP
    {
        UINTN Index;

        for (Index = 0U; Index < LosX64GetPhysicalMemoryDescriptorCount(); ++Index)
        {
            const LOS_X64_PHYSICAL_MEMORY_DESCRIPTOR *Descriptor;

            Descriptor = LosX64GetPhysicalMemoryDescriptor(Index);
            if (Descriptor == 0)
            {
                continue;
            }

            LosKernelSerialWriteText("[Kernel] EFI descriptor ");
            LosKernelSerialWriteUnsigned(Index);
            LosKernelSerialWriteText(": base=");
            LosKernelSerialWriteHex64(Descriptor->BaseAddress);
            LosKernelSerialWriteText(" length=");
            LosKernelSerialWriteHex64(Descriptor->Length);
            LosKernelSerialWriteText(" type=");
            LosKernelSerialWriteUnsigned(Descriptor->Type);
            LosKernelSerialWriteText("\n");
        }

        for (Index = 0U; Index < LosX64GetMemoryRegionCount(); ++Index)
        {
            const LOS_X64_MEMORY_REGION *Region;

            Region = LosX64GetMemoryRegion(Index);
            if (Region == 0)
            {
                continue;
            }

            LosKernelSerialWriteText("[Kernel] Memory region ");
            LosKernelSerialWriteUnsigned(Index);
            LosKernelSerialWriteText(": base=");
            LosKernelSerialWriteHex64(Region->Base);
            LosKernelSerialWriteText(" length=");
            LosKernelSerialWriteHex64(Region->Length);
            LosKernelSerialWriteText(" type=");
            LosKernelSerialWriteUnsigned(Region->Type);
            LosKernelSerialWriteText(" flags=");
            LosKernelSerialWriteUnsigned(Region->Flags);
            LosKernelSerialWriteText(" owner=");
            LosKernelSerialWriteUnsigned(Region->Owner);
            LosKernelSerialWriteText(" source=");
            LosKernelSerialWriteUnsigned(Region->Source);
            LosKernelSerialWriteText("\n");
        }
    }
#endif
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
