/*
 * File Name: SchedulerLifecycleSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from SchedulerLifecycle.c.
 */

static void WriteUserTransitionFrame(
    UINT64 FrameStackPointer,
    UINT64 InstructionPointer,
    UINT64 CodeSelector,
    UINT64 Rflags,
    UINT64 StackPointer,
    UINT64 StackSelector)
{
    LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *Frame;

    if (FrameStackPointer == 0ULL)
    {
        return;
    }

    Frame = (LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *)(UINTN)FrameStackPointer;
    ZeroBytes(Frame, sizeof(*Frame));
    Frame->Rip = InstructionPointer;
    Frame->Cs = CodeSelector;
    Frame->Rflags = Rflags;
    Frame->Rsp = StackPointer;
    Frame->Ss = StackSelector;
}

static LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *ResolveSchedulerAddressSpaceObject(UINT64 AddressSpaceObjectPhysicalAddress)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;

    if (AddressSpaceObjectPhysicalAddress == 0ULL)
    {
        return 0;
    }

    AddressSpaceObject = (LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *)LosX64GetDirectMapVirtualAddress(
        AddressSpaceObjectPhysicalAddress,
        sizeof(*AddressSpaceObject));
    if (AddressSpaceObject == 0)
    {
        return 0;
    }
    if (AddressSpaceObject->State < LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_READY ||
        AddressSpaceObject->RootTablePhysicalAddress == 0ULL ||
        (AddressSpaceObject->RootTablePhysicalAddress & 0xFFFULL) != 0ULL)
    {
        return 0;
    }

    return AddressSpaceObject;
}

static UINT64 *TranslateSchedulerPageTable(UINT64 PhysicalAddress)
{
    return (UINT64 *)LosX64GetDirectMapVirtualAddress(PhysicalAddress, 0x1000ULL);
}

