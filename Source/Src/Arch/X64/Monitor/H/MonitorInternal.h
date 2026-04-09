/*
 * File Name: MonitorInternal.h
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T10:54:19Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements monitor-stage functionality for Liberation OS.
 */

#ifndef LOS_MONITOR_INTERNAL_H
#define LOS_MONITOR_INTERNAL_H

#include "Efi.h"
#include "MonitorMain.h"
#include "CapabilitiesServiceAbi.h"

#define LOS_TEXT(TextLiteral) ((const CHAR16 *)L##TextLiteral)
#define LOS_KERNEL_PATH_MAX_INFO_BUFFER 512
#define LOS_PAGE_SIZE 4096ULL
#define LOS_BOOT_INFO_MAX_CHARACTERS 128U
#define LOS_BOOT_CONTEXT_TEXT_CHARACTERS 128U
#define LOS_ELF_MAGIC_0 0x7FU
#define LOS_ELF_MAGIC_1 0x45U
#define LOS_ELF_MAGIC_2 0x4CU
#define LOS_ELF_MAGIC_3 0x46U
#define LOS_ELF_CLASS_64 2U
#define LOS_ELF_DATA_LITTLE_ENDIAN 1U
#define LOS_ELF_MACHINE_X86_64 0x3EU
#define LOS_ELF_TYPE_EXEC 2U
#define LOS_ELF_PROGRAM_TYPE_LOAD 1U
#define LOS_BOOT_CONTEXT_SIGNATURE 0x544F4F424F534F4CULL
#define LOS_BOOT_CONTEXT_VERSION 9U
#define LOS_BOOT_CONTEXT_FLAG_MEMORY_MANAGER_IMAGE_VALID 0x0000000000000004ULL
#define LOS_BOOT_CONTEXT_FLAG_MONITOR_HANDOFF_ONLY 0x0000000000000001ULL
#define LOS_BOOT_CONTEXT_FLAG_KERNEL_SEGMENTS_VALID 0x0000000000000002ULL
#define LOS_BOOT_CONTEXT_FLAG_CAPABILITIES_VALID 0x0000000000000008ULL
#define LOS_BOOT_CONTEXT_MAX_LOAD_SEGMENTS 8U
#define LOS_ELF_PROGRAM_FLAG_EXECUTE 0x1U
#define LOS_ELF_PROGRAM_FLAG_WRITE 0x2U
#define LOS_ELF_PROGRAM_FLAG_READ 0x4U

typedef struct
{
    UINT64 VirtualAddress;
    UINT64 PhysicalAddress;
    UINT64 FileSize;
    UINT64 MemorySize;
    UINT64 Flags;
    UINT64 Reserved;
} LOS_BOOT_CONTEXT_LOAD_SEGMENT;

typedef struct
{
    UINT64 Signature;
    UINT32 Version;
    UINT32 Reserved;
    UINT64 Flags;
    UINT64 BootContextAddress;
    UINT64 BootContextSize;
    UINT64 KernelImagePhysicalAddress;
    UINT64 KernelImageSize;
    UINT64 MemoryMapAddress;
    UINT64 MemoryMapSize;
    UINT64 MemoryMapBufferSize;
    UINT64 MemoryMapDescriptorSize;
    UINT64 MemoryMapDescriptorVersion;
    UINT64 MemoryRegionCount;
    UINT64 FrameBufferPhysicalAddress;
    UINT64 FrameBufferSize;
    UINT32 FrameBufferWidth;
    UINT32 FrameBufferHeight;
    UINT32 FrameBufferPixelsPerScanLine;
    UINT32 FrameBufferPixelFormat;
    UINT64 KernelFontPhysicalAddress;
    UINT64 KernelFontSize;
    UINT64 MemoryManagerImagePhysicalAddress;
    UINT64 MemoryManagerImageSize;
    UINT64 KernelLoadSegmentCount;
    LOS_BOOT_CONTEXT_LOAD_SEGMENT KernelLoadSegments[LOS_BOOT_CONTEXT_MAX_LOAD_SEGMENTS];
    LOS_CAPABILITIES_BOOTSTRAP_CONTEXT Capabilities;
    CHAR16 BootSourceText[LOS_BOOT_CONTEXT_TEXT_CHARACTERS];
    CHAR16 KernelPartitionText[LOS_BOOT_CONTEXT_TEXT_CHARACTERS];
} LOS_BOOT_CONTEXT;

typedef void (EFIAPI *LOS_KERNEL_ENTRY)(const LOS_BOOT_CONTEXT *BootContext);

typedef enum
{
    AllHandles = 0,
    ByRegisterNotify = 1,
    ByProtocol = 2
} EFI_LOCATE_SEARCH_TYPE;

