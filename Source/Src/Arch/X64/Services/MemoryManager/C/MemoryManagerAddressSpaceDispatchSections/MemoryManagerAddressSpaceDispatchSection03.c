/*
 * File Name: MemoryManagerAddressSpaceDispatchSection03.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerAddressSpaceDispatch.c.
 */

static BOOLEAN PopulateExistingImageResult(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *Result)
{
    if (AddressSpaceObject == 0 || Result == 0)
    {
        return 0;
    }
    if (AddressSpaceObject->EntryVirtualAddress == 0ULL &&
        AddressSpaceObject->ServiceImageVirtualBase == 0ULL)
    {
        return 0;
    }

    AddressSpaceServiceSerialWriteText("[MemManager][diag] populate-existing-enter\n");
    AddressSpaceServiceSerialWriteNamedHex("populate-existing-result", (UINT64)(UINTN)Result);
    ZeroBytes(Result, sizeof(*Result));
    AddressSpaceServiceSerialWriteText("[MemManager][diag] populate-existing-zeroed\n");
    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->AddressSpaceObjectPhysicalAddress = AddressSpaceObjectPhysicalAddress;
    Result->ImagePhysicalAddress = AddressSpaceObject->ServiceImagePhysicalAddress;
    Result->ImageSize = AddressSpaceObject->ServiceImageSize;
    Result->ImageVirtualBase = AddressSpaceObject->ServiceImageVirtualBase;
    Result->EntryVirtualAddress = AddressSpaceObject->EntryVirtualAddress != 0ULL ?
        AddressSpaceObject->EntryVirtualAddress : AddressSpaceObject->ServiceImageVirtualBase;
    Result->ImagePageCount = AlignBytesToPageCount(AddressSpaceObject->ServiceImageSize);
    AddressSpaceServiceSerialWriteText("[MemManager][diag] populate-existing-pre-ensure\n");
    (void)EnsureReservedImageRegionRecorded((LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *)AddressSpaceObject);
    AddressSpaceServiceSerialWriteText("[MemManager][diag] populate-existing-exit\n");
    return 1;
}

static BOOLEAN PopulateExistingStackResult(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result)
{
    if (AddressSpaceObject == 0 || Result == 0)
    {
        return 0;
    }
    if (AddressSpaceObject->StackBaseVirtualAddress == 0ULL ||
        AddressSpaceObject->StackTopVirtualAddress <= AddressSpaceObject->StackBaseVirtualAddress)
    {
        return 0;
    }

    ZeroBytes(Result, sizeof(*Result));
    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->AddressSpaceObjectPhysicalAddress = AddressSpaceObjectPhysicalAddress;
    Result->StackPhysicalAddress = AddressSpaceObject->StackPhysicalAddress;
    Result->StackPageCount = AddressSpaceObject->StackPageCount;
    Result->StackBaseVirtualAddress = AddressSpaceObject->StackBaseVirtualAddress;
    Result->StackTopVirtualAddress = AddressSpaceObject->StackTopVirtualAddress;
    (void)EnsureReservedStackRegionRecorded((LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *)AddressSpaceObject);
    return 1;
}

static BOOLEAN PopulateReservedImageResult(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *Result)
{
    UINT32 RegionIndex;

    if (AddressSpaceObject == 0 || Result == 0)
    {
        return 0;
    }

    for (RegionIndex = 0U; RegionIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++RegionIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Region;

        Region = &AddressSpaceObject->ReservedVirtualRegions[RegionIndex];
        if (Region->Type != LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_IMAGE ||
            Region->BackingPhysicalAddress == 0ULL ||
            Region->PageCount == 0ULL)
        {
            continue;
        }

        AddressSpaceObject->ServiceImageVirtualBase = Region->BaseVirtualAddress;
        AddressSpaceObject->ServiceImagePhysicalAddress = Region->BackingPhysicalAddress;
        if (AddressSpaceObject->ServiceImageSize == 0ULL)
        {
            AddressSpaceObject->ServiceImageSize = Region->PageCount * 0x1000ULL;
        }
        if (AddressSpaceObject->EntryVirtualAddress == 0ULL)
        {
            AddressSpaceObject->EntryVirtualAddress = Region->BaseVirtualAddress;
        }
        AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE;
        return PopulateExistingImageResult(AddressSpaceObject, AddressSpaceObjectPhysicalAddress, Result);
    }

    return 0;
}

