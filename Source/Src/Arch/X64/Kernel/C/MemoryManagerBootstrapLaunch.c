#include "MemoryManagerBootstrapInternal.h"
#include "VirtualMemoryInternal.h"

#define LOS_ELF_MAGIC_0 0x7FU
#define LOS_ELF_MAGIC_1 0x45U
#define LOS_ELF_MAGIC_2 0x4CU
#define LOS_ELF_MAGIC_3 0x46U
#define LOS_ELF_CLASS_64 2U
#define LOS_ELF_DATA_LITTLE_ENDIAN 1U
#define LOS_ELF_MACHINE_X86_64 0x3EU
#define LOS_ELF_TYPE_EXEC 2U
#define LOS_ELF_PROGRAM_HEADER_TYPE_LOAD 1U
#define LOS_ELF_PROGRAM_HEADER_FLAG_EXECUTE 0x1U
#define LOS_ELF_PROGRAM_HEADER_FLAG_WRITE 0x2U
#define LOS_ELF_PROGRAM_HEADER_FLAG_READ 0x4U

#define LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_IMAGE_MAPPED 0x0000000000002000ULL
#define LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_ENTRY_INVOKED 0x0000000000004000ULL

#define LOS_MEMORY_MANAGER_PREP_STAGE_CLONE_ROOT 100ULL
#define LOS_MEMORY_MANAGER_PREP_STAGE_VALIDATE_ELF 101ULL
#define LOS_MEMORY_MANAGER_PREP_STAGE_MAP_SEGMENT 102ULL
#define LOS_MEMORY_MANAGER_PREP_STAGE_MAP_STACK 103ULL
#define LOS_MEMORY_MANAGER_PREP_STAGE_PUBLISH_LAUNCH 104ULL
#define LOS_MEMORY_MANAGER_PREP_STAGE_VERIFY_CONTEXT 105ULL

#define LOS_MEMORY_MANAGER_PREP_DETAIL_BEGIN 100ULL
#define LOS_MEMORY_MANAGER_PREP_DETAIL_CLAIM_ROOT_PAGE 101ULL
#define LOS_MEMORY_MANAGER_PREP_DETAIL_DIRECT_MAP_ROOT 102ULL
#define LOS_MEMORY_MANAGER_PREP_DETAIL_COPY_HIGHER_HALF 103ULL
#define LOS_MEMORY_MANAGER_PREP_DETAIL_INVALID_ELF 104ULL
#define LOS_MEMORY_MANAGER_PREP_DETAIL_SEGMENT_FRAME_CLAIM 105ULL
#define LOS_MEMORY_MANAGER_PREP_DETAIL_SEGMENT_MAP_STATUS 106ULL
#define LOS_MEMORY_MANAGER_PREP_DETAIL_SEGMENT_DIRECT_MAP 107ULL
#define LOS_MEMORY_MANAGER_PREP_DETAIL_STACK_MAP_STATUS 108ULL
#define LOS_MEMORY_MANAGER_PREP_DETAIL_STACK_TOP_ZERO 109ULL
#define LOS_MEMORY_MANAGER_PREP_DETAIL_ENTRY_ZERO 110ULL
#define LOS_MEMORY_MANAGER_PREP_DETAIL_ROOT_ZERO 111ULL

#define LOS_X64_SERVICE_STACK_VIRTUAL_BASE 0x0000000000800000ULL

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

static UINT64 ReadRsp(void)
{
    UINT64 Value;

    __asm__ __volatile__("mov %%rsp, %0" : "=r"(Value));
    return Value;
}

static UINT64 ReadRbp(void)
{
    UINT64 Value;

    __asm__ __volatile__("mov %%rbp, %0" : "=r"(Value));
    return Value;
}

