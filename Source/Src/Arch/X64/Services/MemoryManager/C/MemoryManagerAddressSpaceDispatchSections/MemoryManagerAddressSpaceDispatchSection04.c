/*
 * File Name: MemoryManagerAddressSpaceDispatchSection04.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerAddressSpaceDispatch.c.
 */

static void ZeroBytes(void *Buffer, UINTN ByteCount)
{
    UINT8 *Bytes;
    UINTN Index;

    if (Buffer == 0)
    {
        return;
    }

    Bytes = (UINT8 *)Buffer;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        Bytes[Index] = 0U;
    }
}

static void CopyBytes(void *Destination, const void *Source, UINTN ByteCount)
{
    UINT8 *DestinationBytes;
    const UINT8 *SourceBytes;
    UINTN Index;

    if (Destination == 0 || Source == 0)
    {
        return;
    }

    DestinationBytes = (UINT8 *)Destination;
    SourceBytes = (const UINT8 *)Source;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        DestinationBytes[Index] = SourceBytes[Index];
    }
}

static UINT64 AlignDownToPage(UINT64 Value)
{
    return Value & ~0xFFFULL;
}

static UINT64 AlignUpToPage(UINT64 Value)
{
    return (Value + 0xFFFULL) & ~0xFFFULL;
}

static BOOLEAN CanReserveRegion(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 BaseVirtualAddress,
    UINT64 PageCount)
{
    UINT32 ScanIndex;
    UINT64 EndVirtualAddress;

    if (AddressSpaceObject == 0 ||
        PageCount == 0ULL ||
        (BaseVirtualAddress & 0xFFFULL) != 0ULL ||
        BaseVirtualAddress >= LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT)
    {
        return 0;
    }

    EndVirtualAddress = BaseVirtualAddress + (PageCount * 0x1000ULL);
    if (EndVirtualAddress <= BaseVirtualAddress ||
        EndVirtualAddress > LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT ||
        AddressSpaceObject->ReservedVirtualRegionCount >= LOS_MEMORY_MANAGER_MAX_RESERVED_VIRTUAL_REGIONS)
    {
        return 0;
    }

    for (ScanIndex = 0U; ScanIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++ScanIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Current;
        UINT64 CurrentEnd;

        Current = &AddressSpaceObject->ReservedVirtualRegions[ScanIndex];
        CurrentEnd = Current->BaseVirtualAddress + (Current->PageCount * 0x1000ULL);
        if (!(EndVirtualAddress <= Current->BaseVirtualAddress || CurrentEnd <= BaseVirtualAddress))
        {
            return 0;
        }
    }

    return 1;
}

static BOOLEAN ValidateElfHeader(
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    UINT64 ImageSize,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER **ProgramHeaders)
{
    UINT64 ProgramHeadersEnd;

    if (ProgramHeaders != 0)
    {
        *ProgramHeaders = 0;
    }
    if (Header == 0 || ProgramHeaders == 0 || ImageSize < sizeof(*Header))
    {
        return 0;
    }

    if (Header->Ident[0] != LOS_ELF_MAGIC_0 ||
        Header->Ident[1] != LOS_ELF_MAGIC_1 ||
        Header->Ident[2] != LOS_ELF_MAGIC_2 ||
        Header->Ident[3] != LOS_ELF_MAGIC_3 ||
        Header->Ident[4] != LOS_ELF_CLASS_64 ||
        Header->Ident[5] != LOS_ELF_DATA_LITTLE_ENDIAN ||
        Header->Machine != LOS_ELF_MACHINE_X86_64 ||
        Header->Type != LOS_ELF_TYPE_EXEC ||
        Header->ProgramHeaderCount == 0U ||
        Header->ProgramHeaderEntrySize < sizeof(LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER))
    {
        return 0;
    }

    ProgramHeadersEnd = Header->ProgramHeaderOffset + ((UINT64)Header->ProgramHeaderCount * Header->ProgramHeaderEntrySize);
    if (ProgramHeadersEnd < Header->ProgramHeaderOffset || ProgramHeadersEnd > ImageSize)
    {
        return 0;
    }

    *ProgramHeaders = (const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *)((const UINT8 *)Header + Header->ProgramHeaderOffset);
    return 1;
}

static BOOLEAN TryGetLoadSegmentGeometry(
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

static BOOLEAN DescribeImageLayout(
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

        if (!TryGetLoadSegmentGeometry(ProgramHeader, &SegmentVirtualBase, &SegmentMappedBytes, &SegmentPageCount))
        {
            continue;
        }
        (void)SegmentPageCount;

        SegmentVirtualEnd = SegmentVirtualBase + SegmentMappedBytes;
        if (SegmentVirtualEnd <= SegmentVirtualBase || SegmentVirtualEnd > LOS_MEMORY_MANAGER_ADDRESS_SPACE_LOW_HALF_LIMIT)
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

static BOOLEAN ResolveImageEntryVirtualAddress(
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

static UINT64 ProgramHeaderPageFlags(UINT32 ProgramHeaderFlags)
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

static UINT64 ComputeImagePageFlags(
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
        if (!TryGetLoadSegmentGeometry(ProgramHeader, &SegmentVirtualBase, &SegmentMappedBytes, &SegmentPageCount))
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

            SegmentPageFlags = ProgramHeaderPageFlags(ProgramHeader->Flags);
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
