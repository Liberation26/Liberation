/*
 * File Name: MemoryManagerAddressSpacePolicySection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerAddressSpacePolicy.c.
 */

BOOLEAN LosMemoryManagerSelectStackBaseVirtualAddress(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 DesiredBaseVirtualAddress,
    UINT64 StackPageCount,
    UINT64 *StackBaseVirtualAddress)
{
    UINT64 CandidateBase;
    UINT32 ScanIndex;

    if (StackBaseVirtualAddress != 0)
    {
        *StackBaseVirtualAddress = 0ULL;
    }
    if (AddressSpaceObject == 0 || StackBaseVirtualAddress == 0 || StackPageCount == 0ULL)
    {
        return 0;
    }

    CandidateBase = DesiredBaseVirtualAddress;
    if (CandidateBase == 0ULL)
    {
        CandidateBase = LOS_MEMORY_MANAGER_ADDRESS_SPACE_DEFAULT_STACK_BASE;
    }
    CandidateBase = AlignDownToPage(CandidateBase);

    if (DesiredBaseVirtualAddress != 0ULL)
    {
        for (ScanIndex = 0U; ScanIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++ScanIndex)
        {
            const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Current;

            Current = &AddressSpaceObject->ReservedVirtualRegions[ScanIndex];
            if (RangesOverlap(CandidateBase, StackPageCount, Current->BaseVirtualAddress, Current->PageCount))
            {
                return 0;
            }
        }
        if (CandidateBase + (StackPageCount * 0x1000ULL) <= CandidateBase ||
            CandidateBase + (StackPageCount * 0x1000ULL) > LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT)
        {
            return 0;
        }

        *StackBaseVirtualAddress = CandidateBase;
        return 1;
    }

    for (ScanIndex = 0U; ScanIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++ScanIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Current;

        Current = &AddressSpaceObject->ReservedVirtualRegions[ScanIndex];
        while (RangesOverlap(CandidateBase, StackPageCount, Current->BaseVirtualAddress, Current->PageCount))
        {
            UINT64 NextCandidate;

            NextCandidate = AlignUpToPage(
                Current->BaseVirtualAddress + (Current->PageCount * 0x1000ULL) + LOS_MEMORY_MANAGER_ADDRESS_SPACE_STACK_GAP_BYTES);
            if (NextCandidate <= CandidateBase)
            {
                return 0;
            }
            CandidateBase = NextCandidate;
        }
    }

    if (CandidateBase + (StackPageCount * 0x1000ULL) <= CandidateBase ||
        CandidateBase + (StackPageCount * 0x1000ULL) > LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT)
    {
        return 0;
    }

    *StackBaseVirtualAddress = CandidateBase;
    return 1;
}

