/*
 * File Name: VirtualMemoryStateSection03.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from VirtualMemoryState.c.
 */

LOS_X64_BOOTSTRAP_SECTION
static void ReserveBootstrapRanges(const LOS_BOOT_CONTEXT *BootContext)
{
    UINT64 GdtPhysicalBase;
    UINT64 IdtPhysicalBase;

    if (BootContext == 0)
    {
        return;
    }

    if (BootContext->KernelImagePhysicalAddress != 0ULL && BootContext->KernelImageSize != 0ULL)
    {
        LosX64ReservePhysicalRange(BootContext->KernelImagePhysicalAddress, BootContext->KernelImageSize, LOS_X64_PHYSICAL_FRAME_RESERVED_KERNEL_IMAGE);
    }
    if (BootContext->BootContextAddress != 0ULL && BootContext->BootContextSize != 0ULL)
    {
        LosX64ReservePhysicalRange(BootContext->BootContextAddress, BootContext->BootContextSize, LOS_X64_PHYSICAL_FRAME_RESERVED_BOOT_CONTEXT);
    }
    if (BootContext->MemoryMapAddress != 0ULL && BootContext->MemoryMapBufferSize != 0ULL)
    {
        LosX64ReservePhysicalRange(BootContext->MemoryMapAddress, BootContext->MemoryMapBufferSize, LOS_X64_PHYSICAL_FRAME_RESERVED_MEMORY_MAP);
    }
    if (BootContext->KernelFontPhysicalAddress != 0ULL && BootContext->KernelFontSize != 0ULL)
    {
        LosX64ReservePhysicalRange(BootContext->KernelFontPhysicalAddress, BootContext->KernelFontSize, LOS_X64_PHYSICAL_FRAME_RESERVED_KERNEL_FONT);
    }

    LosX64ReservePhysicalRange(LosX64GetBootstrapPageTableStorageBase(), LosX64GetBootstrapPageTableStorageSize(), LOS_X64_PHYSICAL_FRAME_RESERVED_BOOTSTRAP_PAGE_TABLES);
    LosX64ReservePhysicalRange(LosX64GetBootstrapTransitionStackBase(), LosX64GetBootstrapTransitionStackSize(), LOS_X64_PHYSICAL_FRAME_RESERVED_BOOTSTRAP_STACK);
    LosX64ReservePhysicalRange(LosX64GetKernelStackBackingBase(), LosX64GetKernelStackBackingSize(), LOS_X64_PHYSICAL_FRAME_RESERVED_KERNEL_STACK);

    if (TryTranslateHigherHalfPointerToPhysical((UINT64)(UINTN)&LosGdt[0], &GdtPhysicalBase) != 0U)
    {
        LosX64ReservePhysicalRange(GdtPhysicalBase, sizeof(LosGdt), LOS_X64_PHYSICAL_FRAME_RESERVED_GDT);
    }

    if (TryTranslateHigherHalfPointerToPhysical((UINT64)(UINTN)&LosX64Idt[0], &IdtPhysicalBase) != 0U)
    {
        LosX64ReservePhysicalRange(IdtPhysicalBase, sizeof(LosX64Idt), LOS_X64_PHYSICAL_FRAME_RESERVED_IDT);
    }
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64InitializeVirtualMemoryState(const LOS_BOOT_CONTEXT *BootContext)
{
    LOS_KERNEL_ENTER();
    UINTN Index;
    (void)BootContext;

    ZeroEntries(LosPageMapLevel4, 512U);
    ZeroEntries(LosIdentityDirectoryPointer, 512U);
    ZeroEntries(LosIdentityDirectory, 512U);
    ZeroEntries(LosKernelWindowDirectoryPointer, 512U);
    ZeroEntries(LosKernelWindowDirectory, 512U);
    for (Index = 0U; Index < LOS_X64_PAGE_TABLE_POOL_PAGES; ++Index)
    {
        ZeroEntries(LosPageTablePool[Index], 512U);
    }
    for (Index = 0U; Index < LOS_X64_KERNEL_STACK_COMMITTED_PAGES; ++Index)
    {
        ZeroEntries(LosKernelStackBacking[Index], 512U);
    }
    for (Index = 0U; Index < LOS_X64_MAX_PHYSICAL_MEMORY_DESCRIPTORS; ++Index)
    {
        LosPhysicalMemoryDescriptors[Index].BaseAddress = 0ULL;
        LosPhysicalMemoryDescriptors[Index].Length = 0ULL;
        LosPhysicalMemoryDescriptors[Index].Type = 0U;
        LosPhysicalMemoryDescriptors[Index].Flags = 0U;
        LosPhysicalMemoryDescriptors[Index].Attributes = 0ULL;
    }
    ZeroBytes((UINT8 *)&LosPhysicalFrameRegions[0], sizeof(LosPhysicalFrameRegions));
    ZeroBytes((UINT8 *)&LosPhysicalFrameRegionScratch[0], sizeof(LosPhysicalFrameRegionScratch));
    ZeroBytes((UINT8 *)&LosBaseMemoryRegions[0], sizeof(LosBaseMemoryRegions));
    ZeroBytes((UINT8 *)&LosOverlayMemoryRegions[0], sizeof(LosOverlayMemoryRegions));
    ZeroBytes((UINT8 *)&LosMemoryRegions[0], sizeof(LosMemoryRegions));
    ZeroBytes((UINT8 *)&LosMemoryRegionScratch[0], sizeof(LosMemoryRegionScratch));
    ZeroBytes((UINT8 *)&LosPhysicalSpanScratch[0], sizeof(LosPhysicalSpanScratch));
    ZeroBytes((UINT8 *)&LosMemoryManagerHandoff, sizeof(LosMemoryManagerHandoff));

    LosPageTablePoolNextIndex = 0U;
    LosPhysicalMemoryDescriptorCount = 0U;
    LosPhysicalFrameRegionCount = 0U;
    LosBaseMemoryRegionCount = 0U;
    LosOverlayMemoryRegionCount = 0U;
    LosMemoryRegionCount = 0U;
    LosAddressSpaceGapBytes = 0ULL;
    LosLayout.BootstrapIdentityBase = 0ULL;
    LosLayout.BootstrapIdentitySize = AlignUpLargePageBoundary((UINT64)(UINTN)__LosKernelBootstrapEnd);
    if (LosLayout.BootstrapIdentitySize < LOS_X64_BOOTSTRAP_IDENTITY_BYTES)
    {
        LosLayout.BootstrapIdentitySize = LOS_X64_BOOTSTRAP_IDENTITY_BYTES;
    }
    LosLayout.HigherHalfDirectMapBase = LOS_X64_HIGHER_HALF_BASE;
    LosLayout.HigherHalfDirectMapSize = 0ULL;
    LosLayout.KernelWindowBase = LOS_X64_KERNEL_WINDOW_BASE;
    LosLayout.KernelWindowSize = 0x40000000ULL;
    LosLayout.KernelStackBase = LOS_X64_KERNEL_STACK_BASE + (LOS_X64_KERNEL_STACK_GUARD_PAGES * 4096ULL);
    LosLayout.KernelStackSize = LOS_X64_KERNEL_STACK_SIZE_BYTES;
    LosLayout.KernelStackTop = LOS_X64_KERNEL_STACK_TOP;
    LosLayout.HighestDiscoveredPhysicalAddress = 0ULL;
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64BuildPhysicalMemoryState(const LOS_BOOT_CONTEXT *BootContext)
{
    LOS_KERNEL_ENTER();
    UINTN DescriptorCount;
    UINTN Index;
    UINT64 HighestUsableEnd;

    if (BootContext == 0 || BootContext->MemoryMapAddress == 0ULL || BootContext->MemoryMapDescriptorSize == 0ULL)
    {
        return;
    }

    DescriptorCount = (UINTN)(BootContext->MemoryMapSize / BootContext->MemoryMapDescriptorSize);
    HighestUsableEnd = 0ULL;

    for (Index = 0U; Index < DescriptorCount && LosPhysicalMemoryDescriptorCount < LOS_X64_MAX_PHYSICAL_MEMORY_DESCRIPTORS; ++Index)
    {
        const EFI_MEMORY_DESCRIPTOR *SourceDescriptor;
        LOS_X64_PHYSICAL_MEMORY_DESCRIPTOR *TargetDescriptor;
        UINT64 Length;
        UINT64 EndAddress;
        UINT32 State;

        SourceDescriptor = (const EFI_MEMORY_DESCRIPTOR *)(UINTN)(BootContext->MemoryMapAddress + ((UINT64)Index * BootContext->MemoryMapDescriptorSize));
        Length = SourceDescriptor->NumberOfPages * 4096ULL;
        if (Length == 0ULL)
        {
            continue;
        }

        EndAddress = SourceDescriptor->PhysicalStart + Length;
        if (EndAddress > LosLayout.HighestDiscoveredPhysicalAddress)
        {
            LosLayout.HighestDiscoveredPhysicalAddress = EndAddress;
        }

        TargetDescriptor = &LosPhysicalMemoryDescriptors[LosPhysicalMemoryDescriptorCount++];
        TargetDescriptor->BaseAddress = SourceDescriptor->PhysicalStart;
        TargetDescriptor->Length = Length;
        TargetDescriptor->Type = SourceDescriptor->Type;
        TargetDescriptor->Flags = GetPhysicalMemoryDescriptorFlags(SourceDescriptor->Type);
        TargetDescriptor->Attributes = SourceDescriptor->Attribute;

        State = ClassifyFrameState(SourceDescriptor->Type);
        if (!AppendPhysicalFrameRegion(SourceDescriptor->PhysicalStart, SourceDescriptor->NumberOfPages, State, SourceDescriptor->Type, State == LOS_X64_PHYSICAL_FRAME_STATE_RESERVED ? LOS_X64_PHYSICAL_FRAME_RESERVED_FIRMWARE : LOS_X64_PHYSICAL_FRAME_RESERVED_NONE, SourceDescriptor->Attribute))
        {
            LosX64BootstrapSerialWriteText("[Kernel] Physical frame region database overflow.\n");
            LosX64BootstrapHaltForever();
        }

        if (!AppendBaseMemoryRegion(
                SourceDescriptor->PhysicalStart,
                Length,
                ClassifyMemoryRegionType(SourceDescriptor->Type),
                GetMemoryRegionFlagsForDescriptorType(SourceDescriptor->Type),
                GetMemoryRegionOwnerForDescriptorType(SourceDescriptor->Type),
                GetMemoryRegionSourceForDescriptorType(SourceDescriptor->Type)))
        {
            LosX64BootstrapSerialWriteText("[Kernel] Memory-manager region database overflow.\n");
            LosX64BootstrapHaltForever();
        }

        if ((TargetDescriptor->Flags & 1U) != 0U && EndAddress > HighestUsableEnd)
        {
            HighestUsableEnd = EndAddress;
        }
    }

    LosAddressSpaceGapBytes = CalculateAddressSpaceGapBytes();
    LosLayout.HigherHalfDirectMapSize = HighestUsableEnd;
    if (!RebuildPublishedMemoryRegions())
    {
        LosX64BootstrapSerialWriteText("[Kernel] Normalized memory-region database overflow.\n");
        LosX64BootstrapHaltForever();
    }
    RefreshMemoryManagerHandoff();
    ReserveBootstrapRanges(BootContext);
    RefreshMemoryManagerHandoff();
}

LOS_X64_BOOTSTRAP_SECTION
const LOS_X64_VIRTUAL_MEMORY_LAYOUT *LosX64GetVirtualMemoryLayout(void)
{
    LOS_KERNEL_ENTER();
    return &LosLayout;
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64GetPageMapLevel4(void)
{
    LOS_KERNEL_ENTER();
    return &LosPageMapLevel4[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64GetIdentityDirectoryPointer(void)
{
    LOS_KERNEL_ENTER();
    return &LosIdentityDirectoryPointer[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64GetIdentityDirectory(void)
{
    LOS_KERNEL_ENTER();
    return &LosIdentityDirectory[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64GetKernelWindowDirectoryPointer(void)
{
    LOS_KERNEL_ENTER();
    return &LosKernelWindowDirectoryPointer[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64GetKernelWindowDirectory(void)
{
    LOS_KERNEL_ENTER();
    return &LosKernelWindowDirectory[0];
}

LOS_X64_BOOTSTRAP_SECTION
UINT64 *LosX64AllocatePageTablePage(void)
{
    LOS_KERNEL_ENTER();
    if (LosPageTablePoolNextIndex >= LOS_X64_PAGE_TABLE_POOL_PAGES)
    {
        return 0;
    }

    return &LosPageTablePool[LosPageTablePoolNextIndex++][0];
}

LOS_X64_BOOTSTRAP_SECTION
UINTN LosX64GetPhysicalMemoryDescriptorCount(void)
{
    LOS_KERNEL_ENTER();
    return LosPhysicalMemoryDescriptorCount;
}

LOS_X64_BOOTSTRAP_SECTION
const LOS_X64_PHYSICAL_MEMORY_DESCRIPTOR *LosX64GetPhysicalMemoryDescriptor(UINTN Index)
{
    LOS_KERNEL_ENTER();
    if (Index >= LosPhysicalMemoryDescriptorCount)
    {
        return 0;
    }

    return &LosPhysicalMemoryDescriptors[Index];
}

LOS_X64_BOOTSTRAP_SECTION
UINTN LosX64GetPhysicalFrameRegionCount(void)
{
    LOS_KERNEL_ENTER();
    return LosPhysicalFrameRegionCount;
}

LOS_X64_BOOTSTRAP_SECTION
const LOS_X64_PHYSICAL_FRAME_REGION *LosX64GetPhysicalFrameRegion(UINTN Index)
{
    LOS_KERNEL_ENTER();
    if (Index >= LosPhysicalFrameRegionCount)
    {
        return 0;
    }

    return &LosPhysicalFrameRegions[Index];
}

LOS_X64_BOOTSTRAP_SECTION
UINTN LosX64GetMemoryRegionCount(void)
{
    LOS_KERNEL_ENTER();
    return LosMemoryRegionCount;
}

LOS_X64_BOOTSTRAP_SECTION
const LOS_X64_MEMORY_REGION *LosX64GetMemoryRegion(UINTN Index)
{
    LOS_KERNEL_ENTER();
    if (Index >= LosMemoryRegionCount)
    {
        return 0;
    }

    return &LosMemoryRegions[Index];
}

LOS_X64_BOOTSTRAP_SECTION
const LOS_X64_MEMORY_MANAGER_HANDOFF *LosX64GetMemoryManagerHandoff(void)
{
    LOS_KERNEL_ENTER();
    return &LosMemoryManagerHandoff;
}

LOS_X64_BOOTSTRAP_SECTION
UINTN LosX64GetPageTablePoolUsedCount(void)
{
    LOS_KERNEL_ENTER();
    return LosPageTablePoolNextIndex;
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64IsPhysicalRangeDiscovered(UINT64 PhysicalAddress, UINT64 Length)
{
    UINT64 EndAddress;

    if (Length == 0ULL)
    {
        return 0;
    }

    EndAddress = PhysicalAddress + Length;
    if (EndAddress < PhysicalAddress)
    {
        return 0;
    }

    return IsRangeCoveredByAnyRegion(PhysicalAddress, EndAddress);
}