static BOOLEAN PopulateReservedStackResult(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result)
{
    UINT32 RegionIndex;

    if (AddressSpaceObject == 0 || Result == 0)
    {
        return 0;
    }

    for (RegionIndex = 0U; RegionIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++RegionIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Region;

        Region = &AddressSpaceObject->ReservedVirtualRegions[RegionIndex];
        if (Region->Type != LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_STACK ||
            Region->BackingPhysicalAddress == 0ULL ||
            Region->PageCount == 0ULL)
        {
            continue;
        }

        AddressSpaceObject->StackPhysicalAddress = Region->BackingPhysicalAddress;
        AddressSpaceObject->StackPageCount = Region->PageCount;
        AddressSpaceObject->StackBaseVirtualAddress = Region->BaseVirtualAddress;
        AddressSpaceObject->StackTopVirtualAddress = Region->BaseVirtualAddress + (Region->PageCount * 0x1000ULL);
        AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK;
        return PopulateExistingStackResult(AddressSpaceObject, AddressSpaceObjectPhysicalAddress, Result);
    }

    return 0;
}

static BOOLEAN EnsureReservedImageRegionRecorded(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject)
{
    UINT32 RegionIndex;
    UINT64 ImagePageCount;

    AddressSpaceServiceSerialWriteText("[MemManager][diag] ensure-image-region-enter\n");
    if (AddressSpaceObject != 0)
    {
        AddressSpaceServiceSerialWriteNamedUnsigned("ensure-image-region-count", AddressSpaceObject->ReservedVirtualRegionCount);
        AddressSpaceServiceSerialWriteNamedHex("ensure-image-region-base", AddressSpaceObject->ServiceImageVirtualBase);
        AddressSpaceServiceSerialWriteNamedHex("ensure-image-region-phys", AddressSpaceObject->ServiceImagePhysicalAddress);
        AddressSpaceServiceSerialWriteNamedHex("ensure-image-region-size", AddressSpaceObject->ServiceImageSize);
    }

    if (AddressSpaceObject == 0 ||
        AddressSpaceObject->ServiceImageVirtualBase == 0ULL ||
        AddressSpaceObject->ServiceImagePhysicalAddress == 0ULL ||
        AddressSpaceObject->ServiceImageSize == 0ULL)
    {
        return 0;
    }

    for (RegionIndex = 0U; RegionIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++RegionIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Region;

        Region = &AddressSpaceObject->ReservedVirtualRegions[RegionIndex];
        if (Region->Type == LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_IMAGE &&
            Region->BaseVirtualAddress == AddressSpaceObject->ServiceImageVirtualBase)
        {
            AddressSpaceServiceSerialWriteText("[MemManager][diag] ensure-image-region-found-existing\n");
            return 1;
        }
    }

    ImagePageCount = AlignBytesToPageCount(AddressSpaceObject->ServiceImageSize);
    if (ImagePageCount == 0ULL)
    {
        return 0;
    }

    AddressSpaceServiceSerialWriteText("[MemManager][diag] ensure-image-region-reserving\n");
    return LosMemoryManagerReserveVirtualRegion(
        AddressSpaceObject,
        AddressSpaceObject->ServiceImageVirtualBase,
        ImagePageCount,
        LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_IMAGE,
        0U,
        AddressSpaceObject->ServiceImagePhysicalAddress);
}

static BOOLEAN EnsureReservedStackRegionRecorded(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject)
{
    UINT32 RegionIndex;

    if (AddressSpaceObject == 0 ||
        AddressSpaceObject->StackBaseVirtualAddress == 0ULL ||
        AddressSpaceObject->StackPhysicalAddress == 0ULL ||
        AddressSpaceObject->StackPageCount == 0ULL ||
        AddressSpaceObject->StackTopVirtualAddress <= AddressSpaceObject->StackBaseVirtualAddress)
    {
        return 0;
    }

    for (RegionIndex = 0U; RegionIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++RegionIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Region;

        Region = &AddressSpaceObject->ReservedVirtualRegions[RegionIndex];
        if (Region->Type == LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_STACK &&
            Region->BaseVirtualAddress == AddressSpaceObject->StackBaseVirtualAddress)
        {
            return 1;
        }
    }

    return LosMemoryManagerReserveVirtualRegion(
        AddressSpaceObject,
        AddressSpaceObject->StackBaseVirtualAddress,
        AddressSpaceObject->StackPageCount,
        LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_STACK,
        0U,
        AddressSpaceObject->StackPhysicalAddress);
}