static void TraceTransferContext(
    UINT64 PreviousRootPhysicalAddress,
    UINT64 TargetRootPhysicalAddress,
    UINT64 LaunchBlockDirectMapAddress,
    UINT64 TargetEntryVirtualAddress,
    UINT64 TargetStackTopVirtualAddress)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    LosKernelTrace("Memory-manager task-transfer trace follows.");
    LosKernelTraceHex64("Memory-manager transfer current CR3: ", PreviousRootPhysicalAddress);
    LosKernelTraceHex64("Memory-manager transfer target CR3: ", TargetRootPhysicalAddress);
    LosKernelTraceHex64("Memory-manager transfer current RSP: ", ReadRsp());
    LosKernelTraceHex64("Memory-manager transfer current RBP: ", ReadRbp());
    LosKernelTraceHex64("Memory-manager transfer target RSP: ", TargetStackTopVirtualAddress);
    LosKernelTraceHex64("Memory-manager transfer target RIP: ", TargetEntryVirtualAddress);
    LosKernelTraceHex64("Memory-manager transfer launch block physical: ", State->Info.LaunchBlockPhysicalAddress);
    LosKernelTraceHex64("Memory-manager transfer launch block direct-map: ", LaunchBlockDirectMapAddress);
    LosKernelTraceHex64("Memory-manager transfer stack physical: ", State->Info.ServiceStackPhysicalAddress);
    LosKernelTraceHex64("Memory-manager transfer stack pages: ", State->Info.ServiceStackPageCount);
    LosKernelTraceHex64("Memory-manager transfer stack top physical: ", State->LaunchBlock != 0 ? State->LaunchBlock->ServiceStackTopPhysicalAddress : 0ULL);
    LosKernelTraceHex64("Memory-manager transfer stack top virtual: ", TargetStackTopVirtualAddress);
    LosKernelTraceHex64("Memory-manager transfer service root object: ", State->ServiceAddressSpaceObject != 0 ? State->ServiceAddressSpaceObject->RootTablePhysicalAddress : 0ULL);
    LosKernelTraceHex64("Memory-manager transfer task entry object: ", State->ServiceTaskObject != 0 ? State->ServiceTaskObject->EntryVirtualAddress : 0ULL);
    LosKernelTraceHex64("Memory-manager transfer task stack object: ", State->ServiceTaskObject != 0 ? State->ServiceTaskObject->StackTopVirtualAddress : 0ULL);
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
    LosKernelTrace("Memory-manager service root lower-half cleared before first service mappings.");

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

static UINT64 ComputeServiceImagePageFlags(
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    UINT64 PageVirtualAddress)
{
    UINT16 ProgramHeaderIndex;
    UINT64 PageFlags;
    BOOLEAN PageCovered;

    if (Header == 0 || ProgramHeaders == 0)
    {
        return 0ULL;
    }

    PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER | LOS_X64_PAGE_NX;
    PageCovered = 0;
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
        PageCovered = 1;
    }

    return PageCovered ? PageFlags : 0ULL;
}

