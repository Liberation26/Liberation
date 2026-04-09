/*
 * File Name: MemoryManagerAddressSpaceDispatchSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerAddressSpaceDispatch.c.
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

static void ZeroBytes(void *Buffer, UINTN ByteCount);
static BOOLEAN DescribeImageLayout(
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    UINT64 *ImageVirtualBase,
    UINT64 *ImageMappedBytes,
    UINT64 *ImagePageCount);
static BOOLEAN ResolveImageEntryVirtualAddress(
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    UINT64 ImageVirtualBase,
    UINT64 ImageMappedBytes,
    UINT64 *EntryVirtualAddress);
static BOOLEAN PopulateExistingImageResult(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *Result);
static BOOLEAN PopulateExistingStackResult(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result);
static UINT64 AlignBytesToPageCount(UINT64 ByteCount);
static BOOLEAN EnsureReservedImageRegionRecorded(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject);
static BOOLEAN EnsureReservedStackRegionRecorded(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject);
static BOOLEAN PopulateRequestedImageResult(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *Result);
static BOOLEAN PopulateRequestedStackResult(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    UINT64 DesiredStackBaseVirtualAddress,
    UINT64 StackPageCount,
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result);


static inline __attribute__((unused)) void AddressSpaceServiceOut8(UINT16 Port, UINT8 Value)
{
    (void)Port;
    (void)Value;
}

static inline __attribute__((unused)) UINT8 AddressSpaceServiceIn8(UINT16 Port)
{
    (void)Port;
    return 0U;
}

static __attribute__((unused)) void AddressSpaceServiceSerialWriteChar(char Character)
{
    (void)Character;
}

static void AddressSpaceServiceSerialWriteText(const char *Text)
{
    (void)Text;
}

static void AddressSpaceServiceSerialWriteHex64(UINT64 Value)
{
    (void)Value;
}

static void AddressSpaceServiceSerialWriteUnsigned(UINT64 Value)
{
    (void)Value;
}

static void AddressSpaceServiceSerialWriteNamedHex(const char *Name, UINT64 Value)
{
    (void)Name;
    (void)Value;
}

static void AddressSpaceServiceSerialWriteNamedUnsigned(const char *Name, UINT64 Value)
{
    (void)Name;
    (void)Value;
}

static BOOLEAN AddressSpaceServiceRangesOverlap(UINT64 LeftBase, UINT64 LeftLength, UINT64 RightBase, UINT64 RightLength)
{
    UINT64 LeftEnd;
    UINT64 RightEnd;

    if (LeftLength == 0ULL || RightLength == 0ULL)
    {
        return 0;
    }

    LeftEnd = LeftBase + LeftLength;
    RightEnd = RightBase + RightLength;
    if (LeftEnd <= LeftBase || RightEnd <= RightBase)
    {
        return 1;
    }

    return !(LeftEnd <= RightBase || RightEnd <= LeftBase);
}

static const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *AddressSpaceServiceFindFrameEntryContainingPhysicalAddress(
    const LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 PhysicalAddress)
{
    UINTN Index;

    if (View == 0)
    {
        return 0;
    }

    for (Index = 0U; Index < View->PageFrameDatabaseEntryCount; ++Index)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry;
        UINT64 EntryEnd;

        Entry = &View->PageFrameDatabase[Index];
        if (!LosMemoryManagerTryGetRangeEnd(Entry->BaseAddress, Entry->PageCount * 4096ULL, &EntryEnd))
        {
            continue;
        }
        if (PhysicalAddress >= Entry->BaseAddress && PhysicalAddress < EntryEnd)
        {
            return Entry;
        }
    }

    return 0;
}

static void AddressSpaceServiceLogFrameEntryForPhysicalAddress(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const char *Label,
    UINT64 PhysicalAddress)
{
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry;

    if (State == 0)
    {
        return;
    }

    Entry = AddressSpaceServiceFindFrameEntryContainingPhysicalAddress(&State->MemoryView, PhysicalAddress);
    LosMemoryManagerServiceSerialWriteText("[MemManager][diag] ");
    LosMemoryManagerServiceSerialWriteText(Label != 0 ? Label : "frame-entry");
    LosMemoryManagerServiceSerialWriteText("-phys=");
    LosMemoryManagerServiceSerialWriteHex64(PhysicalAddress);
    LosMemoryManagerServiceSerialWriteText("\n");
    if (Entry == 0)
    {
        LosMemoryManagerServiceSerialWriteText("[MemManager][diag] ");
        LosMemoryManagerServiceSerialWriteText(Label != 0 ? Label : "frame-entry");
        LosMemoryManagerServiceSerialWriteText("-entry=missing\n");
        return;
    }

    LosMemoryManagerServiceSerialWriteText("[MemManager][diag] ");
    LosMemoryManagerServiceSerialWriteText(Label != 0 ? Label : "frame-entry");
    LosMemoryManagerServiceSerialWriteText("-base=");
    LosMemoryManagerServiceSerialWriteHex64(Entry->BaseAddress);
    LosMemoryManagerServiceSerialWriteText("\n");
    LosMemoryManagerServiceSerialWriteText("[MemManager][diag] ");
    LosMemoryManagerServiceSerialWriteText(Label != 0 ? Label : "frame-entry");
    LosMemoryManagerServiceSerialWriteText("-pages=");
    LosMemoryManagerServiceSerialWriteUnsigned(Entry->PageCount);
    LosMemoryManagerServiceSerialWriteText("\n");
    LosMemoryManagerServiceSerialWriteText("[MemManager][diag] ");
    LosMemoryManagerServiceSerialWriteText(Label != 0 ? Label : "frame-entry");
    LosMemoryManagerServiceSerialWriteText("-state=");
    LosMemoryManagerServiceSerialWriteUnsigned(Entry->State);
    LosMemoryManagerServiceSerialWriteText("\n");
    LosMemoryManagerServiceSerialWriteText("[MemManager][diag] ");
    LosMemoryManagerServiceSerialWriteText(Label != 0 ? Label : "frame-entry");
    LosMemoryManagerServiceSerialWriteText("-usage=");
    LosMemoryManagerServiceSerialWriteUnsigned(Entry->Usage);
    LosMemoryManagerServiceSerialWriteText("\n");
    LosMemoryManagerServiceSerialWriteText("[MemManager][diag] ");
    LosMemoryManagerServiceSerialWriteText(Label != 0 ? Label : "frame-entry");
    LosMemoryManagerServiceSerialWriteText("-owner=");
    LosMemoryManagerServiceSerialWriteUnsigned(Entry->Owner);
    LosMemoryManagerServiceSerialWriteText("\n");
    LosMemoryManagerServiceSerialWriteText("[MemManager][diag] ");
    LosMemoryManagerServiceSerialWriteText(Label != 0 ? Label : "frame-entry");
    LosMemoryManagerServiceSerialWriteText("-source=");
    LosMemoryManagerServiceSerialWriteUnsigned(Entry->Source);
    LosMemoryManagerServiceSerialWriteText("\n");
}

static void AddressSpaceServiceVerifyAttachImageState(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_REQUEST *Request,
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 StageCode,
    UINT64 PageIndex,
    UINT64 RunVirtualAddress,
    UINT64 RunPhysicalAddress,
    UINT64 RunPageCount)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *ExpectedAddressSpaceObject;
    UINT64 ExpectedPointerValue;
    UINT64 ActualPointerValue;

    if (State == 0 || Request == 0)
    {
        return;
    }

    ExpectedAddressSpaceObject = (LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *)LosMemoryManagerTranslatePhysical(
        State,
        Request->AddressSpaceObjectPhysicalAddress,
        sizeof(LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT));
    ExpectedPointerValue = (UINT64)(UINTN)ExpectedAddressSpaceObject;
    ActualPointerValue = (UINT64)(UINTN)AddressSpaceObject;

    if ((StageCode & 0xFFULL) == 0ULL)
    {
        AddressSpaceServiceSerialWriteNamedHex("attach-stage", StageCode);
        AddressSpaceServiceSerialWriteNamedHex("attach-object-phys", Request->AddressSpaceObjectPhysicalAddress);
        AddressSpaceServiceSerialWriteNamedHex("attach-object-expected", ExpectedPointerValue);
        AddressSpaceServiceSerialWriteNamedHex("attach-object-actual", ActualPointerValue);
        AddressSpaceServiceSerialWriteNamedHex("attach-root-phys", AddressSpaceObject != 0 ? AddressSpaceObject->RootTablePhysicalAddress : 0ULL);
        AddressSpaceServiceSerialWriteNamedUnsigned("attach-page-index", PageIndex);
        AddressSpaceServiceSerialWriteNamedHex("attach-run-virt", RunVirtualAddress);
        AddressSpaceServiceSerialWriteNamedHex("attach-run-phys", RunPhysicalAddress);
        AddressSpaceServiceSerialWriteNamedUnsigned("attach-run-pages", RunPageCount);
        if (AddressSpaceObject != 0)
        {
            AddressSpaceServiceLogFrameEntryForPhysicalAddress(State, "attach-root-entry", AddressSpaceObject->RootTablePhysicalAddress);
        }
    }

    if (ExpectedAddressSpaceObject == 0 || AddressSpaceObject != ExpectedAddressSpaceObject)
    {
        AddressSpaceServiceSerialWriteText("[MemManager][diag] attach image state mismatch detected before reserve/map continuation.\n");
        AddressSpaceServiceSerialWriteNamedHex("attach-object-expected", ExpectedPointerValue);
        AddressSpaceServiceSerialWriteNamedHex("attach-object-actual", ActualPointerValue);
        AddressSpaceServiceSerialWriteNamedHex("attach-stage", StageCode);
        LosMemoryManagerHardFail("attach-image-address-space-pointer-clobbered", ActualPointerValue, ExpectedPointerValue, StageCode);
    }

    if (AddressSpaceObject->RootTablePhysicalAddress == 0ULL)
    {
        AddressSpaceServiceSerialWriteNamedHex("attach-stage", StageCode);
        LosMemoryManagerHardFail("attach-image-root-became-zero", Request->AddressSpaceObjectPhysicalAddress, ActualPointerValue, StageCode);
    }
}

static const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *AddressSpaceServiceFindFrameEntryForPhysicalRange(
    const LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 BaseAddress,
    UINT64 Length)
{
    UINTN Index;
    UINT64 EndAddress;

    if (View == 0 || Length == 0ULL || !LosMemoryManagerTryGetRangeEnd(BaseAddress, Length, &EndAddress))
    {
        return 0;
    }

    for (Index = 0U; Index < View->PageFrameDatabaseEntryCount; ++Index)
    {
        const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry;
        UINT64 EntryEnd;

        Entry = &View->PageFrameDatabase[Index];
        if (!LosMemoryManagerTryGetRangeEnd(Entry->BaseAddress, Entry->PageCount * 4096ULL, &EntryEnd))
        {
            continue;
        }
        if (BaseAddress >= Entry->BaseAddress && EndAddress <= EntryEnd)
        {
            return Entry;
        }
    }

    return 0;
}
