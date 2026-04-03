#include "BootInternal.h"

#if !defined(LIBERATION_BOOT_FROM_ISO)
EFI_STATUS LosBootOpenRootForHandle(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE DeviceHandle, EFI_FILE_PROTOCOL **Root)
{
    LOS_BOOT_ENTER(SystemTable);
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

EFI_STATUS LosBootReadFileInfo(EFI_FILE_PROTOCOL *File, EFI_FILE_INFO *FileInfo, UINTN FileInfoBufferSize)
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

EFI_STATUS LosBootLocateHandleBufferByProtocol(EFI_SYSTEM_TABLE *SystemTable, EFI_GUID *ProtocolGuid, EFI_HANDLE **Handles, UINTN *HandleCount)
{
    LOS_BOOT_ENTER(SystemTable);
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
#endif
