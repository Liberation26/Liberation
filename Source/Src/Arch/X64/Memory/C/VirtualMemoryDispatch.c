#include "VirtualMemoryInternal.h"

static const char LosBootstrapVmInitializeBeginMessage[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Initializing bootstrap virtual memory.\n";
static const char LosBootstrapVmStateReadyMessage[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Bootstrap virtual-memory state initialized.\n";
static const char LosBootstrapVmPhysicalReadyMessage[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] EFI physical-memory state captured.\n";
static const char LosBootstrapVmPolicyReadyMessage[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Bootstrap paging policy built.\n";
static const char LosBootstrapVmKernelStackReadyMessage[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Higher-half kernel stack mapping prepared.\n";
static const char LosBootstrapVmKernelStackVerifiedMessage[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Higher-half kernel stack mapping verified.\n";
static const char LosBootstrapVmKernelStackFailedMessage[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Higher-half kernel stack mapping failed.\n";
static const char LosBootstrapVmOwnershipReadyMessage[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] CR3 switched to Liberation paging.\n";

LOS_X64_BOOTSTRAP_SECTION
void LosX64InitializeVirtualMemory(const LOS_BOOT_CONTEXT *BootContext)
{
    LOS_KERNEL_ENTER();
    LosX64BootstrapSerialWriteText(LosBootstrapVmInitializeBeginMessage);
    LosX64InitializeVirtualMemoryState(BootContext);
    LosX64BootstrapSerialWriteStatusTagOk();
    LosX64BootstrapSerialWriteText(LosBootstrapVmStateReadyMessage);
    LosX64BuildPhysicalMemoryState(BootContext);
    LosX64BootstrapSerialWriteStatusTagOk();
    LosX64BootstrapSerialWriteText(LosBootstrapVmPhysicalReadyMessage);
    LosX64BuildVirtualMemoryPolicy(BootContext);
    LosX64BootstrapSerialWriteStatusTagOk();
    LosX64BootstrapSerialWriteText(LosBootstrapVmPolicyReadyMessage);
    if (!LosX64InstallKernelStackMapping())
    {
        LosX64BootstrapSerialWriteStatusTagFail();
        LosX64BootstrapSerialWriteText(LosBootstrapVmKernelStackFailedMessage);
        LosX64BootstrapHaltForever();
    }
    LosX64BootstrapSerialWriteStatusTagOk();
    LosX64BootstrapSerialWriteText(LosBootstrapVmKernelStackReadyMessage);
    LosX64ValidateKernelStackMappingOrHalt();
    LosX64BootstrapSerialWriteStatusTagOk();
    LosX64BootstrapSerialWriteText(LosBootstrapVmKernelStackVerifiedMessage);
    LosX64TakeVirtualMemoryOwnership();
    LosX64BootstrapSerialWriteStatusTagOk();
    LosX64BootstrapSerialWriteText(LosBootstrapVmOwnershipReadyMessage);
}

static const char LosBootstrapVmHigherHalfJumpMessage[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Jumping to higher-half kernel entry.\n";

LOS_X64_BOOTSTRAP_SECTION
void LosX64ReportHigherHalfEntryJump(void)
{
    LosX64BootstrapSerialWriteStatusTagOk();
    LosX64BootstrapSerialWriteText(LosBootstrapVmHigherHalfJumpMessage);
}

LOS_X64_BOOTSTRAP_SECTION
static UINT16 LosX64GetPml4Index(UINT64 Address)
{
    LOS_KERNEL_ENTER();
    return (UINT16)((Address >> 39) & 0x1FFULL);
}

LOS_X64_BOOTSTRAP_SECTION
static UINT16 LosX64GetPdptIndex(UINT64 Address)
{
    LOS_KERNEL_ENTER();
    return (UINT16)((Address >> 30) & 0x1FFULL);
}

LOS_X64_BOOTSTRAP_SECTION
static UINT16 LosX64GetPdIndex(UINT64 Address)
{
    LOS_KERNEL_ENTER();
    return (UINT16)((Address >> 21) & 0x1FFULL);
}

LOS_X64_BOOTSTRAP_SECTION
static UINT16 LosX64GetPtIndex(UINT64 Address)
{
    LOS_KERNEL_ENTER();
    return (UINT16)((Address >> 12) & 0x1FFULL);
}

LOS_X64_BOOTSTRAP_SECTION
static LOS_X64_BOOTSTRAP_SECTION BOOLEAN LosX64MapVirtualPageInternal(UINT64 VirtualAddress, UINT64 PhysicalAddress, UINT64 Flags, BOOLEAN RequireDiscoveredPhysicalRange)
{
    UINT64 *PageMapLevel4;
    UINT64 *PageDirectoryPointerTable;
    UINT64 *PageDirectory;
    UINT64 *PageTable;
    UINT16 Pml4Index;
    UINT16 PdptIndex;
    UINT16 PdIndex;
    UINT16 PtIndex;

    if ((VirtualAddress & 0xFFFULL) != 0ULL || (PhysicalAddress & 0xFFFULL) != 0ULL)
    {
        return 0;
    }

    if (RequireDiscoveredPhysicalRange != 0U && !LosX64IsPhysicalRangeDiscovered(PhysicalAddress, 4096ULL))
    {
        return 0;
    }

    PageMapLevel4 = LosX64GetPageMapLevel4();
    Pml4Index = LosX64GetPml4Index(VirtualAddress);
    PdptIndex = LosX64GetPdptIndex(VirtualAddress);
    PdIndex = LosX64GetPdIndex(VirtualAddress);
    PtIndex = LosX64GetPtIndex(VirtualAddress);

    if ((PageMapLevel4[Pml4Index] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        PageDirectoryPointerTable = LosX64AllocatePageTablePage();
        if (PageDirectoryPointerTable == 0)
        {
            return 0;
        }
        PageMapLevel4[Pml4Index] = ((UINT64)(UINTN)PageDirectoryPointerTable) | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE;
    }

    PageDirectoryPointerTable = (UINT64 *)(UINTN)(PageMapLevel4[Pml4Index] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if ((PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        PageDirectory = LosX64AllocatePageTablePage();
        if (PageDirectory == 0)
        {
            return 0;
        }
        PageDirectoryPointerTable[PdptIndex] = ((UINT64)(UINTN)PageDirectory) | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE;
    }

    PageDirectory = (UINT64 *)(UINTN)(PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if ((PageDirectory[PdIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        PageTable = LosX64AllocatePageTablePage();
        if (PageTable == 0)
        {
            return 0;
        }
        PageDirectory[PdIndex] = ((UINT64)(UINTN)PageTable) | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE;
    }

    PageTable = (UINT64 *)(UINTN)(PageDirectory[PdIndex] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    PageTable[PtIndex] = (PhysicalAddress & LOS_X64_PAGE_TABLE_ADDRESS_MASK) | (Flags & ~LOS_X64_PAGE_TABLE_ADDRESS_MASK) | LOS_X64_PAGE_PRESENT;
    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64MapVirtualPage(UINT64 VirtualAddress, UINT64 PhysicalAddress, UINT64 Flags)
{
    LOS_KERNEL_ENTER();
    return LosX64MapVirtualPageInternal(VirtualAddress, PhysicalAddress, Flags, 1U);
}

static LOS_X64_BOOTSTRAP_SECTION BOOLEAN LosX64MapVirtualRangeInternal(UINT64 VirtualAddress, UINT64 PhysicalAddress, UINTN PageCount, UINT64 Flags, BOOLEAN RequireDiscoveredPhysicalRange)
{
    UINTN PageIndex;

    if (PageCount == 0U)
    {
        return 0;
    }

    for (PageIndex = 0U; PageIndex < PageCount; ++PageIndex)
    {
        if (!LosX64MapVirtualPageInternal(
                VirtualAddress + ((UINT64)PageIndex * 4096ULL),
                PhysicalAddress + ((UINT64)PageIndex * 4096ULL),
                Flags,
                RequireDiscoveredPhysicalRange))
        {
            return 0;
        }
    }

    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64MapVirtualRange(UINT64 VirtualAddress, UINT64 PhysicalAddress, UINTN PageCount, UINT64 Flags)
{
    LOS_KERNEL_ENTER();
    return LosX64MapVirtualRangeInternal(VirtualAddress, PhysicalAddress, PageCount, Flags, 1U);
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64MapVirtualRangeUnchecked(UINT64 VirtualAddress, UINT64 PhysicalAddress, UINTN PageCount, UINT64 Flags)
{
    LOS_KERNEL_ENTER();
    return LosX64MapVirtualRangeInternal(VirtualAddress, PhysicalAddress, PageCount, Flags, 0U);
}

LOS_X64_BOOTSTRAP_SECTION
static BOOLEAN LosX64IsVirtualAddressMapped(UINT64 VirtualAddress)
{
    UINT64 *PageMapLevel4;
    UINT64 *PageDirectoryPointerTable;
    UINT64 *PageDirectory;
    UINT64 *PageTable;
    UINT16 Pml4Index;
    UINT16 PdptIndex;
    UINT16 PdIndex;
    UINT16 PtIndex;

    PageMapLevel4 = LosX64GetPageMapLevel4();
    Pml4Index = LosX64GetPml4Index(VirtualAddress);
    PdptIndex = LosX64GetPdptIndex(VirtualAddress);
    PdIndex = LosX64GetPdIndex(VirtualAddress);
    PtIndex = LosX64GetPtIndex(VirtualAddress);

    if ((PageMapLevel4[Pml4Index] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }

    PageDirectoryPointerTable = (UINT64 *)(UINTN)(PageMapLevel4[Pml4Index] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if ((PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }

    if ((PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 1;
    }

    PageDirectory = (UINT64 *)(UINTN)(PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if ((PageDirectory[PdIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }

    if ((PageDirectory[PdIndex] & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 1;
    }

    PageTable = (UINT64 *)(UINTN)(PageDirectory[PdIndex] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    return (PageTable[PtIndex] & LOS_X64_PAGE_PRESENT) != 0ULL;
}

LOS_X64_BOOTSTRAP_SECTION
static void LosX64InvalidatePage(UINT64 VirtualAddress)
{
    __asm__ __volatile__("invlpg (%0)" : : "r"((UINTN)VirtualAddress) : "memory");
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64UnmapVirtualPage(UINT64 VirtualAddress)
{
    UINT64 *PageMapLevel4;
    UINT64 *PageDirectoryPointerTable;
    UINT64 *PageDirectory;
    UINT64 *PageTable;
    UINT16 Pml4Index;
    UINT16 PdptIndex;
    UINT16 PdIndex;
    UINT16 PtIndex;

    LOS_KERNEL_ENTER();
    if ((VirtualAddress & 0xFFFULL) != 0ULL)
    {
        return 0;
    }

    PageMapLevel4 = LosX64GetPageMapLevel4();
    Pml4Index = LosX64GetPml4Index(VirtualAddress);
    PdptIndex = LosX64GetPdptIndex(VirtualAddress);
    PdIndex = LosX64GetPdIndex(VirtualAddress);
    PtIndex = LosX64GetPtIndex(VirtualAddress);

    if ((PageMapLevel4[Pml4Index] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }

    PageDirectoryPointerTable = (UINT64 *)(UINTN)(PageMapLevel4[Pml4Index] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if ((PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }

    if ((PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        PageDirectoryPointerTable[PdptIndex] = 0ULL;
        LosX64InvalidatePage(VirtualAddress);
        return 1;
    }

    PageDirectory = (UINT64 *)(UINTN)(PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if ((PageDirectory[PdIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }

    if ((PageDirectory[PdIndex] & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        PageDirectory[PdIndex] = 0ULL;
        LosX64InvalidatePage(VirtualAddress);
        return 1;
    }

    PageTable = (UINT64 *)(UINTN)(PageDirectory[PdIndex] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if ((PageTable[PtIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }

    PageTable[PtIndex] = 0ULL;
    LosX64InvalidatePage(VirtualAddress);
    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
BOOLEAN LosX64UnmapVirtualRange(UINT64 VirtualAddress, UINTN PageCount)
{
    UINTN PageIndex;

    LOS_KERNEL_ENTER();
    if (PageCount == 0U)
    {
        return 0;
    }

    for (PageIndex = 0U; PageIndex < PageCount; ++PageIndex)
    {
        if (!LosX64UnmapVirtualPage(VirtualAddress + ((UINT64)PageIndex * 4096ULL)))
        {
            return 0;
        }
    }

    return 1;
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64ValidateKernelStackMappingOrHalt(void)
{
    UINT64 BaseAddress;
    UINTN PageIndex;

    BaseAddress = LOS_X64_KERNEL_STACK_BASE + (LOS_X64_KERNEL_STACK_GUARD_PAGES * 4096ULL);
    for (PageIndex = 0U; PageIndex < LOS_X64_KERNEL_STACK_COMMITTED_PAGES; ++PageIndex)
    {
        if (!LosX64IsVirtualAddressMapped(BaseAddress + ((UINT64)PageIndex * 4096ULL)))
        {
            LosX64BootstrapSerialWriteStatusTagFail();
        LosX64BootstrapSerialWriteText(LosBootstrapVmKernelStackFailedMessage);
            LosX64BootstrapSerialWriteLineHex64("[Kernel] Missing kernel stack virtual page: ", BaseAddress + ((UINT64)PageIndex * 4096ULL));
            LosX64BootstrapHaltForever();
        }
    }
}

LOS_X64_BOOTSTRAP_SECTION
void *LosX64GetKernelStackTop(void)
{
    LOS_KERNEL_ENTER();
    return (void *)(UINTN)LOS_X64_KERNEL_STACK_TOP;
}
