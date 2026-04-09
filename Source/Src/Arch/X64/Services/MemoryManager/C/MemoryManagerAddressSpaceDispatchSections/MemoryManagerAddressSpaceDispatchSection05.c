/*
 * File Name: MemoryManagerAddressSpaceDispatchSection05.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerAddressSpaceDispatch.c.
 */

static BOOLEAN StageImageIntoPhysicalMemory(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    UINT64 SourceImageSize,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    UINT64 *ImageVirtualBase,
    UINT64 *ImageMappedBytes,
    UINT64 *ImagePageCount,
    UINT64 *ImagePhysicalBase)
{
    void *ImageTarget;
    UINT16 ProgramHeaderIndex;

    if (State == 0 ||
        Header == 0 ||
        ProgramHeaders == 0 ||
        ImageVirtualBase == 0 ||
        ImageMappedBytes == 0 ||
        ImagePageCount == 0 ||
        ImagePhysicalBase == 0)
    {
        return 0;
    }

    if (!DescribeImageLayout(Header, ProgramHeaders, ImageVirtualBase, ImageMappedBytes, ImagePageCount))
    {
        return 0;
    }

    if (!LosMemoryManagerServiceClaimTrackedFrames(
            State,
            *ImagePageCount,
            0x1000ULL,
            LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS,
            LOS_X64_MEMORY_REGION_OWNER_CLAIMED,
            LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_IMAGE,
            ImagePhysicalBase))
    {
        return 0;
    }

    if (State->LaunchBlock != 0)
    {
        if (AddressSpaceServiceRangesOverlap(*ImagePhysicalBase, *ImageMappedBytes, State->LaunchBlock->ServiceImagePhysicalAddress, State->LaunchBlock->ServiceImageSize))
        {
            AddressSpaceServiceSerialWriteText("[MemManager][diag] staged image overlaps current MM service image range.\n");
            AddressSpaceServiceSerialWriteNamedHex("claimed-image-phys", *ImagePhysicalBase);
            AddressSpaceServiceSerialWriteNamedHex("claimed-image-bytes", *ImageMappedBytes);
            AddressSpaceServiceSerialWriteNamedHex("current-service-image-phys", State->LaunchBlock->ServiceImagePhysicalAddress);
            AddressSpaceServiceSerialWriteNamedHex("current-service-image-bytes", State->LaunchBlock->ServiceImageSize);
            LosMemoryManagerHardFail("attach-image-overlaps-mm-image", *ImagePhysicalBase, *ImageMappedBytes, State->LaunchBlock->ServiceImagePhysicalAddress);
        }
        if (AddressSpaceServiceRangesOverlap(*ImagePhysicalBase, *ImageMappedBytes, State->LaunchBlock->ServiceStackPhysicalAddress, State->LaunchBlock->ServiceStackPageCount * 0x1000ULL))
        {
            AddressSpaceServiceSerialWriteText("[MemManager][diag] staged image overlaps current MM service stack range.\n");
            AddressSpaceServiceSerialWriteNamedHex("claimed-image-phys", *ImagePhysicalBase);
            AddressSpaceServiceSerialWriteNamedHex("claimed-image-bytes", *ImageMappedBytes);
            AddressSpaceServiceSerialWriteNamedHex("current-service-stack-phys", State->LaunchBlock->ServiceStackPhysicalAddress);
            AddressSpaceServiceSerialWriteNamedHex("current-service-stack-bytes", State->LaunchBlock->ServiceStackPageCount * 0x1000ULL);
            LosMemoryManagerHardFail("attach-image-overlaps-mm-stack", *ImagePhysicalBase, *ImageMappedBytes, State->LaunchBlock->ServiceStackPhysicalAddress);
        }
    }

    ImageTarget = LosMemoryManagerTranslatePhysical(State, *ImagePhysicalBase, *ImageMappedBytes);
    if (ImageTarget == 0)
    {
        LosMemoryManagerServiceFreeTrackedFrames(State, *ImagePhysicalBase, *ImagePageCount);
        *ImagePhysicalBase = 0ULL;
        return 0;
    }

    ZeroBytes(ImageTarget, (UINTN)*ImageMappedBytes);
    for (ProgramHeaderIndex = 0U; ProgramHeaderIndex < Header->ProgramHeaderCount; ++ProgramHeaderIndex)
    {
        const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeader;
        UINT64 SegmentVirtualBase;
        UINT64 SegmentMappedBytes;
        UINT64 SegmentPageCount;
        UINT64 SegmentImageOffset;
        void *SegmentTarget;
        const void *SegmentSource;

        ProgramHeader = (const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *)((const UINT8 *)ProgramHeaders + ((UINTN)ProgramHeaderIndex * Header->ProgramHeaderEntrySize));
        if (ProgramHeader->Type != LOS_ELF_PROGRAM_HEADER_TYPE_LOAD)
        {
            continue;
        }
        if (ProgramHeader->Offset + ProgramHeader->FileSize < ProgramHeader->Offset ||
            ProgramHeader->Offset + ProgramHeader->FileSize > SourceImageSize)
        {
            LosMemoryManagerServiceFreeTrackedFrames(State, *ImagePhysicalBase, *ImagePageCount);
            *ImagePhysicalBase = 0ULL;
            return 0;
        }
        if (!TryGetLoadSegmentGeometry(ProgramHeader, &SegmentVirtualBase, &SegmentMappedBytes, &SegmentPageCount))
        {
            continue;
        }
        (void)SegmentVirtualBase;
        (void)SegmentMappedBytes;
        (void)SegmentPageCount;

        SegmentImageOffset = ProgramHeader->VirtualAddress - *ImageVirtualBase;
        SegmentTarget = (UINT8 *)ImageTarget + SegmentImageOffset;
        if (ProgramHeader->FileSize != 0ULL)
        {
            SegmentSource = (const void *)((const UINT8 *)Header + ProgramHeader->Offset);
            CopyBytes(SegmentTarget, SegmentSource, (UINTN)ProgramHeader->FileSize);
        }
    }

    return 1;
}

