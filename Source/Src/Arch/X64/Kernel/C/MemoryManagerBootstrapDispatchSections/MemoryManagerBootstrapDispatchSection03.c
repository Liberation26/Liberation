/*
 * File Name: MemoryManagerBootstrapDispatchSection03.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapDispatch.c.
 */

static BOOLEAN TryGetLoadSegmentGeometryLocal(
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeader,
    UINT64 *SegmentVirtualBase,
    UINT64 *SegmentMappedBytes,
    UINT64 *SegmentPageCount)
{
    UINT64 OffsetIntoFirstPage;

    if (ProgramHeader == 0 ||
        SegmentVirtualBase == 0 ||
        SegmentMappedBytes == 0 ||
        SegmentPageCount == 0 ||
        ProgramHeader->MemorySize == 0ULL)
    {
        return 0;
    }

    *SegmentVirtualBase = AlignDownToPage(ProgramHeader->VirtualAddress);
    OffsetIntoFirstPage = ProgramHeader->VirtualAddress - *SegmentVirtualBase;
    *SegmentMappedBytes = AlignUpToPage(ProgramHeader->MemorySize + OffsetIntoFirstPage);
    *SegmentPageCount = *SegmentMappedBytes / 0x1000ULL;
    return *SegmentPageCount != 0ULL;
}

static BOOLEAN DescribeImageLayoutLocal(
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    UINT64 *ImageVirtualBase,
    UINT64 *ImageMappedBytes,
    UINT64 *ImagePageCount)
{
    UINT16 ProgramHeaderIndex;
    UINT64 LowestVirtualBase;
    UINT64 HighestVirtualEnd;
    BOOLEAN FoundLoadSegment;

    if (Header == 0 ||
        ProgramHeaders == 0 ||
        ImageVirtualBase == 0 ||
        ImageMappedBytes == 0 ||
        ImagePageCount == 0)
    {
        return 0;
    }

    LowestVirtualBase = 0ULL;
    HighestVirtualEnd = 0ULL;
    FoundLoadSegment = 0;
    for (ProgramHeaderIndex = 0U; ProgramHeaderIndex < Header->ProgramHeaderCount; ++ProgramHeaderIndex)
    {
        const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeader;
        UINT64 SegmentVirtualBase;
        UINT64 SegmentMappedBytes;
        UINT64 SegmentPageCount;
        UINT64 SegmentVirtualEnd;

        ProgramHeader = (const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *)((const UINT8 *)ProgramHeaders + ((UINTN)ProgramHeaderIndex * Header->ProgramHeaderEntrySize));
        if (ProgramHeader->Type != LOS_ELF_PROGRAM_HEADER_TYPE_LOAD)
        {
            continue;
        }

        if (!TryGetLoadSegmentGeometryLocal(ProgramHeader, &SegmentVirtualBase, &SegmentMappedBytes, &SegmentPageCount))
        {
            continue;
        }
        (void)SegmentPageCount;

        SegmentVirtualEnd = SegmentVirtualBase + SegmentMappedBytes;
        if (SegmentVirtualEnd <= SegmentVirtualBase || SegmentVirtualEnd > LOS_MEMORY_MANAGER_BOOTSTRAP_LOCAL_LOW_HALF_LIMIT)
        {
            return 0;
        }
        if (!FoundLoadSegment || SegmentVirtualBase < LowestVirtualBase)
        {
            LowestVirtualBase = SegmentVirtualBase;
        }
        if (!FoundLoadSegment || SegmentVirtualEnd > HighestVirtualEnd)
        {
            HighestVirtualEnd = SegmentVirtualEnd;
        }
        FoundLoadSegment = 1;
    }

    if (!FoundLoadSegment || HighestVirtualEnd <= LowestVirtualBase)
    {
        return 0;
    }

    *ImageVirtualBase = LowestVirtualBase;
    *ImageMappedBytes = HighestVirtualEnd - LowestVirtualBase;
    *ImagePageCount = *ImageMappedBytes / 0x1000ULL;
    return *ImagePageCount != 0ULL;
}

static UINT64 ProgramHeaderPageFlagsLocal(UINT32 ProgramHeaderFlags)
{
    UINT64 PageFlags;

    PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER;
    if ((ProgramHeaderFlags & LOS_ELF_PROGRAM_HEADER_FLAG_WRITE) != 0U)
    {
        PageFlags |= LOS_X64_PAGE_WRITABLE;
    }
    if ((ProgramHeaderFlags & LOS_ELF_PROGRAM_HEADER_FLAG_EXECUTE) == 0U)
    {
        PageFlags |= LOS_X64_PAGE_NX;
    }
    return PageFlags;
}

