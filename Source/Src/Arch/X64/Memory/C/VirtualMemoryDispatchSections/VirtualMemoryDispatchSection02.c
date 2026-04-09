/*
 * File Name: VirtualMemoryDispatchSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from VirtualMemoryDispatch.c.
 */

LOS_X64_BOOTSTRAP_SECTION
static UINT32 LosX64UnmapSinglePageFromAddressSpace(UINT64 PageMapLevel4PhysicalAddress, UINT64 VirtualAddress, UINT32 RequestFlags)
{
    UINT64 *PageMapLevel4;
    UINT64 *PageDirectoryPointerTable;
    UINT64 *PageDirectory;
    UINT64 *PageTable;
    UINT64 CurrentRootPhysicalAddress;
    UINT16 Pml4Index;
    UINT16 PdptIndex;
    UINT16 PdIndex;
    UINT16 PtIndex;

    if ((VirtualAddress & 0xFFFULL) != 0ULL)
    {
        return LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    }

    PageMapLevel4 = LosX64GetPageTableVirtualFromPhysical(PageMapLevel4PhysicalAddress);
    if (PageMapLevel4 == 0)
    {
        return LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
    }

    Pml4Index = LosX64GetPml4Index(VirtualAddress);
    PdptIndex = LosX64GetPdptIndex(VirtualAddress);
    PdIndex = LosX64GetPdIndex(VirtualAddress);
    PtIndex = LosX64GetPtIndex(VirtualAddress);
    CurrentRootPhysicalAddress = LosX64GetCurrentPageMapLevel4PhysicalAddress();

    if ((PageMapLevel4[Pml4Index] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
    }

    PageDirectoryPointerTable = LosX64GetPageTableVirtualFromPhysical(PageMapLevel4[Pml4Index] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageDirectoryPointerTable == 0)
    {
        return LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
    }
    if ((PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
    }
    if ((PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        if ((RequestFlags & LOS_X64_UNMAP_PAGES_FLAG_ALLOW_LARGE) == 0U)
        {
            return LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
        }
        PageDirectoryPointerTable[PdptIndex] = 0ULL;
        if (CurrentRootPhysicalAddress == PageMapLevel4PhysicalAddress)
        {
            LosX64InvalidatePage(VirtualAddress);
        }
        return LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    }

    PageDirectory = LosX64GetPageTableVirtualFromPhysical(PageDirectoryPointerTable[PdptIndex] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageDirectory == 0)
    {
        return LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
    }
    if ((PageDirectory[PdIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
    }
    if ((PageDirectory[PdIndex] & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        if ((RequestFlags & LOS_X64_UNMAP_PAGES_FLAG_ALLOW_LARGE) == 0U)
        {
            return LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
        }
        PageDirectory[PdIndex] = 0ULL;
        if (CurrentRootPhysicalAddress == PageMapLevel4PhysicalAddress)
        {
            LosX64InvalidatePage(VirtualAddress);
        }
        return LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    }

    PageTable = LosX64GetPageTableVirtualFromPhysical(PageDirectory[PdIndex] & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageTable == 0)
    {
        return LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
    }
    if ((PageTable[PtIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
    }

    PageTable[PtIndex] = 0ULL;
    if (CurrentRootPhysicalAddress == PageMapLevel4PhysicalAddress)
    {
        LosX64InvalidatePage(VirtualAddress);
    }
    return LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64MapPages(const LOS_X64_MAP_PAGES_REQUEST *Request, LOS_X64_MAP_PAGES_RESULT *Result)
{
    UINT64 PageMapLevel4PhysicalAddress;
    UINT64 *PageMapLevel4;
    UINT64 PhysicalAddress;
    UINT64 VirtualAddress;
    UINT64 PageIndex;
    UINT32 Status;

    LOS_KERNEL_ENTER();
    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->TargetPageMapLevel4PhysicalAddress = 0ULL;
    Result->PagesProcessed = 0ULL;
    Result->LastVirtualAddress = 0ULL;

    if (Request == 0 || Request->PageCount == 0ULL)
    {
        return;
    }

    if (!LosX64ResolveTargetPageMapLevel4(Request->PageMapLevel4PhysicalAddress, &PageMapLevel4PhysicalAddress, &PageMapLevel4))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
        return;
    }
    (void)PageMapLevel4;

    PhysicalAddress = Request->PhysicalAddress;
    VirtualAddress = Request->VirtualAddress;
    for (PageIndex = 0ULL; PageIndex < Request->PageCount; ++PageIndex)
    {
        Status = LosX64MapSinglePageIntoAddressSpace(PageMapLevel4PhysicalAddress, VirtualAddress, PhysicalAddress, Request->PageFlags, Request->Flags);
        if (Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS)
        {
            Result->Status = Status;
            Result->TargetPageMapLevel4PhysicalAddress = PageMapLevel4PhysicalAddress;
            return;
        }

        Result->PagesProcessed += 1ULL;
        Result->LastVirtualAddress = VirtualAddress;
        VirtualAddress += 4096ULL;
        PhysicalAddress += 4096ULL;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->TargetPageMapLevel4PhysicalAddress = PageMapLevel4PhysicalAddress;
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64UnmapPages(const LOS_X64_UNMAP_PAGES_REQUEST *Request, LOS_X64_UNMAP_PAGES_RESULT *Result)
{
    UINT64 PageMapLevel4PhysicalAddress;
    UINT64 *PageMapLevel4;
    UINT64 VirtualAddress;
    UINT64 PageIndex;
    UINT32 Status;

    LOS_KERNEL_ENTER();
    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->TargetPageMapLevel4PhysicalAddress = 0ULL;
    Result->PagesProcessed = 0ULL;
    Result->LastVirtualAddress = 0ULL;

    if (Request == 0 || Request->PageCount == 0ULL)
    {
        return;
    }

    if (!LosX64ResolveTargetPageMapLevel4(Request->PageMapLevel4PhysicalAddress, &PageMapLevel4PhysicalAddress, &PageMapLevel4))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_OUT_OF_RANGE;
        return;
    }
    (void)PageMapLevel4;

    VirtualAddress = Request->VirtualAddress;
    for (PageIndex = 0ULL; PageIndex < Request->PageCount; ++PageIndex)
    {
        Status = LosX64UnmapSinglePageFromAddressSpace(PageMapLevel4PhysicalAddress, VirtualAddress, Request->Flags);
        if (Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS)
        {
            Result->Status = Status;
            Result->TargetPageMapLevel4PhysicalAddress = PageMapLevel4PhysicalAddress;
            return;
        }

        Result->PagesProcessed += 1ULL;
        Result->LastVirtualAddress = VirtualAddress;
        VirtualAddress += 4096ULL;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->TargetPageMapLevel4PhysicalAddress = PageMapLevel4PhysicalAddress;
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
