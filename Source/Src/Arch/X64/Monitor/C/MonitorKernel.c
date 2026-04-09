/*
 * File Name: MonitorKernel.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements monitor-stage functionality for Liberation OS.
 */

#include "MonitorInternal.h"

EFI_STATUS LosMonitorLoadKernelFileFromRoot(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL *Root, const CHAR16 *KernelPath, void **KernelEntryAddress, UINT64 *KernelImagePhysicalAddress, UINTN *KernelImageSize, LOS_BOOT_CONTEXT_LOAD_SEGMENT *KernelLoadSegments, UINT64 *KernelLoadSegmentCount)
{
    LOS_MONITOR_ENTER(SystemTable);
    EFI_FILE_PROTOCOL *KernelFile;
    EFI_STATUS Status;
    UINT8 FileInfoBuffer[LOS_KERNEL_PATH_MAX_INFO_BUFFER];
    EFI_FILE_INFO *FileInfo;
    UINTN ReadSize;
    void *FileBuffer;
    const LOS_ELF64_HEADER *ElfHeader;
    const LOS_ELF64_PROGRAM_HEADER *ProgramHeaders;
    UINT16 ProgramIndex;
    UINT64 LowestAddress;
    UINT64 HighestAddress;
    UINT64 SegmentAddress;
    UINT64 SegmentEndAddress;
    UINT64 AllocationBase;
    UINT64 AllocationLimit;
    UINT64 AllocationAddress;
    UINT64 EntryAddress;
    UINTN PageCount;
    UINT8 *LoadedBase;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || Root == 0 || KernelPath == 0 || KernelEntryAddress == 0 || KernelImagePhysicalAddress == 0 || KernelImageSize == 0 || KernelLoadSegments == 0 || KernelLoadSegmentCount == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *KernelEntryAddress = 0;
    *KernelImagePhysicalAddress = 0ULL;
    *KernelImageSize = 0;
    KernelFile = 0;
    FileBuffer = 0;
    *KernelLoadSegmentCount = 0ULL;
    LosMonitorMemorySet((void *)KernelLoadSegments, 0, sizeof(LOS_BOOT_CONTEXT_LOAD_SEGMENT) * LOS_BOOT_CONTEXT_MAX_LOAD_SEGMENTS);

    LosMonitorTracePath(SystemTable, LOS_TEXT("Open kernel file path: "), KernelPath);
    Status = Root->Open(Root, &KernelFile, (CHAR16 *)KernelPath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || KernelFile == 0)
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("Open kernel file failed: "), Status);
        return Status;
    }

    LosMonitorTrace(SystemTable, LOS_TEXT("Kernel file opened."));

    FileInfo = (EFI_FILE_INFO *)(void *)FileInfoBuffer;
    Status = LosMonitorReadFileInfo(KernelFile, FileInfo, sizeof(FileInfoBuffer));
    if (EFI_ERROR(Status))
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("Read kernel file info failed: "), Status);
        KernelFile->Close(KernelFile);
        return Status;
    }

    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel file size bytes: "), (UINT64)FileInfo->FileSize);
    if (FileInfo->FileSize < sizeof(LOS_ELF64_HEADER))
    {
        KernelFile->Close(KernelFile);
        return EFI_LOAD_ERROR;
    }

    Status = SystemTable->BootServices->AllocatePool(EfiLoaderData, (UINTN)FileInfo->FileSize, &FileBuffer);
    if (EFI_ERROR(Status) || FileBuffer == 0)
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("AllocatePool for kernel file failed: "), Status);
        KernelFile->Close(KernelFile);
        return Status;
    }

    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel file buffer: "), (UINT64)(UINTN)FileBuffer);
    ReadSize = (UINTN)FileInfo->FileSize;
    Status = KernelFile->Read(KernelFile, &ReadSize, FileBuffer);
    KernelFile->Close(KernelFile);
    if (EFI_ERROR(Status) || ReadSize != (UINTN)FileInfo->FileSize)
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("Read kernel file failed: "), Status);
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

    LosMonitorTrace(SystemTable, LOS_TEXT("Kernel ELF file read into memory."));
    ElfHeader = (const LOS_ELF64_HEADER *)FileBuffer;
    if (ElfHeader->Ident[0] != LOS_ELF_MAGIC_0 || ElfHeader->Ident[1] != LOS_ELF_MAGIC_1 || ElfHeader->Ident[2] != LOS_ELF_MAGIC_2 || ElfHeader->Ident[3] != LOS_ELF_MAGIC_3 || ElfHeader->Ident[4] != LOS_ELF_CLASS_64 || ElfHeader->Ident[5] != LOS_ELF_DATA_LITTLE_ENDIAN || ElfHeader->Machine != LOS_ELF_MACHINE_X86_64 || ElfHeader->Type != LOS_ELF_TYPE_EXEC || ElfHeader->ProgramHeaderCount == 0U || ElfHeader->ProgramHeaderEntrySize != sizeof(LOS_ELF64_PROGRAM_HEADER) || ElfHeader->ProgramHeaderOffset > FileInfo->FileSize)
    {
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

    if ((UINT64)ElfHeader->ProgramHeaderCount > ((UINT64)FileInfo->FileSize - ElfHeader->ProgramHeaderOffset) / sizeof(LOS_ELF64_PROGRAM_HEADER))
    {
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel ELF entry: "), ElfHeader->Entry);
    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel program header count: "), ElfHeader->ProgramHeaderCount);
    ProgramHeaders = (const LOS_ELF64_PROGRAM_HEADER *)((const UINT8 *)FileBuffer + ElfHeader->ProgramHeaderOffset);
    LowestAddress = ~0ULL;
    HighestAddress = 0ULL;
    for (ProgramIndex = 0; ProgramIndex < ElfHeader->ProgramHeaderCount; ++ProgramIndex)
    {
        const LOS_ELF64_PROGRAM_HEADER *ProgramHeader = &ProgramHeaders[ProgramIndex];
        if (ProgramHeader->Type != LOS_ELF_PROGRAM_TYPE_LOAD)
        {
            continue;
        }
        if (ProgramHeader->MemorySize < ProgramHeader->FileSize)
        {
            SystemTable->BootServices->FreePool(FileBuffer);
            return EFI_LOAD_ERROR;
        }
        SegmentAddress = LosMonitorAlignDown(ProgramHeader->PhysicalAddress != 0ULL ? ProgramHeader->PhysicalAddress : ProgramHeader->VirtualAddress, LOS_PAGE_SIZE);
        SegmentEndAddress = LosMonitorAlignUp((ProgramHeader->PhysicalAddress != 0ULL ? ProgramHeader->PhysicalAddress : ProgramHeader->VirtualAddress) + ProgramHeader->MemorySize, LOS_PAGE_SIZE);
        if (SegmentEndAddress < SegmentAddress)
        {
            SystemTable->BootServices->FreePool(FileBuffer);
            return EFI_LOAD_ERROR;
        }
        if (SegmentAddress < LowestAddress)
        {
            LowestAddress = SegmentAddress;
        }
        if (SegmentEndAddress > HighestAddress)
        {
            HighestAddress = SegmentEndAddress;
        }
    }

    if (LowestAddress == ~0ULL || HighestAddress <= LowestAddress)
    {
        LosMonitorTrace(SystemTable, LOS_TEXT("Kernel ELF had no loadable segments."));
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

    AllocationBase = LowestAddress;
    AllocationLimit = HighestAddress;
    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel allocation base: "), AllocationBase);
    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel allocation limit: "), AllocationLimit);
    AllocationAddress = AllocationBase;
    PageCount = (UINTN)((AllocationLimit - AllocationBase) / LOS_PAGE_SIZE);
    Status = SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, PageCount, &AllocationAddress);
    if (EFI_ERROR(Status) || AllocationAddress != AllocationBase)
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("AllocatePages for kernel image failed: "), Status);
        SystemTable->BootServices->FreePool(FileBuffer);
        return Status;
    }

    LoadedBase = (UINT8 *)(UINTN)AllocationAddress;
    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel loaded base: "), (UINT64)(UINTN)LoadedBase);
    LosMonitorMemorySet(LoadedBase, 0, (UINTN)(AllocationLimit - AllocationBase));
    for (ProgramIndex = 0; ProgramIndex < ElfHeader->ProgramHeaderCount; ++ProgramIndex)
    {
        const LOS_ELF64_PROGRAM_HEADER *ProgramHeader = &ProgramHeaders[ProgramIndex];
        if (ProgramHeader->Type != LOS_ELF_PROGRAM_TYPE_LOAD)
        {
            continue;
        }
        if (ProgramHeader->Offset > FileInfo->FileSize || ProgramHeader->FileSize > (UINT64)FileInfo->FileSize - ProgramHeader->Offset)
        {
            SystemTable->BootServices->FreePool(FileBuffer);
            return EFI_LOAD_ERROR;
        }
        UINT64 SegmentPhysicalAddress;
        SegmentPhysicalAddress = ProgramHeader->PhysicalAddress != 0ULL ? ProgramHeader->PhysicalAddress : ProgramHeader->VirtualAddress;
        LosMonitorTraceHex64(SystemTable, LOS_TEXT("Copy segment destination: "), SegmentPhysicalAddress);
        LosMonitorTraceHex64(SystemTable, LOS_TEXT("Copy segment file bytes: "), ProgramHeader->FileSize);
        LosMonitorTraceHex64(SystemTable, LOS_TEXT("Copy segment memory bytes: "), ProgramHeader->MemorySize);
        LosMonitorMemoryCopy((void *)(UINTN)SegmentPhysicalAddress, (const UINT8 *)FileBuffer + ProgramHeader->Offset, (UINTN)ProgramHeader->FileSize);
        if (*KernelLoadSegmentCount < LOS_BOOT_CONTEXT_MAX_LOAD_SEGMENTS)
        {
            LOS_BOOT_CONTEXT_LOAD_SEGMENT *LoadSegment;
            LoadSegment = &KernelLoadSegments[*KernelLoadSegmentCount];
            LoadSegment->VirtualAddress = ProgramHeader->VirtualAddress;
            LoadSegment->PhysicalAddress = SegmentPhysicalAddress;
            LoadSegment->FileSize = ProgramHeader->FileSize;
            LoadSegment->MemorySize = ProgramHeader->MemorySize;
            LoadSegment->Flags = ProgramHeader->Flags;
            LoadSegment->Reserved = 0ULL;
            *KernelLoadSegmentCount += 1ULL;
        }
    }

    EntryAddress = ElfHeader->Entry;
    if (EntryAddress < AllocationBase || EntryAddress >= AllocationLimit)
    {
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

    SystemTable->BootServices->FreePool(FileBuffer);
    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Kernel entry address validated: "), EntryAddress);
    *KernelEntryAddress = (void *)(UINTN)EntryAddress;
    *KernelImagePhysicalAddress = AllocationBase;
    *KernelImageSize = (UINTN)(AllocationLimit - AllocationBase);
    return EFI_SUCCESS;
}