static BOOLEAN ResolveImageEntryVirtualAddressLocal(
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    UINT64 ImageVirtualBase,
    UINT64 ImageMappedBytes,
    UINT64 *EntryVirtualAddress)
{
    UINT64 ImageVirtualEnd;
    UINT64 CandidateEntry;

    if (EntryVirtualAddress != 0)
    {
        *EntryVirtualAddress = 0ULL;
    }
    if (Header == 0 ||
        EntryVirtualAddress == 0 ||
        ImageVirtualBase == 0ULL ||
        ImageMappedBytes == 0ULL)
    {
        return 0;
    }

    ImageVirtualEnd = ImageVirtualBase + ImageMappedBytes;
    if (ImageVirtualEnd <= ImageVirtualBase)
    {
        return 0;
    }

    CandidateEntry = Header->Entry;
    if (CandidateEntry >= ImageVirtualBase && CandidateEntry < ImageVirtualEnd)
    {
        *EntryVirtualAddress = CandidateEntry;
        return 1;
    }

    if (CandidateEntry < ImageMappedBytes)
    {
        CandidateEntry += ImageVirtualBase;
        if (CandidateEntry >= ImageVirtualBase && CandidateEntry < ImageVirtualEnd)
        {
            *EntryVirtualAddress = CandidateEntry;
            return 1;
        }
    }

    return 0;
}

static UINT64 ComputeImagePageFlagsLocal(
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    UINT64 PageVirtualAddress)
{
    UINT16 ProgramHeaderIndex;
    UINT64 PageFlags;
    BOOLEAN Covered;

    if (Header == 0 || ProgramHeaders == 0)
    {
        return 0ULL;
    }

    PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER | LOS_X64_PAGE_NX;
    Covered = 0;
    for (ProgramHeaderIndex = 0U; ProgramHeaderIndex < Header->ProgramHeaderCount; ++ProgramHeaderIndex)
    {
        const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeader;
        UINT64 SegmentVirtualBase;
        UINT64 SegmentMappedBytes;
        UINT64 SegmentPageCount;
        UINT64 SegmentVirtualEnd;

        ProgramHeader = (const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *)((const UINT8 *)ProgramHeaders + ((UINTN)ProgramHeaderIndex * Header->ProgramHeaderEntrySize));
        if (ProgramHeader->Type != LOS_ELF_PROGRAM_HEADER_TYPE_LOAD)
        {
            continue;
        }
        if (!TryGetLoadSegmentGeometryLocal(ProgramHeader, &SegmentVirtualBase, &SegmentMappedBytes, &SegmentPageCount))
        {
            continue;
        }
        (void)SegmentPageCount;

        SegmentVirtualEnd = SegmentVirtualBase + SegmentMappedBytes;
        if (PageVirtualAddress < SegmentVirtualBase || PageVirtualAddress >= SegmentVirtualEnd)
        {
            continue;
        }

        {
            UINT64 SegmentPageFlags;

            SegmentPageFlags = ProgramHeaderPageFlagsLocal(ProgramHeader->Flags);
            if ((SegmentPageFlags & LOS_X64_PAGE_WRITABLE) != 0ULL)
            {
                PageFlags |= LOS_X64_PAGE_WRITABLE;
            }
            if ((SegmentPageFlags & LOS_X64_PAGE_NX) == 0ULL)
            {
                PageFlags &= ~LOS_X64_PAGE_NX;
            }
        }
        Covered = 1;
    }

    return Covered ? PageFlags : 0ULL;
}

static BOOLEAN StageImageIntoPhysicalMemoryLocal(
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

    if (Header == 0 ||
        ProgramHeaders == 0 ||
        ImageVirtualBase == 0 ||
        ImageMappedBytes == 0 ||
        ImagePageCount == 0 ||
        ImagePhysicalBase == 0)
    {
        return 0;
    }

    if (!DescribeImageLayoutLocal(Header, ProgramHeaders, ImageVirtualBase, ImageMappedBytes, ImagePageCount))
    {
        return 0;
    }
    if (!ClaimContiguousPagesLocal(*ImagePageCount, ImagePhysicalBase))
    {
        return 0;
    }

    ImageTarget = LosX64GetDirectMapVirtualAddress(*ImagePhysicalBase, *ImageMappedBytes);
    if (ImageTarget == 0)
    {
        return 0;
    }

    ZeroMemory(ImageTarget, (UINTN)*ImageMappedBytes);
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
            return 0;
        }
        if (!TryGetLoadSegmentGeometryLocal(ProgramHeader, &SegmentVirtualBase, &SegmentMappedBytes, &SegmentPageCount))
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

