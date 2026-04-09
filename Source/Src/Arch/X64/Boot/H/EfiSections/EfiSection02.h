/*
 * File Name: EfiSection02.h
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from Efi.h.
 */

struct EFI_RUNTIME_SERVICES
{
    EFI_TABLE_HEADER Hdr;
    VOID *GetTime;
    VOID *SetTime;
    VOID *GetWakeupTime;
    VOID *SetWakeupTime;
    VOID *SetVirtualAddressMap;
    VOID *ConvertPointer;
    VOID *GetVariable;
    VOID *GetNextVariableName;
    VOID *SetVariable;
    VOID *GetNextHighMonotonicCount;
    EFI_STATUS (EFIAPI *ResetSystem)(
        EFI_RESET_TYPE ResetType,
        EFI_STATUS ResetStatus,
        UINTN DataSize,
        VOID *ResetData);
};

struct EFI_BOOT_SERVICES
{
    EFI_TABLE_HEADER Hdr;
    VOID *RaiseTPL;
    VOID *RestoreTPL;
    EFI_STATUS (EFIAPI *AllocatePages)(
        EFI_ALLOCATE_TYPE Type,
        EFI_MEMORY_TYPE MemoryType,
        UINTN Pages,
        UINT64 *Memory);

    EFI_STATUS (EFIAPI *FreePages)(
        UINT64 Memory,
        UINTN Pages);

    EFI_STATUS (EFIAPI *GetMemoryMap)(
        UINTN *MemoryMapSize,
        VOID *MemoryMap,
        UINTN *MapKey,
        UINTN *DescriptorSize,
        UINT32 *DescriptorVersion);

    EFI_STATUS (EFIAPI *AllocatePool)(
        EFI_MEMORY_TYPE PoolType,
        UINTN Size,
        VOID **Buffer);

    EFI_STATUS (EFIAPI *FreePool)(
        VOID *Buffer);

    VOID *CreateEvent;
    VOID *SetTimer;
    EFI_STATUS (EFIAPI *WaitForEvent)(
        UINTN NumberOfEvents,
        VOID **Event,
        UINTN *Index);
    VOID *SignalEvent;
    VOID *CloseEvent;
    VOID *CheckEvent;
    VOID *InstallProtocolInterface;
    VOID *ReinstallProtocolInterface;
    VOID *UninstallProtocolInterface;
    EFI_STATUS (EFIAPI *HandleProtocol)(
        EFI_HANDLE Handle,
        EFI_GUID *Protocol,
        VOID **Interface);

    VOID *Reserved;
    VOID *RegisterProtocolNotify;
    EFI_STATUS (EFIAPI *LocateHandle)(
        UINTN SearchType,
        EFI_GUID *Protocol,
        VOID *SearchKey,
        UINTN *BufferSize,
        EFI_HANDLE *Buffer);
    VOID *LocateDevicePath;
    VOID *InstallConfigurationTable;
    VOID *LoadImage;
    VOID *StartImage;
    VOID *Exit;
    VOID *UnloadImage;
    EFI_STATUS (EFIAPI *ExitBootServices)(
        EFI_HANDLE ImageHandle,
        UINTN MapKey);

    VOID *GetNextMonotonicCount;
    EFI_STATUS (EFIAPI *Stall)(
        UINTN Microseconds);

    VOID *SetWatchdogTimer;
    EFI_STATUS (EFIAPI *ConnectController)(
        EFI_HANDLE ControllerHandle,
        EFI_HANDLE *DriverImageHandle,
        VOID *RemainingDevicePath,
        BOOLEAN Recursive);
    EFI_STATUS (EFIAPI *DisconnectController)(
        EFI_HANDLE ControllerHandle,
        EFI_HANDLE DriverImageHandle,
        EFI_HANDLE ChildHandle);
    VOID *OpenProtocol;
    VOID *CloseProtocol;
    VOID *OpenProtocolInformation;
    VOID *ProtocolsPerHandle;
    VOID *LocateHandleBuffer;
    VOID *LocateProtocol;
    VOID *InstallMultipleProtocolInterfaces;
    VOID *UninstallMultipleProtocolInterfaces;
    VOID *CalculateCrc32;
    VOID *CopyMem;
    VOID *SetMem;
    VOID *CreateEventEx;
};

typedef struct
{
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    VOID *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    VOID *ConfigurationTable;
} EFI_SYSTEM_TABLE;

struct EFI_FILE_PROTOCOL
{
    UINT64 Revision;

    EFI_STATUS (EFIAPI *Open)(
        EFI_FILE_PROTOCOL *This,
        EFI_FILE_PROTOCOL **NewHandle,
        CHAR16 *FileName,
        UINT64 OpenMode,
        UINT64 Attributes);

