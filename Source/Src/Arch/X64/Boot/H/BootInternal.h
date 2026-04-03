#ifndef LOS_BOOT_INTERNAL_H
#define LOS_BOOT_INTERNAL_H

#include "Efi.h"
#include "InstallerMain.h"

#define LOS_TEXT(TextLiteral) ((const CHAR16 *)L##TextLiteral)
#define LOS_KERNEL_PATH_MAX_INFO_BUFFER 512
#define LOS_PAGE_SIZE 4096ULL
#define LOS_BOOT_INFO_MAX_CHARACTERS 128U
#define LOS_ELF_MAGIC_0 0x7FU
#define LOS_ELF_MAGIC_1 0x45U
#define LOS_ELF_MAGIC_2 0x4CU
#define LOS_ELF_MAGIC_3 0x46U
#define LOS_ELF_CLASS_64 2U
#define LOS_ELF_DATA_LITTLE_ENDIAN 1U
#define LOS_ELF_MACHINE_X86_64 0x3EU
#define LOS_ELF_TYPE_EXEC 2U
#define LOS_ELF_PROGRAM_TYPE_LOAD 1U

typedef EFI_STATUS (EFIAPI *EFI_LOAD_IMAGE)(BOOLEAN BootPolicy, EFI_HANDLE ParentImageHandle, VOID *DevicePath, VOID *SourceBuffer, UINTN SourceSize, EFI_HANDLE *ImageHandle);
typedef EFI_STATUS (EFIAPI *EFI_START_IMAGE)(EFI_HANDLE ImageHandle, UINTN *ExitDataSize, CHAR16 **ExitData);

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

void LosBootPrint(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text);
void LosBootStatusOk(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text);
void LosBootStatusFail(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text);
void LosBootClear(EFI_SYSTEM_TABLE *SystemTable);
void LosBootPrintHex64(EFI_SYSTEM_TABLE *SystemTable, UINT64 Value);
void LosBootPrintStatusError(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, EFI_STATUS Status);
void LosBootAnnounceFunction(EFI_SYSTEM_TABLE *SystemTable, const char *FunctionName);
void LosBootTrace(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text);
void LosBootTracePath(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, const CHAR16 *Path);
void LosBootTraceHex64(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, UINT64 Value);
void LosBootTraceStatus(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, EFI_STATUS Status);
#define LOS_BOOT_ENTER(SystemTable) LosBootAnnounceFunction((SystemTable), __func__)

#if !defined(LIBERATION_BOOT_FROM_ISO)
void LosBootHaltForever(void);
void LosBootMemorySet(void *Destination, UINT8 Value, UINTN Size);
void LosBootMemoryCopy(void *Destination, const void *Source, UINTN Size);
UINT64 LosBootAlignDown(UINT64 Value, UINT64 Alignment);
UINT64 LosBootAlignUp(UINT64 Value, UINT64 Alignment);
EFI_STATUS LosBootOpenRootForHandle(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE DeviceHandle, EFI_FILE_PROTOCOL **Root);
EFI_STATUS LosBootReadFileInfo(EFI_FILE_PROTOCOL *File, EFI_FILE_INFO *FileInfo, UINTN FileInfoBufferSize);
EFI_STATUS LosBootLocateHandleBufferByProtocol(EFI_SYSTEM_TABLE *SystemTable, EFI_GUID *ProtocolGuid, EFI_HANDLE **Handles, UINTN *HandleCount);
EFI_STATUS LosBootLoadKernelFileFromRoot(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL *Root, const CHAR16 *KernelPath, void **KernelEntryAddress, UINTN *KernelImageSize);
EFI_STATUS LosBootLoadKernelFromSiblingFileSystem(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *KernelPath, void **KernelEntryAddress, UINTN *KernelImageSize);
EFI_STATUS LosBootLoadKernelFile(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *KernelPath, void **KernelEntryAddress, UINTN *KernelImageSize);
EFI_STATUS LosBootReadTextFile(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *TextPath, CHAR16 **TextBuffer);
EFI_STATUS LosBootLaunchMonitor(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *MonitorPath);
extern const CHAR16 *const LosBootMonitorPath;
#endif

#endif
