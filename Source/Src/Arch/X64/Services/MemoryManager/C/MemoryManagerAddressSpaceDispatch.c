#include "MemoryManagerAddressSpaceInternal.h"

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

#define LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE 0x3F8U

static inline void AddressSpaceServiceOut8(UINT16 Port, UINT8 Value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static inline UINT8 AddressSpaceServiceIn8(UINT16 Port)
{
    UINT8 Value;

    __asm__ __volatile__("inb %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

static void AddressSpaceServiceSerialWriteChar(char Character)
{
    while ((AddressSpaceServiceIn8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 5U) & 0x20U) == 0U)
    {
    }

    AddressSpaceServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 0U, (UINT8)Character);
}

static void AddressSpaceServiceSerialWriteText(const char *Text)
{
    UINTN Index;

    if (Text == 0)
    {
        return;
    }

    for (Index = 0U; Text[Index] != '\0'; ++Index)
    {
        if (Text[Index] == '\n')
        {
            AddressSpaceServiceSerialWriteChar('\r');
        }
        AddressSpaceServiceSerialWriteChar(Text[Index]);
    }
}

static void AddressSpaceServiceSerialWriteHex64(UINT64 Value)
{
    UINTN Shift;

    AddressSpaceServiceSerialWriteText("0x");
    for (Shift = 16U; Shift > 0U; --Shift)
    {
        UINT8 Nibble;

        Nibble = (UINT8)((Value >> ((Shift - 1U) * 4U)) & 0xFULL);
        AddressSpaceServiceSerialWriteChar((char)(Nibble < 10U ? ('0' + Nibble) : ('A' + (Nibble - 10U))));
    }
}

static void AddressSpaceServiceSerialWriteUnsigned(UINT64 Value)
{
    char Buffer[32];
    UINTN Index;

    if (Value == 0ULL)
    {
        AddressSpaceServiceSerialWriteChar('0');
        return;
    }

    Index = 0U;
    while (Value != 0ULL && Index < (sizeof(Buffer) / sizeof(Buffer[0])))
    {
        Buffer[Index] = (char)('0' + (Value % 10ULL));
        Value /= 10ULL;
        ++Index;
    }

    while (Index > 0U)
    {
        --Index;
        AddressSpaceServiceSerialWriteChar(Buffer[Index]);
    }
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
        if (!LosMemoryManagerServiceFreeTrackedFrames(
                State,
                AddressSpaceObject->ServiceImagePhysicalAddress,
                AddressSpaceObject->ServiceImageSize / 0x1000ULL))
        {
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
            return;
        }
        ReleasedPageCount += AddressSpaceObject->ServiceImageSize / 0x1000ULL;
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
    if ((AddressSpaceObject->Flags & LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE) != 0ULL)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    Header = (const LOS_MEMORY_MANAGER_ELF64_HEADER *)LosMemoryManagerTranslatePhysical(
        State,
        Request->StagedImagePhysicalAddress,
        Request->StagedImageSize);
    if (Header == 0 || !ValidateElfHeader(Header, Request->StagedImageSize, &ProgramHeaders))
    {
        return;
    }

    if (!DescribeImageLayout(Header, ProgramHeaders, &ImageVirtualBase, &ImageMappedBytes, &ImagePageCount))
    {
        return;
    }
    if (!CanReserveRegion(AddressSpaceObject, ImageVirtualBase, ImagePageCount))
    {
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

        if (!LosMemoryManagerMapPagesIntoAddressSpace(
                State,
                AddressSpaceObject->RootTablePhysicalAddress,
                RunVirtualAddress,
                RunPhysicalAddress,
                RunPageCount,
                PageFlags))
        {
            LosMemoryManagerServiceFreeTrackedFrames(State, ImagePhysicalBase, ImagePageCount);
            Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
            return;
        }

        PageIndex += RunPageCount;
    }

    if (!LosMemoryManagerReserveVirtualRegion(
            AddressSpaceObject,
            ImageVirtualBase,
            ImagePageCount,
            LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_IMAGE,
            0U,
            ImagePhysicalBase))
    {
        LosMemoryManagerServiceFreeTrackedFrames(State, ImagePhysicalBase, ImagePageCount);
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    AddressSpaceObject->ServiceImagePhysicalAddress = ImagePhysicalBase;
    AddressSpaceObject->ServiceImageSize = ImageMappedBytes;
    AddressSpaceObject->ServiceImageVirtualBase = ImageVirtualBase;
    AddressSpaceObject->EntryVirtualAddress = Header->Entry;
    AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE;

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->AddressSpaceObjectPhysicalAddress = Request->AddressSpaceObjectPhysicalAddress;
    Result->ImagePhysicalAddress = ImagePhysicalBase;
    Result->ImageSize = ImageMappedBytes;
    Result->ImageVirtualBase = ImageVirtualBase;
    Result->EntryVirtualAddress = Header->Entry;
    Result->ImagePageCount = ImagePageCount;
}

void LosMemoryManagerServiceAllocateAddressSpaceStack(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_REQUEST *Request,
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
    UINT64 StackBaseVirtualAddress;
    UINT64 StackPhysicalAddress;

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
    if (Request->PageCount == 0ULL)
    {
        return;
    }

    AddressSpaceObject = 0;
    if (!LosMemoryManagerResolveAddressSpaceObject(State, Request->AddressSpaceObjectPhysicalAddress, 0, &AddressSpaceObject))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        return;
    }
    if ((AddressSpaceObject->Flags & LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK) != 0ULL)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    StackBaseVirtualAddress = 0ULL;
    if (!LosMemoryManagerSelectStackBaseVirtualAddress(
            AddressSpaceObject,
            Request->DesiredStackBaseVirtualAddress,
            Request->PageCount,
            &StackBaseVirtualAddress))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }
    if (!CanReserveRegion(AddressSpaceObject, StackBaseVirtualAddress, Request->PageCount))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    StackPhysicalAddress = 0ULL;
    if (!LosMemoryManagerServiceClaimTrackedFrames(
            State,
            Request->PageCount,
            0x1000ULL,
            LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS,
            LOS_X64_MEMORY_REGION_OWNER_CLAIMED,
            LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_STACK,
            &StackPhysicalAddress))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    if (!LosMemoryManagerMapPagesIntoAddressSpace(
            State,
            AddressSpaceObject->RootTablePhysicalAddress,
            StackBaseVirtualAddress,
            StackPhysicalAddress,
            Request->PageCount,
            LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_USER | LOS_X64_PAGE_NX))
    {
        LosMemoryManagerServiceFreeTrackedFrames(State, StackPhysicalAddress, Request->PageCount);
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    if (!LosMemoryManagerReserveVirtualRegion(
            AddressSpaceObject,
            StackBaseVirtualAddress,
            Request->PageCount,
            LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_STACK,
            0U,
            StackPhysicalAddress))
    {
        LosMemoryManagerServiceFreeTrackedFrames(State, StackPhysicalAddress, Request->PageCount);
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        return;
    }

    AddressSpaceObject->StackPhysicalAddress = StackPhysicalAddress;
    AddressSpaceObject->StackPageCount = Request->PageCount;
    AddressSpaceObject->StackBaseVirtualAddress = StackBaseVirtualAddress;
    AddressSpaceObject->StackTopVirtualAddress = StackBaseVirtualAddress + (Request->PageCount * 0x1000ULL);
    AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK;

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->AddressSpaceObjectPhysicalAddress = Request->AddressSpaceObjectPhysicalAddress;
    Result->StackPhysicalAddress = StackPhysicalAddress;
    Result->StackPageCount = Request->PageCount;
    Result->StackBaseVirtualAddress = StackBaseVirtualAddress;
    Result->StackTopVirtualAddress = AddressSpaceObject->StackTopVirtualAddress;
}
