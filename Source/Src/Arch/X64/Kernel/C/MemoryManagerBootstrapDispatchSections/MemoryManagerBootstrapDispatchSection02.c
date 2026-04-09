/*
 * File Name: MemoryManagerBootstrapDispatchSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapDispatch.c.
 */

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
