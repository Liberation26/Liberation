/*
 * File Name: MemoryManagerAddressSpacePolicySection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerAddressSpacePolicy.c.
 */

static UINT64 LosMemoryManagerDiagnosticAttachImagePhysicalBase;
static UINT64 LosMemoryManagerDiagnosticAttachImageMappedBytes;
static UINT64 LosMemoryManagerDiagnosticAttachTargetRootPhysicalAddress;
static UINT64 LosMemoryManagerDiagnosticAttachTargetObjectPhysicalAddress;
static UINT32 LosMemoryManagerDiagnosticAttachContextActive;

void LosMemoryManagerDiagnosticsSetAttachImageContext(
    UINT64 ImagePhysicalBase,
    UINT64 ImageMappedBytes,
    UINT64 TargetRootPhysicalAddress,
    UINT64 TargetObjectPhysicalAddress)
{
    LosMemoryManagerDiagnosticAttachImagePhysicalBase = ImagePhysicalBase;
    LosMemoryManagerDiagnosticAttachImageMappedBytes = ImageMappedBytes;
    LosMemoryManagerDiagnosticAttachTargetRootPhysicalAddress = TargetRootPhysicalAddress;
    LosMemoryManagerDiagnosticAttachTargetObjectPhysicalAddress = TargetObjectPhysicalAddress;
    LosMemoryManagerDiagnosticAttachContextActive = 1U;
}

void LosMemoryManagerDiagnosticsClearAttachImageContext(void)
{
    LosMemoryManagerDiagnosticAttachImagePhysicalBase = 0ULL;
    LosMemoryManagerDiagnosticAttachImageMappedBytes = 0ULL;
    LosMemoryManagerDiagnosticAttachTargetRootPhysicalAddress = 0ULL;
    LosMemoryManagerDiagnosticAttachTargetObjectPhysicalAddress = 0ULL;
    LosMemoryManagerDiagnosticAttachContextActive = 0U;
}


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

        if (LosMemoryManagerDiagnosticAttachContextActive != 0U)
        {
            LosMemoryManagerServiceSerialWriteText("[MemManager][diag] child-table-allocated-phys=");
            LosMemoryManagerServiceSerialWriteHex64(ChildPhysicalAddress);
            LosMemoryManagerServiceSerialWriteText("\n");
            LosMemoryManagerServiceSerialWriteText("[MemManager][diag] child-table-entry-index=");
            LosMemoryManagerServiceSerialWriteUnsigned((UINT64)EntryIndex);
            LosMemoryManagerServiceSerialWriteText("\n");
            if ((LosMemoryManagerDiagnosticAttachImageMappedBytes != 0ULL &&
                 RangesOverlap(ChildPhysicalAddress, 1ULL, LosMemoryManagerDiagnosticAttachImagePhysicalBase, LosMemoryManagerDiagnosticAttachImageMappedBytes / 0x1000ULL)) ||
                RangesOverlap(ChildPhysicalAddress, 1ULL, LosMemoryManagerDiagnosticAttachTargetRootPhysicalAddress, 1ULL) ||
                RangesOverlap(ChildPhysicalAddress, 1ULL, LosMemoryManagerDiagnosticAttachTargetObjectPhysicalAddress & ~0xFFFULL, 1ULL))
            {
                LosMemoryManagerHardFail("attach-child-table-overlap", ChildPhysicalAddress, LosMemoryManagerDiagnosticAttachImagePhysicalBase, LosMemoryManagerDiagnosticAttachTargetRootPhysicalAddress);
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

    if (LosMemoryManagerDiagnosticAttachContextActive != 0U)
    {
        LosMemoryManagerServiceSerialWriteText("[MemManager][diag] reserve-enter-base=");
        LosMemoryManagerServiceSerialWriteHex64(BaseVirtualAddress);
        LosMemoryManagerServiceSerialWriteText("\n");
        LosMemoryManagerServiceSerialWriteText("[MemManager][diag] reserve-enter-pages=");
        LosMemoryManagerServiceSerialWriteUnsigned(PageCount);
        LosMemoryManagerServiceSerialWriteText("\n");
        LosMemoryManagerServiceSerialWriteText("[MemManager][diag] reserve-enter-backing=");
        LosMemoryManagerServiceSerialWriteHex64(BackingPhysicalAddress);
        LosMemoryManagerServiceSerialWriteText("\n");
        LosMemoryManagerServiceSerialWriteText("[MemManager][diag] reserve-enter-count=");
        LosMemoryManagerServiceSerialWriteUnsigned(AddressSpaceObject != 0 ? AddressSpaceObject->ReservedVirtualRegionCount : 0ULL);
        LosMemoryManagerServiceSerialWriteText("\n");
    }

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

    if (LosMemoryManagerDiagnosticAttachContextActive != 0U)
    {
        LosMemoryManagerServiceSerialWriteText("[MemManager][diag] reserve-insert-index=");
        LosMemoryManagerServiceSerialWriteUnsigned(InsertIndex);
        LosMemoryManagerServiceSerialWriteText("\n");
    }

    for (ScanIndex = AddressSpaceObject->ReservedVirtualRegionCount; ScanIndex > InsertIndex; --ScanIndex)
    {
        if (LosMemoryManagerDiagnosticAttachContextActive != 0U)
        {
            LosMemoryManagerServiceSerialWriteText("[MemManager][diag] reserve-shift-index=");
            LosMemoryManagerServiceSerialWriteUnsigned(ScanIndex);
            LosMemoryManagerServiceSerialWriteText("\n");
        }
        AddressSpaceObject->ReservedVirtualRegions[ScanIndex] = AddressSpaceObject->ReservedVirtualRegions[ScanIndex - 1U];
    }

    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].BaseVirtualAddress = BaseVirtualAddress;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].PageCount = PageCount;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].Type = Type;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].Flags = Flags;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].BackingPhysicalAddress = BackingPhysicalAddress;
    AddressSpaceObject->ReservedVirtualRegionCount += 1U;
    if (LosMemoryManagerDiagnosticAttachContextActive != 0U)
    {
        LosMemoryManagerServiceSerialWriteText("[MemManager][diag] reserve-exit-count=");
        LosMemoryManagerServiceSerialWriteUnsigned(AddressSpaceObject->ReservedVirtualRegionCount);
        LosMemoryManagerServiceSerialWriteText("\n");
        LosMemoryManagerServiceSerialWriteText("[MemManager][diag] reserve-return-address=");
        LosMemoryManagerServiceSerialWriteHex64((UINT64)(UINTN)__builtin_return_address(0));
        LosMemoryManagerServiceSerialWriteText("\n");
    }
    return 1;
}
