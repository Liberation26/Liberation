#ifndef LOS_INSTALLER_INTERNAL_H
#define LOS_INSTALLER_INTERNAL_H

#include "Efi.h"
#include "InstallerMain.h"

#define LOS_TEXT(TextLiteral) ((const CHAR16 *)L##TextLiteral)
#define LOS_FILE_INFO_BUFFER_SIZE 512U
#define LOS_GPT_PARTITION_ENTRY_COUNT 128U
#define LOS_GPT_PARTITION_ENTRY_SIZE 128U
#define LOS_GPT_PARTITION_ARRAY_SECTORS 32U
#define LOS_SECTOR_SIZE_512 512U
#define LOS_MIN_TARGET_DISK_MIB 256U
#define LOS_FAT32_RESERVED_SECTORS 32U
#define LOS_FAT32_FAT_COUNT 2U
#define LOS_FAT32_ROOT_CLUSTER 2U
#define LOS_MAX_INSTALL_CANDIDATES 16U
#define LOS_REBOOT_COUNTDOWN_SECONDS 5U
#define LOS_BOOT_INFO_FILE_CHARACTERS 64U

typedef enum
{
    AllHandles = 0,
    ByRegisterNotify = 1,
    ByProtocol = 2
} EFI_LOCATE_SEARCH_TYPE;

typedef enum
{
    LosFat32VolumeContentsEmpty = 0,
    LosFat32VolumeContentsEsp = 1,
    LosFat32VolumeContentsKernelData = 2
} LOS_FAT32_VOLUME_CONTENTS;

typedef struct
{
    EFI_HANDLE Handle;
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    UINT64 TotalBlocks;
    UINT64 TotalBytes;
} LOS_INSTALL_CANDIDATE;

typedef struct
{
    UINT64 TotalSectors;
    UINT64 BackupHeaderLba;
    UINT64 BackupPartitionEntriesLba;
    UINT64 FirstUsableLba;
    UINT64 LastUsableLba;
    UINT64 EspStartLba;
    UINT64 EspEndLba;
    UINT64 DataStartLba;
    UINT64 DataEndLba;
    UINT64 EspSectors;
    UINT64 DataSectors;
} LOS_INSTALL_LAYOUT;

typedef struct
{
    UINT64 StartLba;
    UINT32 TotalSectors;
    UINT8 SectorsPerCluster;
    UINT16 ReservedSectors;
    UINT8 FatCount;
    UINT32 FatSectors;
    UINT32 ClusterCount;
    UINT32 RootCluster;
    UINT32 DataStartSector;
    UINT32 TotalClustersAvailable;
    UINT32 VolumeId;
    CHAR16 VolumeLabel[12];
} LOS_FAT32_VOLUME;

typedef struct
{
    UINT32 StartCluster;
    UINT32 ClusterCount;
} LOS_FAT32_ALLOCATION;

typedef struct __attribute__((packed))
{
    UINT8 BootIndicator;
    UINT8 StartHead;
    UINT8 StartSector;
    UINT8 StartTrack;
    UINT8 OsType;
    UINT8 EndHead;
    UINT8 EndSector;
    UINT8 EndTrack;
    UINT32 StartingLba;
    UINT32 SizeInLba;
} LOS_MBR_PARTITION_ENTRY;

typedef struct __attribute__((packed))
{
    UINT8 BootCode[440];
    UINT32 DiskSignature;
    UINT16 Reserved;
    LOS_MBR_PARTITION_ENTRY PartitionEntry[4];
    UINT16 Signature;
} LOS_PROTECTIVE_MBR;

typedef struct __attribute__((packed))
{
    UINT8 Signature[8];
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 HeaderCrc32;
    UINT32 Reserved;
    UINT64 CurrentLba;
    UINT64 BackupLba;
    UINT64 FirstUsableLba;
    UINT64 LastUsableLba;
    EFI_GUID DiskGuid;
    UINT64 PartitionEntryLba;
    UINT32 NumberOfPartitionEntries;
    UINT32 SizeOfPartitionEntry;
    UINT32 PartitionEntryArrayCrc32;
    UINT8 ReservedTail[420];
} LOS_GPT_HEADER;

typedef struct __attribute__((packed))
{
    EFI_GUID PartitionTypeGuid;
    EFI_GUID UniquePartitionGuid;
    UINT64 FirstLba;
    UINT64 LastLba;
    UINT64 Attributes;
    CHAR16 PartitionName[36];
} LOS_GPT_PARTITION_ENTRY;

