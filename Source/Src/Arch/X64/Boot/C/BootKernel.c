#include "BootInternal.h"

#if !defined(LIBERATION_BOOT_FROM_ISO)
EFI_STATUS LosBootLoadKernelFileFromRoot(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL *Root, const CHAR16 *KernelPath, void **KernelEntryAddress, UINTN *KernelImageSize)
{
    LOS_BOOT_ENTER(SystemTable);
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

    if (SystemTable == 0 || SystemTable->BootServices == 0 || Root == 0 || KernelPath == 0 || KernelEntryAddress == 0 || KernelImageSize == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *KernelEntryAddress = 0;
    *KernelImageSize = 0;
    KernelFile = 0;
    FileBuffer = 0;

    Status = Root->Open(Root, &KernelFile, (CHAR16 *)KernelPath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || KernelFile == 0)
    {
        return Status;
    }

    FileInfo = (EFI_FILE_INFO *)(void *)FileInfoBuffer;
    Status = LosBootReadFileInfo(KernelFile, FileInfo, sizeof(FileInfoBuffer));
    if (EFI_ERROR(Status))
    {
        KernelFile->Close(KernelFile);
        return Status;
    }

    if (FileInfo->FileSize < sizeof(LOS_ELF64_HEADER))
    {
        KernelFile->Close(KernelFile);
        return EFI_LOAD_ERROR;
    }

    Status = SystemTable->BootServices->AllocatePool(EfiLoaderData, (UINTN)FileInfo->FileSize, &FileBuffer);
    if (EFI_ERROR(Status) || FileBuffer == 0)
    {
        KernelFile->Close(KernelFile);
        return Status;
    }

    ReadSize = (UINTN)FileInfo->FileSize;
    Status = KernelFile->Read(KernelFile, &ReadSize, FileBuffer);
    KernelFile->Close(KernelFile);
    if (EFI_ERROR(Status) || ReadSize != (UINTN)FileInfo->FileSize)
    {
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

    ElfHeader = (const LOS_ELF64_HEADER *)FileBuffer;
    if (ElfHeader->Ident[0] != LOS_ELF_MAGIC_0 ||
        ElfHeader->Ident[1] != LOS_ELF_MAGIC_1 ||
        ElfHeader->Ident[2] != LOS_ELF_MAGIC_2 ||
        ElfHeader->Ident[3] != LOS_ELF_MAGIC_3 ||
        ElfHeader->Ident[4] != LOS_ELF_CLASS_64 ||
        ElfHeader->Ident[5] != LOS_ELF_DATA_LITTLE_ENDIAN ||
        ElfHeader->Machine != LOS_ELF_MACHINE_X86_64 ||
        ElfHeader->Type != LOS_ELF_TYPE_EXEC ||
        ElfHeader->ProgramHeaderCount == 0U ||
        ElfHeader->ProgramHeaderEntrySize != sizeof(LOS_ELF64_PROGRAM_HEADER) ||
        ElfHeader->ProgramHeaderOffset > FileInfo->FileSize)
    {
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

    if ((UINT64)ElfHeader->ProgramHeaderCount > ((UINT64)FileInfo->FileSize - ElfHeader->ProgramHeaderOffset) / sizeof(LOS_ELF64_PROGRAM_HEADER))
    {
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

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
        SegmentAddress = LosBootAlignDown(ProgramHeader->PhysicalAddress != 0ULL ? ProgramHeader->PhysicalAddress : ProgramHeader->VirtualAddress, LOS_PAGE_SIZE);
        SegmentEndAddress = LosBootAlignUp((ProgramHeader->PhysicalAddress != 0ULL ? ProgramHeader->PhysicalAddress : ProgramHeader->VirtualAddress) + ProgramHeader->MemorySize, LOS_PAGE_SIZE);
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
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

    AllocationBase = LowestAddress;
    AllocationLimit = HighestAddress;
    if (AllocationLimit <= AllocationBase)
    {
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

    AllocationAddress = AllocationBase;
    PageCount = (UINTN)((AllocationLimit - AllocationBase) / LOS_PAGE_SIZE);
    Status = SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, PageCount, &AllocationAddress);
    if (EFI_ERROR(Status) || AllocationAddress != AllocationBase)
    {
        SystemTable->BootServices->FreePool(FileBuffer);
        return Status;
    }

    LoadedBase = (UINT8 *)(UINTN)AllocationAddress;
    LosBootMemorySet(LoadedBase, 0, (UINTN)(AllocationLimit - AllocationBase));
    for (ProgramIndex = 0; ProgramIndex < ElfHeader->ProgramHeaderCount; ++ProgramIndex)
    {
        const LOS_ELF64_PROGRAM_HEADER *ProgramHeader = &ProgramHeaders[ProgramIndex];
        UINT8 *SegmentDestination;

        if (ProgramHeader->Type != LOS_ELF_PROGRAM_TYPE_LOAD)
        {
            continue;
        }
        if (ProgramHeader->Offset > FileInfo->FileSize || ProgramHeader->FileSize > (UINT64)FileInfo->FileSize - ProgramHeader->Offset)
        {
            SystemTable->BootServices->FreePool(FileBuffer);
            return EFI_LOAD_ERROR;
        }

        SegmentDestination = (UINT8 *)(UINTN)(ProgramHeader->PhysicalAddress != 0ULL ? ProgramHeader->PhysicalAddress : ProgramHeader->VirtualAddress);
        LosBootMemoryCopy(SegmentDestination, (const UINT8 *)FileBuffer + ProgramHeader->Offset, (UINTN)ProgramHeader->FileSize);
    }

    EntryAddress = ElfHeader->Entry;
    if (EntryAddress < AllocationBase || EntryAddress >= AllocationLimit)
    {
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

    SystemTable->BootServices->FreePool(FileBuffer);
    *KernelEntryAddress = (void *)(UINTN)EntryAddress;
    *KernelImageSize = (UINTN)(AllocationLimit - AllocationBase);
    return EFI_SUCCESS;
}

EFI_STATUS LosBootLoadKernelFromSiblingFileSystem(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *KernelPath, void **KernelEntryAddress, UINTN *KernelImageSize)
{
    LOS_BOOT_ENTER(SystemTable);
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_HANDLE *Handles;
    UINTN HandleCount;
    EFI_STATUS Status;
    UINTN HandleIndex;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || KernelPath == 0 || KernelEntryAddress == 0 || KernelImageSize == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *KernelEntryAddress = 0;
    *KernelImageSize = 0;
    LoadedImage = 0;
    Handles = 0;
    HandleCount = 0;

    Status = SystemTable->BootServices->HandleProtocol(ImageHandle, (EFI_GUID *)&EfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    if (EFI_ERROR(Status) || LoadedImage == 0)
    {
        return Status;
    }

    Status = LosBootLocateHandleBufferByProtocol(SystemTable, (EFI_GUID *)&EfiSimpleFileSystemProtocolGuid, &Handles, &HandleCount);
    if (EFI_ERROR(Status) || Handles == 0 || HandleCount == 0U)
    {
        return Status;
    }

    for (HandleIndex = 0; HandleIndex < HandleCount; ++HandleIndex)
    {
        EFI_FILE_PROTOCOL *Root = 0;
        EFI_STATUS LoadStatus;

        if (Handles[HandleIndex] == LoadedImage->DeviceHandle)
        {
            continue;
        }

        Status = LosBootOpenRootForHandle(SystemTable, Handles[HandleIndex], &Root);
        if (EFI_ERROR(Status) || Root == 0)
        {
            continue;
        }

        LoadStatus = LosBootLoadKernelFileFromRoot(SystemTable, Root, KernelPath, KernelEntryAddress, KernelImageSize);
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

EFI_STATUS LosBootLoadKernelFile(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *KernelPath, void **KernelEntryAddress, UINTN *KernelImageSize)
{
    LOS_BOOT_ENTER(SystemTable);
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_FILE_PROTOCOL *Root;
    EFI_STATUS Status;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || KernelPath == 0 || KernelEntryAddress == 0 || KernelImageSize == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *KernelEntryAddress = 0;
    *KernelImageSize = 0;
    LoadedImage = 0;
    Root = 0;

    Status = SystemTable->BootServices->HandleProtocol(ImageHandle, (EFI_GUID *)&EfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    if (EFI_ERROR(Status) || LoadedImage == 0)
    {
        return Status;
    }

    Status = LosBootOpenRootForHandle(SystemTable, LoadedImage->DeviceHandle, &Root);
    if (EFI_ERROR(Status) || Root == 0)
    {
        return Status;
    }

    Status = LosBootLoadKernelFileFromRoot(SystemTable, Root, KernelPath, KernelEntryAddress, KernelImageSize);
    Root->Close(Root);
    if (!EFI_ERROR(Status))
    {
        return EFI_SUCCESS;
    }

    Status = LosBootLoadKernelFromSiblingFileSystem(ImageHandle, SystemTable, KernelPath, KernelEntryAddress, KernelImageSize);
    if (!EFI_ERROR(Status))
    {
        return EFI_SUCCESS;
    }

    Status = SystemTable->BootServices->HandleProtocol(ImageHandle, (EFI_GUID *)&EfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    if (EFI_ERROR(Status) || LoadedImage == 0)
    {
        return Status;
    }

    Status = LosBootOpenRootForHandle(SystemTable, LoadedImage->DeviceHandle, &Root);
    if (!EFI_ERROR(Status) && Root != 0)
    {
        EFI_STATUS FallbackStatus = LosBootLoadKernelFileFromRoot(SystemTable, Root, LOS_TEXT("\\EFI\\BOOT\\KERNELX64.ELF"), KernelEntryAddress, KernelImageSize);
        Root->Close(Root);
        if (!EFI_ERROR(FallbackStatus))
        {
            return EFI_SUCCESS;
        }
    }

    return Status;
}

EFI_STATUS LosBootReadTextFile(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *TextPath, CHAR16 **TextBuffer)
{
    LOS_BOOT_ENTER(SystemTable);
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *Root;
    EFI_FILE_PROTOCOL *TextFile;
    EFI_STATUS Status;
    UINT8 FileInfoBuffer[LOS_KERNEL_PATH_MAX_INFO_BUFFER];
    EFI_FILE_INFO *FileInfo;
    UINTN ReadSize;
    UINTN CharacterCount;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || TextBuffer == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *TextBuffer = 0;
    LoadedImage = 0;
    FileSystem = 0;
    Root = 0;
    TextFile = 0;

    Status = SystemTable->BootServices->HandleProtocol(ImageHandle, (EFI_GUID *)&EfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    if (EFI_ERROR(Status) || LoadedImage == 0)
    {
        return Status;
    }

    Status = SystemTable->BootServices->HandleProtocol(LoadedImage->DeviceHandle, (EFI_GUID *)&EfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
    if (EFI_ERROR(Status) || FileSystem == 0)
    {
        return Status;
    }

    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status) || Root == 0)
    {
        return Status;
    }

    Status = Root->Open(Root, &TextFile, (CHAR16 *)TextPath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || TextFile == 0)
    {
        Root->Close(Root);
        return Status;
    }

    FileInfo = (EFI_FILE_INFO *)(void *)FileInfoBuffer;
    Status = LosBootReadFileInfo(TextFile, FileInfo, sizeof(FileInfoBuffer));
    if (EFI_ERROR(Status))
    {
        TextFile->Close(TextFile);
        Root->Close(Root);
        return Status;
    }

    if (FileInfo->FileSize < sizeof(CHAR16))
    {
        TextFile->Close(TextFile);
        Root->Close(Root);
        return EFI_NOT_FOUND;
    }

    CharacterCount = (UINTN)(FileInfo->FileSize / sizeof(CHAR16));
    if (CharacterCount > LOS_BOOT_INFO_MAX_CHARACTERS - 1U)
    {
        CharacterCount = LOS_BOOT_INFO_MAX_CHARACTERS - 1U;
    }

    Status = SystemTable->BootServices->AllocatePool(EfiLoaderData, LOS_BOOT_INFO_MAX_CHARACTERS * sizeof(CHAR16), (void **)TextBuffer);
    if (EFI_ERROR(Status) || *TextBuffer == 0)
    {
        TextFile->Close(TextFile);
        Root->Close(Root);
        return Status;
    }

    LosBootMemorySet(*TextBuffer, 0, LOS_BOOT_INFO_MAX_CHARACTERS * sizeof(CHAR16));
    ReadSize = CharacterCount * sizeof(CHAR16);
    Status = TextFile->Read(TextFile, &ReadSize, *TextBuffer);
    TextFile->Close(TextFile);
    Root->Close(Root);
    if (EFI_ERROR(Status))
    {
        SystemTable->BootServices->FreePool(*TextBuffer);
        *TextBuffer = 0;
        return Status;
    }

    (*TextBuffer)[CharacterCount] = 0;
    return EFI_SUCCESS;
}
#endif