static void DispatchLocalAttachStagedImage(
    const LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_REQUEST *Request,
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *Result,
    UINT32 *Status)
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

    if (Status != 0)
    {
        *Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    }
    if (Result == 0)
    {
        return;
    }

    ZeroMemory(Result, sizeof(*Result));
    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
    if (Request == 0 ||
        Request->AddressSpaceObjectPhysicalAddress == 0ULL ||
        Request->StagedImagePhysicalAddress == 0ULL ||
        Request->StagedImageSize < sizeof(LOS_MEMORY_MANAGER_ELF64_HEADER))
    {
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    Result->AddressSpaceObjectPhysicalAddress = Request->AddressSpaceObjectPhysicalAddress;
    AddressSpaceObject = ResolveAddressSpaceObjectLocal(Request->AddressSpaceObjectPhysicalAddress);
    if (AddressSpaceObject == 0)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }
    if ((AddressSpaceObject->Flags & LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE) != 0ULL)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    Header = (const LOS_MEMORY_MANAGER_ELF64_HEADER *)LosX64GetDirectMapVirtualAddress(
        Request->StagedImagePhysicalAddress,
        Request->StagedImageSize);
    if (Header == 0 || !ValidateElfHeaderLocal(Header, Request->StagedImageSize, &ProgramHeaders))
    {
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    if (!DescribeImageLayoutLocal(Header, ProgramHeaders, &ImageVirtualBase, &ImageMappedBytes, &ImagePageCount) ||
        !ResolveImageEntryVirtualAddressLocal(Header, ImageVirtualBase, ImageMappedBytes, &EntryVirtualAddress) ||
        !CanReserveRegionLocal(AddressSpaceObject, ImageVirtualBase, ImagePageCount))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    ImagePhysicalBase = 0ULL;
    if (!StageImageIntoPhysicalMemoryLocal(
            Header,
            Request->StagedImageSize,
            ProgramHeaders,
            &ImageVirtualBase,
            &ImageMappedBytes,
            &ImagePageCount,
            &ImagePhysicalBase))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    PageIndex = 0ULL;
    while (PageIndex < ImagePageCount)
    {
        UINT64 RunVirtualAddress;
        UINT64 RunPhysicalAddress;
        UINT64 RunPageCount;
        UINT64 PageFlags;

        RunVirtualAddress = ImageVirtualBase + (PageIndex * 0x1000ULL);
        RunPhysicalAddress = ImagePhysicalBase + (PageIndex * 0x1000ULL);
        PageFlags = ComputeImagePageFlagsLocal(Header, ProgramHeaders, RunVirtualAddress);
        if (PageFlags == 0ULL)
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_INVALID_ARGUMENT;
            if (Status != 0)
            {
                *Status = Result->Status;
            }
            return;
        }

        RunPageCount = 1ULL;
        while (PageIndex + RunPageCount < ImagePageCount)
        {
            UINT64 NextVirtualAddress;
            UINT64 NextPageFlags;

            NextVirtualAddress = ImageVirtualBase + ((PageIndex + RunPageCount) * 0x1000ULL);
            NextPageFlags = ComputeImagePageFlagsLocal(Header, ProgramHeaders, NextVirtualAddress);
            if (NextPageFlags != PageFlags)
            {
                break;
            }
            RunPageCount += 1ULL;
        }

        if (!MapPagesIntoAddressSpaceLocal(
                AddressSpaceObject->RootTablePhysicalAddress,
                RunVirtualAddress,
                RunPhysicalAddress,
                RunPageCount,
                PageFlags))
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
            if (Status != 0)
            {
                *Status = Result->Status;
            }
            return;
        }

        PageIndex += RunPageCount;
    }

    if (!ReserveVirtualRegionLocal(
            AddressSpaceObject,
            ImageVirtualBase,
            ImagePageCount,
            LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_IMAGE,
            0U,
            ImagePhysicalBase))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    AddressSpaceObject->ServiceImagePhysicalAddress = ImagePhysicalBase;
    AddressSpaceObject->ServiceImageSize = ImageMappedBytes;
    AddressSpaceObject->ServiceImageVirtualBase = ImageVirtualBase;
    AddressSpaceObject->EntryVirtualAddress = EntryVirtualAddress;
    AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE;

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->ImagePhysicalAddress = ImagePhysicalBase;
    Result->ImageSize = ImageMappedBytes;
    Result->ImageVirtualBase = ImageVirtualBase;
    Result->EntryVirtualAddress = EntryVirtualAddress;
    Result->ImagePageCount = ImagePageCount;
    if (Status != 0)
    {
        *Status = Result->Status;
    }
}
