#include "MemoryManagerAddressSpaceInternal.h"

static UINT64 AlignUpToPage(UINT64 Value)
{
    return (Value + 0xFFFULL) & ~0xFFFULL;
}

static UINT64 AlignDownToPage(UINT64 Value)
{
    return Value & ~0xFFFULL;
}

static UINTN GetPml4Index(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 39U) & 0x1FFULL);
}

static UINTN GetPdptIndex(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 30U) & 0x1FFULL);
}

static UINTN GetPdIndex(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 21U) & 0x1FFULL);
}

static UINTN GetPtIndex(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 12U) & 0x1FFULL);
}

static BOOLEAN RangesOverlap(UINT64 LeftBase, UINT64 LeftPageCount, UINT64 RightBase, UINT64 RightPageCount)
{
    UINT64 LeftEnd;
    UINT64 RightEnd;

    LeftEnd = LeftBase + (LeftPageCount * 0x1000ULL);
    RightEnd = RightBase + (RightPageCount * 0x1000ULL);
    if (LeftEnd <= LeftBase || RightEnd <= RightBase)
    {
        return 1;
    }

    return !(LeftEnd <= RightBase || RightEnd <= LeftBase);
}

static BOOLEAN AreLeafPageFlagsValid(UINT64 PageFlags)
{
    return (PageFlags & ~LOS_MEMORY_MANAGER_ALLOWED_LEAF_PAGE_FLAGS) == 0ULL;
}

static BOOLEAN IsVirtualRangeReserved(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 VirtualAddress,
    UINT64 PageCount)
{
    UINT32 ScanIndex;

    if (AddressSpaceObject == 0 || PageCount == 0ULL)
    {
        return 0;
    }

    for (ScanIndex = 0U; ScanIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++ScanIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Current;

        Current = &AddressSpaceObject->ReservedVirtualRegions[ScanIndex];
        if (VirtualAddress >= Current->BaseVirtualAddress &&
            !RangesOverlap(VirtualAddress, PageCount, Current->BaseVirtualAddress, Current->PageCount))
        {
            break;
        }
        if (VirtualAddress >= Current->BaseVirtualAddress &&
            VirtualAddress + (PageCount * 0x1000ULL) <= Current->BaseVirtualAddress + (Current->PageCount * 0x1000ULL) &&
            VirtualAddress + (PageCount * 0x1000ULL) > VirtualAddress)
        {
            return 1;
        }
    }

    return 0;
}

BOOLEAN LosMemoryManagerValidateAddressSpaceAccess(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 VirtualAddress,
    UINT64 PageCount,
    UINT64 PageFlags,
    BOOLEAN RequireValidPageFlags,
    BOOLEAN RequireReservedVirtualRange)
{
    UINT64 RangeBytes;
    UINT64 EndAddress;

    if (State == 0 || AddressSpaceObject == 0 || PageCount == 0ULL || (VirtualAddress & 0xFFFULL) != 0ULL)
    {
        return 0;
    }

    RangeBytes = PageCount * 0x1000ULL;
    if (RangeBytes == 0ULL || RangeBytes / 0x1000ULL != PageCount)
    {
        LosMemoryManagerHardFail("page-count-overflow", VirtualAddress, PageCount, AddressSpaceObject->AddressSpaceId);
    }
    if (!LosMemoryManagerTryGetRangeEnd(VirtualAddress, RangeBytes, &EndAddress) || EndAddress > LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT)
    {
        LosMemoryManagerHardFail("base-plus-size-wrap", VirtualAddress, RangeBytes, AddressSpaceObject->AddressSpaceId);
    }
    if (RequireValidPageFlags && !AreLeafPageFlagsValid(PageFlags))
    {
        LosMemoryManagerHardFail("invalid-protection-flags", PageFlags, VirtualAddress, AddressSpaceObject->AddressSpaceId);
    }
    if (RequireReservedVirtualRange && !IsVirtualRangeReserved(AddressSpaceObject, VirtualAddress, PageCount))
    {
        LosMemoryManagerHardFail("mapping-into-unreserved-virtual-space", VirtualAddress, PageCount, AddressSpaceObject->AddressSpaceId);
    }

    (void)State;
    return 1;
}

static UINT64 *TranslatePageTable(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 PhysicalAddress)
{
    return (UINT64 *)LosMemoryManagerTranslatePhysical(State, PhysicalAddress, 0x1000ULL);
}

