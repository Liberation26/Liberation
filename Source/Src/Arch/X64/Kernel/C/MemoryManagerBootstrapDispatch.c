/*
 * File Name: MemoryManagerBootstrapDispatch.c
 * File Version: 0.3.22
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-08T11:10:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

#include "MemoryManagerBootstrapInternal.h"

#define LOS_MEMORY_MANAGER_BOOTSTRAP_LOCAL_STACK_BASE 0x0000000000800000ULL
#define LOS_MEMORY_MANAGER_BOOTSTRAP_LOCAL_STACK_GAP_BYTES 0x0000000000010000ULL
#define LOS_MEMORY_MANAGER_BOOTSTRAP_LOCAL_LOW_HALF_LIMIT 0x0000800000000000ULL

#define LOS_X64_PAGE_PRESENT 0x001ULL
#define LOS_X64_PAGE_WRITABLE 0x002ULL
#define LOS_X64_PAGE_USER 0x004ULL
#define LOS_X64_PAGE_NX 0x8000000000000000ULL

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

static void ZeroMemory(void *Buffer, UINTN ByteCount);
static void CopyBytes(void *Destination, const void *Source, UINTN ByteCount);
static void CopyText(char *Destination, UINTN Capacity, const char *Source);
static void PopulateCallerIdentity(LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request);

static void CopyText(char *Destination, UINTN Capacity, const char *Source)
{
    UINTN Index;

    if (Destination == 0 || Capacity == 0U)
    {
        return;
    }

    for (Index = 0U; Index < Capacity; ++Index)
    {
        Destination[Index] = 0;
    }

    if (Source == 0)
    {
        return;
    }

    for (Index = 0U; Index + 1U < Capacity && Source[Index] != 0; ++Index)
    {
        Destination[Index] = Source[Index];
    }
}

static void PopulateCallerIdentity(LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request)
{
    if (Request == 0)
    {
        return;
    }

    if (Request->CallerPrincipalType != 0U)
    {
        return;
    }

    Request->CallerPrincipalType = LOS_CAPABILITIES_PRINCIPAL_TYPE_TASK;
    Request->CallerPrincipalId = 0ULL;
    CopyText(Request->CallerPrincipalName, LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH, "kernel");
}

static const char *OperationName(UINT32 Operation)
{
    switch (Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH:
            return "BootstrapAttach";
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES:
            return "AllocateFrames";
        case LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES:
            return "FreeFrames";
        case LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE:
            return "CreateAddressSpace";
        case LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE:
            return "DestroyAddressSpace";
        case LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES:
            return "MapPages";
        case LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES:
            return "UnmapPages";
        case LOS_MEMORY_MANAGER_OPERATION_PROTECT_PAGES:
            return "ProtectPages";
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MAPPING:
            return "QueryMapping";
        case LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE:
            return "AttachStagedImage";
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK:
            return "AllocateAddressSpaceStack";
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
            return "QueryMemoryRegions";
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
            return "ReserveFrames";
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            return "ClaimFrames";
        default:
            return "Unknown";
    }
}


static UINTN QueryPml4Index(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 39U) & 0x1FFULL);
}

static UINTN QueryPdptIndex(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 30U) & 0x1FFULL);
}

static UINTN QueryPdIndex(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 21U) & 0x1FFULL);
}

static UINTN QueryPtIndex(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 12U) & 0x1FFULL);
}

static UINT64 *QueryTranslatePageTable(UINT64 PhysicalAddress)
{
    return (UINT64 *)LosX64GetDirectMapVirtualAddress(PhysicalAddress, 0x1000ULL);
}

static BOOLEAN QueryLookupLeafPageEntry(UINT64 RootTablePhysicalAddress, UINT64 VirtualAddress, UINT64 *Entry)
{
    UINT64 *PageMapLevel4;
    UINT64 CurrentEntry;
    UINT64 *PageDirectoryPointerTable;
    UINT64 *PageDirectory;
    UINT64 *PageTable;

    if (Entry != 0)
    {
        *Entry = 0ULL;
    }
    if (RootTablePhysicalAddress == 0ULL || (VirtualAddress & 0xFFFULL) != 0ULL)
    {
        return 0;
    }

    PageMapLevel4 = QueryTranslatePageTable(RootTablePhysicalAddress);
    if (PageMapLevel4 == 0)
    {
        return 0;
    }

    CurrentEntry = PageMapLevel4[QueryPml4Index(VirtualAddress)];
    if ((CurrentEntry & 0x001ULL) == 0ULL || (CurrentEntry & 0x080ULL) != 0ULL)
    {
        return 0;
    }

    PageDirectoryPointerTable = QueryTranslatePageTable(CurrentEntry & 0x000FFFFFFFFFF000ULL);
    if (PageDirectoryPointerTable == 0)
    {
        return 0;
    }

    CurrentEntry = PageDirectoryPointerTable[QueryPdptIndex(VirtualAddress)];
    if ((CurrentEntry & 0x001ULL) == 0ULL || (CurrentEntry & 0x080ULL) != 0ULL)
    {
        return 0;
    }

    PageDirectory = QueryTranslatePageTable(CurrentEntry & 0x000FFFFFFFFFF000ULL);
    if (PageDirectory == 0)
    {
        return 0;
    }

    CurrentEntry = PageDirectory[QueryPdIndex(VirtualAddress)];
    if ((CurrentEntry & 0x001ULL) == 0ULL || (CurrentEntry & 0x080ULL) != 0ULL)
    {
        return 0;
    }

    PageTable = QueryTranslatePageTable(CurrentEntry & 0x000FFFFFFFFFF000ULL);
    if (PageTable == 0)
    {
        return 0;
    }

    CurrentEntry = PageTable[QueryPtIndex(VirtualAddress)];
    if ((CurrentEntry & 0x001ULL) == 0ULL)
    {
        return 0;
    }

    if (Entry != 0)
    {
        *Entry = CurrentEntry;
    }
    return 1;
}

static LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *ResolveAddressSpaceObjectLocal(UINT64 AddressSpaceObjectPhysicalAddress)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;

    if (AddressSpaceObjectPhysicalAddress == 0ULL)
    {
        return 0;
    }

    AddressSpaceObject = (LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *)LosX64GetDirectMapVirtualAddress(
        AddressSpaceObjectPhysicalAddress,
        sizeof(LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT));
    if (AddressSpaceObject == 0)
    {
        return 0;
    }
    if (AddressSpaceObject->State < LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_READY ||
        AddressSpaceObject->ReservedVirtualRegionCount > LOS_MEMORY_MANAGER_MAX_RESERVED_VIRTUAL_REGIONS ||
        AddressSpaceObject->RootTablePhysicalAddress == 0ULL ||
        (AddressSpaceObject->RootTablePhysicalAddress & 0xFFFULL) != 0ULL)
    {
        return 0;
    }

    return AddressSpaceObject;
}

static void DispatchLocalQueryMapping(
    const LOS_MEMORY_MANAGER_QUERY_MAPPING_REQUEST *Request,
    LOS_MEMORY_MANAGER_QUERY_MAPPING_RESULT *Result,
    UINT32 *Status)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
    UINT64 Entry;

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
    if (Request == 0 || Request->AddressSpaceObjectPhysicalAddress == 0ULL ||
        Request->VirtualAddress == 0ULL || (Request->VirtualAddress & 0xFFFULL) != 0ULL ||
        Request->VirtualAddress >= 0x0000800000000000ULL)
    {
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    Result->AddressSpaceObjectPhysicalAddress = Request->AddressSpaceObjectPhysicalAddress;
    Result->VirtualAddress = Request->VirtualAddress;
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

    if (!QueryLookupLeafPageEntry(AddressSpaceObject->RootTablePhysicalAddress, Request->VirtualAddress, &Entry))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->PhysicalAddress = Entry & 0x000FFFFFFFFFF000ULL;
    Result->PageFlags = Entry & ~0x000FFFFFFFFFF000ULL;
    Result->PageCount = 1ULL;
    if (Status != 0)
    {
        *Status = Result->Status;
    }
}

static void TraceKernelToMemoryManagerRequest(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request)
{
    if (Request == 0)
    {
        return;
    }

    LosKernelSerialWriteText("[MemManager] Kernel -> Memory Manager request=");
    LosKernelSerialWriteText(OperationName(Request->Operation));
    LosKernelSerialWriteText(" id=");
    LosKernelSerialWriteHex64(Request->RequestId);
    if (Request->Operation == LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)
    {
        LosKernelSerialWriteText(" root=");
        LosKernelSerialWriteHex64(Request->Payload.BootstrapAttach.ServicePageMapLevel4PhysicalAddress);
        LosKernelSerialWriteText(" image=");
        LosKernelSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress);
        LosKernelSerialWriteText(" image-bytes=");
        LosKernelSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceImageSize);
        LosKernelSerialWriteText(" entry=");
        LosKernelSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceEntryVirtualAddress);
        LosKernelSerialWriteText(" stack-top=");
        LosKernelSerialWriteHex64(Request->Payload.BootstrapAttach.ServiceStackTopVirtualAddress);
    }
    LosKernelSerialWriteText("\n");
}

static void TraceMemoryManagerToKernelResponse(const LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    if (Response == 0)
    {
        return;
    }

    LosKernelSerialWriteText("[MemManager] Memory Manager -> Kernel response=");
    LosKernelSerialWriteText(OperationName(Response->Operation));
    LosKernelSerialWriteText(" id=");
    LosKernelSerialWriteHex64(Response->RequestId);
    LosKernelSerialWriteText(" status=");
    LosKernelSerialWriteUnsigned(Response->Status);
    if (Response->Operation == LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH)
    {
        LosKernelSerialWriteText(" bootstrap=");
        LosKernelSerialWriteUnsigned(Response->Payload.BootstrapAttach.BootstrapResult);
    }
    LosKernelSerialWriteText("\n");
}

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


static UINT64 SaveInterruptFlagsAndDisable(void)
{
    UINT64 Flags;

    __asm__ __volatile__("pushfq; popq %0" : "=r"(Flags) : : "memory");
    __asm__ __volatile__("cli" : : : "memory");
    return Flags;
}

static void RestoreInterruptFlags(UINT64 Flags)
{
    if ((Flags & 0x200ULL) != 0ULL)
    {
        __asm__ __volatile__("sti" : : : "memory");
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

static BOOLEAN ClaimContiguousPagesLocal(UINT64 PageCount, UINT64 *BaseAddress)
{
    LOS_X64_CLAIM_FRAMES_REQUEST Request;
    LOS_X64_CLAIM_FRAMES_RESULT Result;

    if (BaseAddress != 0)
    {
        *BaseAddress = 0ULL;
    }
    if (BaseAddress == 0 || PageCount == 0ULL)
    {
        return 0;
    }

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

static BOOLEAN MapPagesIntoAddressSpaceLocal(
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 VirtualAddress,
    UINT64 PhysicalAddress,
    UINT64 PageCount,
    UINT64 PageFlags)
{
    LOS_X64_MAP_PAGES_REQUEST Request;
    LOS_X64_MAP_PAGES_RESULT Result;

    if (PageMapLevel4PhysicalAddress == 0ULL ||
        VirtualAddress == 0ULL ||
        PhysicalAddress == 0ULL ||
        PageCount == 0ULL ||
        PageFlags == 0ULL)
    {
        return 0;
    }

    ZeroMemory(&Request, sizeof(Request));
    ZeroMemory(&Result, sizeof(Result));
    Request.PageMapLevel4PhysicalAddress = PageMapLevel4PhysicalAddress;
    Request.VirtualAddress = VirtualAddress;
    Request.PhysicalAddress = PhysicalAddress;
    Request.PageCount = PageCount;
    Request.PageFlags = PageFlags;
    Request.Flags = 0U;
    LosX64MapPages(&Request, &Result);
    return Result.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS && Result.PagesProcessed == PageCount;
}

static BOOLEAN CanReserveRegionLocal(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 BaseVirtualAddress,
    UINT64 PageCount)
{
    UINT32 ScanIndex;
    UINT64 EndVirtualAddress;

    if (AddressSpaceObject == 0 ||
        PageCount == 0ULL ||
        (BaseVirtualAddress & 0xFFFULL) != 0ULL ||
        BaseVirtualAddress >= LOS_MEMORY_MANAGER_BOOTSTRAP_LOCAL_LOW_HALF_LIMIT)
    {
        return 0;
    }

    EndVirtualAddress = BaseVirtualAddress + (PageCount * 0x1000ULL);
    if (EndVirtualAddress <= BaseVirtualAddress ||
        EndVirtualAddress > LOS_MEMORY_MANAGER_BOOTSTRAP_LOCAL_LOW_HALF_LIMIT ||
        AddressSpaceObject->ReservedVirtualRegionCount >= LOS_MEMORY_MANAGER_MAX_RESERVED_VIRTUAL_REGIONS)
    {
        return 0;
    }

    for (ScanIndex = 0U; ScanIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++ScanIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Current;

        Current = &AddressSpaceObject->ReservedVirtualRegions[ScanIndex];
        if (RangesOverlap(BaseVirtualAddress, PageCount, Current->BaseVirtualAddress, Current->PageCount))
        {
            return 0;
        }
    }

    return 1;
}

static BOOLEAN ReserveVirtualRegionLocal(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 BaseVirtualAddress,
    UINT64 PageCount,
    UINT32 Type,
    UINT32 Flags,
    UINT64 BackingPhysicalAddress)
{
    UINT32 InsertIndex;
    UINT32 ScanIndex;

    if (!CanReserveRegionLocal(AddressSpaceObject, BaseVirtualAddress, PageCount))
    {
        return 0;
    }

    InsertIndex = AddressSpaceObject->ReservedVirtualRegionCount;
    for (ScanIndex = 0U; ScanIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++ScanIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Current;

        Current = &AddressSpaceObject->ReservedVirtualRegions[ScanIndex];
        if (BaseVirtualAddress < Current->BaseVirtualAddress)
        {
            InsertIndex = ScanIndex;
            break;
        }
    }

    for (ScanIndex = AddressSpaceObject->ReservedVirtualRegionCount; ScanIndex > InsertIndex; --ScanIndex)
    {
        AddressSpaceObject->ReservedVirtualRegions[ScanIndex] = AddressSpaceObject->ReservedVirtualRegions[ScanIndex - 1U];
    }

    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].BaseVirtualAddress = BaseVirtualAddress;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].PageCount = PageCount;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].Type = Type;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].Flags = Flags;
    AddressSpaceObject->ReservedVirtualRegions[InsertIndex].BackingPhysicalAddress = BackingPhysicalAddress;
    AddressSpaceObject->ReservedVirtualRegionCount += 1U;
    return 1;
}

static BOOLEAN SelectStackBaseVirtualAddressLocal(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 DesiredBaseVirtualAddress,
    UINT64 StackPageCount,
    UINT64 *StackBaseVirtualAddress)
{
    UINT64 CandidateBase;
    UINT32 ScanIndex;

    if (StackBaseVirtualAddress != 0)
    {
        *StackBaseVirtualAddress = 0ULL;
    }
    if (AddressSpaceObject == 0 || StackBaseVirtualAddress == 0 || StackPageCount == 0ULL)
    {
        return 0;
    }

    CandidateBase = DesiredBaseVirtualAddress != 0ULL ? DesiredBaseVirtualAddress : LOS_MEMORY_MANAGER_BOOTSTRAP_LOCAL_STACK_BASE;
    CandidateBase = AlignDownToPage(CandidateBase);

    if (DesiredBaseVirtualAddress != 0ULL)
    {
        if (!CanReserveRegionLocal(AddressSpaceObject, CandidateBase, StackPageCount))
        {
            return 0;
        }
        *StackBaseVirtualAddress = CandidateBase;
        return 1;
    }

    for (ScanIndex = 0U; ScanIndex < AddressSpaceObject->ReservedVirtualRegionCount; ++ScanIndex)
    {
        const LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION *Current;

        Current = &AddressSpaceObject->ReservedVirtualRegions[ScanIndex];
        while (RangesOverlap(CandidateBase, StackPageCount, Current->BaseVirtualAddress, Current->PageCount))
        {
            UINT64 NextCandidate;

            NextCandidate = AlignUpToPage(
                Current->BaseVirtualAddress + (Current->PageCount * 0x1000ULL) + LOS_MEMORY_MANAGER_BOOTSTRAP_LOCAL_STACK_GAP_BYTES);
            if (NextCandidate <= CandidateBase)
            {
                return 0;
            }
            CandidateBase = NextCandidate;
        }
    }

    if (!CanReserveRegionLocal(AddressSpaceObject, CandidateBase, StackPageCount))
    {
        return 0;
    }

    *StackBaseVirtualAddress = CandidateBase;
    return 1;
}

static BOOLEAN ValidateElfHeaderLocal(
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
    AddressSpaceObject->EntryVirtualAddress = Header->Entry;
    AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_IMAGE;

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->ImagePhysicalAddress = ImagePhysicalBase;
    Result->ImageSize = ImageMappedBytes;
    Result->ImageVirtualBase = ImageVirtualBase;
    Result->EntryVirtualAddress = Header->Entry;
    Result->ImagePageCount = ImagePageCount;
    if (Status != 0)
    {
        *Status = Result->Status;
    }
}

static void DispatchLocalAllocateAddressSpaceStack(
    const LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_REQUEST *Request,
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result,
    UINT32 *Status)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
    UINT64 StackBaseVirtualAddress;
    UINT64 StackPhysicalAddress;

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
        Request->PageCount == 0ULL)
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
    if ((AddressSpaceObject->Flags & LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK) != 0ULL)
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    StackBaseVirtualAddress = 0ULL;
    if (!SelectStackBaseVirtualAddressLocal(
            AddressSpaceObject,
            Request->DesiredStackBaseVirtualAddress,
            Request->PageCount,
            &StackBaseVirtualAddress))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    StackPhysicalAddress = 0ULL;
    if (!ClaimContiguousPagesLocal(Request->PageCount, &StackPhysicalAddress) ||
        !MapPagesIntoAddressSpaceLocal(
            AddressSpaceObject->RootTablePhysicalAddress,
            StackBaseVirtualAddress,
            StackPhysicalAddress,
            Request->PageCount,
            LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_USER | LOS_X64_PAGE_NX) ||
        !ReserveVirtualRegionLocal(
            AddressSpaceObject,
            StackBaseVirtualAddress,
            Request->PageCount,
            LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION_TYPE_STACK,
            0U,
            StackPhysicalAddress))
    {
        Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        if (Status != 0)
        {
            *Status = Result->Status;
        }
        return;
    }

    AddressSpaceObject->StackPhysicalAddress = StackPhysicalAddress;
    AddressSpaceObject->StackPageCount = Request->PageCount;
    AddressSpaceObject->StackBaseVirtualAddress = StackBaseVirtualAddress;
    AddressSpaceObject->StackTopVirtualAddress = StackBaseVirtualAddress + (Request->PageCount * 0x1000ULL);
    AddressSpaceObject->Flags |= LOS_MEMORY_MANAGER_ADDRESS_SPACE_FLAG_HAS_STACK;

    Result->Status = LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS;
    Result->StackPhysicalAddress = StackPhysicalAddress;
    Result->StackPageCount = Request->PageCount;
    Result->StackBaseVirtualAddress = StackBaseVirtualAddress;
    Result->StackTopVirtualAddress = AddressSpaceObject->StackTopVirtualAddress;
    if (Status != 0)
    {
        *Status = Result->Status;
    }
}

static BOOLEAN EndpointReady(const LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *Endpoint, UINT64 EndpointId, UINT32 Role)
{
    if (Endpoint == 0)
    {
        return 0;
    }

    return Endpoint->EndpointId == EndpointId &&
           Endpoint->Role == Role &&
           Endpoint->State >= LOS_MEMORY_MANAGER_ENDPOINT_STATE_READY &&
           (Endpoint->Flags & LOS_MEMORY_MANAGER_ENDPOINT_FLAG_MAILBOX_ATTACHED) != 0ULL;
}

static void InitializeResponse(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    if (Response == 0)
    {
        return;
    }

    ZeroMemory(Response, sizeof(*Response));
    if (Request != 0)
    {
        Response->Operation = Request->Operation;
        Response->RequestId = Request->RequestId;
    }
}

static BOOLEAN PostResponse(const LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *Mailbox;
    UINT64 Index;
    LOS_MEMORY_MANAGER_RESPONSE_SLOT *Slot;

    Mailbox = LosMemoryManagerBootstrapGetResponseMailbox();
    if (!EndpointReady(LosMemoryManagerBootstrapGetServiceToKernelEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_SERVICE_TO_KERNEL, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_REPLY) ||
        Mailbox == 0 || Response == 0)
    {
        return 0;
    }

    Index = Mailbox->Header.ProduceIndex % Mailbox->Header.SlotCount;
    Slot = &Mailbox->Slots[Index];
    if (Slot->SlotState == LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    CopyBytes(&Slot->Message, Response, sizeof(*Response));
    Slot->Sequence = Response->RequestId;
    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY;
    Mailbox->Header.ProduceIndex += 1ULL;
    return 1;
}

BOOLEAN LosMemoryManagerBootstrapEnqueueRequest(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request)
{
    LOS_MEMORY_MANAGER_REQUEST_MAILBOX *Mailbox;
    UINT64 Index;
    LOS_MEMORY_MANAGER_REQUEST_SLOT *Slot;

    Mailbox = LosMemoryManagerBootstrapGetRequestMailbox();
    if (!EndpointReady(LosMemoryManagerBootstrapGetKernelToServiceEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_KERNEL_TO_SERVICE, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_RECEIVE) ||
        Mailbox == 0 || Request == 0)
    {
        return 0;
    }

    Index = Mailbox->Header.ProduceIndex % Mailbox->Header.SlotCount;
    Slot = &Mailbox->Slots[Index];
    if (Slot->SlotState == LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    CopyBytes(&Slot->Message, Request, sizeof(*Request));
    Slot->Sequence = Request->RequestId;
    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY;
    Mailbox->Header.ProduceIndex += 1ULL;
    return 1;
}


BOOLEAN LosMemoryManagerBootstrapDequeueResponse(UINT64 RequestId, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *Mailbox;
    UINT64 Scanned;

    Mailbox = LosMemoryManagerBootstrapGetResponseMailbox();
    if (!EndpointReady(LosMemoryManagerBootstrapGetServiceToKernelEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_SERVICE_TO_KERNEL, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_REPLY) ||
        Mailbox == 0 || Response == 0)
    {
        return 0;
    }

    for (Scanned = 0ULL; Scanned < Mailbox->Header.SlotCount; ++Scanned)
    {
        UINT64 Index;
        LOS_MEMORY_MANAGER_RESPONSE_SLOT *Slot;

        Index = (Mailbox->Header.ConsumeIndex + Scanned) % Mailbox->Header.SlotCount;
        Slot = &Mailbox->Slots[Index];
        if (Slot->SlotState != LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
        {
            continue;
        }
        if (Slot->Sequence != RequestId)
        {
            continue;
        }

        CopyBytes(Response, &Slot->Message, sizeof(*Response));
        Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
        Slot->Sequence = 0ULL;
        Mailbox->Header.ConsumeIndex = Index + 1ULL;
        return 1;
    }

    return 0;
}

static BOOLEAN HostedServiceTaskReady(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    UINT32 TaskState;

    State = LosMemoryManagerBootstrapState();
    TaskState = State->ServiceTaskObject != 0 ? State->ServiceTaskObject->State : LOS_MEMORY_MANAGER_TASK_STATE_OFFLINE;
    return State->ServiceTaskObject != 0 &&
           State->ServiceAddressSpaceObject != 0 &&
           TaskState >= LOS_MEMORY_MANAGER_TASK_STATE_READY &&
           TaskState != LOS_MEMORY_MANAGER_TASK_STATE_RUNNING &&
           State->ServiceAddressSpaceObject->State >= LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_READY;
}

BOOLEAN LosMemoryManagerBootstrapHostedServiceStep(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    LOS_MEMORY_MANAGER_REQUEST_MAILBOX *RequestMailbox;
    LOS_MEMORY_MANAGER_REQUEST_SLOT *Slot;

    State = LosMemoryManagerBootstrapState();
    if (!HostedServiceTaskReady())
    {
        return 0;
    }

    RequestMailbox = LosMemoryManagerBootstrapGetRequestMailbox();
    if (RequestMailbox == 0)
    {
        return 0;
    }

    Slot = &RequestMailbox->Slots[(RequestMailbox->Header.ConsumeIndex) % RequestMailbox->Header.SlotCount];
    if (Slot->SlotState != LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        return 0;
    }

    State->ServiceTaskObject->LastRequestId = 0ULL;
    State->ServiceTaskObject->Heartbeat = 0ULL;
    State->KernelToServiceEndpointObject->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->ServiceToKernelEndpointObject->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->ServiceEventsEndpointObject->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;

    if (!LosMemoryManagerBootstrapInvokeServiceEntry())
    {
        return 0;
    }

    LosMemoryManagerBootstrapTransitionTo(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_SERVICE_ONLINE);
    State->Info.Flags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_ONLINE;
    return 1;
}

void LosMemoryManagerBootstrapDispatch(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    LOS_MEMORY_MANAGER_REQUEST_MAILBOX *RequestMailbox;
    LOS_MEMORY_MANAGER_REQUEST_SLOT *Slot;

    if (Request == 0 || Response == 0)
    {
        return;
    }

    InitializeResponse(Request, Response);

    RequestMailbox = LosMemoryManagerBootstrapGetRequestMailbox();
    if (!LosMemoryManagerBootstrapEndpointsReady() || RequestMailbox == 0)
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        return;
    }

    Slot = &RequestMailbox->Slots[(RequestMailbox->Header.ConsumeIndex) % RequestMailbox->Header.SlotCount];
    if (Slot->SlotState != LOS_MEMORY_MANAGER_MAILBOX_SLOT_READY)
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
        return;
    }

    if (!LosMemoryManagerBootstrapOperationSupported(Slot->Message.Operation))
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
        Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
        Slot->Sequence = 0ULL;
        RequestMailbox->Header.ConsumeIndex += 1ULL;
        PostResponse(Response);
        return;
    }

    switch (Slot->Message.Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.BootstrapAttach.BootstrapResult = LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_INVALID_REQUEST;
            Response->Payload.BootstrapAttach.BootstrapState = LosGetMemoryManagerBootstrapInfo()->State;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES:
        {
            LOS_X64_CLAIM_FRAMES_REQUEST ClaimRequest;
            LOS_X64_CLAIM_FRAMES_RESULT ClaimResult;

            ZeroMemory(&ClaimRequest, sizeof(ClaimRequest));
            ClaimRequest.DesiredPhysicalAddress = Slot->Message.Payload.AllocateFrames.DesiredPhysicalAddress;
            ClaimRequest.MinimumPhysicalAddress = Slot->Message.Payload.AllocateFrames.MinimumPhysicalAddress;
            ClaimRequest.MaximumPhysicalAddress = Slot->Message.Payload.AllocateFrames.MaximumPhysicalAddress;
            ClaimRequest.AlignmentBytes = Slot->Message.Payload.AllocateFrames.AlignmentBytes;
            ClaimRequest.PageCount = Slot->Message.Payload.AllocateFrames.PageCount;
            ClaimRequest.Flags = Slot->Message.Payload.AllocateFrames.Flags;
            ClaimRequest.Owner = Slot->Message.Payload.AllocateFrames.Owner;
            LosX64ClaimFrames(&ClaimRequest, &ClaimResult);
            Response->Payload.AllocateFrames.Status = ClaimResult.Status;
            Response->Payload.AllocateFrames.BaseAddress = ClaimResult.BaseAddress;
            Response->Payload.AllocateFrames.PageCount = ClaimResult.PageCount;
            Response->Status = Response->Payload.AllocateFrames.Status;
            break;
        }
        case LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.FreeFrames.Status = Response->Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES:
        {
            LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
            LOS_X64_MAP_PAGES_REQUEST MapRequest;
            LOS_X64_MAP_PAGES_RESULT MapResult;

            ZeroMemory(&MapRequest, sizeof(MapRequest));
            AddressSpaceObject = ResolveAddressSpaceObjectLocal(Slot->Message.Payload.MapPages.AddressSpaceObjectPhysicalAddress);
            if (AddressSpaceObject == 0)
            {
                Response->Payload.MapPages.Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
                Response->Payload.MapPages.AddressSpaceObjectPhysicalAddress = Slot->Message.Payload.MapPages.AddressSpaceObjectPhysicalAddress;
                Response->Status = Response->Payload.MapPages.Status;
                break;
            }

            MapRequest.PageMapLevel4PhysicalAddress = AddressSpaceObject->RootTablePhysicalAddress;
            MapRequest.VirtualAddress = Slot->Message.Payload.MapPages.VirtualAddress;
            MapRequest.PhysicalAddress = Slot->Message.Payload.MapPages.PhysicalAddress;
            MapRequest.PageCount = Slot->Message.Payload.MapPages.PageCount;
            MapRequest.PageFlags = Slot->Message.Payload.MapPages.PageFlags;
            MapRequest.Flags = Slot->Message.Payload.MapPages.Flags;
            LosX64MapPages(&MapRequest, &MapResult);
            Response->Payload.MapPages.Status = MapResult.Status;
            Response->Payload.MapPages.AddressSpaceObjectPhysicalAddress = Slot->Message.Payload.MapPages.AddressSpaceObjectPhysicalAddress;
            Response->Payload.MapPages.PagesProcessed = MapResult.PagesProcessed;
            Response->Payload.MapPages.LastVirtualAddress = MapResult.LastVirtualAddress;
            Response->Status = Response->Payload.MapPages.Status;
            break;
        }
        case LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES:
        {
            LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
            LOS_X64_UNMAP_PAGES_REQUEST UnmapRequest;
            LOS_X64_UNMAP_PAGES_RESULT UnmapResult;

            ZeroMemory(&UnmapRequest, sizeof(UnmapRequest));
            AddressSpaceObject = ResolveAddressSpaceObjectLocal(Slot->Message.Payload.UnmapPages.AddressSpaceObjectPhysicalAddress);
            if (AddressSpaceObject == 0)
            {
                Response->Payload.UnmapPages.Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
                Response->Payload.UnmapPages.AddressSpaceObjectPhysicalAddress = Slot->Message.Payload.UnmapPages.AddressSpaceObjectPhysicalAddress;
                Response->Status = Response->Payload.UnmapPages.Status;
                break;
            }

            UnmapRequest.PageMapLevel4PhysicalAddress = AddressSpaceObject->RootTablePhysicalAddress;
            UnmapRequest.VirtualAddress = Slot->Message.Payload.UnmapPages.VirtualAddress;
            UnmapRequest.PageCount = Slot->Message.Payload.UnmapPages.PageCount;
            UnmapRequest.Flags = Slot->Message.Payload.UnmapPages.Flags;
            LosX64UnmapPages(&UnmapRequest, &UnmapResult);
            Response->Payload.UnmapPages.Status = UnmapResult.Status;
            Response->Payload.UnmapPages.AddressSpaceObjectPhysicalAddress = Slot->Message.Payload.UnmapPages.AddressSpaceObjectPhysicalAddress;
            Response->Payload.UnmapPages.PagesProcessed = UnmapResult.PagesProcessed;
            Response->Payload.UnmapPages.LastVirtualAddress = UnmapResult.LastVirtualAddress;
            Response->Status = Response->Payload.UnmapPages.Status;
            break;
        }
        case LOS_MEMORY_MANAGER_OPERATION_PROTECT_PAGES:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.ProtectPages.Status = Response->Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MAPPING:
            DispatchLocalQueryMapping(&Slot->Message.Payload.QueryMapping, &Response->Payload.QueryMapping, &Response->Status);
            break;
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
            LosX64QueryMemoryRegions(
                Slot->Message.Payload.QueryMemoryRegions.Buffer,
                Slot->Message.Payload.QueryMemoryRegions.BufferRegionCapacity,
                &Response->Payload.QueryMemoryRegions);
            Response->Status = Response->Payload.QueryMemoryRegions.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
            LosX64ReserveFrames(&Slot->Message.Payload.ReserveFrames, &Response->Payload.ReserveFrames);
            Response->Status = Response->Payload.ReserveFrames.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            LosX64ClaimFrames(&Slot->Message.Payload.ClaimFrames, &Response->Payload.ClaimFrames);
            Response->Status = Response->Payload.ClaimFrames.Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.CreateAddressSpace.Status = Response->Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            Response->Payload.DestroyAddressSpace.Status = Response->Status;
            break;
        case LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE:
            DispatchLocalAttachStagedImage(&Slot->Message.Payload.AttachStagedImage, &Response->Payload.AttachStagedImage, &Response->Status);
            break;
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK:
            DispatchLocalAllocateAddressSpaceStack(&Slot->Message.Payload.AllocateAddressSpaceStack, &Response->Payload.AllocateAddressSpaceStack, &Response->Status);
            break;
        default:
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_SUPPORTED;
            break;
    }

    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_COMPLETE;
    RequestMailbox->Header.ConsumeIndex += 1ULL;
    PostResponse(Response);
    Slot->SlotState = LOS_MEMORY_MANAGER_MAILBOX_SLOT_FREE;
    Slot->Sequence = 0ULL;
}

static BOOLEAN RequestRequiresRealServiceReply(UINT32 Operation)
{
    switch (Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH:
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE:
        case LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE:
        case LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES:
        case LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES:
        case LOS_MEMORY_MANAGER_OPERATION_PROTECT_PAGES:
        case LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE:
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK:
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            return 1;
        default:
            return 0;
    }
}

static BOOLEAN RequestMayUseBootstrapFallback(UINT32 Operation)
{
    (void)Operation;
    return 0;
}

static UINT32 HostedServiceStepBudgetForOperation(UINT32 Operation)
{
    return RequestRequiresRealServiceReply(Operation) ? 8U : 1U;
}

static void PopulateBootstrapAttachRequest(LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    const LOS_MEMORY_MANAGER_BOOTSTRAP_INFO *Info;
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    UINT64 ServiceRootPhysicalAddress;
    UINT64 ServiceEntryVirtualAddress;
    UINT64 ServiceStackTopVirtualAddress;

    if (Request == 0)
    {
        return;
    }

    State = LosMemoryManagerBootstrapState();
    Info = LosGetMemoryManagerBootstrapInfo();
    LaunchBlock = LosGetMemoryManagerLaunchBlock();
    ServiceRootPhysicalAddress = 0ULL;
    ServiceEntryVirtualAddress = 0ULL;
    ServiceStackTopVirtualAddress = 0ULL;
    Request->Operation = LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH;
    Request->Payload.BootstrapAttach.Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    Request->Payload.BootstrapAttach.LaunchBlockPhysicalAddress = Info->LaunchBlockPhysicalAddress;
    Request->Payload.BootstrapAttach.OfferedOperations = Info->SupportedOperations;
    Request->Payload.BootstrapAttach.BootstrapFlags = Info->Flags;
    Request->Payload.BootstrapAttach.KernelToServiceEndpointId = Info->Endpoints.KernelToService;
    Request->Payload.BootstrapAttach.ServiceToKernelEndpointId = Info->Endpoints.ServiceToKernel;
    Request->Payload.BootstrapAttach.ServiceEventsEndpointId = Info->Endpoints.ServiceEvents;
    Request->Payload.BootstrapAttach.RequestMailboxPhysicalAddress = Info->RequestMailboxPhysicalAddress;
    Request->Payload.BootstrapAttach.ResponseMailboxPhysicalAddress = Info->ResponseMailboxPhysicalAddress;
    Request->Payload.BootstrapAttach.EventMailboxPhysicalAddress = Info->EventMailboxPhysicalAddress;
    Request->Payload.BootstrapAttach.MemoryRegionTablePhysicalAddress = Info->MemoryRegionTablePhysicalAddress;
    Request->Payload.BootstrapAttach.MemoryRegionCount = Info->MemoryRegionCount;
    Request->Payload.BootstrapAttach.MemoryRegionEntrySize = Info->MemoryRegionEntrySize;
    ServiceRootPhysicalAddress = Info->ServicePageMapLevel4PhysicalAddress;
    Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress = Info->ServiceImagePhysicalAddress;
    Request->Payload.BootstrapAttach.ServiceImageSize = Info->ServiceImageSize;
    ServiceEntryVirtualAddress = Info->ServiceEntryVirtualAddress;

    if (LaunchBlock != 0)
    {
        Request->Payload.BootstrapAttach.LaunchBlockPhysicalAddress = LaunchBlock->LaunchBlockPhysicalAddress;
        Request->Payload.BootstrapAttach.KernelToServiceEndpointId = LaunchBlock->Endpoints.KernelToService;
        Request->Payload.BootstrapAttach.ServiceToKernelEndpointId = LaunchBlock->Endpoints.ServiceToKernel;
        Request->Payload.BootstrapAttach.ServiceEventsEndpointId = LaunchBlock->Endpoints.ServiceEvents;
        Request->Payload.BootstrapAttach.RequestMailboxPhysicalAddress = LaunchBlock->RequestMailboxPhysicalAddress;
        Request->Payload.BootstrapAttach.ResponseMailboxPhysicalAddress = LaunchBlock->ResponseMailboxPhysicalAddress;
        Request->Payload.BootstrapAttach.EventMailboxPhysicalAddress = LaunchBlock->EventMailboxPhysicalAddress;
        Request->Payload.BootstrapAttach.MemoryRegionTablePhysicalAddress = LaunchBlock->MemoryRegionTablePhysicalAddress;
        Request->Payload.BootstrapAttach.MemoryRegionCount = LaunchBlock->MemoryRegionCount;
        Request->Payload.BootstrapAttach.MemoryRegionEntrySize = LaunchBlock->MemoryRegionEntrySize;
        if (LaunchBlock->ServicePageMapLevel4PhysicalAddress != 0ULL)
        {
            ServiceRootPhysicalAddress = LaunchBlock->ServicePageMapLevel4PhysicalAddress;
        }
        Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress = LaunchBlock->ServiceImagePhysicalAddress;
        Request->Payload.BootstrapAttach.ServiceImageSize = LaunchBlock->ServiceImageSize;
        if (LaunchBlock->ServiceEntryVirtualAddress != 0ULL)
        {
            ServiceEntryVirtualAddress = LaunchBlock->ServiceEntryVirtualAddress;
        }
        if (LaunchBlock->ServiceStackTopVirtualAddress != 0ULL)
        {
            ServiceStackTopVirtualAddress = LaunchBlock->ServiceStackTopVirtualAddress;
        }
    }

    if (State != 0)
    {
        if (State->ServiceAddressSpaceObject != 0 && State->ServiceAddressSpaceObject->RootTablePhysicalAddress != 0ULL)
        {
            ServiceRootPhysicalAddress = State->ServiceAddressSpaceObject->RootTablePhysicalAddress;
        }
        if (State->ServiceTaskObject != 0)
        {
            if (State->ServiceTaskObject->EntryVirtualAddress != 0ULL)
            {
                ServiceEntryVirtualAddress = State->ServiceTaskObject->EntryVirtualAddress;
            }
            if (State->ServiceTaskObject->StackTopVirtualAddress != 0ULL)
            {
                ServiceStackTopVirtualAddress = State->ServiceTaskObject->StackTopVirtualAddress;
            }
        }

        if (ServiceRootPhysicalAddress != 0ULL)
        {
            State->Info.ServicePageMapLevel4PhysicalAddress = ServiceRootPhysicalAddress;
            if (State->LaunchBlock != 0)
            {
                State->LaunchBlock->ServicePageMapLevel4PhysicalAddress = ServiceRootPhysicalAddress;
            }
            if (State->ServiceAddressSpaceObject != 0)
            {
                State->ServiceAddressSpaceObject->RootTablePhysicalAddress = ServiceRootPhysicalAddress;
            }
        }
        if (ServiceEntryVirtualAddress != 0ULL)
        {
            State->Info.ServiceEntryVirtualAddress = ServiceEntryVirtualAddress;
            if (State->LaunchBlock != 0)
            {
                State->LaunchBlock->ServiceEntryVirtualAddress = ServiceEntryVirtualAddress;
            }
            if (State->ServiceTaskObject != 0)
            {
                State->ServiceTaskObject->EntryVirtualAddress = ServiceEntryVirtualAddress;
            }
        }
        if (ServiceStackTopVirtualAddress != 0ULL)
        {
            if (State->LaunchBlock != 0)
            {
                State->LaunchBlock->ServiceStackTopVirtualAddress = ServiceStackTopVirtualAddress;
            }
            if (State->ServiceTaskObject != 0)
            {
                State->ServiceTaskObject->StackTopVirtualAddress = ServiceStackTopVirtualAddress;
            }
        }
    }

    if (State != 0)
    {
        if (State->ServiceAddressSpaceObject != 0 && State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress != 0ULL)
        {
            State->Info.ServiceImagePhysicalAddress = State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress;
            if (State->LaunchBlock != 0)
            {
                State->LaunchBlock->ServiceImagePhysicalAddress = State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress;
            }
        }
        if (State->LaunchBlock != 0)
        {
            if (State->ServiceAddressSpaceObject != 0 && State->ServiceAddressSpaceObject->ServiceImageSize != 0ULL)
            {
                State->LaunchBlock->ServiceImageSize = State->ServiceAddressSpaceObject->ServiceImageSize;
            }
            else if (State->Info.ServiceImageSize != 0ULL && State->LaunchBlock->ServiceImageSize == 0ULL)
            {
                State->LaunchBlock->ServiceImageSize = State->Info.ServiceImageSize;
            }
        }
    }

    Request->Payload.BootstrapAttach.ServicePageMapLevel4PhysicalAddress = ServiceRootPhysicalAddress;
    if (State != 0 && State->ServiceAddressSpaceObject != 0 && State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress != 0ULL)
    {
        Request->Payload.BootstrapAttach.ServiceImagePhysicalAddress = State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress;
    }
    if (State != 0)
    {
        if (State->ServiceAddressSpaceObject != 0 && State->ServiceAddressSpaceObject->ServiceImageSize != 0ULL)
        {
            Request->Payload.BootstrapAttach.ServiceImageSize = State->ServiceAddressSpaceObject->ServiceImageSize;
        }
        else if (State->Info.ServiceImageSize != 0ULL)
        {
            Request->Payload.BootstrapAttach.ServiceImageSize = State->Info.ServiceImageSize;
        }
    }
    Request->Payload.BootstrapAttach.ServiceEntryVirtualAddress = ServiceEntryVirtualAddress;
    Request->Payload.BootstrapAttach.ServiceStackTopVirtualAddress = ServiceStackTopVirtualAddress;
}

static void SendRequestAndAwaitResponse(LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response)
{
    BOOLEAN ResponsePublishedByService;
    UINT32 Attempt;
    UINT32 AttemptBudget;
    UINT64 InterruptFlags;

    InitializeResponse(Request, Response);
    PopulateCallerIdentity(Request);

    TraceKernelToMemoryManagerRequest(Request);
    InterruptFlags = SaveInterruptFlagsAndDisable();
    if (!LosMemoryManagerBootstrapEnqueueRequest(Request))
    {
        Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
        RestoreInterruptFlags(InterruptFlags);
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap failed to enqueue a request.");
    }

    ResponsePublishedByService = 0;
    AttemptBudget = HostedServiceStepBudgetForOperation(Request->Operation);
    for (Attempt = 0U; Attempt < AttemptBudget; ++Attempt)
    {
        if (!LosMemoryManagerBootstrapHostedServiceStep())
        {
            if (Attempt == 0U && !RequestMayUseBootstrapFallback(Request->Operation) &&
                RequestRequiresRealServiceReply(Request->Operation))
            {
                Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NO_RESOURCES;
                RestoreInterruptFlags(InterruptFlags);
                LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager hosted service step failed.");
            }

            continue;
        }

        ResponsePublishedByService = LosMemoryManagerBootstrapDequeueResponse(Request->RequestId, Response);
        if (ResponsePublishedByService)
        {
            break;
        }
    }

    if (!ResponsePublishedByService)
    {
        if (!RequestMayUseBootstrapFallback(Request->Operation) && RequestRequiresRealServiceReply(Request->Operation))
        {
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
            RestoreInterruptFlags(InterruptFlags);
            LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap expected a real service reply and none was published.");
        }

        LosMemoryManagerBootstrapDispatch(Request, Response);
        if (!LosMemoryManagerBootstrapDequeueResponse(Request->RequestId, Response))
        {
            Response->Status = LOS_X64_MEMORY_OPERATION_STATUS_NOT_FOUND;
            RestoreInterruptFlags(InterruptFlags);
            LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap failed to dequeue a response.");
        }
    }

    RestoreInterruptFlags(InterruptFlags);
    LosMemoryManagerBootstrapRecordCompletion(Response->Operation, Response->Status);
    TraceMemoryManagerToKernelResponse(Response);
}

void LosMemoryManagerBootstrapPublishLaunchReady(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;

    State = LosMemoryManagerBootstrapState();
    if (State->LaunchBlock == 0 || State->Info.ServiceEntryVirtualAddress == 0ULL)
    {
        return;
    }

    State->LaunchBlock->Flags = State->Info.Flags | LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_ENTRY_PUBLISHED | LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_LAUNCH_PREPARED;
    State->LaunchBlock->ServiceEntryVirtualAddress = State->Info.ServiceEntryVirtualAddress;
    LosMemoryManagerBootstrapSetEndpointState(LosMemoryManagerBootstrapGetKernelToServiceEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_STATE_BOUND);
    LosMemoryManagerBootstrapSetEndpointState(LosMemoryManagerBootstrapGetServiceToKernelEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_STATE_BOUND);
    LosMemoryManagerBootstrapSetEndpointState(LosMemoryManagerBootstrapGetServiceEventsEndpointObject(), LOS_MEMORY_MANAGER_ENDPOINT_STATE_BOUND);
    State->Info.Flags = State->LaunchBlock->Flags;
    LosMemoryManagerBootstrapTransitionTo(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_SERVICE_PREPARED);
}

void LosMemoryManagerSendBootstrapAttach(LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT *Result)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    State = LosMemoryManagerBootstrapState();
    if (State->BootstrapAttachRequestId != 0ULL)
    {
        LosMemoryManagerBootstrapReportFailureAndHalt("Memory-manager bootstrap attach request was attempted more than once.");
    }

    ZeroMemory(&Request, sizeof(Request));
    PopulateBootstrapAttachRequest(&Request);
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    State->BootstrapAttachRequestId = Request.RequestId;
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    LosMemoryManagerBootstrapRecordBootstrapResult(Response.Payload.BootstrapAttach.BootstrapResult);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.BootstrapAttach, sizeof(*Result));
    }
}

void LosMemoryManagerSendQueryMemoryRegions(LOS_X64_MEMORY_REGION *Buffer, UINTN BufferRegionCapacity, LOS_X64_QUERY_MEMORY_REGIONS_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    Request.Payload.QueryMemoryRegions.Buffer = Buffer;
    Request.Payload.QueryMemoryRegions.BufferRegionCapacity = BufferRegionCapacity;
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.QueryMemoryRegions, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendReserveFrames(const LOS_X64_RESERVE_FRAMES_REQUEST *RequestData, LOS_X64_RESERVE_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.ReserveFrames, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.ReserveFrames, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendAllocateFrames(const LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_REQUEST *RequestData, LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.AllocateFrames, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.AllocateFrames, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendClaimFrames(const LOS_X64_CLAIM_FRAMES_REQUEST *RequestData, LOS_X64_CLAIM_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_REQUEST AllocateRequest;
    LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_RESULT AllocateResult;

    ZeroMemory(&AllocateRequest, sizeof(AllocateRequest));
    ZeroMemory(&AllocateResult, sizeof(AllocateResult));
    if (RequestData != 0)
    {
        AllocateRequest.DesiredPhysicalAddress = RequestData->DesiredPhysicalAddress;
        AllocateRequest.MinimumPhysicalAddress = RequestData->MinimumPhysicalAddress;
        AllocateRequest.MaximumPhysicalAddress = RequestData->MaximumPhysicalAddress;
        AllocateRequest.AlignmentBytes = RequestData->AlignmentBytes;
        AllocateRequest.PageCount = RequestData->PageCount;
        AllocateRequest.Flags = RequestData->Flags;
        AllocateRequest.Owner = RequestData->Owner;
    }
    LosMemoryManagerSendAllocateFrames(&AllocateRequest, &AllocateResult);
    if (Result != 0)
    {
        Result->Status = AllocateResult.Status;
        Result->Reserved = 0U;
        Result->BaseAddress = AllocateResult.BaseAddress;
        Result->PageCount = AllocateResult.PageCount;
    }
}

void LosMemoryManagerSendFreeFrames(const LOS_X64_FREE_FRAMES_REQUEST *RequestData, LOS_X64_FREE_FRAMES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.FreeFrames, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.FreeFrames, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendMapPages(const LOS_MEMORY_MANAGER_MAP_PAGES_REQUEST *RequestData, LOS_MEMORY_MANAGER_MAP_PAGES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.MapPages, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.MapPages, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendUnmapPages(const LOS_MEMORY_MANAGER_UNMAP_PAGES_REQUEST *RequestData, LOS_MEMORY_MANAGER_UNMAP_PAGES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.UnmapPages, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.UnmapPages, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendProtectPages(const LOS_MEMORY_MANAGER_PROTECT_PAGES_REQUEST *RequestData, LOS_MEMORY_MANAGER_PROTECT_PAGES_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_PROTECT_PAGES;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.ProtectPages, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.ProtectPages, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendQueryMapping(const LOS_MEMORY_MANAGER_QUERY_MAPPING_REQUEST *RequestData, LOS_MEMORY_MANAGER_QUERY_MAPPING_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_QUERY_MAPPING;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.QueryMapping, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.QueryMapping, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendCreateAddressSpace(const LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_REQUEST *RequestData, LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.CreateAddressSpace, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.CreateAddressSpace, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendDestroyAddressSpace(const LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_REQUEST *RequestData, LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.DestroyAddressSpace, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.DestroyAddressSpace, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendAttachStagedImage(const LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_REQUEST *RequestData, LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.AttachStagedImage, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.AttachStagedImage, sizeof(*Result));
        Result->Status = Response.Status;
    }
}

void LosMemoryManagerSendAllocateAddressSpaceStack(const LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_REQUEST *RequestData, LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result)
{
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Request;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Response;

    ZeroMemory(&Request, sizeof(Request));
    Request.Operation = LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK;
    Request.RequestId = LosMemoryManagerBootstrapAllocateRequestId();
    if (RequestData != 0)
    {
        CopyBytes(&Request.Payload.AllocateAddressSpaceStack, RequestData, sizeof(*RequestData));
    }
    LosMemoryManagerBootstrapRecordRequest(Request.Operation);
    SendRequestAndAwaitResponse(&Request, &Response);
    if (Result != 0)
    {
        CopyBytes(Result, &Response.Payload.AllocateAddressSpaceStack, sizeof(*Result));
        Result->Status = Response.Status;
    }
}