static BOOLEAN PopulateRequestedImageResult(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *Result)
{
    UINT64 RequestedImageVirtualBase;
    UINT64 RequestedImageMappedBytes;
    UINT64 RequestedImagePageCount;
    UINT64 RequestedEntryVirtualAddress;

    if (AddressSpaceObject == 0 ||
        Header == 0 ||
        ProgramHeaders == 0 ||
        Result == 0 ||
        !DescribeImageLayout(Header, ProgramHeaders, &RequestedImageVirtualBase, &RequestedImageMappedBytes, &RequestedImagePageCount) ||
        !ResolveImageEntryVirtualAddress(Header, RequestedImageVirtualBase, RequestedImageMappedBytes, &RequestedEntryVirtualAddress))
    {
        return 0;
    }

    if (PopulateExistingImageResult(AddressSpaceObject, AddressSpaceObjectPhysicalAddress, Result) &&
        Result->ImageVirtualBase == RequestedImageVirtualBase &&
        Result->ImagePageCount == RequestedImagePageCount &&
        Result->EntryVirtualAddress == RequestedEntryVirtualAddress)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
        return 1;
    }

    if (PopulateReservedImageResult(AddressSpaceObject, AddressSpaceObjectPhysicalAddress, Result) &&
        Result->ImageVirtualBase == RequestedImageVirtualBase &&
        Result->ImagePageCount == RequestedImagePageCount &&
        Result->EntryVirtualAddress == RequestedEntryVirtualAddress)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
        return 1;
    }

    if (RepairImageStateFromCurrentMappings(
            State,
            AddressSpaceObject,
            AddressSpaceObjectPhysicalAddress,
            Header,
            ProgramHeaders,
            Result) &&
        Result->ImageVirtualBase == RequestedImageVirtualBase &&
        Result->ImagePageCount == RequestedImagePageCount &&
        Result->EntryVirtualAddress == RequestedEntryVirtualAddress)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
        return 1;
    }

    ZeroBytes(Result, sizeof(*Result));
    return 0;
}

static BOOLEAN PopulateRequestedStackResult(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    UINT64 DesiredStackBaseVirtualAddress,
    UINT64 StackPageCount,
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result)
{
    UINT64 StackBaseVirtualAddress;

    if (AddressSpaceObject == 0 ||
        Result == 0 ||
        StackPageCount == 0ULL)
    {
        return 0;
    }

    StackBaseVirtualAddress = DesiredStackBaseVirtualAddress;
    if (StackBaseVirtualAddress == 0ULL)
    {
        if (!LosMemoryManagerSelectStackBaseVirtualAddress(
                AddressSpaceObject,
                DesiredStackBaseVirtualAddress,
                StackPageCount,
                &StackBaseVirtualAddress))
        {
            return 0;
        }
    }

    if (PopulateExistingStackResult(AddressSpaceObject, AddressSpaceObjectPhysicalAddress, Result) &&
        Result->StackBaseVirtualAddress == StackBaseVirtualAddress &&
        Result->StackPageCount == StackPageCount)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
        return 1;
    }

    if (PopulateReservedStackResult(AddressSpaceObject, AddressSpaceObjectPhysicalAddress, Result) &&
        Result->StackBaseVirtualAddress == StackBaseVirtualAddress &&
        Result->StackPageCount == StackPageCount)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
        return 1;
    }

    if (RepairStackStateFromCurrentMappings(
            State,
            AddressSpaceObject,
            AddressSpaceObjectPhysicalAddress,
            StackBaseVirtualAddress,
            StackPageCount,
            Result) &&
        Result->StackBaseVirtualAddress == StackBaseVirtualAddress &&
        Result->StackPageCount == StackPageCount)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
        return 1;
    }

    ZeroBytes(Result, sizeof(*Result));
    return 0;
}