static BOOLEAN StageServiceImageInPhysicalMemory(
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    UINT64 *ImageVirtualBase,
    UINT64 *ImageMappedBytes,
    UINT64 *ImagePageCount,
    UINT64 *ImagePhysicalBase)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
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

    if (!DescribeServiceImageLayout(Header, ProgramHeaders, ImageVirtualBase, ImageMappedBytes, ImagePageCount))
    {
        return 0;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_SEGMENT, LOS_MEMORY_MANAGER_PREP_DETAIL_SEGMENT_FRAME_CLAIM);
    if (!ClaimContiguousPages(*ImagePageCount, ImagePhysicalBase))
    {
        return 0;
    }

    ImageTarget = LosX64GetDirectMapVirtualAddress(*ImagePhysicalBase, *ImageMappedBytes);
    if (ImageTarget == 0)
    {
        SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_SEGMENT, LOS_MEMORY_MANAGER_PREP_DETAIL_SEGMENT_DIRECT_MAP);
        return 0;
    }

    ZeroMemory(ImageTarget, (UINTN)*ImageMappedBytes);
    LosKernelTraceHex64("Memory-manager image virtual base: ", *ImageVirtualBase);
    LosKernelTraceHex64("Memory-manager image physical base: ", *ImagePhysicalBase);
    LosKernelTraceHex64("Memory-manager image mapped bytes: ", *ImageMappedBytes);
    LosKernelTraceHex64("Memory-manager image page count: ", *ImagePageCount);

    for (ProgramHeaderIndex = 0U; ProgramHeaderIndex < Header->ProgramHeaderCount; ++ProgramHeaderIndex)
    {
        const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeader;
        UINT64 SegmentVirtualBase;
        UINT64 SegmentMappedBytes;
        UINT64 SegmentPageCount;
        UINT64 SegmentPhysicalBase;
        UINT64 SegmentImageOffset;
        void *SegmentTarget;
        const void *SegmentSource;

        ProgramHeader = (const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *)((const UINT8 *)ProgramHeaders + ((UINTN)ProgramHeaderIndex * Header->ProgramHeaderEntrySize));
        if (ProgramHeader->Type != LOS_ELF_PROGRAM_HEADER_TYPE_LOAD)
        {
            continue;
        }

        if (!TryGetLoadSegmentGeometry(ProgramHeader, &SegmentVirtualBase, &SegmentMappedBytes, &SegmentPageCount))
        {
            continue;
        }

        SegmentImageOffset = ProgramHeader->VirtualAddress - *ImageVirtualBase;
        SegmentPhysicalBase = *ImagePhysicalBase + (SegmentVirtualBase - *ImageVirtualBase);
        SegmentTarget = (UINT8 *)ImageTarget + SegmentImageOffset;

        LosKernelTraceHex64("Memory-manager segment virtual base: ", SegmentVirtualBase);
        LosKernelTraceHex64("Memory-manager segment physical base: ", SegmentPhysicalBase);
        LosKernelTraceHex64("Memory-manager segment file bytes: ", ProgramHeader->FileSize);
        LosKernelTraceHex64("Memory-manager segment memory bytes: ", ProgramHeader->MemorySize);
        LosKernelTraceHex64("Memory-manager segment page count: ", SegmentPageCount);
        LosKernelTraceHex64("Memory-manager segment flags: ", ProgramHeader->Flags);

        if (ProgramHeader->FileSize != 0ULL)
        {
            SegmentSource = (const void *)((const UINT8 *)Header + ProgramHeader->Offset);
            CopyMemory(SegmentTarget, SegmentSource, (UINTN)ProgramHeader->FileSize);
        }
    }

    State = LosMemoryManagerBootstrapState();
    State->Info.ServiceImagePhysicalAddress = *ImagePhysicalBase;
    State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress = *ImagePhysicalBase;
    State->LaunchBlock->ServiceImagePhysicalAddress = *ImagePhysicalBase;
    return 1;
}

