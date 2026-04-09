/*
 * File Name: MemoryManagerAddressSpaceDispatchSection06.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerAddressSpaceDispatch.c.
 */

void LosMemoryManagerServiceCreateAddressSpace(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_REQUEST *Request,
    LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_RESULT *Result)
{
    UINT64 AddressSpaceObjectPhysicalAddress;
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;

    if (Result == 0)
    {
        return;
    }

    ZeroBytes(Result, sizeof(*Result));
    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    (void)Request;
    if (State == 0 || Request == 0 || State->Online == 0U || State->AttachComplete == 0U)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    AddressSpaceObjectPhysicalAddress = 0ULL;
    AddressSpaceObject = 0;
    if (!LosMemoryManagerCreateAddressSpaceObject(State, &AddressSpaceObjectPhysicalAddress, &AddressSpaceObject))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->AddressSpaceObjectPhysicalAddress = AddressSpaceObjectPhysicalAddress;
    Result->RootTablePhysicalAddress = AddressSpaceObject->RootTablePhysicalAddress;
    Result->AddressSpaceId = AddressSpaceObject->AddressSpaceId;
    AddressSpaceServiceLogCreated(
        AddressSpaceObject->AddressSpaceId,
        AddressSpaceObjectPhysicalAddress,
        AddressSpaceObject->RootTablePhysicalAddress);
}

void LosMemoryManagerServiceDestroyAddressSpace(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_REQUEST *Request,
    LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_RESULT *Result)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
    UINT64 ReleasedPageCount;
    UINT64 ReleasedVirtualRegionCount;
    UINT64 AddressSpaceObjectPhysicalAddress;

    if (Result == 0)
    {
        return;
    }

    ZeroBytes(Result, sizeof(*Result));
    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    if (State == 0 || Request == 0 || State->Online == 0U || State->AttachComplete == 0U)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    AddressSpaceObject = 0;
    if (!LosMemoryManagerResolveAddressSpaceObject(State, Request->AddressSpaceObjectPhysicalAddress, 0, &AddressSpaceObject))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        return;
    }

    AddressSpaceObjectPhysicalAddress = Request->AddressSpaceObjectPhysicalAddress;
    ReleasedVirtualRegionCount = AddressSpaceObject->ReservedVirtualRegionCount;
    ReleasedPageCount = 0ULL;

    if (AddressSpaceObject->ServiceImagePhysicalAddress != 0ULL && AddressSpaceObject->ServiceImageSize != 0ULL)
    {
        UINT64 ImagePageCount;

        ImagePageCount = AlignBytesToPageCount(AddressSpaceObject->ServiceImageSize);
        if (ImagePageCount != 0ULL)
        {
            if (!LosMemoryManagerServiceFreeTrackedFrames(
                    State,
                    AddressSpaceObject->ServiceImagePhysicalAddress,
                    ImagePageCount))
            {
                Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
                return;
            }
            ReleasedPageCount += ImagePageCount;
        }
    }

    if (AddressSpaceObject->StackPhysicalAddress != 0ULL && AddressSpaceObject->StackPageCount != 0ULL)
    {
        if (!LosMemoryManagerServiceFreeTrackedFrames(State, AddressSpaceObject->StackPhysicalAddress, AddressSpaceObject->StackPageCount))
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
            return;
        }
        ReleasedPageCount += AddressSpaceObject->StackPageCount;
    }

    if (!LosMemoryManagerDestroyAddressSpaceMappings(State, AddressSpaceObject, &ReleasedPageCount))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    ZeroBytes(AddressSpaceObject, sizeof(*AddressSpaceObject));
    {
        UINT64 ReleasedFromHeap;

        ReleasedFromHeap = 0ULL;
        if (!LosMemoryManagerHeapFree(State, AddressSpaceObjectPhysicalAddress, &ReleasedFromHeap))
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
            return;
        }
        ReleasedPageCount += ReleasedFromHeap;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->AddressSpaceObjectPhysicalAddress = AddressSpaceObjectPhysicalAddress;
    Result->ReleasedPageCount = ReleasedPageCount;
    Result->ReleasedVirtualRegionCount = ReleasedVirtualRegionCount;
}