void LosMemoryManagerServiceMapPages(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_MAP_PAGES_REQUEST *Request,
    LOS_MEMORY_MANAGER_MAP_PAGES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;

    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->AddressSpaceObjectPhysicalAddress = 0ULL;
    Result->PagesProcessed = 0ULL;
    Result->LastVirtualAddress = 0ULL;

    if (State == 0 || Request == 0)
    {
        if (State == 0)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        }
        return;
    }

    Result->AddressSpaceObjectPhysicalAddress = Request->AddressSpaceObjectPhysicalAddress;
    if (!LosMemoryManagerResolveAddressSpaceObject(State, Request->AddressSpaceObjectPhysicalAddress, 1, &AddressSpaceObject))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        return;
    }
    if (!LosMemoryManagerValidateAddressSpaceAccess(
            State,
            AddressSpaceObject,
            Request->VirtualAddress,
            Request->PageCount,
            Request->PageFlags,
            1,
            1))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
        return;
    }

    if (!LosMemoryManagerMapPagesIntoAddressSpace(
            State,
            AddressSpaceObject->RootTablePhysicalAddress,
            Request->VirtualAddress,
            Request->PhysicalAddress,
            Request->PageCount,
            Request->PageFlags))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->PagesProcessed = Request->PageCount;
    Result->LastVirtualAddress = Request->VirtualAddress + ((Request->PageCount - 1ULL) * 0x1000ULL);
}

void LosMemoryManagerServiceUnmapPages(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_UNMAP_PAGES_REQUEST *Request,
    LOS_MEMORY_MANAGER_UNMAP_PAGES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;

    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->AddressSpaceObjectPhysicalAddress = 0ULL;
    Result->PagesProcessed = 0ULL;
    Result->LastVirtualAddress = 0ULL;

    if (State == 0 || Request == 0)
    {
        if (State == 0)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        }
        return;
    }

    Result->AddressSpaceObjectPhysicalAddress = Request->AddressSpaceObjectPhysicalAddress;
    if (!LosMemoryManagerResolveAddressSpaceObject(State, Request->AddressSpaceObjectPhysicalAddress, 1, &AddressSpaceObject))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        return;
    }
    if (!LosMemoryManagerValidateAddressSpaceAccess(
            State,
            AddressSpaceObject,
            Request->VirtualAddress,
            Request->PageCount,
            0ULL,
            0,
            1))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
        return;
    }

    if (!LosMemoryManagerUnmapPagesFromAddressSpace(
            State,
            AddressSpaceObject->RootTablePhysicalAddress,
            Request->VirtualAddress,
            Request->PageCount,
            &Result->PagesProcessed,
            &Result->LastVirtualAddress))
    {
        Result->Status = Result->PagesProcessed == 0ULL ? LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND : LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    LosMemoryManagerDiagnosticsClearAttachImageContext();
    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
}

void LosMemoryManagerServiceProtectPages(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_PROTECT_PAGES_REQUEST *Request,
    LOS_MEMORY_MANAGER_PROTECT_PAGES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;

    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->AddressSpaceObjectPhysicalAddress = 0ULL;
    Result->PagesProcessed = 0ULL;
    Result->LastVirtualAddress = 0ULL;
    Result->AppliedPageFlags = 0ULL;

    if (State == 0 || Request == 0)
    {
        if (State == 0)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        }
        return;
    }

    Result->AddressSpaceObjectPhysicalAddress = Request->AddressSpaceObjectPhysicalAddress;
    if (!LosMemoryManagerResolveAddressSpaceObject(State, Request->AddressSpaceObjectPhysicalAddress, 1, &AddressSpaceObject))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        return;
    }
    if (!LosMemoryManagerValidateAddressSpaceAccess(
            State,
            AddressSpaceObject,
            Request->VirtualAddress,
            Request->PageCount,
            Request->PageFlags,
            1,
            1))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
        return;
    }

    if (!LosMemoryManagerProtectPagesInAddressSpace(
            State,
            AddressSpaceObject->RootTablePhysicalAddress,
            Request->VirtualAddress,
            Request->PageCount,
            Request->PageFlags,
            &Result->PagesProcessed,
            &Result->LastVirtualAddress))
    {
        Result->Status = Result->PagesProcessed == 0ULL ? LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND : LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->AppliedPageFlags = Request->PageFlags;
}