static BOOLEAN MapStagedServiceImageIntoAddressSpace(
    UINT64 PageMapLevel4PhysicalAddress,
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    UINT64 ImageVirtualBase,
    UINT64 ImagePageCount,
    UINT64 ImagePhysicalBase)
{
    UINT64 PageIndex;

    if (PageMapLevel4PhysicalAddress == 0ULL ||
        Header == 0 ||
        ProgramHeaders == 0 ||
        ImagePageCount == 0ULL ||
        ImagePhysicalBase == 0ULL)
    {
        return 0;
    }

    for (PageIndex = 0ULL; PageIndex < ImagePageCount; ++PageIndex)
    {
        UINT64 PageVirtualAddress;
        UINT64 PagePhysicalAddress;
        UINT64 PageFlags;
        LOS_X64_MAP_PAGES_REQUEST MapRequest;
        LOS_X64_MAP_PAGES_RESULT MapResult;

        PageVirtualAddress = ImageVirtualBase + (PageIndex * 0x1000ULL);
        PagePhysicalAddress = ImagePhysicalBase + (PageIndex * 0x1000ULL);
        PageFlags = ComputeServiceImagePageFlags(Header, ProgramHeaders, PageVirtualAddress);
        if (PageFlags == 0ULL)
        {
            SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_SEGMENT, LOS_MEMORY_MANAGER_PREP_DETAIL_SEGMENT_MAP_STATUS);
            LosKernelTraceHex64("Memory-manager image page had no segment coverage: ", PageVirtualAddress);
            return 0;
        }

        ZeroMemory(&MapRequest, sizeof(MapRequest));
        ZeroMemory(&MapResult, sizeof(MapResult));
        MapRequest.PageMapLevel4PhysicalAddress = PageMapLevel4PhysicalAddress;
        MapRequest.VirtualAddress = PageVirtualAddress;
        MapRequest.PhysicalAddress = PagePhysicalAddress;
        MapRequest.PageCount = 1ULL;
        MapRequest.PageFlags = PageFlags;
        MapRequest.Flags = 0U;
        LosX64MapPages(&MapRequest, &MapResult);
        if (MapResult.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS || MapResult.PagesProcessed != 1ULL)
        {
            SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_SEGMENT, LOS_MEMORY_MANAGER_PREP_DETAIL_SEGMENT_MAP_STATUS);
            LosKernelTraceUnsigned("Memory-manager segment map status: ", MapResult.Status);
            LosKernelTraceHex64("Memory-manager segment map pages processed: ", MapResult.PagesProcessed);
            LosKernelTraceHex64("Memory-manager segment map requested pages: ", 1ULL);
            LosKernelTraceHex64("Memory-manager segment map virtual page: ", PageVirtualAddress);
            LosKernelTraceHex64("Memory-manager segment map physical page: ", PagePhysicalAddress);
            LosKernelTraceHex64("Memory-manager segment map page flags: ", PageFlags);
            return 0;
        }
    }

    return 1;
}

static BOOLEAN MapServiceStackIntoAddressSpace(
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 StackPhysicalAddress,
    UINT64 StackPageCount,
    UINT64 *StackTopVirtualAddress)
{
    LOS_X64_MAP_PAGES_REQUEST MapRequest;
    LOS_X64_MAP_PAGES_RESULT MapResult;
    if (PageMapLevel4PhysicalAddress == 0ULL ||
        StackPhysicalAddress == 0ULL ||
        StackPageCount == 0ULL ||
        StackTopVirtualAddress == 0)
    {
        return 0;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_STACK, LOS_MEMORY_MANAGER_PREP_DETAIL_BEGIN);
    *StackTopVirtualAddress = 0ULL;

    ZeroMemory(&MapRequest, sizeof(MapRequest));
    ZeroMemory(&MapResult, sizeof(MapResult));
    MapRequest.PageMapLevel4PhysicalAddress = PageMapLevel4PhysicalAddress;
    MapRequest.VirtualAddress = LOS_X64_SERVICE_STACK_VIRTUAL_BASE;
    MapRequest.PhysicalAddress = StackPhysicalAddress;
    MapRequest.PageCount = StackPageCount;
    MapRequest.PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_USER | LOS_X64_PAGE_NX;
    MapRequest.Flags = 0U;
    LosX64MapPages(&MapRequest, &MapResult);
    if (MapResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS && MapResult.PagesProcessed == StackPageCount)
    {
        *StackTopVirtualAddress = LOS_X64_SERVICE_STACK_VIRTUAL_BASE + (StackPageCount * 0x1000ULL);
        return 1;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_STACK, LOS_MEMORY_MANAGER_PREP_DETAIL_STACK_MAP_STATUS);
    LosKernelTraceUnsigned("Memory-manager stack map status: ", MapResult.Status);
    LosKernelTraceHex64("Memory-manager stack map pages processed: ", MapResult.PagesProcessed);
    LosKernelTraceHex64("Memory-manager stack map requested pages: ", StackPageCount);
    return 0;
}