    EFI_STATUS (EFIAPI *Close)(
        EFI_FILE_PROTOCOL *This);

    EFI_STATUS (EFIAPI *Delete)(
        EFI_FILE_PROTOCOL *This);

    EFI_STATUS (EFIAPI *Read)(
        EFI_FILE_PROTOCOL *This,
        UINTN *BufferSize,
        VOID *Buffer);

    EFI_STATUS (EFIAPI *Write)(
        EFI_FILE_PROTOCOL *This,
        UINTN *BufferSize,
        VOID *Buffer);

    EFI_STATUS (EFIAPI *GetPosition)(
        EFI_FILE_PROTOCOL *This,
        UINT64 *Position);

    EFI_STATUS (EFIAPI *SetPosition)(
        EFI_FILE_PROTOCOL *This,
        UINT64 Position);

    EFI_STATUS (EFIAPI *GetInfo)(
        EFI_FILE_PROTOCOL *This,
        EFI_GUID *InformationType,
        UINTN *BufferSize,
        VOID *Buffer);

    EFI_STATUS (EFIAPI *SetInfo)(
        EFI_FILE_PROTOCOL *This,
        EFI_GUID *InformationType,
        UINTN BufferSize,
        VOID *Buffer);

    EFI_STATUS (EFIAPI *Flush)(
        EFI_FILE_PROTOCOL *This);

    VOID *OpenEx;
    VOID *ReadEx;
    VOID *WriteEx;
    VOID *FlushEx;
};

struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
{
    UINT64 Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
        EFI_FILE_PROTOCOL **Root);
};

struct EFI_LOADED_IMAGE_PROTOCOL
{
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
    VOID *FilePath;
    VOID *Reserved;
    UINT32 LoadOptionsSize;
    VOID *LoadOptions;
    VOID *ImageBase;
    UINT64 ImageSize;
    EFI_MEMORY_TYPE ImageCodeType;
    EFI_MEMORY_TYPE ImageDataType;
    VOID *Unload;
};

struct EFI_BLOCK_IO_MEDIA
{
    UINT32 MediaId;
    BOOLEAN RemovableMedia;
    BOOLEAN MediaPresent;
    BOOLEAN LogicalPartition;
    BOOLEAN ReadOnly;
    BOOLEAN WriteCaching;
    UINT32 BlockSize;
    UINT32 IoAlign;
    EFI_LBA LastBlock;
    EFI_LBA LowestAlignedLba;
    UINT32 LogicalBlocksPerPhysicalBlock;
    UINT32 OptimalTransferLengthGranularity;
};

struct EFI_BLOCK_IO_PROTOCOL
{
    UINT64 Revision;
    EFI_BLOCK_IO_MEDIA *Media;
    EFI_STATUS (EFIAPI *Reset)(
        EFI_BLOCK_IO_PROTOCOL *This,
        BOOLEAN ExtendedVerification);
    EFI_STATUS (EFIAPI *ReadBlocks)(
        EFI_BLOCK_IO_PROTOCOL *This,
        UINT32 MediaId,
        EFI_LBA Lba,
        UINTN BufferSize,
        VOID *Buffer);
    EFI_STATUS (EFIAPI *WriteBlocks)(
        EFI_BLOCK_IO_PROTOCOL *This,
        UINT32 MediaId,
        EFI_LBA Lba,
        UINTN BufferSize,
        VOID *Buffer);
    EFI_STATUS (EFIAPI *FlushBlocks)(
        EFI_BLOCK_IO_PROTOCOL *This);
};

typedef struct
{
    UINT64 Size;
    UINT64 FileSize;
    UINT64 PhysicalSize;
    UINT64 CreateTime[2];
    UINT64 LastAccessTime[2];
    UINT64 ModificationTime[2];
    UINT64 Attribute;
    CHAR16 FileName[1];
} EFI_FILE_INFO;

#define EFI_FILE_MODE_READ ((UINT64)0x0000000000000001ULL)
#define EFI_FILE_MODE_WRITE ((UINT64)0x0000000000000002ULL)
#define EFI_FILE_MODE_CREATE ((UINT64)0x8000000000000000ULL)
#define EFI_FILE_DIRECTORY ((UINT64)0x0000000000000010ULL)

static const EFI_GUID EfiLoadedImageProtocolGuid =
{
    0x5B1B31A1,
    0x9562,
    0x11D2,
    {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

static const EFI_GUID EfiSimpleFileSystemProtocolGuid =
{
    0x964E5B22,
    0x6459,
    0x11D2,
    {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

static const EFI_GUID EfiBlockIoProtocolGuid =
{
    0x964E5B21,
    0x6459,
    0x11D2,
    {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

static const EFI_GUID EfiFileInfoGuid =
{
    0x09576E92,
    0x6D3F,
    0x11D2,
    {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