static __attribute__((unused)) BOOLEAN SchedulerQueryLeafPageEntry(UINT64 RootTablePhysicalAddress, UINT64 VirtualAddress, UINT64 *Entry)
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
        return 0U;
    }

    PageMapLevel4 = TranslateSchedulerPageTable(RootTablePhysicalAddress);
    if (PageMapLevel4 == 0)
    {
        return 0U;
    }

    CurrentEntry = PageMapLevel4[(UINTN)((VirtualAddress >> 39U) & 0x1FFULL)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL || (CurrentEntry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 0U;
    }

    PageDirectoryPointerTable = TranslateSchedulerPageTable(CurrentEntry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageDirectoryPointerTable == 0)
    {
        return 0U;
    }

    CurrentEntry = PageDirectoryPointerTable[(UINTN)((VirtualAddress >> 30U) & 0x1FFULL)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL || (CurrentEntry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 0U;
    }

    PageDirectory = TranslateSchedulerPageTable(CurrentEntry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageDirectory == 0)
    {
        return 0U;
    }

    CurrentEntry = PageDirectory[(UINTN)((VirtualAddress >> 21U) & 0x1FFULL)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL || (CurrentEntry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 0U;
    }

    PageTable = TranslateSchedulerPageTable(CurrentEntry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageTable == 0)
    {
        return 0U;
    }

    CurrentEntry = PageTable[(UINTN)((VirtualAddress >> 12U) & 0x1FFULL)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0U;
    }

    if (Entry != 0)
    {
        *Entry = CurrentEntry;
    }
    return 1U;
}

static __attribute__((unused)) BOOLEAN QueryAddressSpaceMappingFromRecordedMetadata(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 VirtualAddress,
    UINT64 *PhysicalAddress,
    UINT64 *PageFlags)
{
    UINT64 PageVirtualAddress;

    if (PhysicalAddress != 0)
    {
        *PhysicalAddress = 0ULL;
    }
    if (PageFlags != 0)
    {
        *PageFlags = 0ULL;
    }
    if (AddressSpaceObject == 0)
    {
        return 0U;
    }

    PageVirtualAddress = VirtualAddress & ~0xFFFULL;
    if (AddressSpaceObject->ServiceImageVirtualBase != 0ULL &&
        AddressSpaceObject->ServiceImageSize != 0ULL &&
        PageVirtualAddress >= AddressSpaceObject->ServiceImageVirtualBase &&
        PageVirtualAddress < AddressSpaceObject->ServiceImageVirtualBase +
            (((AddressSpaceObject->ServiceImageSize + 0xFFFULL) / 0x1000ULL) * 0x1000ULL) &&
        AddressSpaceObject->ServiceImagePhysicalAddress != 0ULL)
    {
        if (PhysicalAddress != 0)
        {
            *PhysicalAddress = AddressSpaceObject->ServiceImagePhysicalAddress +
                (PageVirtualAddress - AddressSpaceObject->ServiceImageVirtualBase);
        }
        if (PageFlags != 0)
        {
            *PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER |
                ((AddressSpaceObject->EntryVirtualAddress != 0ULL &&
                  PageVirtualAddress == (AddressSpaceObject->EntryVirtualAddress & ~0xFFFULL)) ?
                    0ULL : LOS_X64_PAGE_NX);
        }
        return 1U;
    }
    if (AddressSpaceObject->StackBaseVirtualAddress != 0ULL &&
        AddressSpaceObject->StackTopVirtualAddress > AddressSpaceObject->StackBaseVirtualAddress &&
        PageVirtualAddress >= AddressSpaceObject->StackBaseVirtualAddress &&
        PageVirtualAddress < AddressSpaceObject->StackTopVirtualAddress &&
        AddressSpaceObject->StackPhysicalAddress != 0ULL)
    {
        if (PhysicalAddress != 0)
        {
            *PhysicalAddress = AddressSpaceObject->StackPhysicalAddress +
                (PageVirtualAddress - AddressSpaceObject->StackBaseVirtualAddress);
        }
        if (PageFlags != 0)
        {
            *PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE |
                LOS_X64_PAGE_USER | LOS_X64_PAGE_NX;
        }
        return 1U;
    }

    return 0U;
}


static BOOLEAN ApplyFirstUserTaskImageMetadata(
    const LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *AttachResult,
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 *ResolvedEntryVirtualAddress)
{
    UINT64 CandidateEntry;
    UINT64 CandidateBase;
    UINT64 CandidatePages;

    if (ResolvedEntryVirtualAddress == 0)
    {
        return 0U;
    }

    CandidateEntry = 0ULL;
    CandidateBase = 0ULL;
    CandidatePages = 0ULL;
    if (AttachResult != 0)
    {
        CandidateEntry = AttachResult->EntryVirtualAddress;
        CandidateBase = AttachResult->ImageVirtualBase;
        CandidatePages = AttachResult->ImagePageCount;
    }
    if ((CandidateEntry == 0ULL || CandidateBase == 0ULL || CandidatePages == 0ULL) && AddressSpaceObject != 0)
    {
        if (CandidateEntry == 0ULL)
        {
            CandidateEntry = AddressSpaceObject->EntryVirtualAddress;
            if (CandidateEntry == 0ULL)
            {
                CandidateEntry = AddressSpaceObject->ServiceImageVirtualBase;
            }
        }
        if (CandidateBase == 0ULL)
        {
            CandidateBase = AddressSpaceObject->ServiceImageVirtualBase;
        }
        if (CandidatePages == 0ULL && AddressSpaceObject->ServiceImageSize != 0ULL)
        {
            CandidatePages = (AddressSpaceObject->ServiceImageSize + 0xFFFULL) / 0x1000ULL;
        }
    }

    if (CandidateEntry == 0ULL || CandidateBase == 0ULL || CandidatePages == 0ULL)
    {
        return 0U;
    }
    if (CandidateEntry < CandidateBase || CandidateEntry >= CandidateBase + (CandidatePages * 0x1000ULL))
    {
        return 0U;
    }

    *ResolvedEntryVirtualAddress = CandidateEntry;
    return 1U;
}

static BOOLEAN ApplyFirstUserTaskStackMetadata(
    const LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *StackResult,
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 *ResolvedStackTopVirtualAddress)
{
    UINT64 CandidateBase;
    UINT64 CandidateTop;
    UINT64 CandidatePages;

    if (ResolvedStackTopVirtualAddress == 0)
    {
        return 0U;
    }

    CandidateBase = 0ULL;
    CandidateTop = 0ULL;
    CandidatePages = 0ULL;
    if (StackResult != 0)
    {
        CandidateBase = StackResult->StackBaseVirtualAddress;
        CandidateTop = StackResult->StackTopVirtualAddress;
        CandidatePages = StackResult->StackPageCount;
    }
    if ((CandidateBase == 0ULL || CandidateTop == 0ULL || CandidatePages == 0ULL) && AddressSpaceObject != 0)
    {
        if (CandidateBase == 0ULL)
        {
            CandidateBase = AddressSpaceObject->StackBaseVirtualAddress;
        }
        if (CandidateTop == 0ULL)
        {
            CandidateTop = AddressSpaceObject->StackTopVirtualAddress;
        }
        if (CandidatePages == 0ULL)
        {
            CandidatePages = AddressSpaceObject->StackPageCount;
        }
    }

    if (CandidateBase == 0ULL || CandidateTop == 0ULL || CandidatePages == 0ULL)
    {
        return 0U;
    }
    if (CandidateTop != CandidateBase + (CandidatePages * 0x1000ULL))
    {
        return 0U;
    }

    *ResolvedStackTopVirtualAddress = CandidateTop;
    return 1U;
}

static BOOLEAN QueryAddressSpaceMapping(UINT64 AddressSpaceObjectPhysicalAddress, UINT64 VirtualAddress, UINT64 *PhysicalAddress, UINT64 *PageFlags)
{
    LOS_MEMORY_MANAGER_QUERY_MAPPING_REQUEST Request;
    LOS_MEMORY_MANAGER_QUERY_MAPPING_RESULT Result;

    ZeroBytes(&Request, sizeof(Request));
    ZeroBytes(&Result, sizeof(Result));
    Request.AddressSpaceObjectPhysicalAddress = AddressSpaceObjectPhysicalAddress;
    Request.VirtualAddress = VirtualAddress;
    LosMemoryManagerSendQueryMapping(&Request, &Result);
    if (Result.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS || Result.PhysicalAddress == 0ULL)
    {
        if (PhysicalAddress != 0)
        {
            *PhysicalAddress = 0ULL;
        }
        if (PageFlags != 0)
        {
            *PageFlags = 0ULL;
        }
        return 0U;
    }

    if (PhysicalAddress != 0)
    {
        *PhysicalAddress = Result.PhysicalAddress;
    }
    if (PageFlags != 0)
    {
        *PageFlags = Result.PageFlags;
    }
    return 1U;
}
