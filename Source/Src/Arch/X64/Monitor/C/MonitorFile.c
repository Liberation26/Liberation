/*
 * File Name: MonitorFile.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements monitor-stage functionality for Liberation OS.
 */

#include "MonitorInternal.h"

EFI_STATUS LosMonitorOpenRootForHandle(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE DeviceHandle, EFI_FILE_PROTOCOL **Root)
{
    LOS_MONITOR_ENTER(SystemTable);
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_STATUS Status;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || Root == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *Root = 0;
    FileSystem = 0;
    Status = SystemTable->BootServices->HandleProtocol(DeviceHandle, (EFI_GUID *)&EfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
    if (EFI_ERROR(Status) || FileSystem == 0)
    {
        return Status;
    }

    return FileSystem->OpenVolume(FileSystem, Root);
}

EFI_STATUS LosMonitorReadFileInfo(EFI_FILE_PROTOCOL *File, EFI_FILE_INFO *FileInfo, UINTN FileInfoBufferSize)
{
    EFI_STATUS Status;
    UINTN FileInfoSize;

    if (File == 0 || FileInfo == 0 || FileInfoBufferSize < sizeof(EFI_FILE_INFO))
    {
        return EFI_INVALID_PARAMETER;
    }

    FileInfoSize = FileInfoBufferSize;
    Status = File->GetInfo(File, (EFI_GUID *)&EfiFileInfoGuid, &FileInfoSize, FileInfo);
    if (EFI_ERROR(Status) || FileInfoSize < sizeof(EFI_FILE_INFO))
    {
        return EFI_LOAD_ERROR;
    }

    return EFI_SUCCESS;
}

EFI_STATUS LosMonitorLocateHandleBufferByProtocol(EFI_SYSTEM_TABLE *SystemTable, EFI_GUID *ProtocolGuid, EFI_HANDLE **Handles, UINTN *HandleCount)
{
    LOS_MONITOR_ENTER(SystemTable);
    EFI_STATUS Status;
    UINTN Size;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || ProtocolGuid == 0 || Handles == 0 || HandleCount == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *Handles = 0;
    *HandleCount = 0;
    Size = 0;
    Status = SystemTable->BootServices->LocateHandle(ByProtocol, ProtocolGuid, 0, &Size, 0);
    if (Status != EFI_BUFFER_TOO_SMALL || Size == 0U)
    {
        return Status;
    }

    Status = SystemTable->BootServices->AllocatePool(EfiLoaderData, Size, (void **)Handles);
    if (EFI_ERROR(Status) || *Handles == 0)
    {
        return Status;
    }

    Status = SystemTable->BootServices->LocateHandle(ByProtocol, ProtocolGuid, 0, &Size, *Handles);
    if (EFI_ERROR(Status))
    {
        SystemTable->BootServices->FreePool(*Handles);
        *Handles = 0;
        return Status;
    }

    *HandleCount = Size / sizeof(EFI_HANDLE);
    return EFI_SUCCESS;
}

EFI_STATUS LosMonitorGetParentDeviceHandle(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE *ParentDeviceHandle)
{
    LOS_MONITOR_ENTER(SystemTable);
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || ParentDeviceHandle == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *ParentDeviceHandle = 0;
    LoadedImage = 0;
    if (EFI_ERROR(SystemTable->BootServices->HandleProtocol(ImageHandle, (EFI_GUID *)&EfiLoadedImageProtocolGuid, (void **)&LoadedImage)) || LoadedImage == 0)
    {
        return EFI_NOT_FOUND;
    }

    *ParentDeviceHandle = LoadedImage->DeviceHandle;
    if (*ParentDeviceHandle == 0)
    {
        LosMonitorTrace(SystemTable, LOS_TEXT("LoadedImage->DeviceHandle is null."));
    }
    else
    {
        LosMonitorTraceHex64(SystemTable, LOS_TEXT("LoadedImage->DeviceHandle: "), (UINT64)(UINTN)(*ParentDeviceHandle));
    }

    return EFI_SUCCESS;
}

EFI_STATUS LosMonitorReadTextFileFromRoot(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL *Root, const CHAR16 *TextPath, CHAR16 **TextBuffer)
{
    LOS_MONITOR_ENTER(SystemTable);
    EFI_FILE_PROTOCOL *TextFile;
    EFI_STATUS Status;
    UINT8 FileInfoBuffer[LOS_KERNEL_PATH_MAX_INFO_BUFFER];
    EFI_FILE_INFO *FileInfo;
    UINTN ReadSize;
    UINTN CharacterCount;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || Root == 0 || TextPath == 0 || TextBuffer == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *TextBuffer = 0;
    TextFile = 0;

    Status = Root->Open(Root, &TextFile, (CHAR16 *)TextPath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || TextFile == 0)
    {
        return Status;
    }

    FileInfo = (EFI_FILE_INFO *)(void *)FileInfoBuffer;
    Status = LosMonitorReadFileInfo(TextFile, FileInfo, sizeof(FileInfoBuffer));
    if (EFI_ERROR(Status))
    {
        TextFile->Close(TextFile);
        return Status;
    }

    if (FileInfo->FileSize < sizeof(CHAR16))
    {
        TextFile->Close(TextFile);
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
        return Status;
    }

    LosMonitorMemorySet(*TextBuffer, 0, LOS_BOOT_INFO_MAX_CHARACTERS * sizeof(CHAR16));
    ReadSize = CharacterCount * sizeof(CHAR16);
    Status = TextFile->Read(TextFile, &ReadSize, *TextBuffer);
    TextFile->Close(TextFile);
    if (EFI_ERROR(Status))
    {
        SystemTable->BootServices->FreePool(*TextBuffer);
        *TextBuffer = 0;
        return Status;
    }

    (*TextBuffer)[CharacterCount] = 0;
    return EFI_SUCCESS;
}

EFI_STATUS LosMonitorReadBinaryFileFromRoot(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL *Root, const CHAR16 *FilePath, UINT64 *FilePhysicalAddress, UINT64 *FileSize)
{
    EFI_FILE_PROTOCOL *File;
    EFI_STATUS Status;
    UINT8 FileInfoBuffer[LOS_KERNEL_PATH_MAX_INFO_BUFFER];
    EFI_FILE_INFO *FileInfo;
    UINTN ReadSize;
    UINTN PageCount;
    UINT64 PhysicalAddress;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || Root == 0 || FilePath == 0 || FilePhysicalAddress == 0 || FileSize == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *FilePhysicalAddress = 0ULL;
    *FileSize = 0ULL;
    File = 0;
    Status = Root->Open(Root, &File, (CHAR16 *)FilePath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || File == 0)
    {
        return Status;
    }

    FileInfo = (EFI_FILE_INFO *)(void *)FileInfoBuffer;
    Status = LosMonitorReadFileInfo(File, FileInfo, sizeof(FileInfoBuffer));
    if (EFI_ERROR(Status) || FileInfo->FileSize == 0ULL)
    {
        File->Close(File);
        return EFI_NOT_FOUND;
    }

    PageCount = (UINTN)((FileInfo->FileSize + (LOS_PAGE_SIZE - 1ULL)) / LOS_PAGE_SIZE);
    PhysicalAddress = 0ULL;
    Status = SystemTable->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, PageCount, &PhysicalAddress);
    if (EFI_ERROR(Status) || PhysicalAddress == 0ULL)
    {
        File->Close(File);
        return Status;
    }

    LosMonitorMemorySet((void *)(UINTN)PhysicalAddress, 0, PageCount * (UINTN)LOS_PAGE_SIZE);
    ReadSize = (UINTN)FileInfo->FileSize;
    Status = File->Read(File, &ReadSize, (void *)(UINTN)PhysicalAddress);
    File->Close(File);
    if (EFI_ERROR(Status) || ReadSize != (UINTN)FileInfo->FileSize)
    {
        SystemTable->BootServices->FreePages(PhysicalAddress, PageCount);
        return EFI_LOAD_ERROR;
    }

    *FilePhysicalAddress = PhysicalAddress;
    *FileSize = (UINT64)FileInfo->FileSize;
    return EFI_SUCCESS;
}