static BOOLEAN MapServiceImageIntoOwnAddressSpace(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header;
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders;
    const LOS_X64_VIRTUAL_MEMORY_LAYOUT *Layout;
    UINT64 ServiceRootPhysicalAddress;
    UINT64 ImageVirtualBase;
    UINT64 ImageMappedBytes;
    UINT64 ImagePageCount;
    UINT64 ImagePhysicalBase;

    State = LosMemoryManagerBootstrapState();
    if (State->ServiceImageVirtualAddress == 0ULL)
    {
        return 0;
    }

    if ((State->Info.Flags & LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_IMAGE_MAPPED) != 0ULL)
    {
        return 1;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_CLONE_ROOT, LOS_MEMORY_MANAGER_PREP_DETAIL_BEGIN);
    if (!CloneCurrentRootPageMap(&ServiceRootPhysicalAddress))
    {
        return 0;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VALIDATE_ELF, LOS_MEMORY_MANAGER_PREP_DETAIL_BEGIN);
    Header = (const LOS_MEMORY_MANAGER_ELF64_HEADER *)(UINTN)State->ServiceImageVirtualAddress;
    if (Header == 0)
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
        Header->Type != LOS_ELF_TYPE_EXEC)
    {
        SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VALIDATE_ELF, LOS_MEMORY_MANAGER_PREP_DETAIL_INVALID_ELF);
        return 0;
    }

    ProgramHeaders = (const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *)((const UINT8 *)Header + Header->ProgramHeaderOffset);
    if (!StageServiceImageInPhysicalMemory(
            Header,
            ProgramHeaders,
            &ImageVirtualBase,
            &ImageMappedBytes,
            &ImagePageCount,
            &ImagePhysicalBase))
    {
        return 0;
    }

    if (!MapStagedServiceImageIntoAddressSpace(
            ServiceRootPhysicalAddress,
            Header,
            ProgramHeaders,
            ImageVirtualBase,
            ImagePageCount,
            ImagePhysicalBase))
    {
        return 0;
    }

    Layout = LosX64GetVirtualMemoryLayout();
    State->ServiceAddressSpaceObject->RootTablePhysicalAddress = ServiceRootPhysicalAddress;
    State->ServiceAddressSpaceObject->DirectMapBase = Layout->HigherHalfDirectMapBase;
    State->ServiceAddressSpaceObject->DirectMapSize = Layout->HigherHalfDirectMapSize;
    LosKernelTraceHex64("Memory-manager service root physical before stack map: ", ServiceRootPhysicalAddress);
    LosKernelTraceHex64("Memory-manager service stack physical before stack map: ", State->Info.ServiceStackPhysicalAddress);
    LosKernelTraceHex64("Memory-manager service stack pages before stack map: ", State->Info.ServiceStackPageCount);
    if (!MapServiceStackIntoAddressSpace(
            ServiceRootPhysicalAddress,
            State->Info.ServiceStackPhysicalAddress,
            State->Info.ServiceStackPageCount,
            &State->ServiceTaskObject->StackTopVirtualAddress))
    {
        LosKernelTraceFail("Memory-manager service stack map into service root failed.");
        return 0;
    }
    if (State->ServiceTaskObject->StackTopVirtualAddress == 0ULL)
    {
        LosKernelTraceFail("Memory-manager service stack top virtual address remained zero after stack map.");
        return 0;
    }
    LosKernelTraceHex64("Memory-manager service stack top virtual after stack map: ", State->ServiceTaskObject->StackTopVirtualAddress);
    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_PUBLISH_LAUNCH, LOS_MEMORY_MANAGER_PREP_DETAIL_BEGIN);
    State->Info.Flags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_IMAGE_MAPPED;
    State->LaunchBlock->Flags = State->Info.Flags;
    State->LaunchBlock->ServiceStackTopVirtualAddress = State->ServiceTaskObject->StackTopVirtualAddress;
    return 1;
}