typedef struct __attribute__((packed))
{
    UINT8 Ident[16];
    UINT16 Type;
    UINT16 Machine;
    UINT32 Version;
    UINT64 Entry;
    UINT64 ProgramHeaderOffset;
    UINT64 SectionHeaderOffset;
    UINT32 Flags;
    UINT16 HeaderSize;
    UINT16 ProgramHeaderEntrySize;
    UINT16 ProgramHeaderCount;
    UINT16 SectionHeaderEntrySize;
    UINT16 SectionHeaderCount;
    UINT16 SectionNameStringIndex;
} LOS_ELF64_HEADER;

typedef struct __attribute__((packed))
{
    UINT32 Type;
    UINT32 Flags;
    UINT64 Offset;
    UINT64 VirtualAddress;
    UINT64 PhysicalAddress;
    UINT64 FileSize;
    UINT64 MemorySize;
    UINT64 Alignment;
} LOS_ELF64_PROGRAM_HEADER;

void LosMonitorPrint(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text);
void LosMonitorStatusOk(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text);
void LosMonitorStatusFail(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text);
void LosMonitorAnnounceFunction(EFI_SYSTEM_TABLE *SystemTable, const char *FunctionName);
void LosMonitorTrace(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text);
void LosMonitorTracePath(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, const CHAR16 *Path);
void LosMonitorTraceHex64(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, UINT64 Value);
void LosMonitorTraceStatus(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, EFI_STATUS Status);
#define LOS_MONITOR_ENTER(SystemTable) LosMonitorAnnounceFunction((SystemTable), __func__)
void LosMonitorHaltForever(void);
void LosMonitorMemorySet(void *Destination, UINT8 Value, UINTN Size);
void LosMonitorMemoryCopy(void *Destination, const void *Source, UINTN Size);
UINT64 LosMonitorAlignDown(UINT64 Value, UINT64 Alignment);
UINT64 LosMonitorAlignUp(UINT64 Value, UINT64 Alignment);
void LosMonitorUtf16Copy(CHAR16 *Destination, UINTN DestinationCharacterCount, const CHAR16 *Source);
void LosMonitorCaptureFramebufferInfo(EFI_SYSTEM_TABLE *SystemTable, LOS_BOOT_CONTEXT *BootContext);
void LosMonitorInitializeBootContext(LOS_BOOT_CONTEXT *BootContext, UINT64 BootContextAddress, UINT64 BootContextSize, UINT64 KernelImagePhysicalAddress, UINT64 KernelImageSize, const LOS_BOOT_CONTEXT_LOAD_SEGMENT *KernelLoadSegments, UINT64 KernelLoadSegmentCount, const CHAR16 *BootSourceText, const CHAR16 *KernelPartitionText);
EFI_STATUS LosMonitorOpenRootForHandle(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE DeviceHandle, EFI_FILE_PROTOCOL **Root);
EFI_STATUS LosMonitorReadFileInfo(EFI_FILE_PROTOCOL *File, EFI_FILE_INFO *FileInfo, UINTN FileInfoBufferSize);
EFI_STATUS LosMonitorLocateHandleBufferByProtocol(EFI_SYSTEM_TABLE *SystemTable, EFI_GUID *ProtocolGuid, EFI_HANDLE **Handles, UINTN *HandleCount);
EFI_STATUS LosMonitorGetParentDeviceHandle(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE *ParentDeviceHandle);
EFI_STATUS LosMonitorLoadKernelFileFromRoot(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL *Root, const CHAR16 *KernelPath, void **KernelEntryAddress, UINT64 *KernelImagePhysicalAddress, UINTN *KernelImageSize, LOS_BOOT_CONTEXT_LOAD_SEGMENT *KernelLoadSegments, UINT64 *KernelLoadSegmentCount);
EFI_STATUS LosMonitorLoadKernelFromSiblingFileSystemHandle(EFI_HANDLE DeviceHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *KernelPath, void **KernelEntryAddress, UINT64 *KernelImagePhysicalAddress, UINTN *KernelImageSize, LOS_BOOT_CONTEXT_LOAD_SEGMENT *KernelLoadSegments, UINT64 *KernelLoadSegmentCount);
EFI_STATUS LosMonitorReadTextFileFromRoot(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL *Root, const CHAR16 *TextPath, CHAR16 **TextBuffer);
EFI_STATUS LosMonitorReadBinaryFileFromRoot(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL *Root, const CHAR16 *FilePath, UINT64 *FilePhysicalAddress, UINT64 *FileSize);
EFI_STATUS LosMonitorReadBinaryFileFromSiblingFileSystemHandle(EFI_HANDLE DeviceHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *FilePath, UINT64 *FilePhysicalAddress, UINT64 *FileSize);
EFI_STATUS LosMonitorLoadCapabilitiesFromEsp(EFI_HANDLE DeviceHandle, EFI_SYSTEM_TABLE *SystemTable, LOS_BOOT_CONTEXT *BootContext);
BOOLEAN LosMonitorElf64ValidateLoadedImage(const void *ImageBase, UINT64 ImageSize, UINT64 *EntryAddress);
EFI_STATUS LosMonitorExitBootServicesWithMemoryMap(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, LOS_BOOT_CONTEXT *BootContext);

#endif
