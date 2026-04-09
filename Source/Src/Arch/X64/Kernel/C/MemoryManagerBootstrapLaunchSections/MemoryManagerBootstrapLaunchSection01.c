/*
 * File Name: MemoryManagerBootstrapLaunchSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapLaunch.c.
 */

typedef struct __attribute__((packed))
{
    UINT8 Ident[16];
    UINT16 Type;
    UINT16 Machine;
    UINT32 Version;
    UINT64 Entry;
    UINT64 ProgramHeaderOffset;
    UINT64 SectionHeaderOffset;
    UINT32 Flags;
    UINT16 HeaderSize;
    UINT16 ProgramHeaderEntrySize;
    UINT16 ProgramHeaderCount;
    UINT16 SectionHeaderEntrySize;
    UINT16 SectionHeaderCount;
    UINT16 SectionNameStringIndex;
} LOS_MEMORY_MANAGER_ELF64_HEADER;

typedef struct __attribute__((packed))
{
    UINT32 Type;
    UINT32 Flags;
    UINT64 Offset;
    UINT64 VirtualAddress;
    UINT64 PhysicalAddress;
    UINT64 FileSize;
    UINT64 MemorySize;
    UINT64 Align;
} LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER;

static void ZeroMemory(void *Buffer, UINTN ByteCount)
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

static void CopyMemory(void *Destination, const void *Source, UINTN ByteCount)
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

static void SetKernelPrepareDiagnostic(UINT64 Stage, UINT64 Detail)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    if (State->ServiceTaskObject == 0)
    {
        return;
    }

    State->ServiceTaskObject->LastRequestId = Stage;
    State->ServiceTaskObject->Heartbeat = Detail;
}

static UINT64 AlignDown(UINT64 Value, UINT64 Alignment)
{
    return Value & ~(Alignment - 1ULL);
}

static UINT64 AlignUp(UINT64 Value, UINT64 Alignment)
{
    return (Value + Alignment - 1ULL) & ~(Alignment - 1ULL);
}

static UINT64 SelectServiceStackBaseVirtualAddress(UINT64 ImageVirtualBase, UINT64 ImagePageCount)
{
    UINT64 ImageEndVirtualAddress;

    if (ImageVirtualBase == 0ULL || ImagePageCount == 0ULL)
    {
        return 0ULL;
    }

    ImageEndVirtualAddress = ImageVirtualBase + (ImagePageCount * 0x1000ULL);
    return AlignUp(ImageEndVirtualAddress + LOS_X64_SERVICE_STACK_GAP_BYTES, 0x1000ULL);
}

static UINT64 ReadCr3(void)
{
    UINT64 Value;

    __asm__ __volatile__("mov %%cr3, %0" : "=r"(Value));
    return Value;
}

static void WriteCr3(UINT64 Value)
{
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(Value) : "memory");
}

static void TraceTransferContext(
    UINT64 PreviousRootPhysicalAddress,
    UINT64 TargetRootPhysicalAddress,
    UINT64 LaunchBlockDirectMapAddress,
    UINT64 TargetEntryVirtualAddress,
    UINT64 TargetStackTopVirtualAddress)
{
    (void)PreviousRootPhysicalAddress;
    (void)TargetRootPhysicalAddress;
    (void)LaunchBlockDirectMapAddress;
    (void)TargetEntryVirtualAddress;
    (void)TargetStackTopVirtualAddress;
}

static BOOLEAN ClaimContiguousPages(UINT64 PageCount, UINT64 *BaseAddress)
{
    LOS_X64_CLAIM_FRAMES_REQUEST Request;
    LOS_X64_CLAIM_FRAMES_RESULT Result;

    ZeroMemory(&Request, sizeof(Request));
    ZeroMemory(&Result, sizeof(Result));

    Request.MinimumPhysicalAddress = 0x1000ULL;
    Request.AlignmentBytes = 0x1000ULL;
    Request.PageCount = PageCount;
    Request.Flags = LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS;
    Request.Owner = LOS_X64_MEMORY_REGION_OWNER_CLAIMED;

    LosX64ClaimFrames(&Request, &Result);
    if (Result.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS || Result.PageCount != PageCount)
    {
        return 0;
    }

    *BaseAddress = Result.BaseAddress;
    return 1;
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

static BOOLEAN CloneCurrentRootPageMap(UINT64 *NewRootPhysicalAddress)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    UINT64 *NewRoot;
    UINT64 *CurrentRoot;
    UINTN EntryIndex;

    if (NewRootPhysicalAddress == 0)
    {
        return 0;
    }

    State = LosMemoryManagerBootstrapState();
    if (State->Info.ServicePageMapLevel4PhysicalAddress != 0ULL)
    {
        *NewRootPhysicalAddress = State->Info.ServicePageMapLevel4PhysicalAddress;
        return 1;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_CLONE_ROOT, LOS_MEMORY_MANAGER_PREP_DETAIL_CLAIM_ROOT_PAGE);
    if (!ClaimContiguousPages(1ULL, &State->Info.ServicePageMapLevel4PhysicalAddress))
    {
        return 0;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_CLONE_ROOT, LOS_MEMORY_MANAGER_PREP_DETAIL_DIRECT_MAP_ROOT);
    NewRoot = (UINT64 *)(UINTN)LosX64GetDirectMapVirtualAddress(State->Info.ServicePageMapLevel4PhysicalAddress, 0x1000ULL);
    CurrentRoot = LosX64GetPageMapLevel4();
    if (NewRoot == 0 || CurrentRoot == 0)
    {
        return 0;
    }

    for (EntryIndex = 0U; EntryIndex < 512U; ++EntryIndex)
    {
        NewRoot[EntryIndex] = 0ULL;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_CLONE_ROOT, LOS_MEMORY_MANAGER_PREP_DETAIL_COPY_HIGHER_HALF);
    for (EntryIndex = 256U; EntryIndex < 512U; ++EntryIndex)
    {
        NewRoot[EntryIndex] = CurrentRoot[EntryIndex];
    }

    State->ServiceAddressSpaceObject->KernelRootTablePhysicalAddress = LosX64GetCurrentPageMapLevel4PhysicalAddress();
    State->ServiceAddressSpaceObject->RootTablePhysicalAddress = State->Info.ServicePageMapLevel4PhysicalAddress;
    State->Info.ServicePageMapLevel4PhysicalAddress = State->ServiceAddressSpaceObject->RootTablePhysicalAddress;
    State->Info.Flags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_ISOLATED_ROOT_READY;
    State->LaunchBlock->Flags = State->Info.Flags;
    State->LaunchBlock->ServicePageMapLevel4PhysicalAddress = State->Info.ServicePageMapLevel4PhysicalAddress;
    *NewRootPhysicalAddress = State->Info.ServicePageMapLevel4PhysicalAddress;
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

    *SegmentVirtualBase = AlignDown(ProgramHeader->VirtualAddress, 0x1000ULL);
    OffsetIntoFirstPage = ProgramHeader->VirtualAddress - *SegmentVirtualBase;
    *SegmentMappedBytes = AlignUp(ProgramHeader->MemorySize + OffsetIntoFirstPage, 0x1000ULL);
    *SegmentPageCount = *SegmentMappedBytes / 0x1000ULL;
    return *SegmentPageCount != 0ULL;
}

static BOOLEAN DescribeServiceImageLayout(
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

        SegmentVirtualEnd = SegmentVirtualBase + SegmentMappedBytes;
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

static BOOLEAN ResolveServiceEntryVirtualAddress(
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
