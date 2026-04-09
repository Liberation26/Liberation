/*
 * File Name: InstallerLayout.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "InstallerInternal.h"

const EFI_GUID LosGptPartitionTypeEfiSystem =
{
    0xC12A7328,
    0xF81F,
    0x11D2,
    {0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}
};

const EFI_GUID LosGptPartitionTypeLiberationData =
{
    0xEBD0A0A2,
    0xB9E5,
    0x4433,
    {0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7}
};

EFI_STATUS LosInstallerConfirmDestructiveInstall(EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_INPUT_KEY Key;
    EFI_STATUS Status;

    LosInstallerPrint(SystemTable, LOS_TEXT("This will erase the selected disk. Press Y to continue or N to answer again.\r\n"));
    for (;;)
    {
        Status = LosInstallerReadSingleKey(SystemTable, &Key);
        if (EFI_ERROR(Status))
        {
            return Status;
        }

        if (Key.UnicodeChar == L'Y' || Key.UnicodeChar == L'y')
        {
            LosInstallerPrint(SystemTable, LOS_TEXT("You pressed: "));
            LosInstallerPrintChar(SystemTable, Key.UnicodeChar);
            LosInstallerPrint(SystemTable, LOS_TEXT("\r\nConfirmed.\r\n\r\n"));
            return EFI_SUCCESS;
        }

        if (Key.UnicodeChar == L'N' || Key.UnicodeChar == L'n')
        {
            LosInstallerPrint(SystemTable, LOS_TEXT("You pressed: "));
            LosInstallerPrintChar(SystemTable, Key.UnicodeChar);
            LosInstallerPrint(SystemTable, LOS_TEXT("\r\nPlease answer the confirmation again. Press Y to continue.\r\n"));
            continue;
        }

        LosInstallerPrint(SystemTable, LOS_TEXT("Please press Y to continue.\r\n"));
    }
}

EFI_STATUS LosInstallerCalculateInstallLayout(const LOS_INSTALL_CANDIDATE *Candidate, LOS_INSTALL_LAYOUT *Layout)
{
    UINT64 TotalSectors;
    UINT64 FirstUsableLba;
    UINT64 BackupHeaderLba;
    UINT64 BackupPartitionEntriesLba;
    UINT64 LastUsableLba;
    UINT64 EspSizeSectors;
    UINT64 EspStartLba;
    UINT64 EspEndLba;
    UINT64 DataStartLba;
    UINT64 DataEndLba;

    if (Candidate == 0 || Layout == 0 || Candidate->BlockIo == 0 || Candidate->BlockIo->Media == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    TotalSectors = Candidate->TotalBlocks;
    if (TotalSectors < 524288ULL)
    {
        return EFI_UNSUPPORTED;
    }

    BackupHeaderLba = TotalSectors - 1ULL;
    BackupPartitionEntriesLba = BackupHeaderLba - LOS_GPT_PARTITION_ARRAY_SECTORS;
    FirstUsableLba = 2ULL + LOS_GPT_PARTITION_ARRAY_SECTORS;
    LastUsableLba = BackupPartitionEntriesLba - 1ULL;

    EspSizeSectors = 262144ULL;
    if (TotalSectors < 1048576ULL)
    {
        EspSizeSectors = 131072ULL;
    }
    if (TotalSectors < 786432ULL)
    {
        EspSizeSectors = 65536ULL;
    }

    EspStartLba = LosInstallerAlignUp(FirstUsableLba, 2048ULL);
    EspEndLba = EspStartLba + EspSizeSectors - 1ULL;
    DataStartLba = LosInstallerAlignUp(EspEndLba + 1ULL, 2048ULL);
    DataEndLba = LastUsableLba;

    if (EspEndLba >= DataStartLba || DataStartLba >= DataEndLba)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    Layout->TotalSectors = TotalSectors;
    Layout->BackupHeaderLba = BackupHeaderLba;
    Layout->BackupPartitionEntriesLba = BackupPartitionEntriesLba;
    Layout->FirstUsableLba = FirstUsableLba;
    Layout->LastUsableLba = LastUsableLba;
    Layout->EspStartLba = EspStartLba;
    Layout->EspEndLba = EspEndLba;
    Layout->DataStartLba = DataStartLba;
    Layout->DataEndLba = DataEndLba;
    Layout->EspSectors = EspEndLba - EspStartLba + 1ULL;
    Layout->DataSectors = DataEndLba - DataStartLba + 1ULL;
    return EFI_SUCCESS;
}

void LosInstallerPrintInstallLayout(EFI_SYSTEM_TABLE *SystemTable, const LOS_INSTALL_LAYOUT *Layout)
{
    LosInstallerPrint(SystemTable, LOS_TEXT("Planned disk layout:\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("  ESP FAT32: "));
    LosInstallerPrintMiB(SystemTable, Layout->EspSectors * 512ULL);
    LosInstallerPrint(SystemTable, LOS_TEXT("\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("  Liberation Data FAT32: "));
    LosInstallerPrintMiB(SystemTable, Layout->DataSectors * 512ULL);
    LosInstallerPrint(SystemTable, LOS_TEXT("\r\n\r\n"));
}

EFI_STATUS LosInstallerAllocateZeroPool(EFI_SYSTEM_TABLE *SystemTable, UINTN Size, void **Buffer)
{
    EFI_STATUS Status;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || Buffer == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *Buffer = 0;
    Status = SystemTable->BootServices->AllocatePool(EfiLoaderData, Size, Buffer);
    if (!EFI_ERROR(Status) && *Buffer != 0)
    {
        LosInstallerMemorySet(*Buffer, 0, Size);
    }
    return Status;
}

EFI_STATUS LosInstallerWriteBlocksChecked(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_LBA Lba,
    UINTN BufferSize,
    void *Buffer)
{
    if (BlockIo == 0 || BlockIo->Media == 0 || Buffer == 0 || BufferSize == 0U)
    {
        return EFI_INVALID_PARAMETER;
    }

    if ((BufferSize % BlockIo->Media->BlockSize) != 0U)
    {
        return EFI_BAD_BUFFER_SIZE;
    }

    return BlockIo->WriteBlocks(BlockIo, BlockIo->Media->MediaId, Lba, BufferSize, Buffer);
}

EFI_STATUS LosInstallerFlushBlocksChecked(EFI_BLOCK_IO_PROTOCOL *BlockIo)
{
    if (BlockIo == 0 || BlockIo->FlushBlocks == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    return BlockIo->FlushBlocks(BlockIo);
}

EFI_STATUS LosInstallerZeroLbaRange(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_LBA StartLba,
    UINT64 SectorCount)
{
    EFI_STATUS Status;
    UINT8 *ZeroBuffer;
    UINT64 Remaining;
    UINT64 CurrentLba;
    UINTN ChunkSectors;

    if (SystemTable == 0 || BlockIo == 0 || BlockIo->Media == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    ChunkSectors = 128U;
    Status = LosInstallerAllocateZeroPool(SystemTable, ChunkSectors * BlockIo->Media->BlockSize, (void **)&ZeroBuffer);
    if (EFI_ERROR(Status) || ZeroBuffer == 0)
    {
        return Status;
    }

    Remaining = SectorCount;
    CurrentLba = StartLba;
    while (Remaining != 0ULL)
    {
        UINTN ThisChunk = (Remaining > (UINT64)ChunkSectors) ? ChunkSectors : (UINTN)Remaining;
        Status = LosInstallerWriteBlocksChecked(BlockIo, CurrentLba, ThisChunk * BlockIo->Media->BlockSize, ZeroBuffer);
        if (EFI_ERROR(Status))
        {
            SystemTable->BootServices->FreePool(ZeroBuffer);
            return Status;
        }

        Remaining -= (UINT64)ThisChunk;
        CurrentLba += (UINT64)ThisChunk;
    }

    SystemTable->BootServices->FreePool(ZeroBuffer);
    return EFI_SUCCESS;
}

EFI_STATUS LosInstallerWriteProtectiveMbr(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    UINT64 TotalSectors)
{
    LOS_PROTECTIVE_MBR *Mbr;
    EFI_STATUS Status;
    UINT32 MbrSectors;

    Status = LosInstallerAllocateZeroPool(SystemTable, LOS_SECTOR_SIZE_512, (void **)&Mbr);
    if (EFI_ERROR(Status) || Mbr == 0)
    {
        return Status;
    }

    Mbr->PartitionEntry[0].OsType = 0xEEU;
    Mbr->PartitionEntry[0].StartingLba = 1U;
    MbrSectors = (TotalSectors > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (UINT32)(TotalSectors - 1ULL);
    Mbr->PartitionEntry[0].SizeInLba = MbrSectors;
    Mbr->Signature = 0xAA55U;

    Status = LosInstallerWriteBlocksChecked(BlockIo, 0, LOS_SECTOR_SIZE_512, Mbr);
    SystemTable->BootServices->FreePool(Mbr);
    return Status;
}

EFI_STATUS LosInstallerWriteGptTables(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    const LOS_INSTALL_LAYOUT *Layout)
{
    LOS_GPT_PARTITION_ENTRY *Entries;
    LOS_GPT_HEADER *PrimaryHeader;
    LOS_GPT_HEADER *BackupHeader;
    EFI_GUID DiskGuid;
    UINT32 EntriesCrc32;
    EFI_STATUS Status;

    Status = LosInstallerAllocateZeroPool(
        SystemTable,
        LOS_GPT_PARTITION_ENTRY_COUNT * LOS_GPT_PARTITION_ENTRY_SIZE,
        (void **)&Entries);
    if (EFI_ERROR(Status) || Entries == 0)
    {
        return Status;
    }

    LosInstallerMemoryCopy(&Entries[0].PartitionTypeGuid, &LosGptPartitionTypeEfiSystem, sizeof(EFI_GUID));
    Entries[0].UniquePartitionGuid = LosInstallerCreateGuidFromSeed(Layout->EspStartLba ^ Layout->EspEndLba);
    Entries[0].FirstLba = Layout->EspStartLba;
    Entries[0].LastLba = Layout->EspEndLba;
    LosInstallerCopyPartitionName(Entries[0].PartitionName, LOS_TEXT("EFI System"), 36U);

    LosInstallerMemoryCopy(&Entries[1].PartitionTypeGuid, &LosGptPartitionTypeLiberationData, sizeof(EFI_GUID));
    Entries[1].UniquePartitionGuid = LosInstallerCreateGuidFromSeed(Layout->DataStartLba ^ Layout->DataEndLba ^ 0x4C4F5301ULL);
    Entries[1].FirstLba = Layout->DataStartLba;
    Entries[1].LastLba = Layout->DataEndLba;
    LosInstallerCopyPartitionName(Entries[1].PartitionName, LOS_TEXT("Liberation Data"), 36U);

    EntriesCrc32 = LosInstallerCalculateCrc32(Entries, LOS_GPT_PARTITION_ENTRY_COUNT * LOS_GPT_PARTITION_ENTRY_SIZE);

    Status = LosInstallerAllocateZeroPool(SystemTable, LOS_SECTOR_SIZE_512, (void **)&PrimaryHeader);
    if (EFI_ERROR(Status) || PrimaryHeader == 0)
    {
        SystemTable->BootServices->FreePool(Entries);
        return Status;
    }

    Status = LosInstallerAllocateZeroPool(SystemTable, LOS_SECTOR_SIZE_512, (void **)&BackupHeader);
    if (EFI_ERROR(Status) || BackupHeader == 0)
    {
        SystemTable->BootServices->FreePool(PrimaryHeader);
        SystemTable->BootServices->FreePool(Entries);
        return Status;
    }

    LosInstallerMemoryCopy(PrimaryHeader->Signature, "EFI PART", 8U);
    PrimaryHeader->Revision = 0x00010000U;
    PrimaryHeader->HeaderSize = 92U;
    PrimaryHeader->CurrentLba = 1ULL;
    PrimaryHeader->BackupLba = Layout->BackupHeaderLba;
    PrimaryHeader->FirstUsableLba = Layout->FirstUsableLba;
    PrimaryHeader->LastUsableLba = Layout->LastUsableLba;
    DiskGuid = LosInstallerCreateGuidFromSeed(Layout->TotalSectors ^ 0xA5A55A5A12345678ULL);
    PrimaryHeader->DiskGuid = DiskGuid;
    PrimaryHeader->PartitionEntryLba = 2ULL;
    PrimaryHeader->NumberOfPartitionEntries = LOS_GPT_PARTITION_ENTRY_COUNT;
    PrimaryHeader->SizeOfPartitionEntry = LOS_GPT_PARTITION_ENTRY_SIZE;
    PrimaryHeader->PartitionEntryArrayCrc32 = EntriesCrc32;
    PrimaryHeader->HeaderCrc32 = 0U;
    PrimaryHeader->HeaderCrc32 = LosInstallerCalculateCrc32(PrimaryHeader, PrimaryHeader->HeaderSize);

    LosInstallerMemoryCopy(BackupHeader, PrimaryHeader, LOS_SECTOR_SIZE_512);
    BackupHeader->CurrentLba = Layout->BackupHeaderLba;
    BackupHeader->BackupLba = 1ULL;
    BackupHeader->PartitionEntryLba = Layout->BackupPartitionEntriesLba;
    BackupHeader->HeaderCrc32 = 0U;
    BackupHeader->HeaderCrc32 = LosInstallerCalculateCrc32(BackupHeader, BackupHeader->HeaderSize);

    Status = LosInstallerWriteBlocksChecked(
        BlockIo,
        2ULL,
        LOS_GPT_PARTITION_ENTRY_COUNT * LOS_GPT_PARTITION_ENTRY_SIZE,
        Entries);
    if (!EFI_ERROR(Status))
    {
        Status = LosInstallerWriteBlocksChecked(BlockIo, 1ULL, LOS_SECTOR_SIZE_512, PrimaryHeader);
    }
    if (!EFI_ERROR(Status))
    {
        Status = LosInstallerWriteBlocksChecked(
            BlockIo,
            Layout->BackupPartitionEntriesLba,
            LOS_GPT_PARTITION_ENTRY_COUNT * LOS_GPT_PARTITION_ENTRY_SIZE,
            Entries);
    }
    if (!EFI_ERROR(Status))
    {
        Status = LosInstallerWriteBlocksChecked(BlockIo, Layout->BackupHeaderLba, LOS_SECTOR_SIZE_512, BackupHeader);
    }

    SystemTable->BootServices->FreePool(BackupHeader);
    SystemTable->BootServices->FreePool(PrimaryHeader);
    SystemTable->BootServices->FreePool(Entries);
    return Status;
}
