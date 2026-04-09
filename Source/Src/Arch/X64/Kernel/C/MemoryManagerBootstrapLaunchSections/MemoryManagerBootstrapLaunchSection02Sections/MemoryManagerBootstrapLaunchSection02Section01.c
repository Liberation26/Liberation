/*
 * File Name: MemoryManagerBootstrapLaunchSection02Section01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerBootstrapLaunchSection02.c.
 */

static UINT64 ComputeServiceImagePageFlags(
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    UINT64 PageVirtualAddress)
{
    UINT16 ProgramHeaderIndex;
    UINT64 PageFlags;
    BOOLEAN PageCovered;

    if (Header == 0 || ProgramHeaders == 0)
    {
        return 0ULL;
    }

    PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER | LOS_X64_PAGE_NX;
    PageCovered = 0;
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
        PageCovered = 1;
    }

    return PageCovered ? PageFlags : 0ULL;
}

static BOOLEAN StageServiceImageInPhysicalMemory(
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    UINT64 *ImageVirtualBase,
    UINT64 *ImageMappedBytes,
    UINT64 *ImagePageCount,
    UINT64 *ImagePhysicalBase)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
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

    if (!DescribeServiceImageLayout(Header, ProgramHeaders, ImageVirtualBase, ImageMappedBytes, ImagePageCount))
    {
        return 0;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_SEGMENT, LOS_MEMORY_MANAGER_PREP_DETAIL_SEGMENT_FRAME_CLAIM);
    if (!ClaimContiguousPages(*ImagePageCount, ImagePhysicalBase))
    {
        return 0;
    }

    ImageTarget = LosX64GetDirectMapVirtualAddress(*ImagePhysicalBase, *ImageMappedBytes);
    if (ImageTarget == 0)
    {
        SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_SEGMENT, LOS_MEMORY_MANAGER_PREP_DETAIL_SEGMENT_DIRECT_MAP);
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

        if (!TryGetLoadSegmentGeometry(ProgramHeader, &SegmentVirtualBase, &SegmentMappedBytes, &SegmentPageCount))
        {
            continue;
        }

        SegmentImageOffset = ProgramHeader->VirtualAddress - *ImageVirtualBase;
        SegmentTarget = (UINT8 *)ImageTarget + SegmentImageOffset;


        if (ProgramHeader->FileSize != 0ULL)
        {
            SegmentSource = (const void *)((const UINT8 *)Header + ProgramHeader->Offset);
            CopyMemory(SegmentTarget, SegmentSource, (UINTN)ProgramHeader->FileSize);
        }
    }

    State = LosMemoryManagerBootstrapState();
    State->Info.ServiceImagePhysicalAddress = *ImagePhysicalBase;
    State->ServiceAddressSpaceObject->ServiceImagePhysicalAddress = *ImagePhysicalBase;
    State->LaunchBlock->ServiceImagePhysicalAddress = *ImagePhysicalBase;
    return 1;
}

static BOOLEAN MapServiceImageRunIntoAddressSpace(
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 VirtualAddress,
    UINT64 PhysicalAddress,
    UINT64 PageCount,
    UINT64 PageFlags)
{
    LOS_X64_MAP_PAGES_REQUEST MapRequest;
    LOS_X64_MAP_PAGES_RESULT MapResult;

    if (PageMapLevel4PhysicalAddress == 0ULL ||
        VirtualAddress == 0ULL ||
        PhysicalAddress == 0ULL ||
        PageCount == 0ULL ||
        PageFlags == 0ULL)
    {
        return 0;
    }

    ZeroMemory(&MapRequest, sizeof(MapRequest));
    ZeroMemory(&MapResult, sizeof(MapResult));
    MapRequest.PageMapLevel4PhysicalAddress = PageMapLevel4PhysicalAddress;
    MapRequest.VirtualAddress = VirtualAddress;
    MapRequest.PhysicalAddress = PhysicalAddress;
    MapRequest.PageCount = PageCount;
    MapRequest.PageFlags = PageFlags;
    MapRequest.Flags = 0U;
    LosX64MapPages(&MapRequest, &MapResult);
    if (MapResult.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS || MapResult.PagesProcessed != PageCount)
    {
        SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_SEGMENT, LOS_MEMORY_MANAGER_PREP_DETAIL_SEGMENT_MAP_STATUS);
        LosKernelTraceUnsigned("Memory-manager segment map status: ", MapResult.Status);
        LosKernelTraceHex64("Memory-manager segment map pages processed: ", MapResult.PagesProcessed);
        LosKernelTraceHex64("Memory-manager segment map requested pages: ", PageCount);
        LosKernelTraceHex64("Memory-manager segment map virtual page: ", VirtualAddress);
        LosKernelTraceHex64("Memory-manager segment map physical page: ", PhysicalAddress);
        LosKernelTraceHex64("Memory-manager segment map page flags: ", PageFlags);
        return 0;
    }

    return 1;
}