typedef struct __attribute__((packed))
{
    UINT8 JumpBoot[3];
    UINT8 OemName[8];
    UINT16 BytesPerSector;
    UINT8 SectorsPerCluster;
    UINT16 ReservedSectorCount;
    UINT8 NumberOfFats;
    UINT16 RootEntryCount;
    UINT16 TotalSectors16;
    UINT8 Media;
    UINT16 FatSize16;
    UINT16 SectorsPerTrack;
    UINT16 NumberOfHeads;
    UINT32 HiddenSectors;
    UINT32 TotalSectors32;
    UINT32 FatSize32;
    UINT16 ExtFlags;
    UINT16 FileSystemVersion;
    UINT32 RootCluster;
    UINT16 FileSystemInfo;
    UINT16 BackupBootSector;
    UINT8 Reserved0[12];
    UINT8 DriveNumber;
    UINT8 Reserved1;
    UINT8 BootSignature;
    UINT32 VolumeId;
    UINT8 VolumeLabel[11];
    UINT8 FileSystemType[8];
    UINT8 BootCode[420];
    UINT16 EndSignature;
} LOS_FAT32_BOOT_SECTOR;

typedef struct __attribute__((packed))
{
    UINT32 LeadSignature;
    UINT8 Reserved0[480];
    UINT32 StructureSignature;
    UINT32 FreeCount;
    UINT32 NextFree;
    UINT8 Reserved1[12];
    UINT32 TrailSignature;
} LOS_FAT32_FSINFO;

typedef struct __attribute__((packed))
{
    UINT8 Name[11];
    UINT8 Attributes;
    UINT8 NtReserved;
    UINT8 CreationTimeTenths;
    UINT16 CreationTime;
    UINT16 CreationDate;
    UINT16 LastAccessDate;
    UINT16 FirstClusterHigh;
    UINT16 WriteTime;
    UINT16 WriteDate;
    UINT16 FirstClusterLow;
    UINT32 FileSize;
} LOS_FAT_DIRECTORY_ENTRY;

typedef struct __attribute__((packed))
{
    UINT8 Order;
    UINT16 Name1[5];
    UINT8 Attributes;
    UINT8 Type;
    UINT8 Checksum;
    UINT16 Name2[6];
    UINT16 FirstClusterLow;
    UINT16 Name3[2];
} LOS_FAT_LFN_ENTRY;

extern const EFI_GUID LosGptPartitionTypeEfiSystem;
extern const EFI_GUID LosGptPartitionTypeLiberationData;

