/*
 * File Name: MemoryManagerMemoryInternal.h
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-07T12:35:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#ifndef LOS_MEMORY_MANAGER_SERVICE_MEMORY_INTERNAL_H
#define LOS_MEMORY_MANAGER_SERVICE_MEMORY_INTERNAL_H

#include "MemoryManagerMainInternal.h"

BOOLEAN LosMemoryManagerAppendInternalDescriptor(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 Base,
    UINT64 Length,
    UINT32 Category,
    UINT32 Flags,
    UINT32 Owner,
    UINT32 Source);
BOOLEAN LosMemoryManagerAppendFrameDatabaseEntryToArray(
    LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN *Count,
    UINT64 BaseAddress,
    UINT64 PageCount,
    UINT32 State,
    UINT32 Usage,
    UINT32 Owner,
    UINT32 Source,
    UINT64 Attributes);
BOOLEAN LosMemoryManagerAppendFrameDatabaseEntry(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 BaseAddress,
    UINT64 PageCount,
    UINT32 State,
    UINT32 Usage,
    UINT32 Owner,
    UINT32 Source,
    UINT64 Attributes);
BOOLEAN LosMemoryManagerCopyFrameDatabaseArray(
    LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Destination,
    UINTN *DestinationCount,
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Source,
    UINTN SourceCount);
BOOLEAN LosMemoryManagerRewriteFrameDatabaseRange(
    LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN *Count,
    UINT64 BaseAddress,
    UINT64 Length,
    UINT32 State,
    UINT32 Usage,
    UINT32 Owner);
BOOLEAN LosMemoryManagerReserveFrameDatabaseRange(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 BaseAddress,
    UINT64 Length,
    UINT32 Usage,
    UINT32 Owner);
BOOLEAN LosMemoryManagerRefreshInternalDescriptorsFromCurrentDatabase(LOS_MEMORY_MANAGER_MEMORY_VIEW *View);
void LosMemoryManagerRefreshPageTotals(LOS_MEMORY_MANAGER_MEMORY_VIEW *View);
BOOLEAN LosMemoryManagerInsertDynamicAllocation(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 BaseAddress,
    UINT64 PageCount,
    UINT32 Usage,
    UINT32 Owner);
BOOLEAN LosMemoryManagerRemoveDynamicAllocationRange(
    LOS_MEMORY_MANAGER_MEMORY_VIEW *View,
    UINT64 BaseAddress,
    UINT64 PageCount);
BOOLEAN LosMemoryManagerRebuildCurrentPageFrameDatabase(LOS_MEMORY_MANAGER_MEMORY_VIEW *View);
UINT64 LosMemoryManagerAlignDownPage(UINT64 Value);
UINT64 LosMemoryManagerAlignUpPage(UINT64 Value);
UINT64 LosMemoryManagerAlignUpValue(UINT64 Value, UINT64 Alignment);
UINT64 LosMemoryManagerGetPageCountFromLength(UINT64 Length);
BOOLEAN LosMemoryManagerDoesRangeWrap(UINT64 BaseAddress, UINT64 SizeBytes);
BOOLEAN LosMemoryManagerTryGetRangeEnd(UINT64 BaseAddress, UINT64 SizeBytes, UINT64 *EndAddress);
BOOLEAN LosMemoryManagerIsPowerOfTwo(UINT64 Value);
UINT32 LosMemoryManagerClassifyMemoryCategory(UINT32 RegionType);
UINT32 LosMemoryManagerClassifyFrameState(UINT32 RegionType);
UINT32 LosMemoryManagerClassifyFrameUsage(UINT32 RegionType);
UINT32 LosMemoryManagerDetermineMemoryCategoryFromFrameEntry(const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry);
UINT32 LosMemoryManagerDetermineRegionFlagsFromFrameEntry(const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Entry);
BOOLEAN LosMemoryManagerIsRangeCoveredByPageFrameDatabase(
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN Count,
    UINT64 BaseAddress,
    UINT64 Length);
BOOLEAN LosMemoryManagerIsRangeCoveredByState(
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN Count,
    UINT64 BaseAddress,
    UINT64 Length,
    UINT32 State);
BOOLEAN LosMemoryManagerFindClaimableContiguousRange(
    const LOS_MEMORY_MANAGER_PAGE_FRAME_DATABASE_ENTRY *Array,
    UINTN Count,
    UINT64 MinimumPhysicalAddress,
    UINT64 MaximumPhysicalAddress,
    UINT64 AlignmentBytes,
    UINT64 PageCount,
    UINT64 *BaseAddress);

#endif