static BOOLEAN EnsureChildTable(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 *ParentTable,
    UINTN EntryIndex,
    UINT64 **ChildTable)
{
    UINT64 Entry;
    UINT64 ChildPhysicalAddress;

    if (State == 0 || ParentTable == 0 || ChildTable == 0)
    {
        return 0;
    }

    Entry = ParentTable[EntryIndex];
    if ((Entry & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        if (!LosMemoryManagerHeapAllocatePages(
                State,
                1ULL,
                0x1000ULL,
                LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_PAGE_TABLE,
                (void **)ChildTable,
                &ChildPhysicalAddress))
        {
            return 0;
        }

        if (*ChildTable == 0)
        {
            (void)LosMemoryManagerHeapFree(State, ChildPhysicalAddress, 0);
            return 0;
        }

        {
            UINTN ZeroIndex;
            for (ZeroIndex = 0U; ZeroIndex < 512U; ++ZeroIndex)
            {
                (*ChildTable)[ZeroIndex] = 0ULL;
            }
        }

        ParentTable[EntryIndex] = ChildPhysicalAddress | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_USER;
        return 1;
    }

    if ((Entry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 0;
    }

    ChildPhysicalAddress = Entry & LOS_X64_PAGE_TABLE_ADDRESS_MASK;
    *ChildTable = TranslatePageTable(State, ChildPhysicalAddress);
    return *ChildTable != 0;
}

BOOLEAN LosMemoryManagerMapPagesIntoAddressSpace(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 VirtualAddress,
    UINT64 PhysicalAddress,
    UINT64 PageCount,
    UINT64 PageFlags)
{
    UINT64 *PageMapLevel4;
    UINT64 EffectivePageFlags;
    UINT64 PageIndex;

    if (State == 0 ||
        PageMapLevel4PhysicalAddress == 0ULL ||
        (VirtualAddress & 0xFFFULL) != 0ULL ||
        (PhysicalAddress & 0xFFFULL) != 0ULL ||
        PageCount == 0ULL ||
        VirtualAddress >= LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT)
    {
        return 0;
    }
    if (!AreLeafPageFlagsValid(PageFlags))
    {
        LosMemoryManagerHardFail("invalid-protection-flags", PageFlags, VirtualAddress, PageMapLevel4PhysicalAddress);
    }
    if (LosMemoryManagerDoesRangeWrap(PhysicalAddress, PageCount * 0x1000ULL))
    {
        LosMemoryManagerHardFail("base-plus-size-wrap", PhysicalAddress, PageCount, PageMapLevel4PhysicalAddress);
    }

    PageMapLevel4 = TranslatePageTable(State, PageMapLevel4PhysicalAddress);
    if (PageMapLevel4 == 0)
    {
        return 0;
    }

    EffectivePageFlags = PageFlags | LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER;
    EffectivePageFlags &= ~LOS_X64_PAGE_LARGE;

    for (PageIndex = 0ULL; PageIndex < PageCount; ++PageIndex)
    {
        UINT64 CurrentVirtualAddress;
        UINT64 CurrentPhysicalAddress;
        UINT64 *PageDirectoryPointerTable;
        UINT64 *PageDirectory;
        UINT64 *PageTable;
        UINTN Pml4Index;
        UINTN PdptIndex;
        UINTN PdIndex;
        UINTN PtIndex;

        CurrentVirtualAddress = VirtualAddress + (PageIndex * 0x1000ULL);
        CurrentPhysicalAddress = PhysicalAddress + (PageIndex * 0x1000ULL);
        if (CurrentVirtualAddress >= LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT ||
            CurrentVirtualAddress < VirtualAddress ||
            CurrentPhysicalAddress < PhysicalAddress)
        {
            return 0;
        }

        Pml4Index = GetPml4Index(CurrentVirtualAddress);
        PdptIndex = GetPdptIndex(CurrentVirtualAddress);
        PdIndex = GetPdIndex(CurrentVirtualAddress);
        PtIndex = GetPtIndex(CurrentVirtualAddress);

        if (!EnsureChildTable(State, PageMapLevel4, Pml4Index, &PageDirectoryPointerTable) ||
            !EnsureChildTable(State, PageDirectoryPointerTable, PdptIndex, &PageDirectory) ||
            !EnsureChildTable(State, PageDirectory, PdIndex, &PageTable))
        {
            return 0;
        }

        if ((PageTable[PtIndex] & LOS_X64_PAGE_PRESENT) != 0ULL)
        {
            return 0;
        }

        PageTable[PtIndex] = (CurrentPhysicalAddress & LOS_X64_PAGE_TABLE_ADDRESS_MASK) | EffectivePageFlags;
    }

    return 1;
}

BOOLEAN LosMemoryManagerReserveVirtualRegion(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 BaseVirtualAddress,
    UINT64 PageCount,
    UINT32 Type,
    UINT32 Flags,
    UINT64 BackingPhysicalAddress)
{
    UINT32 InsertIndex;
    UINT32 ScanIndex;

    if (AddressSpaceObject == 0 ||
        PageCount == 0ULL ||
        (BaseVirtualAddress & 0xFFFULL) != 0ULL ||
        BaseVirtualAddress >= LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT)
    {
        return 0;
    }

    if (BaseVirtualAddress + (PageCount * 0x1000ULL) <= BaseVirtualAddress ||
        BaseVirtualAddress + (PageCount * 0x1000ULL) > LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT)
    {
        return 0;
    }
    if (AddressSpaceObject->ReservedVirtualRegionCount >= LOS_MEMORY_MANAGER_MAX_RESERVED_VIRTUAL_REGIONS)
    {
        return 0;
    }

    InsertIndex = AddressSpaceObject->ReservedVirtualRegionCount;
    for (ScanIndex = 0U; ScanIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++ScanIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Current;

        Current = &AddressSpaceObject->ReservedVirtualRegions[ScanIndex];
        if (RangesOverlap(BaseVirtualAddress, PageCount, Current->BaseVirtualAddress, Current->PageCount))
        {
            return 0;
        }
        if (InsertIndex == AddressSpaceObject->ReservedVirtualRegionCount && BaseVirtualAddress < Current->BaseVirtualAddress)
        {
            InsertIndex = ScanIndex;
        }
    }

    for (ScanIndex = AddressSpaceObject->ReservedVirtualRegionCount; ScanIndex > InsertIndex; --ScanIndex)
    {
        AddressSpaceObject->ReservedVirtualRegions[ScanIndex] = AddressSpaceObject->ReservedVirtualRegions[ScanIndex - 1U];
    }

    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].BaseVirtualAddress = BaseVirtualAddress;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].PageCount = PageCount;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].Type = Type;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].Flags = Flags;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].BackingPhysicalAddress = BackingPhysicalAddress;
    AddressSpaceObject->ReservedVirtualRegionCount += 1U;
    return 1;
}

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
