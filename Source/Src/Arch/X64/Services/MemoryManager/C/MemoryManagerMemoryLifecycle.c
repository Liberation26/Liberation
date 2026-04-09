/*
 * File Name: MemoryManagerMemoryLifecycle.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-07T12:35:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#include "MemoryManagerMemoryInternal.h"

#ifndef LOS_MEMORY_MANAGER_LOCAL_PAGE_PRESENT
#define LOS_MEMORY_MANAGER_LOCAL_PAGE_PRESENT 0x001ULL
#define LOS_MEMORY_MANAGER_LOCAL_PAGE_LARGE 0x080ULL
#define LOS_MEMORY_MANAGER_LOCAL_PAGE_TABLE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#endif

static UINT64 *TranslatePageTableForReservation(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 PhysicalAddress)
{
    if (State == 0 || State->DirectMapOffset == 0ULL || PhysicalAddress == 0ULL)
    {
        return 0;
    }

    return (UINT64 *)(UINTN)(State->DirectMapOffset + PhysicalAddress);
}

static BOOLEAN ReservePageTableFrame(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 PhysicalAddress)
{
    return LosMemoryManagerReserveFrameDatabaseRange(
        View,
        PhysicalAddress,
        4096ULL,
        LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_PAGE_TABLE,
        LOS_X64_MEMORY_REGION_OWNER_CLAIMED);
}

static BOOLEAN ReserveActiveServiceLowerHalfPageTables(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View)
{
    UINT64 *PageMapLevel4;
    UINTN Pml4Index;

    if (State == 0 || View == 0 || State->ActiveRootTablePhysicalAddress == 0ULL)
    {
        return 0;
    }

    PageMapLevel4 = TranslatePageTableForReservation(State, State->ActiveRootTablePhysicalAddress);
    if (PageMapLevel4 == 0)
    {
        return 0;
    }

    for (Pml4Index = 0U; Pml4Index < 256U; ++Pml4Index)
    {
        UINT64 Pml4Entry;
        UINT64 PdptPhysicalAddress;
        UINT64 *PageDirectoryPointerTable;
        UINTN PdptIndex;

        Pml4Entry = PageMapLevel4[Pml4Index];
        if ((Pml4Entry & LOS_MEMORY_MANAGER_LOCAL_PAGE_PRESENT) == 0ULL)
        {
            continue;
        }

        PdptPhysicalAddress = Pml4Entry & LOS_MEMORY_MANAGER_LOCAL_PAGE_TABLE_ADDRESS_MASK;
        if (!ReservePageTableFrame(View, PdptPhysicalAddress))
        {
            return 0;
        }

        PageDirectoryPointerTable = TranslatePageTableForReservation(State, PdptPhysicalAddress);
        if (PageDirectoryPointerTable == 0)
        {
            return 0;
        }

        for (PdptIndex = 0U; PdptIndex < 512U; ++PdptIndex)
        {
            UINT64 PdptEntry;
            UINT64 PageDirectoryPhysicalAddress;
            UINT64 *PageDirectory;
            UINTN PdIndex;

            PdptEntry = PageDirectoryPointerTable[PdptIndex];
            if ((PdptEntry & LOS_MEMORY_MANAGER_LOCAL_PAGE_PRESENT) == 0ULL)
            {
                continue;
            }
            if ((PdptEntry & LOS_MEMORY_MANAGER_LOCAL_PAGE_LARGE) != 0ULL)
            {
                continue;
            }

            PageDirectoryPhysicalAddress = PdptEntry & LOS_MEMORY_MANAGER_LOCAL_PAGE_TABLE_ADDRESS_MASK;
            if (!ReservePageTableFrame(View, PageDirectoryPhysicalAddress))
            {
                return 0;
            }

            PageDirectory = TranslatePageTableForReservation(State, PageDirectoryPhysicalAddress);
            if (PageDirectory == 0)
            {
                return 0;
            }

            for (PdIndex = 0U; PdIndex < 512U; ++PdIndex)
            {
                UINT64 PdEntry;
                UINT64 PageTablePhysicalAddress;

                PdEntry = PageDirectory[PdIndex];
                if ((PdEntry & LOS_MEMORY_MANAGER_LOCAL_PAGE_PRESENT) == 0ULL)
                {
                    continue;
                }
                if ((PdEntry & LOS_MEMORY_MANAGER_LOCAL_PAGE_LARGE) != 0ULL)
                {
                    continue;
                }

                PageTablePhysicalAddress = PdEntry & LOS_MEMORY_MANAGER_LOCAL_PAGE_TABLE_ADDRESS_MASK;
                if (!ReservePageTableFrame(View, PageTablePhysicalAddress))
                {
                    return 0;
                }
            }
        }
    }

    return 1;
}

static BOOLEAN IngestNormalizedRegionTable(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 *Detail)
{
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View;
    const LOS_X64_MEMORY_REGION *Regions;
    UINTN Index;

    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_NONE;
    }

    if (State == 0 || State->LaunchBlock == 0 || State->DirectMapOffset == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_TABLE_TRANSLATION;
        }
        return 0;
    }

    View = &State->MemoryView;
    LosMemoryManagerZeroMemory(View, sizeof(*View));
    View->NormalizedRegionTablePhysicalAddress = State->LaunchBlock->MemoryRegionTablePhysicalAddress;
    View->NormalizedRegionCount = State->LaunchBlock->MemoryRegionCount;
    View->NormalizedRegionEntrySize = State->LaunchBlock->MemoryRegionEntrySize;

    if (View->NormalizedRegionTablePhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_TABLE_PHYSICAL;
        }
        return 0;
    }
    if (View->NormalizedRegionCount == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_TABLE_COUNT;
        }
        return 0;
    }
    if (View->NormalizedRegionEntrySize != (UINT64)sizeof(LOS_X64_MEMORY_REGION))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_TABLE_ENTRY_SIZE;
        }
        return 0;
    }
    if (View->NormalizedRegionCount > LOS_MEMORY_MANAGER_MAX_INTERNAL_MEMORY_DESCRIPTORS)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_DESCRIPTOR_CAPACITY;
        }
        return 0;
    }

    Regions = (const LOS_X64_MEMORY_REGION *)(UINTN)(State->DirectMapOffset + View->NormalizedRegionTablePhysicalAddress);
    if (Regions == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_TABLE_TRANSLATION;
        }
        return 0;
    }

    View->NormalizedRegionTable = Regions;
    for (Index = 0U; Index < (UINTN)View->NormalizedRegionCount; ++Index)
    {
        const LOS_X64_MEMORY_REGION *Region;
        UINT32 Category;
        UINT64 RegionEnd;

        Region = &Regions[Index];
        if ((Region->Base & 0xFFFULL) != 0ULL || (Region->Length & 0xFFFULL) != 0ULL || Region->Length == 0ULL)
        {
            if (Detail != 0)
            {
                *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_RANGE_INVALID;
            }
            return 0;
        }
        if (!LosMemoryManagerTryGetRangeEnd(Region->Base, Region->Length, &RegionEnd))
        {
            if (Detail != 0)
            {
                *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_RANGE_INVALID;
            }
            LosMemoryManagerHardFail("base-plus-size-wrap", Region->Base, Region->Length, Index);
        }
        if (Index != 0U)
        {
            const LOS_X64_MEMORY_REGION *Previous;
            UINT64 PreviousEnd;

            Previous = &Regions[Index - 1U];
            if (!LosMemoryManagerTryGetRangeEnd(Previous->Base, Previous->Length, &PreviousEnd))
            {
                if (Detail != 0)
                {
                    *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_RANGE_INVALID;
                }
                LosMemoryManagerHardFail("base-plus-size-wrap", Previous->Base, Previous->Length, Index - 1U);
            }
            if (Region->Base < PreviousEnd)
            {
                if (Detail != 0)
                {
                    *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_RANGE_INVALID;
                }
                LosMemoryManagerHardFail("overlapping-physical-ranges", Previous->Base, Previous->Length, Region->Base);
            }
        }

        Category = LosMemoryManagerClassifyMemoryCategory(Region->Type);
        if (!LosMemoryManagerAppendInternalDescriptor(View, Region->Base, Region->Length, Category, Region->Flags, Region->Owner, Region->Source))
        {
            if (Detail != 0)
            {
                *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_DESCRIPTOR_CAPACITY;
            }
            return 0;
        }

        switch (Category)
        {
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_USABLE:
                View->TotalUsableBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_BOOTSTRAP_RESERVED:
                View->TotalBootstrapReservedBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_FIRMWARE_RESERVED:
                View->TotalFirmwareReservedBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_RUNTIME:
                View->TotalRuntimeBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_MMIO:
                View->TotalMmioBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_ACPI_NVS:
                View->TotalAcpiBytes += Region->Length;
                break;
            case LOS_MEMORY_MANAGER_MEMORY_CATEGORY_UNUSABLE:
            default:
                View->TotalUnusableBytes += Region->Length;
                break;
        }

        if (!LosMemoryManagerAppendFrameDatabaseEntry(
                View,
                Region->Base,
                LosMemoryManagerGetPageCountFromLength(Region->Length),
                LosMemoryManagerClassifyFrameState(Region->Type),
                LosMemoryManagerClassifyFrameUsage(Region->Type),
                Region->Owner,
                Region->Source,
                (UINT64)Region->Flags))
        {
            if (Detail != 0)
            {
                *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_FRAME_DATABASE_CAPACITY;
            }
            return 0;
        }
    }

    return 1;
}

BOOLEAN LosMemoryManagerServiceBuildMemoryView(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 *Detail)
{
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View;

    if (!IngestNormalizedRegionTable(State, Detail))
    {
        return 0;
    }

    View = &State->MemoryView;
    if (!LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceImagePhysicalAddress, State->LaunchBlock->ServiceImageSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_IMAGE, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceStackPhysicalAddress, State->LaunchBlock->ServiceStackPageCount * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_SERVICE_STACK, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->RequestMailboxPhysicalAddress, State->LaunchBlock->RequestMailboxSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_REQUEST_MAILBOX, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->ResponseMailboxPhysicalAddress, State->LaunchBlock->ResponseMailboxSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_RESPONSE_MAILBOX, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->EventMailboxPhysicalAddress, State->LaunchBlock->EventMailboxSize, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_EVENT_MAILBOX, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->LaunchBlockPhysicalAddress, LOS_MEMORY_MANAGER_LAUNCH_BLOCK_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_LAUNCH_BLOCK, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->KernelToServiceEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ENDPOINT_OBJECT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceToKernelEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ENDPOINT_OBJECT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceEventsEndpointObjectPhysicalAddress, LOS_MEMORY_MANAGER_ENDPOINT_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ENDPOINT_OBJECT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress, LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ADDRESS_SPACE_OBJECT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->ServiceTaskObjectPhysicalAddress, LOS_MEMORY_MANAGER_TASK_OBJECT_PAGE_COUNT * 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_TASK_OBJECT, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !LosMemoryManagerReserveFrameDatabaseRange(View, State->LaunchBlock->ServicePageMapLevel4PhysicalAddress, 4096ULL, LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_PAGE_TABLE, LOS_X64_MEMORY_REGION_OWNER_CLAIMED) ||
        !ReserveActiveServiceLowerHalfPageTables(State, View))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_OVERLAY_CONFLICT;
        }
        return 0;
    }

    if (!LosMemoryManagerCopyFrameDatabaseArray(
            View->BaselinePageFrameDatabase,
            &View->BaselinePageFrameDatabaseEntryCount,
            View->PageFrameDatabase,
            View->PageFrameDatabaseEntryCount))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_FRAME_DATABASE_CAPACITY;
        }
        return 0;
    }

    View->DynamicAllocationCount = 0U;
    View->AllocationGeneration = 0ULL;
    LosMemoryManagerRefreshPageTotals(View);
    if (!LosMemoryManagerRefreshInternalDescriptorsFromCurrentDatabase(View))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_DESCRIPTOR_CAPACITY;
        }
        return 0;
    }
    View->Ready = 1U;
    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_MEMORY_VIEW_DETAIL_NONE;
    }
    return 1;
}
