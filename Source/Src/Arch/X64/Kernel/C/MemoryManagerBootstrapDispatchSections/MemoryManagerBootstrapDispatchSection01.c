/*
 * File Name: MemoryManagerBootstrapDispatchSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapDispatch.c.
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