EFI_STATUS LosMonitorReadBinaryFileFromSiblingFileSystemHandle(EFI_HANDLE DeviceHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *FilePath, UINT64 *FilePhysicalAddress, UINT64 *FileSize)
{
    EFI_HANDLE *Handles;
    UINTN HandleCount;
    UINTN HandleIndex;
    EFI_STATUS Status;

    if (SystemTable == 0 || FilePath == 0 || FilePhysicalAddress == 0 || FileSize == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *FilePhysicalAddress = 0ULL;
    *FileSize = 0ULL;
    Handles = 0;
    HandleCount = 0U;
    Status = LosMonitorLocateHandleBufferByProtocol(SystemTable, (EFI_GUID *)&EfiSimpleFileSystemProtocolGuid, &Handles, &HandleCount);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    for (HandleIndex = 0U; HandleIndex < HandleCount; ++HandleIndex)
    {
        EFI_FILE_PROTOCOL *Root;
        EFI_STATUS ReadStatus;

        if (Handles[HandleIndex] == DeviceHandle)
        {
            continue;
        }

        Root = 0;
        Status = LosMonitorOpenRootForHandle(SystemTable, Handles[HandleIndex], &Root);
        if (EFI_ERROR(Status) || Root == 0)
        {
            continue;
        }

        ReadStatus = LosMonitorReadBinaryFileFromRoot(SystemTable, Root, FilePath, FilePhysicalAddress, FileSize);
        Root->Close(Root);
        if (!EFI_ERROR(ReadStatus))
        {
            SystemTable->BootServices->FreePool(Handles);
            return EFI_SUCCESS;
        }
    }

    SystemTable->BootServices->FreePool(Handles);
    return EFI_NOT_FOUND;
}

BOOLEAN LosMonitorElf64ValidateLoadedImage(const void *ImageBase, UINT64 ImageSize, UINT64 *EntryAddress)
{
    const LOS_ELF64_HEADER *ElfHeader;

    if (EntryAddress != 0)
    {
        *EntryAddress = 0ULL;
    }

    if (ImageBase == 0 || ImageSize < sizeof(LOS_ELF64_HEADER))
    {
        return 0;
    }

    ElfHeader = (const LOS_ELF64_HEADER *)ImageBase;
    if (ElfHeader->Ident[0] != LOS_ELF_MAGIC_0 ||
        ElfHeader->Ident[1] != LOS_ELF_MAGIC_1 ||
        ElfHeader->Ident[2] != LOS_ELF_MAGIC_2 ||
        ElfHeader->Ident[3] != LOS_ELF_MAGIC_3 ||
        ElfHeader->Ident[4] != LOS_ELF_CLASS_64 ||
        ElfHeader->Ident[5] != LOS_ELF_DATA_LITTLE_ENDIAN ||
        ElfHeader->Machine != LOS_ELF_MACHINE_X86_64 ||
        ElfHeader->Type != LOS_ELF_TYPE_EXEC ||
        ElfHeader->ProgramHeaderEntrySize != sizeof(LOS_ELF64_PROGRAM_HEADER) ||
        ElfHeader->ProgramHeaderCount == 0U)
    {
        return 0;
    }

    if (ElfHeader->ProgramHeaderOffset > ImageSize)
    {
        return 0;
    }

    if ((UINT64)ElfHeader->ProgramHeaderCount > (ImageSize - ElfHeader->ProgramHeaderOffset) / sizeof(LOS_ELF64_PROGRAM_HEADER))
    {
        return 0;
    }

    if (EntryAddress != 0)
    {
        *EntryAddress = ElfHeader->Entry;
    }

    return 1;
}