static BOOLEAN LookupPageEntry(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 VirtualAddress,
    UINT64 **PageTable,
    UINTN *PageTableIndex)
{
    UINT64 *PageMapLevel4;
    UINT64 Entry;
    UINT64 *PageDirectoryPointerTable;
    UINT64 *PageDirectory;

    if (PageTable != 0)
    {
        *PageTable = 0;
    }
    if (PageTableIndex != 0)
    {
        *PageTableIndex = 0U;
    }
    if (State == 0 || PageTable == 0 || PageTableIndex == 0 ||
        PageMapLevel4PhysicalAddress == 0ULL || (VirtualAddress & 0xFFFULL) != 0ULL ||
        VirtualAddress >= LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT)
    {
        return 0;
    }

    PageMapLevel4 = TranslatePageTable(State, PageMapLevel4PhysicalAddress);
    if (PageMapLevel4 == 0)
    {
        return 0;
    }

    Entry = PageMapLevel4[GetPml4Index(VirtualAddress)];
    if ((Entry & LOS_X64_PAGE_PRESENT) == 0ULL || (Entry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 0;
    }

    PageDirectoryPointerTable = TranslatePageTable(State, Entry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageDirectoryPointerTable == 0)
    {
        return 0;
    }

    Entry = PageDirectoryPointerTable[GetPdptIndex(VirtualAddress)];
    if ((Entry & LOS_X64_PAGE_PRESENT) == 0ULL || (Entry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 0;
    }

    PageDirectory = TranslatePageTable(State, Entry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageDirectory == 0)
    {
        return 0;
    }

    Entry = PageDirectory[GetPdIndex(VirtualAddress)];
    if ((Entry & LOS_X64_PAGE_PRESENT) == 0ULL || (Entry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 0;
    }

    *PageTable = TranslatePageTable(State, Entry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (*PageTable == 0)
    {
        return 0;
    }

    *PageTableIndex = GetPtIndex(VirtualAddress);
    return 1;
}

BOOLEAN LosMemoryManagerUnmapPagesFromAddressSpace(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 VirtualAddress,
    UINT64 PageCount,
    UINT64 *PagesProcessed,
    UINT64 *LastVirtualAddress)
{
    UINT64 PageIndex;

    if (PagesProcessed != 0)
    {
        *PagesProcessed = 0ULL;
    }
    if (LastVirtualAddress != 0)
    {
        *LastVirtualAddress = 0ULL;
    }
    if (State == 0 || PageMapLevel4PhysicalAddress == 0ULL || (VirtualAddress & 0xFFFULL) != 0ULL || PageCount == 0ULL)
    {
        return 0;
    }

    for (PageIndex = 0ULL; PageIndex < PageCount; ++PageIndex)
    {
        UINT64 CurrentVirtualAddress;
        UINT64 *PageTable;
        UINTN PageTableIndex;

        CurrentVirtualAddress = VirtualAddress + (PageIndex * 0x1000ULL);
        if (CurrentVirtualAddress < VirtualAddress)
        {
            return 0;
        }
        if (!LookupPageEntry(State, PageMapLevel4PhysicalAddress, CurrentVirtualAddress, &PageTable, &PageTableIndex))
        {
            return 0;
        }
        if ((PageTable[PageTableIndex] & LOS_X64_PAGE_PRESENT) == 0ULL)
        {
            return 0;
        }

        PageTable[PageTableIndex] = 0ULL;
        if (PagesProcessed != 0)
        {
            *PagesProcessed = PageIndex + 1ULL;
        }
        if (LastVirtualAddress != 0)
        {
            *LastVirtualAddress = CurrentVirtualAddress;
        }
    }

    return 1;
}

BOOLEAN LosMemoryManagerProtectPagesInAddressSpace(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 VirtualAddress,
    UINT64 PageCount,
    UINT64 PageFlags,
    UINT64 *PagesProcessed,
    UINT64 *LastVirtualAddress)
{
    UINT64 PageIndex;
    UINT64 EffectivePageFlags;

    if (PagesProcessed != 0)
    {
        *PagesProcessed = 0ULL;
    }
    if (LastVirtualAddress != 0)
    {
        *LastVirtualAddress = 0ULL;
    }
    if (State == 0 || PageMapLevel4PhysicalAddress == 0ULL || (VirtualAddress & 0xFFFULL) != 0ULL || PageCount == 0ULL)
    {
        return 0;
    }
    if (!AreLeafPageFlagsValid(PageFlags))
    {
        LosMemoryManagerHardFail("invalid-protection-flags", PageFlags, VirtualAddress, PageMapLevel4PhysicalAddress);
    }

    EffectivePageFlags = (PageFlags | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER) & ~LOS_X64_PAGE_LARGE;
    for (PageIndex = 0ULL; PageIndex < PageCount; ++PageIndex)
    {
        UINT64 CurrentVirtualAddress;
        UINT64 *PageTable;
        UINTN PageTableIndex;
        UINT64 ExistingEntry;
        UINT64 PhysicalAddress;

        CurrentVirtualAddress = VirtualAddress + (PageIndex * 0x1000ULL);
        if (CurrentVirtualAddress < VirtualAddress)
        {
            return 0;
        }
        if (!LookupPageEntry(State, PageMapLevel4PhysicalAddress, CurrentVirtualAddress, &PageTable, &PageTableIndex))
        {
            return 0;
        }

        ExistingEntry = PageTable[PageTableIndex];
        if ((ExistingEntry & LOS_X64_PAGE_PRESENT) == 0ULL)
        {
            return 0;
        }

        PhysicalAddress = ExistingEntry & LOS_X64_PAGE_TABLE_ADDRESS_MASK;
        PageTable[PageTableIndex] = PhysicalAddress | EffectivePageFlags;
        if (PagesProcessed != 0)
        {
            *PagesProcessed = PageIndex + 1ULL;
        }
        if (LastVirtualAddress != 0)
        {
            *LastVirtualAddress = CurrentVirtualAddress;
        }
    }

    return 1;
}

BOOLEAN LosMemoryManagerQueryAddressSpaceMapping(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 VirtualAddress,
    UINT64 *PhysicalAddress,
    UINT64 *PageFlags)
{
    UINT64 *PageTable;
    UINTN PageTableIndex;
    UINT64 Entry;

    if (PhysicalAddress != 0)
    {
        *PhysicalAddress = 0ULL;
    }
    if (PageFlags != 0)
    {
        *PageFlags = 0ULL;
    }
    if (State == 0 || PhysicalAddress == 0 || PageFlags == 0)
    {
        return 0;
    }

    if (!LookupPageEntry(State, PageMapLevel4PhysicalAddress, VirtualAddress, &PageTable, &PageTableIndex))
    {
        return 0;
    }

    Entry = PageTable[PageTableIndex];
    if ((Entry & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }

    *PhysicalAddress = Entry & LOS_X64_PAGE_TABLE_ADDRESS_MASK;
    *PageFlags = Entry & ~LOS_X64_PAGE_TABLE_ADDRESS_MASK;
    return 1;
}
