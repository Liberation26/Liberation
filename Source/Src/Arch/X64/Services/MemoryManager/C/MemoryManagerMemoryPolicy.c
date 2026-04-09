/*
 * File Name: MemoryManagerMemoryPolicy.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#include "MemoryManagerMemoryInternal.h"

UINT64 LosMemoryManagerAlignDownPage(UINT64 Value)
{
    return Value & ~0xFFFULL;
}

UINT64 LosMemoryManagerAlignUpPage(UINT64 Value)
{
    return (Value + 0xFFFULL) & ~0xFFFULL;
}

UINT64 LosMemoryManagerAlignUpValue(UINT64 Value, UINT64 Alignment)
{
    if (Alignment == 0ULL)
    {
        return Value;
    }

    return (Value + (Alignment - 1ULL)) & ~(Alignment - 1ULL);
}

UINT64 LosMemoryManagerGetPageCountFromLength(UINT64 Length)
{
    return Length / 4096ULL;
}

BOOLEAN LosMemoryManagerTryGetRangeEnd(UINT64 BaseAddress, UINT64 SizeBytes, UINT64 *EndAddress)
{
    UINT64 EndValue;

    if (EndAddress != 0)
    {
        *EndAddress = 0ULL;
    }
    if (SizeBytes == 0ULL || EndAddress == 0)
    {
        return 0;
    }

    EndValue = BaseAddress + SizeBytes;
    if (EndValue <= BaseAddress)
    {
        return 0;
    }

    *EndAddress = EndValue;
    return 1;
}

BOOLEAN LosMemoryManagerDoesRangeWrap(UINT64 BaseAddress, UINT64 SizeBytes)
{
    UINT64 EndAddress;

    return !LosMemoryManagerTryGetRangeEnd(BaseAddress, SizeBytes, &EndAddress);
}

BOOLEAN LosMemoryManagerIsPowerOfTwo(UINT64 Value)
{
    return Value != 0ULL && (Value & (Value - 1ULL)) == 0ULL;
}

UINT32 LosMemoryManagerClassifyMemoryCategory(UINT32 RegionType)
{
    switch (RegionType)
    {
        case LOS_X64_MEMORY_REGION_TYPE_USABLE:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_USABLE;
        case LOS_X64_MEMORY_REGION_TYPE_BOOT_RESERVED:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_BOOTSTRAP_RESERVED;
        case LOS_X64_MEMORY_REGION_TYPE_RUNTIME:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_RUNTIME;
        case LOS_X64_MEMORY_REGION_TYPE_MMIO:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_MMIO;
        case LOS_X64_MEMORY_REGION_TYPE_ACPI_NVS:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_ACPI_NVS;
        case LOS_X64_MEMORY_REGION_TYPE_FIRMWARE_RESERVED:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_FIRMWARE_RESERVED;
        case LOS_X64_MEMORY_REGION_TYPE_UNUSABLE:
        default:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_UNUSABLE;
    }
}

UINT32 LosMemoryManagerClassifyFrameState(UINT32 RegionType)
{
    switch (RegionType)
    {
        case LOS_X64_MEMORY_REGION_TYPE_USABLE:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE;
        case LOS_X64_MEMORY_REGION_TYPE_RUNTIME:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RUNTIME;
        case LOS_X64_MEMORY_REGION_TYPE_MMIO:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_MMIO;
        default:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RESERVED;
    }
}

UINT32 LosMemoryManagerClassifyFrameUsage(UINT32 RegionType)
{
    switch (RegionType)
    {
        case LOS_X64_MEMORY_REGION_TYPE_USABLE:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_FREE;
        case LOS_X64_MEMORY_REGION_TYPE_BOOT_RESERVED:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_BOOTSTRAP_RESERVED;
        case LOS_X64_MEMORY_REGION_TYPE_RUNTIME:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_RUNTIME;
        case LOS_X64_MEMORY_REGION_TYPE_MMIO:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_MMIO;
        case LOS_X64_MEMORY_REGION_TYPE_ACPI_NVS:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ACPI_NVS;
        case LOS_X64_MEMORY_REGION_TYPE_FIRMWARE_RESERVED:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_FIRMWARE_RESERVED;
        case LOS_X64_MEMORY_REGION_TYPE_UNUSABLE:
        default:
            return LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_UNUSABLE;
    }
}

UINT32 LosMemoryManagerDetermineMemoryCategoryFromFrameEntry(const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry)
{
    if (Entry == 0)
    {
        return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_UNUSABLE;
    }

    switch (Entry->State)
    {
        case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_USABLE;
        case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RUNTIME:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_RUNTIME;
        case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_MMIO:
            return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_MMIO;
        case LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RESERVED:
        default:
            switch (Entry->Usage)
            {
                case LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_FIRMWARE_RESERVED:
                    return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_FIRMWARE_RESERVED;
                case LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ACPI_NVS:
                    return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_ACPI_NVS;
                case LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_UNUSABLE:
                    return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_UNUSABLE;
                default:
                    return LOS_MEMORY_MANAGER_MEMORY_CATEGORY_BOOTSTRAP_RESERVED;
            }
    }
}

UINT32 LosMemoryManagerDetermineRegionFlagsFromFrameEntry(const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry)
{
    UINT32 Flags;

    if (Entry == 0)
    {
        return 0U;
    }

    Flags = (UINT32)Entry->Attributes;
    if (Entry->State == LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_RESERVED &&
        (Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_DYNAMIC_RESERVED ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_DYNAMIC_CLAIMED ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_IMAGE ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_STACK ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_REQUEST_MAILBOX ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_RESPONSE_MAILBOX ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_EVENT_MAILBOX ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_LAUNCH_BLOCK ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ENDPOINT_OBJECT ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ADDRESS_SPACE_OBJECT ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_TASK_OBJECT ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_PAGE_TABLE ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_HEAP_METADATA ||
         Entry->Usage == LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_HEAP_INTERNAL))
    {
        Flags |= LOS_X64_MEMORY_REGION_FLAG_OVERLAY;
        Flags |= LOS_X64_MEMORY_REGION_FLAG_CLAIMED;
    }

    return Flags;
}

BOOLEAN LosMemoryManagerIsRangeCoveredByPageFrameDatabase(
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN Count,
    UINT64 BaseAddress,
    UINT64 Length)
{
    UINT64 EndAddress;
    UINT64 Cursor;
    UINTN Index;

    if (Array == 0 || Length == 0ULL)
    {
        return 0;
    }

    EndAddress = BaseAddress + Length;
    if (EndAddress <= BaseAddress)
    {
        return 0;
    }

    Cursor = BaseAddress;
    for (Index = 0U; Index < Count && Cursor < EndAddress; ++Index)
    {
        UINT64 EntryBase;
        UINT64 EntryEnd;

        EntryBase = Array[Index].BaseAddress;
        EntryEnd = EntryBase + (Array[Index].PageCount * 4096ULL);
        if (EntryEnd <= Cursor)
        {
            continue;
        }
        if (EntryBase > Cursor)
        {
            return 0;
        }
        if (EntryEnd > Cursor)
        {
            Cursor = EntryEnd;
        }
    }

    return Cursor >= EndAddress;
}

BOOLEAN LosMemoryManagerIsRangeCoveredByState(
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN Count,
    UINT64 BaseAddress,
    UINT64 Length,
    UINT32 State)
{
    UINT64 EndAddress;
    UINT64 Cursor;
    UINTN Index;

    if (Array == 0 || Length == 0ULL)
    {
        return 0;
    }

    EndAddress = BaseAddress + Length;
    if (EndAddress <= BaseAddress)
    {
        return 0;
    }

    Cursor = BaseAddress;
    for (Index = 0U; Index < Count && Cursor < EndAddress; ++Index)
    {
        UINT64 EntryBase;
        UINT64 EntryEnd;
        UINT64 OverlapBase;
        UINT64 OverlapEnd;

        EntryBase = Array[Index].BaseAddress;
        EntryEnd = EntryBase + (Array[Index].PageCount * 4096ULL);
        if (EntryEnd <= Cursor || EntryEnd <= BaseAddress || EntryBase >= EndAddress)
        {
            continue;
        }
        OverlapBase = Cursor > EntryBase ? Cursor : EntryBase;
        if (OverlapBase > Cursor)
        {
            return 0;
        }
        if (Array[Index].State != State)
        {
            return 0;
        }
        OverlapEnd = EndAddress < EntryEnd ? EndAddress : EntryEnd;
        Cursor = OverlapEnd;
    }

    return Cursor >= EndAddress;
}

BOOLEAN LosMemoryManagerFindClaimableContiguousRange(
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN Count,
    UINT64 MinimumPhysicalAddress,
    UINT64 MaximumPhysicalAddress,
    UINT64 AlignmentBytes,
    UINT64 PageCount,
    UINT64 *BaseAddress)
{
    UINT64 RequiredBytes;
    UINT64 CurrentRunBase;
    UINT64 CurrentRunEnd;
    BOOLEAN HaveRun;
    UINTN Index;

    if (Array == 0 || BaseAddress == 0 || PageCount == 0ULL)
    {
        return 0;
    }

    RequiredBytes = PageCount * 4096ULL;
    if (RequiredBytes == 0ULL || RequiredBytes / 4096ULL != PageCount)
    {
        return 0;
    }

    CurrentRunBase = 0ULL;
    CurrentRunEnd = 0ULL;
    HaveRun = 0;

    for (Index = 0U; Index < Count; ++Index)
    {
        UINT64 EntryBase;
        UINT64 EntryEnd;
        UINT64 SpanBase;
        UINT64 SpanEnd;
        UINT64 CandidateBase;

        if (Array[Index].State != LOS_MEMORY_MANAGER_PAGE_FRAME_STATE_FREE)
        {
            continue;
        }

        EntryBase = Array[Index].BaseAddress;
        EntryEnd = EntryBase + (Array[Index].PageCount * 4096ULL);
        if (EntryEnd <= MinimumPhysicalAddress || EntryBase >= MaximumPhysicalAddress)
        {
            continue;
        }

        SpanBase = EntryBase > MinimumPhysicalAddress ? EntryBase : MinimumPhysicalAddress;
        SpanEnd = EntryEnd < MaximumPhysicalAddress ? EntryEnd : MaximumPhysicalAddress;
        if (SpanEnd <= SpanBase)
        {
            continue;
        }

        if (HaveRun == 0 || SpanBase > CurrentRunEnd)
        {
            CandidateBase = LosMemoryManagerAlignUpValue(SpanBase, AlignmentBytes);
            if (CandidateBase >= SpanEnd)
            {
                HaveRun = 0;
                continue;
            }

            CurrentRunBase = CandidateBase;
            CurrentRunEnd = SpanEnd;
            HaveRun = 1;
        }
        else if (SpanEnd > CurrentRunEnd)
        {
            CurrentRunEnd = SpanEnd;
        }

        if (HaveRun != 0 && CurrentRunEnd - CurrentRunBase >= RequiredBytes)
        {
            *BaseAddress = CurrentRunBase;
            return 1;
        }
    }

    return 0;
}