void LosInstallerPrint(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text);
void LosInstallerHaltForever(void);
void LosInstallerMemorySet(void *Destination, UINT8 Value, UINTN Size);
void LosInstallerMemoryCopy(void *Destination, const void *Source, UINTN Size);
UINTN LosInstallerStringLength16(const CHAR16 *Text);
void LosInstallerPrintChar(EFI_SYSTEM_TABLE *SystemTable, CHAR16 Character);
void LosInstallerPrintUnsigned(EFI_SYSTEM_TABLE *SystemTable, UINT64 Value);
void LosInstallerPrintHex64(EFI_SYSTEM_TABLE *SystemTable, UINT64 Value);
void LosInstallerPrintMiB(EFI_SYSTEM_TABLE *SystemTable, UINT64 ByteCount);
void LosInstallerStallSeconds(EFI_SYSTEM_TABLE *SystemTable, UINTN Seconds);
void LosInstallerPrintRebootCountdown(EFI_SYSTEM_TABLE *SystemTable, UINTN SecondsRemaining);
void LosInstallerRequestColdReboot(EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS LosInstallerOpenRootForHandle(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE DeviceHandle, EFI_FILE_PROTOCOL **Root);
EFI_STATUS LosInstallerReadFileIntoPool(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL *Root, const CHAR16 *Path, void **Buffer, UINTN *BufferSize);
EFI_STATUS LosInstallerLocateBlockIoHandles(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE **Handles, UINTN *HandleCount);
UINT64 LosInstallerAlignUp(UINT64 Value, UINT64 Alignment);
UINT32 LosInstallerCalculateCrc32(const void *Buffer, UINTN BufferSize);
EFI_GUID LosInstallerCreateGuidFromSeed(UINT64 Seed);
void LosInstallerCopyPartitionName(CHAR16 *Destination, const CHAR16 *Source, UINTN Capacity);
void LosInstallerCopyString16(CHAR16 *Destination, UINTN Capacity, const CHAR16 *Source);
EFI_STATUS LosInstallerGetInstallCandidates(EFI_SYSTEM_TABLE *SystemTable, LOS_INSTALL_CANDIDATE *Candidates, UINTN CandidateCapacity, UINTN *CandidateCount);
void LosInstallerPrintCandidateLine(EFI_SYSTEM_TABLE *SystemTable, UINTN CandidateNumber, const LOS_INSTALL_CANDIDATE *Candidate);
EFI_STATUS LosInstallerReadSingleKey(EFI_SYSTEM_TABLE *SystemTable, EFI_INPUT_KEY *Key);
EFI_STATUS LosInstallerChooseTargetDisk(EFI_SYSTEM_TABLE *SystemTable, const LOS_INSTALL_CANDIDATE *Candidates, UINTN CandidateCount, UINTN *SelectedIndex);
EFI_STATUS LosInstallerConfirmDestructiveInstall(EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS LosInstallerCalculateInstallLayout(const LOS_INSTALL_CANDIDATE *Candidate, LOS_INSTALL_LAYOUT *Layout);
void LosInstallerPrintInstallLayout(EFI_SYSTEM_TABLE *SystemTable, const LOS_INSTALL_LAYOUT *Layout);
EFI_STATUS LosInstallerAllocateZeroPool(EFI_SYSTEM_TABLE *SystemTable, UINTN Size, void **Buffer);
EFI_STATUS LosInstallerWriteBlocksChecked(EFI_BLOCK_IO_PROTOCOL *BlockIo, EFI_LBA Lba, UINTN BufferSize, void *Buffer);
EFI_STATUS LosInstallerFlushBlocksChecked(EFI_BLOCK_IO_PROTOCOL *BlockIo);
EFI_STATUS LosInstallerZeroLbaRange(EFI_SYSTEM_TABLE *SystemTable, EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT64 StartLba, UINT64 SectorCount);
EFI_STATUS LosInstallerWriteProtectiveMbr(EFI_SYSTEM_TABLE *SystemTable, EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT64 TotalSectors);
EFI_STATUS LosInstallerWriteGptTables(EFI_SYSTEM_TABLE *SystemTable, EFI_BLOCK_IO_PROTOCOL *BlockIo, const LOS_INSTALL_LAYOUT *Layout);
UINT8 LosInstallerChooseFat32SectorsPerCluster(UINT32 TotalSectors);
EFI_STATUS LosInstallerPrepareFat32Volume(LOS_FAT32_VOLUME *Volume, UINT64 StartLba, UINT64 SectorCount, const CHAR16 *Label, UINT32 Seed);
UINT64 LosInstallerFat32ClusterToLba(const LOS_FAT32_VOLUME *Volume, UINT32 Cluster);
LOS_FAT32_ALLOCATION LosInstallerAllocateClusters(UINT32 *NextCluster, UINT32 RequestedClusters);
void LosInstallerSetFatChain(UINT32 *FatEntries, LOS_FAT32_ALLOCATION Allocation);
void LosInstallerCopyVolumeLabel11(UINT8 *Destination, const CHAR16 *Source);
void LosInstallerFillShortName(UINT8 *Destination, const char *Base, const char *Extension);
void LosInstallerCreateShortEntry(LOS_FAT_DIRECTORY_ENTRY *Entry, const UINT8 *Name11, UINT8 Attributes, UINT32 StartCluster, UINT32 FileSize);
UINT8 LosInstallerCalculateShortNameChecksum(const UINT8 *ShortName);
void LosInstallerFillLfnNameFields(LOS_FAT_LFN_ENTRY *Entry, const CHAR16 *Name, UINTN StartIndex, UINTN Length);
UINTN LosInstallerGetRequiredLfnEntryCount(const CHAR16 *Name);
void LosInstallerCreateLfnEntries(LOS_FAT_LFN_ENTRY *Entries, const CHAR16 *Name, const UINT8 *ShortName);
EFI_STATUS LosInstallerWriteFat32FileData(EFI_SYSTEM_TABLE *SystemTable, EFI_BLOCK_IO_PROTOCOL *BlockIo, const LOS_FAT32_VOLUME *Volume, LOS_FAT32_ALLOCATION Allocation, const void *Buffer, UINTN BufferSize);
EFI_STATUS LosInstallerFormatFat32Volume(EFI_SYSTEM_TABLE *SystemTable, EFI_BLOCK_IO_PROTOCOL *BlockIo, const LOS_FAT32_VOLUME *Volume, const void *BootLoaderBuffer, UINTN BootLoaderSize, const void *MonitorBuffer, UINTN MonitorSize, const void *KernelBuffer, UINTN KernelSize, const void *BootInfoBuffer, UINTN BootInfoSize, LOS_FAT32_VOLUME_CONTENTS Contents);
EFI_STATUS LosInstallerInstallToRawDisk(EFI_SYSTEM_TABLE *SystemTable, const LOS_INSTALL_CANDIDATE *Candidate, const void *LoaderBuffer, UINTN LoaderSize, const void *MonitorBuffer, UINTN MonitorSize, const void *KernelBuffer, UINTN KernelSize, UINTN SelectedDiskNumber);
void LosInstallerPrintStatusError(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, EFI_STATUS Status);

#endif
