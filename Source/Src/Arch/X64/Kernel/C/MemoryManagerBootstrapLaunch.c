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

    if (!ClaimContiguousPages(1ULL, &State->Info.ServicePageMapLevel4PhysicalAddress))
    {
        return 0;
    }

    NewRoot = (UINT64 *)(UINTN)LosX64GetDirectMapVirtualAddress(State->Info.ServicePageMapLevel4PhysicalAddress, 0x1000ULL);
    CurrentRoot = LosX64GetPageMapLevel4();
    if (NewRoot == 0 || CurrentRoot == 0)
    {
        return 0;
    }

    for (EntryIndex = 0U; EntryIndex < 512U; ++EntryIndex)
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

static BOOLEAN MapSegmentIntoAddressSpace(
    UINT64 PageMapLevel4PhysicalAddress,
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeader)
{
    UINT64 SegmentVirtualBase;
    UINT64 SegmentPageCount;
    UINT64 SegmentPhysicalBase;
    UINT64 OffsetIntoFirstPage;
    UINT64 SegmentMappedBytes;
    LOS_X64_MAP_PAGES_REQUEST MapRequest;
    LOS_X64_MAP_PAGES_RESULT MapResult;
    void *SegmentTarget;
    const void *SegmentSource;

    if (Header == 0 || ProgramHeader == 0 || ProgramHeader->MemorySize == 0ULL)
    {
        return 0;
    }

    SegmentVirtualBase = AlignDown(ProgramHeader->VirtualAddress, 0x1000ULL);
    OffsetIntoFirstPage = ProgramHeader->VirtualAddress - SegmentVirtualBase;
    SegmentMappedBytes = AlignUp(ProgramHeader->MemorySize + OffsetIntoFirstPage, 0x1000ULL);
    SegmentPageCount = SegmentMappedBytes / 0x1000ULL;

    if (!ClaimContiguousPages(SegmentPageCount, &SegmentPhysicalBase))
    {
        return 0;
    }

    ZeroMemory(&MapRequest, sizeof(MapRequest));
    ZeroMemory(&MapResult, sizeof(MapResult));
    MapRequest.PageMapLevel4PhysicalAddress = PageMapLevel4PhysicalAddress;
    MapRequest.VirtualAddress = SegmentVirtualBase;
    MapRequest.PhysicalAddress = SegmentPhysicalBase;
    MapRequest.PageCount = SegmentPageCount;
    MapRequest.PageFlags = ProgramHeaderPageFlags(ProgramHeader->Flags);
    MapRequest.Flags = 0U;
    LosX64MapPages(&MapRequest, &MapResult);
    if (MapResult.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS || MapResult.PagesProcessed != SegmentPageCount)
    {
        return 0;
    }

    SegmentTarget = LosX64GetDirectMapVirtualAddress(SegmentPhysicalBase, SegmentMappedBytes);
    if (SegmentTarget == 0)
    {
        return 0;
    }

    ZeroMemory(SegmentTarget, (UINTN)SegmentMappedBytes);
    if (ProgramHeader->FileSize != 0ULL)
    {
        SegmentSource = (const void *)((const UINT8 *)Header + ProgramHeader->Offset);
        CopyMemory((UINT8 *)SegmentTarget + OffsetIntoFirstPage, SegmentSource, (UINTN)ProgramHeader->FileSize);
    }

    return 1;
}

static BOOLEAN MapServiceImageIntoOwnAddressSpace(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header;
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders;
    const LOS_X64_VIRTUAL_MEMORY_LAYOUT *Layout;
    UINT64 ServiceRootPhysicalAddress;
    UINT16 ProgramHeaderIndex;

    State = LosMemoryManagerBootstrapState();
    if (State->ServiceImageVirtualAddress == 0ULL)
    {
        return 0;
    }

    if ((State->Info.Flags & LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_IMAGE_MAPPED) != 0ULL)
    {
        return 1;
    }

    if (!CloneCurrentRootPageMap(&ServiceRootPhysicalAddress))
    {
        return 0;
    }

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
        return 0;
    }

    ProgramHeaders = (const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *)((const UINT8 *)Header + Header->ProgramHeaderOffset);
    for (ProgramHeaderIndex = 0U; ProgramHeaderIndex < Header->ProgramHeaderCount; ++ProgramHeaderIndex)
    {
        const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeader;

        ProgramHeader = (const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *)((const UINT8 *)ProgramHeaders + ((UINTN)ProgramHeaderIndex * Header->ProgramHeaderEntrySize));
        if (ProgramHeader->Type != LOS_ELF_PROGRAM_HEADER_TYPE_LOAD)
        {
            continue;
        }

        if (!MapSegmentIntoAddressSpace(ServiceRootPhysicalAddress, Header, ProgramHeader))
        {
            return 0;
        }
    }

    Layout = LosX64GetVirtualMemoryLayout();
    State->ServiceAddressSpaceObject->RootTablePhysicalAddress = ServiceRootPhysicalAddress;
    State->ServiceAddressSpaceObject->DirectMapBase = Layout->HigherHalfDirectMapBase;
    State->ServiceAddressSpaceObject->DirectMapSize = Layout->HigherHalfDirectMapSize;
    State->ServiceTaskObject->StackTopVirtualAddress = Layout->HigherHalfDirectMapBase + State->Info.ServiceStackPhysicalAddress + (State->Info.ServiceStackPageCount * 0x1000ULL);
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
        return 0;
    }

    LaunchBlockDirectMapAddress = (UINT64)(UINTN)LosX64GetDirectMapVirtualAddress(State->Info.LaunchBlockPhysicalAddress, State->Info.LaunchBlockSize);
    if (LaunchBlockDirectMapAddress == 0ULL)
    {
        return 0;
    }

    ServiceStackTopVirtualAddress = State->ServiceTaskObject->StackTopVirtualAddress;
    if (ServiceStackTopVirtualAddress == 0ULL)
    {
        return 0;
    }

    PreviousRootPhysicalAddress = ReadCr3();
    State->ServiceAddressSpaceObject->State = LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_ACTIVE;
    State->ServiceTaskObject->State = LOS_MEMORY_MANAGER_TASK_STATE_RUNNING;
    State->Info.Flags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_CONTEXT_SWITCHED;
    State->LaunchBlock->Flags = State->Info.Flags;
    LosMemoryManagerBootstrapTransferToServiceTask(
        State->ServiceAddressSpaceObject->RootTablePhysicalAddress,
        State->Info.ServiceEntryVirtualAddress,
        ServiceStackTopVirtualAddress,
        LaunchBlockDirectMapAddress);
    WriteCr3(PreviousRootPhysicalAddress);
    State->ServiceTaskObject->State = LOS_MEMORY_MANAGER_TASK_STATE_ONLINE;
    State->ServiceTaskObject->Flags &= ~LOS_MEMORY_MANAGER_TASK_FLAG_BOOTSTRAP_HOSTED;
    State->Info.Flags |= LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_ENTRY_INVOKED;
    State->LaunchBlock->Flags = State->Info.Flags;
    return 1;
}