/* Dedicated assembly bootstrap task-transfer helper declared in MemoryManagerBootstrapInternal.h. */

BOOLEAN LosMemoryManagerBootstrapEnsureServiceEntryReady(void)
{
    return MapServiceImageIntoOwnAddressSpace();
}

BOOLEAN LosMemoryManagerBootstrapInvokeServiceEntry(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    UINT64 LaunchBlockDirectMapAddress;
    UINT64 ServiceStackTopVirtualAddress;
    UINT64 PreviousRootPhysicalAddress;

    State = LosMemoryManagerBootstrapState();
    if (!LosMemoryManagerBootstrapEnsureServiceEntryReady())
    {
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager service entry preparation failed.");
    }

    LaunchBlockDirectMapAddress = (UINT64)(UINTN)LosX64GetDirectMapVirtualAddress(State->Info.LaunchBlockPhysicalAddress, State->Info.LaunchBlockSize);
    if (LaunchBlockDirectMapAddress == 0ULL)
    {
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager launch block direct-map translation failed.");
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VERIFY_CONTEXT, LOS_MEMORY_MANAGER_PREP_DETAIL_BEGIN);
    ServiceStackTopVirtualAddress = State->ServiceTaskObject->StackTopVirtualAddress;
    if (ServiceStackTopVirtualAddress == 0ULL)
    {
        SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VERIFY_CONTEXT, LOS_MEMORY_MANAGER_PREP_DETAIL_STACK_TOP_ZERO);
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager service stack top virtual address is zero.");
    }

    PreviousRootPhysicalAddress = ReadCr3();
    TraceTransferContext(
        PreviousRootPhysicalAddress,
        State->ServiceAddressSpaceObject->RootTablePhysicalAddress,
        LaunchBlockDirectMapAddress,
        State->Info.ServiceEntryVirtualAddress,
        ServiceStackTopVirtualAddress);
    State->ServiceTaskObject->LastRequestId = 0x2001ULL;
    State->ServiceTaskObject->Heartbeat = LaunchBlockDirectMapAddress;
    State->ServiceAddressSpaceObject->State = LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_ACTIVE;
    State->ServiceTaskObject->State = LOS_MEMORY_MANAGER_TASK_STATE_RUNNING;
    State->Info.Flags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_CONTEXT_SWITCHED;
    State->LaunchBlock->Flags = State->Info.Flags;
    if (State->ServiceAddressSpaceObject->RootTablePhysicalAddress == 0ULL)
    {
        SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VERIFY_CONTEXT, LOS_MEMORY_MANAGER_PREP_DETAIL_ROOT_ZERO);
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager service root physical address is zero.");
    }
    if (State->Info.ServiceEntryVirtualAddress == 0ULL)
    {
        SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_VERIFY_CONTEXT, LOS_MEMORY_MANAGER_PREP_DETAIL_ENTRY_ZERO);
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager service entry virtual address is zero.");
    }
    LosKernelTrace("Memory-manager task-transfer handoff executing now.");
    LosMemoryManagerBootstrapTransferToServiceTask(
        State->ServiceAddressSpaceObject->RootTablePhysicalAddress,
        State->Info.ServiceEntryVirtualAddress,
        ServiceStackTopVirtualAddress,
        LaunchBlockDirectMapAddress);
    LosKernelTrace("Memory-manager task-transfer helper returned to kernel context.");
    WriteCr3(PreviousRootPhysicalAddress);
    State->ServiceTaskObject->State = LOS_MEMORY_MANAGER_TASK_STATE_ONLINE;
    State->ServiceTaskObject->Flags &= ~LOS_MEMORY_MANAGER_TASK_FLAG_BOOTSTRAP_HOSTED;
    State->Info.Flags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_ENTRY_INVOKED;
    State->LaunchBlock->Flags = State->Info.Flags;
    LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager service entry returned unexpectedly.");
    return 0;
}