static BOOLEAN MapStagedServiceImageIntoAddressSpace(
    UINT64 PageMapLevel4PhysicalAddress,
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header,
    const LOS_MEMORY_MANAGER_ELF64_PROGRAM_HEADER *ProgramHeaders,
    UINT64 ImageVirtualBase,
    UINT64 ImagePageCount,
    UINT64 ImagePhysicalBase)
{
    UINT64 PageIndex;

    if (PageMapLevel4PhysicalAddress == 0ULL ||
        Header == 0 ||
        ProgramHeaders == 0 ||
        ImageVirtualBase == 0ULL ||
        ImagePageCount == 0ULL ||
        ImagePhysicalBase == 0ULL)
    {
        return 0;
    }

    PageIndex = 0ULL;
    while (PageIndex < ImagePageCount)
    {
        UINT64 RunStartIndex;
        UINT64 RunPageCount;
        UINT64 RunVirtualAddress;
        UINT64 RunPhysicalAddress;
        UINT64 PageFlags;

        RunStartIndex = PageIndex;
        RunVirtualAddress = ImageVirtualBase + (RunStartIndex * 0x1000ULL);
        RunPhysicalAddress = ImagePhysicalBase + (RunStartIndex * 0x1000ULL);
        PageFlags = ComputeServiceImagePageFlags(Header, ProgramHeaders, RunVirtualAddress);
        if (PageFlags == 0ULL)
        {
            SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_SEGMENT, LOS_MEMORY_MANAGER_PREP_DETAIL_SEGMENT_MAP_STATUS);
            LosKernelTraceHex64("Memory-manager image page had no segment coverage: ", RunVirtualAddress);
            return 0;
        }

        RunPageCount = 1ULL;
        while ((RunStartIndex + RunPageCount) < ImagePageCount)
        {
            UINT64 NextVirtualAddress;
            UINT64 NextPageFlags;

            NextVirtualAddress = ImageVirtualBase + ((RunStartIndex + RunPageCount) * 0x1000ULL);
            NextPageFlags = ComputeServiceImagePageFlags(Header, ProgramHeaders, NextVirtualAddress);
            if (NextPageFlags != PageFlags)
            {
                break;
            }
            RunPageCount += 1ULL;
        }

        if (!MapServiceImageRunIntoAddressSpace(
                PageMapLevel4PhysicalAddress,
                RunVirtualAddress,
                RunPhysicalAddress,
                RunPageCount,
                PageFlags))
        {
            return 0;
        }

        PageIndex = RunStartIndex + RunPageCount;
    }

    return 1;
}

static BOOLEAN MapServiceStackIntoAddressSpace(
    UINT64 PageMapLevel4PhysicalAddress,
    UINT64 StackBaseVirtualAddress,
    UINT64 StackPhysicalAddress,
    UINT64 StackPageCount,
    UINT64 *StackTopVirtualAddress)
{
    LOS_X64_MAP_PAGES_REQUEST MapRequest;
    LOS_X64_MAP_PAGES_RESULT MapResult;
    if (PageMapLevel4PhysicalAddress == 0ULL ||
        StackBaseVirtualAddress == 0ULL ||
        StackPhysicalAddress == 0ULL ||
        StackPageCount == 0ULL ||
        StackTopVirtualAddress == 0)
    {
        return 0;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_STACK, LOS_MEMORY_MANAGER_PREP_DETAIL_BEGIN);
    *StackTopVirtualAddress = 0ULL;

    ZeroMemory(&MapRequest, sizeof(MapRequest));
    ZeroMemory(&MapResult, sizeof(MapResult));
    MapRequest.PageMapLevel4PhysicalAddress = PageMapLevel4PhysicalAddress;
    MapRequest.VirtualAddress = StackBaseVirtualAddress;
    MapRequest.PhysicalAddress = StackPhysicalAddress;
    MapRequest.PageCount = StackPageCount;
    MapRequest.PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE | LOS_X64_PAGE_USER | LOS_X64_PAGE_NX;
    MapRequest.Flags = 0U;
    LosX64MapPages(&MapRequest, &MapResult);
    if (MapResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS && MapResult.PagesProcessed == StackPageCount)
    {
        *StackTopVirtualAddress = StackBaseVirtualAddress + (StackPageCount * 0x1000ULL);
        return 1;
    }

    SetKernelPrepareDiagnostic(LOS_MEMORY_MANAGER_PREP_STAGE_MAP_STACK, LOS_MEMORY_MANAGER_PREP_DETAIL_STACK_MAP_STATUS);
    LosKernelTraceUnsigned("Memory-manager stack map status: ", MapResult.Status);
    LosKernelTraceHex64("Memory-manager stack map pages processed: ", MapResult.PagesProcessed);
    LosKernelTraceHex64("Memory-manager stack map requested pages: ", StackPageCount);
    return 0;
}