void LosMemoryManagerServiceAttachStagedImage(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_REQUEST *Request,
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *Result)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header;
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders;
    UINT64 ImageVirtualBase;
    UINT64 ImageMappedBytes;
    UINT64 ImagePageCount;
    UINT64 ImagePhysicalBase;
    UINT64 EntryVirtualAddress;
    UINT64 PageIndex;

    if (Result == 0)
    {
        return;
    }

    ZeroBytes(Result, sizeof(*Result));
    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    if (State == 0 || Request == 0 || State->Online == 0U || State->AttachComplete == 0U)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }
    if (Request->StagedImagePhysicalAddress == 0ULL || Request->StagedImageSize < sizeof(LOS_MEMORY_MANAGER_ELF64_HEADER))
    {
        return;
    }

    AddressSpaceObject = 0;
    if (!LosMemoryManagerResolveAddressSpaceObject(State, Request->AddressSpaceObjectPhysicalAddress, 0, &AddressSpaceObject))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        return;
    }

    AddressSpaceServiceVerifyAttachImageState(State, Request, AddressSpaceObject, 0xA100ULL, 0ULL, 0ULL, 0ULL, 0ULL);
    AddressSpaceServiceEnsureAddressSpaceAssetsTracked(State, Request->AddressSpaceObjectPhysicalAddress, AddressSpaceObject);

    Header = (const LOS_MEMORY_MANAGER_ELF64_HEADER *)LosMemoryManagerTranslatePhysical(
        State,
        Request->StagedImagePhysicalAddress,
        Request->StagedImageSize);
    if (Header == 0 || !ValidateElfHeader(Header, Request->StagedImageSize, &ProgramHeaders))
    {
        return;
    }

    if ((AddressSpaceObject->Flags & LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE) != 0ULL)
    {
        if (PopulateRequestedImageResult(
                State,
                AddressSpaceObject,
                Request->AddressSpaceObjectPhysicalAddress,
                Header,
                ProgramHeaders,
                Result))
        {
            return;
        }
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }
    if (AddressSpaceObject->ServiceImageVirtualBase != 0ULL ||
        AddressSpaceObject->ServiceImagePhysicalAddress != 0ULL ||
        AddressSpaceObject->EntryVirtualAddress != 0ULL)
    {
        if (PopulateRequestedImageResult(
                State,
                AddressSpaceObject,
                Request->AddressSpaceObjectPhysicalAddress,
                Header,
                ProgramHeaders,
                Result))
        {
            AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE;
            return;
        }
    }

    if (RepairImageStateFromCurrentMappings(
            State,
            AddressSpaceObject,
            Request->AddressSpaceObjectPhysicalAddress,
            Header,
            ProgramHeaders,
            Result))
    {
        return;
    }

    if (!DescribeImageLayout(Header, ProgramHeaders, &ImageVirtualBase, &ImageMappedBytes, &ImagePageCount) ||
        !ResolveImageEntryVirtualAddress(Header, ImageVirtualBase, ImageMappedBytes, &EntryVirtualAddress))
    {
        return;
    }
    if (!CanReserveRegion(AddressSpaceObject, ImageVirtualBase, ImagePageCount))
    {
        if (PopulateRequestedImageResult(
                State,
                AddressSpaceObject,
                Request->AddressSpaceObjectPhysicalAddress,
                Header,
                ProgramHeaders,
                Result))
        {
            return;
        }
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    ImagePhysicalBase = 0ULL;
    if (!StageImageIntoPhysicalMemory(
            State,
            Header,
            Request->StagedImageSize,
            ProgramHeaders,
            &ImageVirtualBase,
            &ImageMappedBytes,
            &ImagePageCount,
            &ImagePhysicalBase))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    AddressSpaceServiceSerialWriteNamedHex("attach-claimed-image-phys", ImagePhysicalBase);
    AddressSpaceServiceSerialWriteNamedHex("attach-claimed-image-bytes", ImageMappedBytes);
    AddressSpaceServiceSerialWriteNamedHex("attach-target-root-phys", AddressSpaceObject->RootTablePhysicalAddress);
    AddressSpaceServiceSerialWriteNamedHex("attach-target-object-page", Request->AddressSpaceObjectPhysicalAddress & ~0xFFFULL);
    AddressSpaceServiceLogFrameEntryForPhysicalAddress(State, "attach-claimed-image-entry", ImagePhysicalBase);
    AddressSpaceServiceLogFrameEntryForPhysicalAddress(State, "attach-target-object-entry", Request->AddressSpaceObjectPhysicalAddress & ~0xFFFULL);
    AddressSpaceServiceLogFrameEntryForPhysicalAddress(State, "attach-target-root-entry-pre-overlap", AddressSpaceObject->RootTablePhysicalAddress);
    if (ImagePhysicalBase == (AddressSpaceObject->RootTablePhysicalAddress & ~0xFFFULL) ||
        ImagePhysicalBase == (Request->AddressSpaceObjectPhysicalAddress & ~0xFFFULL) ||
        AddressSpaceServiceRangesOverlap(ImagePhysicalBase, ImageMappedBytes, AddressSpaceObject->RootTablePhysicalAddress, 0x1000ULL) ||
        AddressSpaceServiceRangesOverlap(ImagePhysicalBase, ImageMappedBytes, Request->AddressSpaceObjectPhysicalAddress & ~0xFFFULL, 0x1000ULL))
    {
        AddressSpaceServiceSerialWriteText("[MemManager][diag] staged image overlapped target address-space live structures.\n");
        AddressSpaceServiceSerialWriteNamedHex("claimed-image-phys", ImagePhysicalBase);
        AddressSpaceServiceSerialWriteNamedHex("claimed-image-bytes", ImageMappedBytes);
        AddressSpaceServiceSerialWriteNamedHex("target-address-space-object-page", Request->AddressSpaceObjectPhysicalAddress & ~0xFFFULL);
        AddressSpaceServiceSerialWriteNamedHex("target-address-space-root-page", AddressSpaceObject->RootTablePhysicalAddress);
        LosMemoryManagerHardFail("attach-image-overlaps-target-address-space", ImagePhysicalBase, Request->AddressSpaceObjectPhysicalAddress, AddressSpaceObject->RootTablePhysicalAddress);
    }

    LosMemoryManagerDiagnosticsSetAttachImageContext(
        ImagePhysicalBase,
        ImageMappedBytes,
        AddressSpaceObject->RootTablePhysicalAddress,
        Request->AddressSpaceObjectPhysicalAddress);

    for (PageIndex = 0ULL; PageIndex < ImagePageCount; )
    {
        UINT64 RunVirtualAddress;
        UINT64 RunPhysicalAddress;
        UINT64 RunPageCount;
        UINT64 PageFlags;

        RunVirtualAddress = ImageVirtualBase + (PageIndex * 0x1000ULL);
        RunPhysicalAddress = ImagePhysicalBase + (PageIndex * 0x1000ULL);
        PageFlags = ComputeImagePageFlags(Header, ProgramHeaders, RunVirtualAddress);
        if (PageFlags == 0ULL)
        {
            LosMemoryManagerDiagnosticsClearAttachImageContext();
            LosMemoryManagerServiceFreeTrackedFrames(State, ImagePhysicalBase, ImagePageCount);
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
            return;
        }

        RunPageCount = 1ULL;
        while (PageIndex + RunPageCount < ImagePageCount)
        {
            UINT64 NextVirtualAddress;
            UINT64 NextPageFlags;

            NextVirtualAddress = ImageVirtualBase + ((PageIndex + RunPageCount) * 0x1000ULL);
            NextPageFlags = ComputeImagePageFlags(Header, ProgramHeaders, NextVirtualAddress);
            if (NextPageFlags != PageFlags)
            {
                break;
            }
            RunPageCount += 1ULL;
        }

        AddressSpaceServiceVerifyAttachImageState(
            State,
            Request,
            AddressSpaceObject,
            0xA200ULL + PageIndex,
            PageIndex,
            RunVirtualAddress,
            RunPhysicalAddress,
            RunPageCount);

        if (!LosMemoryManagerMapPagesIntoAddressSpace(
                State,
                AddressSpaceObject->RootTablePhysicalAddress,
                RunVirtualAddress,
                RunPhysicalAddress,
                RunPageCount,
                PageFlags))
        {
            if (RepairImageStateFromCurrentMappings(
                    State,
                    AddressSpaceObject,
                    Request->AddressSpaceObjectPhysicalAddress,
                    Header,
                    ProgramHeaders,
                    Result))
            {
                LosMemoryManagerDiagnosticsClearAttachImageContext();
                LosMemoryManagerServiceFreeTrackedFrames(State, ImagePhysicalBase, ImagePageCount);
                return;
            }
            LosMemoryManagerDiagnosticsClearAttachImageContext();
            LosMemoryManagerServiceFreeTrackedFrames(State, ImagePhysicalBase, ImagePageCount);
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
            return;
        }

        AddressSpaceServiceVerifyAttachImageState(
            State,
            Request,
            AddressSpaceObject,
            0xA280ULL + PageIndex,
            PageIndex,
            RunVirtualAddress,
            RunPhysicalAddress,
            RunPageCount);

        {
            UINT64 VerifiedPhysicalAddress;
            UINT64 VerifiedPageFlags;

            VerifiedPhysicalAddress = 0ULL;
            VerifiedPageFlags = 0ULL;
            if (!LosMemoryManagerQueryAddressSpaceMapping(
                    State,
                    AddressSpaceObject->RootTablePhysicalAddress,
                    RunVirtualAddress,
                    &VerifiedPhysicalAddress,
                    &VerifiedPageFlags) ||
                VerifiedPhysicalAddress != RunPhysicalAddress)
            {
                AddressSpaceServiceSerialWriteText("[MemManager][diag] attach-post-map-query-mismatch\n");
                AddressSpaceServiceSerialWriteNamedHex("attach-post-map-virt", RunVirtualAddress);
                AddressSpaceServiceSerialWriteNamedHex("attach-post-map-phys-expected", RunPhysicalAddress);
                AddressSpaceServiceSerialWriteNamedHex("attach-post-map-phys-actual", VerifiedPhysicalAddress);
                AddressSpaceServiceSerialWriteNamedHex("attach-post-map-flags", VerifiedPageFlags);
                LosMemoryManagerHardFail("attach-post-map-query-mismatch", RunVirtualAddress, RunPhysicalAddress, VerifiedPhysicalAddress);
            }
            AddressSpaceServiceSerialWriteNamedHex("attach-post-map-phys", VerifiedPhysicalAddress);
            AddressSpaceServiceSerialWriteNamedHex("attach-post-map-flags", VerifiedPageFlags);
        }

        PageIndex += RunPageCount;
    }

    AddressSpaceServiceVerifyAttachImageState(State, Request, AddressSpaceObject, 0xA300ULL, PageIndex, ImageVirtualBase, ImagePhysicalBase, ImagePageCount);
    AddressSpaceServiceSerialWriteText("[MemManager][diag] attach-pre-reserve\n");
    AddressSpaceServiceSerialWriteNamedUnsigned("attach-reserved-count-before", AddressSpaceObject->ReservedVirtualRegionCount);

    if (!LosMemoryManagerReserveVirtualRegion(
            AddressSpaceObject,
            ImageVirtualBase,
            ImagePageCount,
            LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_IMAGE,
            0U,
            ImagePhysicalBase))
    {
        if (PopulateRequestedImageResult(
                State,
                AddressSpaceObject,
                Request->AddressSpaceObjectPhysicalAddress,
                Header,
                ProgramHeaders,
                Result))
        {
            LosMemoryManagerDiagnosticsClearAttachImageContext();
            LosMemoryManagerServiceFreeTrackedFrames(State, ImagePhysicalBase, ImagePageCount);
            return;
        }
        LosMemoryManagerDiagnosticsClearAttachImageContext();
        LosMemoryManagerServiceFreeTrackedFrames(State, ImagePhysicalBase, ImagePageCount);
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    AddressSpaceServiceSerialWriteText("[MemManager][diag] attach-post-reserve-returned\n");
    AddressSpaceServiceVerifyAttachImageState(State, Request, AddressSpaceObject, 0xA310ULL, PageIndex, ImageVirtualBase, ImagePhysicalBase, ImagePageCount);
    AddressSpaceServiceSerialWriteNamedHex("attach-commit-service-image-phys", ImagePhysicalBase);
    AddressSpaceObject->ServiceImagePhysicalAddress = ImagePhysicalBase;
    AddressSpaceServiceSerialWriteText("[MemManager][diag] attach-commit-service-image-phys-done\n");
    AddressSpaceObject->ServiceImageSize = ImageMappedBytes;
    AddressSpaceObject->ServiceImageVirtualBase = ImageVirtualBase;
    AddressSpaceObject->EntryVirtualAddress = EntryVirtualAddress;
    AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE;
    AddressSpaceServiceSerialWriteText("[MemManager][diag] attach-pre-ensure-recorded\n");
    (void)EnsureReservedImageRegionRecorded(AddressSpaceObject);
    AddressSpaceServiceSerialWriteText("[MemManager][diag] attach-post-ensure-recorded\n");

    AddressSpaceServiceSerialWriteText("[MemManager][diag] attach-pre-populate-existing\n");
    if (!PopulateExistingImageResult(AddressSpaceObject, Request->AddressSpaceObjectPhysicalAddress, Result))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }
    AddressSpaceServiceSerialWriteText("[MemManager][diag] attach-post-populate-existing\n");
    LosMemoryManagerDiagnosticsClearAttachImageContext();
    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    AddressSpaceServiceSerialWriteText("[MemManager][diag] attach-return-success\n");
}
