#include "VirtualMemoryInternal.h"

static const char LosBootstrapIdentityExceededMessage[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Bootstrap identity map exceeded one page-directory span.\n";
static const char LosBootstrapDirectMapFailedMessage[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Failed to build higher-half direct map.\n";
static const char LosBootstrapTextMapFailedMessage[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Failed to map higher-half kernel text.\n";
static const char LosBootstrapDataMapFailedMessage[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Failed to map higher-half kernel data.\n";
static const char LosBootstrapBssMapFailedMessage[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Failed to map higher-half kernel bss.\n";
static const char LosBootstrapImageAlignmentMismatchMessage[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Higher-half kernel image alignment mismatch.\n";

static const char LosBootstrapBlankLine[] __attribute__((section(".bootstrap.rodata"))) = "\n";
static const char LosBootstrapRequestedVirtualStartPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Requested virtual start: ";
static const char LosBootstrapRequestedVirtualEndPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Requested virtual end: ";
static const char LosBootstrapRequestedPhysicalStartPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Requested physical start: ";
static const char LosBootstrapLinkedTextLoadStartPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Linked text load start: ";
static const char LosBootstrapLinkedTextLoadEndPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Linked text load end: ";
static const char LosBootstrapLinkedDataLoadStartPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Linked data load start: ";
static const char LosBootstrapLinkedBssLoadStartPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Linked bss load start: ";
static const char LosBootstrapPhysicalDescriptorCountPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Physical descriptor count: ";
static const char LosBootstrapSegmentMapFailedMessage[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Failed to map higher-half kernel load segment.\n";
static const char LosBootstrapSegmentVirtualStartPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Segment virtual start: ";
static const char LosBootstrapSegmentPhysicalStartPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Segment physical start: ";
static const char LosBootstrapSegmentMemoryBytesPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Segment memory bytes: ";
static const char LosBootstrapSegmentFlagsPrefix[] __attribute__((section(".bootstrap.rodata"))) = "[Kernel] Segment flags: ";


LOS_X64_BOOTSTRAP_SECTION
static UINT64 ReadCr0(void)
{
    UINT64 Value;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(Value));
    return Value;
}

LOS_X64_BOOTSTRAP_SECTION
static void WriteCr0(UINT64 Value)
{
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(Value) : "memory");
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 ReadCr3(void)
{
    UINT64 Value;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(Value));
    return Value;
}

LOS_X64_BOOTSTRAP_SECTION
static void WriteCr3(UINT64 Value)
{
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(Value) : "memory");
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 ReadCr4(void)
{
    UINT64 Value;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(Value));
    return Value;
}

LOS_X64_BOOTSTRAP_SECTION
static void WriteCr4(UINT64 Value)
{
    __asm__ __volatile__("mov %0, %%cr4" : : "r"(Value) : "memory");
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 ReadMsr(UINT32 Register)
{
    UINT32 Low;
    UINT32 High;
    __asm__ __volatile__("rdmsr" : "=a"(Low), "=d"(High) : "c"(Register));
    return ((UINT64)High << 32) | (UINT64)Low;
}

LOS_X64_BOOTSTRAP_SECTION
static void WriteMsr(UINT32 Register, UINT64 Value)
{
    UINT32 Low;
    UINT32 High;
    Low = (UINT32)(Value & 0xFFFFFFFFULL);
    High = (UINT32)(Value >> 32);
    __asm__ __volatile__("wrmsr" : : "c"(Register), "a"(Low), "d"(High));
}

LOS_X64_BOOTSTRAP_SECTION
static UINT16 GetPml4Index(UINT64 Address)
{
    return (UINT16)((Address >> 39) & 0x1FFULL);
}

LOS_X64_BOOTSTRAP_SECTION
static UINT16 GetPdptIndex(UINT64 Address)
{
    return (UINT16)((Address >> 30) & 0x1FFULL);
}

LOS_X64_BOOTSTRAP_SECTION
static UINT16 GetPdIndex(UINT64 Address)
{
    return (UINT16)((Address >> 21) & 0x1FFULL);
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 AlignDownPage(UINT64 Value)
{
    return Value & ~0xFFFULL;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 AlignUpPage(UINT64 Value)
{
    return (Value + 0xFFFULL) & ~0xFFFULL;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN EnsurePageDirectoryPointer(UINT64 *PageMapLevel4, UINT16 Pml4Index, UINT64 **PageDirectoryPointerTable)
{
    if ((PageMapLevel4[Pml4Index] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        UINT64 *NewTable;
        NewTable = LosX64AllocatePageTablePage();
        if (NewTable == 0)
        {
            return 0;
        }
        PageMapLevel4[Pml4Index] = ((UINT64)(UINTN)NewTable) | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE;
    }

    *PageDirectoryPointerTable = (UINT64 *)(UINTN)(PageMapLevel4[Pml4Index] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN EnsurePageDirectory(UINT64 *PageDirectoryPointerTable, UINT16 PdptIndex, UINT64 **PageDirectory)
{
    if ((PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        UINT64 *NewTable;
        NewTable = LosX64AllocatePageTablePage();
        if (NewTable == 0)
        {
            return 0;
        }
        PageDirectoryPointerTable[PdptIndex] = ((UINT64)(UINTN)NewTable) | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE;
    }

    *PageDirectory = (UINT64 *)(UINTN)(PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN MapLargePage(UINT64 VirtualAddress, UINT64 PhysicalAddress, UINT64 Flags)
{
    UINT64 *PageMapLevel4;
    UINT64 *PageDirectoryPointerTable;
    UINT64 *PageDirectory;
    UINT16 Pml4Index;
    UINT16 PdptIndex;
    UINT16 PdIndex;

    PageMapLevel4 = LosX64GetPageMapLevel4();
    Pml4Index = GetPml4Index(VirtualAddress);
    PdptIndex = GetPdptIndex(VirtualAddress);
    PdIndex = GetPdIndex(VirtualAddress);

    if (!EnsurePageDirectoryPointer(PageMapLevel4, Pml4Index, &PageDirectoryPointerTable))
    {
        return 0;
    }

    if (!EnsurePageDirectory(PageDirectoryPointerTable, PdptIndex, &PageDirectory))
    {
        return 0;
    }

    PageDirectory[PdIndex] = (PhysicalAddress & 0x000FFFFFFFE00000ULL) | (Flags & ~LOS_X64_PAGE_TABLE_ADDRESS_MASK) | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_LARGE;
    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN MapKernelImageRange(UINT64 VirtualStart, UINT64 VirtualEnd, UINT64 PhysicalStart, UINT64 Flags)
{
    UINT64 AlignedVirtualStart;
    UINT64 AlignedVirtualEnd;
    UINT64 AlignedPhysicalStart;
    UINT64 ByteCount;
    UINTN PageCount;

    if (VirtualEnd <= VirtualStart)
    {
        return 1;
    }

    AlignedVirtualStart = AlignDownPage(VirtualStart);
    AlignedVirtualEnd = AlignUpPage(VirtualEnd);
    AlignedPhysicalStart = AlignDownPage(PhysicalStart);

    if (((VirtualStart - AlignedVirtualStart) != (PhysicalStart - AlignedPhysicalStart)))
    {
        LosX64BootstrapSerialWriteText(LosBootstrapImageAlignmentMismatchMessage);
        return 0;
    }

    ByteCount = AlignedVirtualEnd - AlignedVirtualStart;
    PageCount = (UINTN)(ByteCount / 4096ULL);
    return LosX64MapVirtualRangeUnchecked(AlignedVirtualStart, AlignedPhysicalStart, PageCount, Flags);
}

LOS_X64_BOOTSTRAP_SECTION
static void ReportKernelImageMapFailure(const char *Prefix, UINT64 VirtualStart, UINT64 VirtualEnd, UINT64 PhysicalStart)
{
    LosX64BootstrapSerialWriteText(Prefix);
    LosX64BootstrapSerialWriteText(LosBootstrapBlankLine);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapRequestedVirtualStartPrefix, VirtualStart);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapRequestedVirtualEndPrefix, VirtualEnd);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapRequestedPhysicalStartPrefix, PhysicalStart);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapLinkedTextLoadStartPrefix, (UINT64)(UINTN)__LosKernelHigherHalfTextLoadStart);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapLinkedTextLoadEndPrefix, (UINT64)(UINTN)__LosKernelHigherHalfTextLoadEnd);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapLinkedDataLoadStartPrefix, (UINT64)(UINTN)__LosKernelHigherHalfDataLoadStart);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapLinkedBssLoadStartPrefix, (UINT64)(UINTN)__LosKernelHigherHalfBssLoadStart);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapPhysicalDescriptorCountPrefix, (UINT64)LosX64GetPhysicalMemoryDescriptorCount());
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN LosX64IsHigherHalfLoadSegment(UINT64 VirtualAddress)
{
    return VirtualAddress >= 0xFFFF000000000000ULL;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 LosX64GetPageFlagsForKernelLoadSegment(UINT64 SegmentFlags)
{
    UINT64 PageFlags;

    PageFlags = LOS_X64_PAGE_GLOBAL;
    if ((SegmentFlags & LOS_ELF_PROGRAM_FLAG_WRITE) != 0ULL)
    {
        PageFlags |= LOS_X64_PAGE_WRITABLE;
    }
    if ((SegmentFlags & LOS_ELF_PROGRAM_FLAG_EXECUTE) == 0ULL)
    {
        PageFlags |= LOS_X64_PAGE_NX;
    }

    return PageFlags;
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN MapKernelLoadSegmentFromBootContext(const LOS_BOOT_CONTEXT_LOAD_SEGMENT *Segment)
{
    if (Segment == 0 || Segment->MemorySize == 0ULL || !LosX64IsHigherHalfLoadSegment(Segment->VirtualAddress))
    {
        return 1;
    }

    return MapKernelImageRange(
        Segment->VirtualAddress,
        Segment->VirtualAddress + Segment->MemorySize,
        Segment->PhysicalAddress,
        LosX64GetPageFlagsForKernelLoadSegment(Segment->Flags));
}

LOS_X64_BOOTSTRAP_SECTION
static void ReportKernelLoadSegmentMapFailure(const LOS_BOOT_CONTEXT_LOAD_SEGMENT *Segment)
{
    LosX64BootstrapSerialWriteText(LosBootstrapSegmentMapFailedMessage);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapSegmentVirtualStartPrefix, Segment != 0 ? Segment->VirtualAddress : 0ULL);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapSegmentPhysicalStartPrefix, Segment != 0 ? Segment->PhysicalAddress : 0ULL);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapSegmentMemoryBytesPrefix, Segment != 0 ? Segment->MemorySize : 0ULL);
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapSegmentFlagsPrefix, Segment != 0 ? Segment->Flags : 0ULL);
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN MapHigherHalfKernelSegmentsFromBootContext(const LOS_BOOT_CONTEXT *BootContext)
{
    UINT64 SegmentIndex;
    BOOLEAN MappedAnySegment;

    if (BootContext == 0 || (BootContext->Flags & LOS_BOOT_CONTEXT_FLAG_KERNEL_SEGMENTS_VALID) == 0ULL || BootContext->KernelLoadSegmentCount == 0ULL)
    {
        return 0;
    }

    MappedAnySegment = 0;
    for (SegmentIndex = 0ULL; SegmentIndex < BootContext->KernelLoadSegmentCount && SegmentIndex < LOS_BOOT_CONTEXT_MAX_LOAD_SEGMENTS; ++SegmentIndex)
    {
        const LOS_BOOT_CONTEXT_LOAD_SEGMENT *Segment;
        Segment = &BootContext->KernelLoadSegments[SegmentIndex];
        if (!LosX64IsHigherHalfLoadSegment(Segment->VirtualAddress) || Segment->MemorySize == 0ULL)
        {
            continue;
        }

        if (!MapKernelLoadSegmentFromBootContext(Segment))
        {
            ReportKernelLoadSegmentMapFailure(Segment);
            LosX64BootstrapHaltForever();
        }

        MappedAnySegment = 1;
    }

    return MappedAnySegment;
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64BuildVirtualMemoryPolicy(const LOS_BOOT_CONTEXT *BootContext)
{
    const LOS_X64_VIRTUAL_MEMORY_LAYOUT *Layout;
    UINT64 *PageMapLevel4;
    UINT64 *IdentityDirectoryPointer;
    UINT64 *IdentityDirectory;
    UINT64 *KernelWindowDirectoryPointer;
    UINT64 *KernelWindowDirectory;
    UINTN DescriptorIndex;
    UINTN DirectoryIndex;
    UINTN IdentityPageCount;
    UINT16 KernelWindowPml4Index;
    UINT16 KernelWindowPdptIndex;

    Layout = LosX64GetVirtualMemoryLayout();
    PageMapLevel4 = LosX64GetPageMapLevel4();
    IdentityDirectoryPointer = LosX64GetIdentityDirectoryPointer();
    IdentityDirectory = LosX64GetIdentityDirectory();
    KernelWindowDirectoryPointer = LosX64GetKernelWindowDirectoryPointer();
    KernelWindowDirectory = LosX64GetKernelWindowDirectory();

    PageMapLevel4[0] = ((UINT64)(UINTN)IdentityDirectoryPointer) | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE;
    IdentityDirectoryPointer[0] = ((UINT64)(UINTN)IdentityDirectory) | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE;
    IdentityPageCount = (UINTN)((Layout->BootstrapIdentitySize + 0x1FFFFFULL) / 0x200000ULL);
    if (IdentityPageCount == 0U)
    {
        IdentityPageCount = 1U;
    }
    if (IdentityPageCount > 512U)
    {
        LosX64BootstrapSerialWriteText(LosBootstrapIdentityExceededMessage);
        LosX64BootstrapHaltForever();
    }
    for (DirectoryIndex = 0U; DirectoryIndex < IdentityPageCount; ++DirectoryIndex)
    {
        IdentityDirectory[DirectoryIndex] =
            ((UINT64)DirectoryIndex * 0x200000ULL) |
            LOS_X64_PAGE_PRESENT |
            LOS_X64_PAGE_WRITABLE |
            LOS_X64_PAGE_LARGE |
            LOS_X64_PAGE_GLOBAL;
    }

    for (DescriptorIndex = 0U; DescriptorIndex < LosX64GetPhysicalMemoryDescriptorCount(); ++DescriptorIndex)
    {
        const LOS_X64_PHYSICAL_MEMORY_DESCRIPTOR *Descriptor;
        UINT64 PhysicalBase;
        UINT64 PhysicalEnd;
        UINT64 CurrentPhysical;

        Descriptor = LosX64GetPhysicalMemoryDescriptor(DescriptorIndex);
        if (Descriptor == 0 || (Descriptor->Flags & 1U) == 0U)
        {
            continue;
        }

        PhysicalBase = Descriptor->BaseAddress & ~0x1FFFFFULL;
        PhysicalEnd = (Descriptor->BaseAddress + Descriptor->Length + 0x1FFFFFULL) & ~0x1FFFFFULL;
        for (CurrentPhysical = PhysicalBase; CurrentPhysical < PhysicalEnd; CurrentPhysical += 0x200000ULL)
        {
            if (!MapLargePage(Layout->HigherHalfDirectMapBase + CurrentPhysical, CurrentPhysical, LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_GLOBAL))
            {
                LosX64BootstrapSerialWriteText(LosBootstrapDirectMapFailedMessage);
                LosX64BootstrapHaltForever();
            }
        }
    }

    if (!MapHigherHalfKernelSegmentsFromBootContext(BootContext))
    {
        if (!MapKernelImageRange((UINT64)(UINTN)__LosKernelHigherHalfTextStart, (UINT64)(UINTN)__LosKernelHigherHalfTextEnd, (UINT64)(UINTN)__LosKernelHigherHalfTextLoadStart, LOS_X64_PAGE_GLOBAL))
        {
            ReportKernelImageMapFailure(
                LosBootstrapTextMapFailedMessage,
                (UINT64)(UINTN)__LosKernelHigherHalfTextStart,
                (UINT64)(UINTN)__LosKernelHigherHalfTextEnd,
                (UINT64)(UINTN)__LosKernelHigherHalfTextLoadStart);
            LosX64BootstrapHaltForever();
        }

        if (!MapKernelImageRange((UINT64)(UINTN)__LosKernelHigherHalfDataStart, (UINT64)(UINTN)__LosKernelHigherHalfDataEnd, (UINT64)(UINTN)__LosKernelHigherHalfDataLoadStart, LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_GLOBAL | LOS_X64_PAGE_NX))
        {
            ReportKernelImageMapFailure(
                LosBootstrapDataMapFailedMessage,
                (UINT64)(UINTN)__LosKernelHigherHalfDataStart,
                (UINT64)(UINTN)__LosKernelHigherHalfDataEnd,
                (UINT64)(UINTN)__LosKernelHigherHalfDataLoadStart);
            LosX64BootstrapHaltForever();
        }

        if (!MapKernelImageRange((UINT64)(UINTN)__LosKernelHigherHalfBssStart, (UINT64)(UINTN)__LosKernelHigherHalfBssEnd, (UINT64)(UINTN)__LosKernelHigherHalfBssLoadStart, LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_GLOBAL | LOS_X64_PAGE_NX))
        {
            ReportKernelImageMapFailure(
                LosBootstrapBssMapFailedMessage,
                (UINT64)(UINTN)__LosKernelHigherHalfBssStart,
                (UINT64)(UINTN)__LosKernelHigherHalfBssEnd,
                (UINT64)(UINTN)__LosKernelHigherHalfBssLoadStart);
            LosX64BootstrapHaltForever();
        }
    }

    KernelWindowPml4Index = GetPml4Index(LOS_X64_KERNEL_WINDOW_BASE);
    KernelWindowPdptIndex = GetPdptIndex(LOS_X64_KERNEL_WINDOW_BASE);
    PageMapLevel4[KernelWindowPml4Index] = ((UINT64)(UINTN)KernelWindowDirectoryPointer) | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE;
    KernelWindowDirectoryPointer[KernelWindowPdptIndex] = ((UINT64)(UINTN)KernelWindowDirectory) | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE;
    for (DirectoryIndex = 0U; DirectoryIndex < 512U; ++DirectoryIndex)
    {
        UINT64 PhysicalBase;
        PhysicalBase = (UINT64)DirectoryIndex * 0x200000ULL;
        KernelWindowDirectory[DirectoryIndex] =
            PhysicalBase | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_LARGE | LOS_X64_PAGE_GLOBAL;
    }
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64TakeVirtualMemoryOwnership(void)
{
    UINT64 Cr0;
    UINT64 Cr4;
    UINT64 Efer;

    Cr4 = ReadCr4();
    Cr4 |= LOS_X64_CR4_PAE | LOS_X64_CR4_PGE;
    WriteCr4(Cr4);

    Efer = ReadMsr(LOS_X64_EFER_MSR);
    Efer |= LOS_X64_EFER_NXE | LOS_X64_EFER_SCE;
    WriteMsr(LOS_X64_EFER_MSR, Efer);

    WriteCr3((UINT64)(UINTN)LosX64GetPageMapLevel4());

    Cr0 = ReadCr0();
    Cr0 |= LOS_X64_CR0_WP;
    WriteCr0(Cr0);

    (void)ReadCr3();
}
