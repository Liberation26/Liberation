/*
 * File Name: BootMonitor.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "BootInternal.h"

#if !defined(LIBERATION_BOOT_FROM_ISO)
const CHAR16 *const LosBootMonitorPath = LOS_TEXT("\\EFI\\BOOT\\MONITORX64.EFI");

#define LOS_EFI_DEVICE_PATH_TYPE_MEDIA 0x04U
#define LOS_EFI_DEVICE_PATH_SUBTYPE_FILE_PATH 0x04U
#define LOS_EFI_DEVICE_PATH_TYPE_END 0x7FU
#define LOS_EFI_DEVICE_PATH_SUBTYPE_END_ENTIRE 0xFFU

typedef struct __attribute__((packed))
{
    UINT8 Type;
    UINT8 SubType;
    UINT8 Length[2];
} LOS_EFI_DEVICE_PATH_NODE;

static UINTN LosBootChar16Length(const CHAR16 *Text)
{
    UINTN Length;

    if (Text == 0)
    {
        return 0;
    }

    Length = 0;
    while (Text[Length] != 0)
    {
        Length += 1U;
    }

    return Length;
}

static UINTN LosBootDevicePathNodeLength(const LOS_EFI_DEVICE_PATH_NODE *Node)
{
    if (Node == 0)
    {
        return 0;
    }

    return (UINTN)Node->Length[0] | ((UINTN)Node->Length[1] << 8U);
}

static BOOLEAN LosBootIsDevicePathEnd(const LOS_EFI_DEVICE_PATH_NODE *Node)
{
    if (Node == 0)
    {
        return 1;
    }

    return (BOOLEAN)(Node->Type == LOS_EFI_DEVICE_PATH_TYPE_END && Node->SubType == LOS_EFI_DEVICE_PATH_SUBTYPE_END_ENTIRE);
}

static EFI_STATUS LosBootBuildSiblingFileDevicePath(
    EFI_SYSTEM_TABLE *SystemTable,
    VOID *ExistingFilePath,
    const CHAR16 *ReplacementPath,
    VOID **NewFilePath)
{
    const LOS_EFI_DEVICE_PATH_NODE *CurrentNode;
    const LOS_EFI_DEVICE_PATH_NODE *LastFilePathNode;
    const LOS_EFI_DEVICE_PATH_NODE *EndNode;
    UINTN PrefixSize;
    UINTN ReplacementCharacterCount;
    UINTN NewFilePathNodeSize;
    UINTN NewTotalSize;
    UINT8 *Buffer;
    LOS_EFI_DEVICE_PATH_NODE *NewNode;
    LOS_EFI_DEVICE_PATH_NODE *NewEndNode;
    UINTN CharacterIndex;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || ExistingFilePath == 0 || ReplacementPath == 0 || NewFilePath == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    LosBootTracePath(SystemTable, LOS_TEXT("Build sibling device path for "), ReplacementPath);

    *NewFilePath = 0;
    CurrentNode = (const LOS_EFI_DEVICE_PATH_NODE *)ExistingFilePath;
    LastFilePathNode = 0;
    EndNode = 0;

    for (;;)
    {
        UINTN NodeLength;

        NodeLength = LosBootDevicePathNodeLength(CurrentNode);
        if (NodeLength < sizeof(LOS_EFI_DEVICE_PATH_NODE))
        {
            return EFI_LOAD_ERROR;
        }

        if (LosBootIsDevicePathEnd(CurrentNode))
        {
            EndNode = CurrentNode;
            break;
        }

        if (CurrentNode->Type == LOS_EFI_DEVICE_PATH_TYPE_MEDIA && CurrentNode->SubType == LOS_EFI_DEVICE_PATH_SUBTYPE_FILE_PATH)
        {
            LastFilePathNode = CurrentNode;
        }

        CurrentNode = (const LOS_EFI_DEVICE_PATH_NODE *)((const UINT8 *)CurrentNode + NodeLength);
    }

    if (LastFilePathNode == 0 || EndNode == 0)
    {
        LosBootTrace(SystemTable, LOS_TEXT("No file-path node found in existing device path."));
        return EFI_NOT_FOUND;
    }

    PrefixSize = (UINTN)((const UINT8 *)LastFilePathNode - (const UINT8 *)ExistingFilePath);
    ReplacementCharacterCount = LosBootChar16Length(ReplacementPath);
    NewFilePathNodeSize = sizeof(LOS_EFI_DEVICE_PATH_NODE) + ((ReplacementCharacterCount + 1U) * sizeof(CHAR16));
    NewTotalSize = PrefixSize + NewFilePathNodeSize + sizeof(LOS_EFI_DEVICE_PATH_NODE);

    Buffer = 0;
    if (EFI_ERROR(SystemTable->BootServices->AllocatePool(EfiLoaderData, NewTotalSize, (void **)&Buffer)) || Buffer == 0)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    LosBootMemorySet(Buffer, 0, NewTotalSize);
    LosBootMemoryCopy(Buffer, ExistingFilePath, PrefixSize);

    NewNode = (LOS_EFI_DEVICE_PATH_NODE *)(void *)(Buffer + PrefixSize);
    NewNode->Type = LOS_EFI_DEVICE_PATH_TYPE_MEDIA;
    NewNode->SubType = LOS_EFI_DEVICE_PATH_SUBTYPE_FILE_PATH;
    NewNode->Length[0] = (UINT8)(NewFilePathNodeSize & 0xFFU);
    NewNode->Length[1] = (UINT8)((NewFilePathNodeSize >> 8U) & 0xFFU);

    for (CharacterIndex = 0; CharacterIndex < ReplacementCharacterCount; CharacterIndex += 1U)
    {
        ((CHAR16 *)(void *)(NewNode + 1))[CharacterIndex] = ReplacementPath[CharacterIndex];
    }
    ((CHAR16 *)(void *)(NewNode + 1))[ReplacementCharacterCount] = 0;

    NewEndNode = (LOS_EFI_DEVICE_PATH_NODE *)(void *)(Buffer + PrefixSize + NewFilePathNodeSize);
    NewEndNode->Type = LOS_EFI_DEVICE_PATH_TYPE_END;
    NewEndNode->SubType = LOS_EFI_DEVICE_PATH_SUBTYPE_END_ENTIRE;
    NewEndNode->Length[0] = (UINT8)sizeof(LOS_EFI_DEVICE_PATH_NODE);
    NewEndNode->Length[1] = 0;

    *NewFilePath = Buffer;
    LosBootTraceHex64(SystemTable, LOS_TEXT("Sibling device path bytes: "), (UINT64)NewTotalSize);
    return EFI_SUCCESS;
}

EFI_STATUS LosBootLaunchMonitor(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *MonitorPath)
{
    LOS_BOOT_ENTER(SystemTable);
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_FILE_PROTOCOL *Root;
    EFI_FILE_PROTOCOL *MonitorFile;
    EFI_STATUS Status;
    UINT8 FileInfoBuffer[LOS_KERNEL_PATH_MAX_INFO_BUFFER];
    EFI_FILE_INFO *FileInfo;
    void *FileBuffer;
    UINTN ReadSize;
    EFI_HANDLE MonitorHandle;
    EFI_LOAD_IMAGE LoadImageFunction;
    EFI_START_IMAGE StartImageFunction;
    void *MonitorDevicePath;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || MonitorPath == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    LoadedImage = 0;
    Root = 0;
    LosBootTracePath(SystemTable, LOS_TEXT("Requested monitor path: "), MonitorPath);
    MonitorFile = 0;
    FileBuffer = 0;
    MonitorHandle = 0;
    MonitorDevicePath = 0;

    Status = SystemTable->BootServices->HandleProtocol(ImageHandle, (EFI_GUID *)&EfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    if (EFI_ERROR(Status) || LoadedImage == 0)
    {
        LosBootTraceStatus(SystemTable, LOS_TEXT("HandleProtocol(LoadedImage) failed: "), Status);
        return Status;
    }

    LosBootTraceHex64(SystemTable, LOS_TEXT("Loader image base: "), (UINT64)(UINTN)LoadedImage->ImageBase);
    LosBootTraceHex64(SystemTable, LOS_TEXT("Loader image size: "), (UINT64)LoadedImage->ImageSize);

    Status = LosBootOpenRootForHandle(SystemTable, LoadedImage->DeviceHandle, &Root);
    if (EFI_ERROR(Status) || Root == 0)
    {
        LosBootTraceStatus(SystemTable, LOS_TEXT("Open root for boot device failed: "), Status);
        return Status;
    }

    LosBootTrace(SystemTable, LOS_TEXT("Boot device root opened."));

    LosBootTrace(SystemTable, LOS_TEXT("Opening monitor file from installed ESP..."));
    Status = Root->Open(Root, &MonitorFile, (CHAR16 *)MonitorPath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || MonitorFile == 0)
    {
        LosBootTraceStatus(SystemTable, LOS_TEXT("Open monitor file failed: "), Status);
        Root->Close(Root);
        return Status;
    }

    LosBootTrace(SystemTable, LOS_TEXT("Monitor file opened."));

    FileInfo = (EFI_FILE_INFO *)(void *)FileInfoBuffer;
    Status = LosBootReadFileInfo(MonitorFile, FileInfo, sizeof(FileInfoBuffer));
    if (EFI_ERROR(Status))
    {
        LosBootTraceStatus(SystemTable, LOS_TEXT("Read monitor file info failed: "), Status);
        MonitorFile->Close(MonitorFile);
        Root->Close(Root);
        return Status;
    }

    LosBootTraceHex64(SystemTable, LOS_TEXT("Monitor file size bytes: "), (UINT64)FileInfo->FileSize);

    Status = SystemTable->BootServices->AllocatePool(EfiLoaderData, (UINTN)FileInfo->FileSize, &FileBuffer);
    if (EFI_ERROR(Status) || FileBuffer == 0)
    {
        LosBootTraceStatus(SystemTable, LOS_TEXT("AllocatePool for monitor failed: "), Status);
        MonitorFile->Close(MonitorFile);
        Root->Close(Root);
        return Status;
    }

    LosBootTraceHex64(SystemTable, LOS_TEXT("Monitor file buffer: "), (UINT64)(UINTN)FileBuffer);

    ReadSize = (UINTN)FileInfo->FileSize;
    Status = MonitorFile->Read(MonitorFile, &ReadSize, FileBuffer);
    MonitorFile->Close(MonitorFile);
    Root->Close(Root);
    if (EFI_ERROR(Status) || ReadSize != (UINTN)FileInfo->FileSize)
    {
        LosBootTraceStatus(SystemTable, LOS_TEXT("Monitor file read failed: "), Status);
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_LOAD_ERROR;
    }

    LosBootTrace(SystemTable, LOS_TEXT("Monitor file read into memory."));

    Status = LosBootBuildSiblingFileDevicePath(SystemTable, LoadedImage->FilePath, MonitorPath, &MonitorDevicePath);
    if (EFI_ERROR(Status))
    {
        LosBootTraceStatus(SystemTable, LOS_TEXT("Build monitor device path failed: "), Status);
        SystemTable->BootServices->FreePool(FileBuffer);
        return Status;
    }

    LosBootTraceHex64(SystemTable, LOS_TEXT("Monitor device path buffer: "), (UINT64)(UINTN)MonitorDevicePath);

    LoadImageFunction = (EFI_LOAD_IMAGE)SystemTable->BootServices->LoadImage;
    StartImageFunction = (EFI_START_IMAGE)SystemTable->BootServices->StartImage;
    if (LoadImageFunction == 0 || StartImageFunction == 0)
    {
        LosBootTrace(SystemTable, LOS_TEXT("LoadImage or StartImage service pointer missing."));
        SystemTable->BootServices->FreePool(MonitorDevicePath);
        SystemTable->BootServices->FreePool(FileBuffer);
        return EFI_UNSUPPORTED;
    }

    LosBootTrace(SystemTable, LOS_TEXT("Calling LoadImage for monitor..."));
    Status = LoadImageFunction(0, ImageHandle, MonitorDevicePath, FileBuffer, (UINTN)FileInfo->FileSize, &MonitorHandle);
    SystemTable->BootServices->FreePool(MonitorDevicePath);
    SystemTable->BootServices->FreePool(FileBuffer);
    if (EFI_ERROR(Status) || MonitorHandle == 0)
    {
        LosBootTraceStatus(SystemTable, LOS_TEXT("LoadImage for monitor failed: "), Status);
        return Status;
    }

    LosBootTraceHex64(SystemTable, LOS_TEXT("Monitor image handle: "), (UINT64)(UINTN)MonitorHandle);
    LosBootTrace(SystemTable, LOS_TEXT("Calling StartImage for monitor..."));
    Status = StartImageFunction(MonitorHandle, 0, 0);
    if (EFI_ERROR(Status))
    {
        LosBootTraceStatus(SystemTable, LOS_TEXT("StartImage for monitor failed: "), Status);
        return Status;
    }

    LosBootTrace(SystemTable, LOS_TEXT("Monitor image returned to boot loader."));
    return EFI_SUCCESS;
}
#endif