EFI_STATUS LosMonitorLoadKernelFromSiblingFileSystemHandle(EFI_HANDLE DeviceHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *KernelPath, void **KernelEntryAddress, UINT64 *KernelImagePhysicalAddress, UINTN *KernelImageSize, LOS_BOOT_CONTEXT_LOAD_SEGMENT *KernelLoadSegments, UINT64 *KernelLoadSegmentCount)
{
    LOS_MONITOR_ENTER(SystemTable);
    EFI_HANDLE *Handles;
    UINTN HandleCount;
    EFI_STATUS Status;
    UINTN HandleIndex;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || KernelPath == 0 || KernelEntryAddress == 0 || KernelImagePhysicalAddress == 0 || KernelImageSize == 0 || KernelLoadSegments == 0 || KernelLoadSegmentCount == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *KernelEntryAddress = 0;
    *KernelImagePhysicalAddress = 0ULL;
    *KernelImageSize = 0;
    Handles = 0;
    HandleCount = 0;

    Status = LosMonitorLocateHandleBufferByProtocol(SystemTable, (EFI_GUID *)&EfiSimpleFileSystemProtocolGuid, &Handles, &HandleCount);
    if (EFI_ERROR(Status) || Handles == 0 || HandleCount == 0U)
    {
        LosMonitorTraceStatus(SystemTable, LOS_TEXT("LocateHandleBuffer(SimpleFileSystem) failed: "), Status);
        return Status;
    }

    LosMonitorTraceHex64(SystemTable, LOS_TEXT("Sibling filesystem handle count: "), (UINT64)HandleCount);
    for (HandleIndex = 0U; HandleIndex < HandleCount; ++HandleIndex)
    {
        EFI_FILE_PROTOCOL *Root = 0;
        EFI_STATUS LoadStatus;

        if (Handles[HandleIndex] == DeviceHandle)
        {
            continue;
        }

        Status = LosMonitorOpenRootForHandle(SystemTable, Handles[HandleIndex], &Root);
        if (EFI_ERROR(Status) || Root == 0)
        {
            continue;
        }

        LoadStatus = LosMonitorLoadKernelFileFromRoot(SystemTable, Root, KernelPath, KernelEntryAddress, KernelImagePhysicalAddress, KernelImageSize, KernelLoadSegments, KernelLoadSegmentCount);
        Root->Close(Root);
        if (!EFI_ERROR(LoadStatus))
        {
            SystemTable->BootServices->FreePool(Handles);
            return EFI_SUCCESS;
        }
        Status = LoadStatus;
    }

    SystemTable->BootServices->FreePool(Handles);
    return Status;
}