void LosMemoryManagerServiceQueryMapping(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_QUERY_MAPPING_REQUEST *Request,
    LOS_MEMORY_MANAGER_QUERY_MAPPING_RESULT *Result)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;

    if (Result == 0)
    {
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    Result->Reserved = 0U;
    Result->AddressSpaceObjectPhysicalAddress = 0ULL;
    Result->VirtualAddress = 0ULL;
    Result->PhysicalAddress = 0ULL;
    Result->PageFlags = 0ULL;
    Result->PageCount = 0ULL;

    if (State == 0 || Request == 0)
    {
        if (State == 0)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        }
        return;
    }

    Result->AddressSpaceObjectPhysicalAddress = Request->AddressSpaceObjectPhysicalAddress;
    Result->VirtualAddress = Request->VirtualAddress;
    if (!LosMemoryManagerResolveAddressSpaceObject(State, Request->AddressSpaceObjectPhysicalAddress, 1, &AddressSpaceObject))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        return;
    }
    if (!LosMemoryManagerValidateAddressSpaceAccess(
            State,
            AddressSpaceObject,
            Request->VirtualAddress,
            1ULL,
            0ULL,
            0,
            0))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
        return;
    }

    {
        UINT64 PageVirtualAddress;
        BOOLEAN MappingResolved;

        PageVirtualAddress = Request->VirtualAddress & ~0xFFFULL;
        MappingResolved = TrySynthesizeMappingFromReservedRegion(
            AddressSpaceObject,
            PageVirtualAddress,
            &Result->PhysicalAddress,
            &Result->PageFlags);
        if (!MappingResolved)
        {
            MappingResolved = LosMemoryManagerQueryAddressSpaceMapping(
                State,
                AddressSpaceObject->RootTablePhysicalAddress,
                Request->VirtualAddress,
                &Result->PhysicalAddress,
                &Result->PageFlags);
        }
        if (!MappingResolved)
        {
            if (AddressSpaceObject->ServiceImageVirtualBase != 0ULL &&
                AddressSpaceObject->ServiceImageSize != 0ULL &&
                PageVirtualAddress >= AddressSpaceObject->ServiceImageVirtualBase &&
                PageVirtualAddress < AddressSpaceObject->ServiceImageVirtualBase +
                    (AlignBytesToPageCount(AddressSpaceObject->ServiceImageSize) * 0x1000ULL))
            {
                Result->PhysicalAddress = AddressSpaceObject->ServiceImagePhysicalAddress +
                    (PageVirtualAddress - AddressSpaceObject->ServiceImageVirtualBase);
                Result->PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER;
                if (AddressSpaceObject->EntryVirtualAddress == 0ULL ||
                    PageVirtualAddress != (AddressSpaceObject->EntryVirtualAddress & ~0xFFFULL))
                {
                    Result->PageFlags |= LOS_X64_PAGE_NX;
                }
                MappingResolved = 1;
            }
            else if (AddressSpaceObject->StackBaseVirtualAddress != 0ULL &&
                     AddressSpaceObject->StackTopVirtualAddress > AddressSpaceObject->StackBaseVirtualAddress &&
                     PageVirtualAddress >= AddressSpaceObject->StackBaseVirtualAddress &&
                     PageVirtualAddress < AddressSpaceObject->StackTopVirtualAddress)
            {
                Result->PhysicalAddress = AddressSpaceObject->StackPhysicalAddress +
                    (PageVirtualAddress - AddressSpaceObject->StackBaseVirtualAddress);
                Result->PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER |
                    LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_NX;
                MappingResolved = 1;
            }
        }
        if (!MappingResolved)
        {
            if (AddressSpaceObject->ServiceImageVirtualBase != 0ULL &&
                AddressSpaceObject->ServiceImagePhysicalAddress != 0ULL &&
                AddressSpaceObject->ServiceImageSize != 0ULL)
            {
                AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE;
                (void)EnsureReservedImageRegionRecorded(AddressSpaceObject);
                MappingResolved = TrySynthesizeMappingFromReservedRegion(
                    AddressSpaceObject,
                    PageVirtualAddress,
                    &Result->PhysicalAddress,
                    &Result->PageFlags);
            }
            if (!MappingResolved &&
                AddressSpaceObject->StackBaseVirtualAddress != 0ULL &&
                AddressSpaceObject->StackPhysicalAddress != 0ULL &&
                AddressSpaceObject->StackPageCount != 0ULL)
            {
                AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK;
                (void)EnsureReservedStackRegionRecorded(AddressSpaceObject);
                MappingResolved = TrySynthesizeMappingFromReservedRegion(
                    AddressSpaceObject,
                    PageVirtualAddress,
                    &Result->PhysicalAddress,
                    &Result->PageFlags);
            }
        }
        if (!MappingResolved)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
            return;
        }
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->PageCount = 1ULL;
}
