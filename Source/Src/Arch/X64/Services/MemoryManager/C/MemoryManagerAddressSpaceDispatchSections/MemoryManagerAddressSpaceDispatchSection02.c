/*
 * File Name: MemoryManagerAddressSpaceDispatchSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerAddressSpaceDispatch.c.
 */

static void AddressSpaceServiceEnsureRangeTrackedReserved(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 BaseAddress,
    UINT64 PageCount,
    UINT32 Usage,
    const char *Label)
{
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry;

    if (State == 0 || PageCount == 0ULL)
    {
        return;
    }

    Entry = AddressSpaceServiceFindFrameEntryForPhysicalRange(&State->MemoryView, BaseAddress, PageCount * 4096ULL);
    if (Entry == 0)
    {
        AddressSpaceServiceSerialWriteText("[MemManager][diag] live physical range missing from page-frame database.\n");
        AddressSpaceServiceSerialWriteText("[MemManager][diag] missing-live-range=");
        AddressSpaceServiceSerialWriteText(Label);
        AddressSpaceServiceSerialWriteText("\n");
        AddressSpaceServiceSerialWriteNamedHex("missing-live-range-base", BaseAddress);
        AddressSpaceServiceSerialWriteNamedUnsigned("missing-live-range-pages", PageCount);
        LosMemoryManagerHardFail("live-range-not-in-frame-database", BaseAddress, PageCount, Usage);
    }

    if (Entry->State == LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE)
    {
        AddressSpaceServiceSerialWriteText("[MemManager][diag] live physical range was unexpectedly free; reserving it now.\n");
        AddressSpaceServiceSerialWriteText("[MemManager][diag] repair-live-range=");
        AddressSpaceServiceSerialWriteText(Label);
        AddressSpaceServiceSerialWriteText("\n");
        AddressSpaceServiceSerialWriteNamedHex("repair-live-range-base", BaseAddress);
        AddressSpaceServiceSerialWriteNamedUnsigned("repair-live-range-pages", PageCount);
        if (!LosMemoryManagerInsertDynamicAllocation(&State->MemoryView, BaseAddress, PageCount, Usage, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
            !LosMemoryManagerRebuildCurrentPageFrameDatabase(&State->MemoryView))
        {
            LosMemoryManagerHardFail("failed-to-repair-live-range-tracking", BaseAddress, PageCount, Usage);
        }
    }
}

static void AddressSpaceServiceEnsureAddressSpaceAssetsTracked(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 AddressSpaceObjectPhysicalAddress,
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject)
{
    if (State == 0 || AddressSpaceObject == 0)
    {
        return;
    }

    AddressSpaceServiceEnsureRangeTrackedReserved(
        State,
        AddressSpaceObjectPhysicalAddress & ~0xFFFULL,
        1ULL,
        LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ADDRESS_SPACE_OBJECT,
        "address-space-object-page");

    AddressSpaceServiceEnsureRangeTrackedReserved(
        State,
        AddressSpaceObject->RootTablePhysicalAddress,
        1ULL,
        LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_PAGE_TABLE,
        "address-space-root-page");
}

static void AddressSpaceServiceLogCreated(UINT64 AddressSpaceId, UINT64 AddressSpaceObjectPhysicalAddress, UINT64 RootTablePhysicalAddress)
{
    AddressSpaceServiceSerialWriteText("[MemManager] Address space created id=");
    AddressSpaceServiceSerialWriteUnsigned(AddressSpaceId);
    AddressSpaceServiceSerialWriteText(" object=");
    AddressSpaceServiceSerialWriteHex64(AddressSpaceObjectPhysicalAddress);
    AddressSpaceServiceSerialWriteText(" root=");
    AddressSpaceServiceSerialWriteHex64(RootTablePhysicalAddress);
    AddressSpaceServiceSerialWriteText("\n");
}

static BOOLEAN TrySynthesizeMappingFromReservedRegion(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 VirtualAddress,
    UINT64 *PhysicalAddress,
    UINT64 *PageFlags)
{
    UINT32 RegionIndex;

    if (PhysicalAddress != 0)
    {
        *PhysicalAddress = 0ULL;
    }
    if (PageFlags != 0)
    {
        *PageFlags = 0ULL;
    }
    if (AddressSpaceObject == 0 || PhysicalAddress == 0 || PageFlags == 0 || (VirtualAddress & 0xFFFULL) != 0ULL)
    {
        return 0;
    }

    for (RegionIndex = 0U; RegionIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++RegionIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Region;
        UINT64 RegionBase;
        UINT64 RegionBytes;
        UINT64 RegionEnd;

        Region = &AddressSpaceObject->ReservedVirtualRegions[RegionIndex];
        RegionBase = Region->BaseVirtualAddress;
        RegionBytes = Region->PageCount * 0x1000ULL;
        RegionEnd = RegionBase + RegionBytes;
        if (RegionBytes == 0ULL || RegionEnd <= RegionBase)
        {
            continue;
        }
        if (VirtualAddress < RegionBase || VirtualAddress >= RegionEnd || Region->BackingPhysicalAddress == 0ULL)
        {
            continue;
        }

        *PhysicalAddress = Region->BackingPhysicalAddress + (VirtualAddress - RegionBase);
        switch (Region->Type)
        {
            case LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_IMAGE:
                *PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER;
                if (AddressSpaceObject->EntryVirtualAddress == 0ULL ||
                    VirtualAddress != (AddressSpaceObject->EntryVirtualAddress & ~0xFFFULL))
                {
                    *PageFlags |= LOS_X64_PAGE_NX;
                }
                break;
            case LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_STACK:
                *PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER | LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_NX;
                break;
            default:
                *PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER | LOS_X64_PAGE_NX;
                break;
        }

        return 1;
    }

    return 0;
}



static BOOLEAN QueryContiguousMappedRange(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 BaseVirtualAddress,
    UINT64 PageCount,
    UINT64 RequiredPageFlagsMask,
    UINT64 RequiredPageFlagsValue,
    UINT64 *PhysicalBase,
    UINT64 *ResolvedPageFlags)
{
    UINT64 PageIndex;
    UINT64 FirstPhysicalAddress;
    UINT64 FirstPageFlags;

    if (PhysicalBase != 0)
    {
        *PhysicalBase = 0ULL;
    }
    if (ResolvedPageFlags != 0)
    {
        *ResolvedPageFlags = 0ULL;
    }
    if (State == 0 ||
        AddressSpaceObject == 0 ||
        AddressSpaceObject->RootTablePhysicalAddress == 0ULL ||
        BaseVirtualAddress == 0ULL ||
        (BaseVirtualAddress & 0xFFFULL) != 0ULL ||
        PageCount == 0ULL ||
        PhysicalBase == 0)
    {
        return 0;
    }

    FirstPhysicalAddress = 0ULL;
    FirstPageFlags = 0ULL;
    for (PageIndex = 0ULL; PageIndex < PageCount; ++PageIndex)
    {
        UINT64 VirtualAddress;
        UINT64 CurrentPhysicalAddress;
        UINT64 CurrentPageFlags;

        VirtualAddress = BaseVirtualAddress + (PageIndex * 0x1000ULL);
        if (VirtualAddress < BaseVirtualAddress ||
            !LosMemoryManagerQueryAddressSpaceMapping(
                State,
                AddressSpaceObject->RootTablePhysicalAddress,
                VirtualAddress,
                &CurrentPhysicalAddress,
                &CurrentPageFlags))
        {
            return 0;
        }
        if ((CurrentPageFlags & RequiredPageFlagsMask) != RequiredPageFlagsValue)
        {
            return 0;
        }
        if (PageIndex == 0ULL)
        {
            FirstPhysicalAddress = CurrentPhysicalAddress;
            FirstPageFlags = CurrentPageFlags;
            continue;
        }
        if (CurrentPhysicalAddress != FirstPhysicalAddress + (PageIndex * 0x1000ULL))
        {
            return 0;
        }
    }

    *PhysicalBase = FirstPhysicalAddress;
    if (ResolvedPageFlags != 0)
    {
        *ResolvedPageFlags = FirstPageFlags;
    }
    return 1;
}

static BOOLEAN RepairImageStateFromCurrentMappings(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *Result)
{
    UINT64 ImageVirtualBase;
    UINT64 ImageMappedBytes;
    UINT64 ImagePageCount;
    UINT64 ImagePhysicalBase;
    UINT64 PageFlags;
    UINT64 EntryVirtualAddress;

    if (State == 0 ||
        AddressSpaceObject == 0 ||
        Header == 0 ||
        ProgramHeaders == 0 ||
        Result == 0 ||
        !DescribeImageLayout(Header, ProgramHeaders, &ImageVirtualBase, &ImageMappedBytes, &ImagePageCount))
    {
        return 0;
    }

    if (!QueryContiguousMappedRange(
            State,
            AddressSpaceObject,
            ImageVirtualBase,
            ImagePageCount,
            LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER,
            LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER,
            &ImagePhysicalBase,
            &PageFlags) ||
        !ResolveImageEntryVirtualAddress(Header, ImageVirtualBase, ImageMappedBytes, &EntryVirtualAddress))
    {
        return 0;
    }

    AddressSpaceObject->ServiceImageVirtualBase = ImageVirtualBase;
    AddressSpaceObject->ServiceImagePhysicalAddress = ImagePhysicalBase;
    AddressSpaceObject->ServiceImageSize = ImageMappedBytes;
    AddressSpaceObject->EntryVirtualAddress = EntryVirtualAddress;
    AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE;
    return PopulateExistingImageResult(AddressSpaceObject, AddressSpaceObjectPhysicalAddress, Result);
}

static BOOLEAN RepairStackStateFromCurrentMappings(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    UINT64 StackBaseVirtualAddress,
    UINT64 StackPageCount,
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result)
{
    UINT64 StackPhysicalAddress;
    UINT64 PageFlags;

    if (State == 0 ||
        AddressSpaceObject == 0 ||
        Result == 0 ||
        StackBaseVirtualAddress == 0ULL ||
        (StackBaseVirtualAddress & 0xFFFULL) != 0ULL ||
        StackPageCount == 0ULL)
    {
        return 0;
    }

    if (!QueryContiguousMappedRange(
            State,
            AddressSpaceObject,
            StackBaseVirtualAddress,
            StackPageCount,
            LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER | LOS_X64_PAGE_WRITABLE,
            LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER | LOS_X64_PAGE_WRITABLE,
            &StackPhysicalAddress,
            &PageFlags))
    {
        return 0;
    }

    AddressSpaceObject->StackPhysicalAddress = StackPhysicalAddress;
    AddressSpaceObject->StackPageCount = StackPageCount;
    AddressSpaceObject->StackBaseVirtualAddress = StackBaseVirtualAddress;
    AddressSpaceObject->StackTopVirtualAddress = StackBaseVirtualAddress + (StackPageCount * 0x1000ULL);
    AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK;
    return PopulateExistingStackResult(AddressSpaceObject, AddressSpaceObjectPhysicalAddress, Result);
}

static UINT64 AlignBytesToPageCount(UINT64 ByteCount)
{
    if (ByteCount == 0ULL)
    {
        return 0ULL;
    }

    return (ByteCount + 0xFFFULL) / 0x1000ULL;
}
