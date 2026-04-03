#include "InstallerInternal.h"

EFI_STATUS LosInstallerFormatFat32Volume(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    const LOS_FAT32_VOLUME *Volume,
    const void *BootLoaderBuffer,
    UINTN BootLoaderSize,
    const void *MonitorBuffer,
    UINTN MonitorSize,
    const void *KernelBuffer,
    UINTN KernelSize,
    const void *BootInfoBuffer,
    UINTN BootInfoSize,
    LOS_FAT32_VOLUME_CONTENTS VolumeContents)
{
    EFI_STATUS Status;
    LOS_FAT32_BOOT_SECTOR *BootSector;
    LOS_FAT32_FSINFO *FsInfo;
    UINT32 *FatEntries;
    UINT32 NextCluster;
    UINT32 UsedClusterCount;
    LOS_FAT32_ALLOCATION RootAllocation;
    LOS_FAT32_ALLOCATION EfiAllocation;
    LOS_FAT32_ALLOCATION BootDirAllocation;
    LOS_FAT32_ALLOCATION LoaderAllocation;
    LOS_FAT32_ALLOCATION MonitorAllocation;
    LOS_FAT32_ALLOCATION KernelAllocation;
    LOS_FAT32_ALLOCATION BootInfoAllocation;
    LOS_FAT32_ALLOCATION LiberationDirAllocation;
    UINT8 *DirectoryCluster;
    UINT8 ShortName[11];
    UINTN ClusterBytes;
    BOOLEAN InstallEspFiles;
    BOOLEAN InstallKernelData;

    BootSector = 0;
    FsInfo = 0;
    FatEntries = 0;
    DirectoryCluster = 0;
    LoaderAllocation.StartCluster = 0U;
    LoaderAllocation.ClusterCount = 0U;
    MonitorAllocation.StartCluster = 0U;
    MonitorAllocation.ClusterCount = 0U;
    KernelAllocation.StartCluster = 0U;
    KernelAllocation.ClusterCount = 0U;
    BootInfoAllocation.StartCluster = 0U;
    BootInfoAllocation.ClusterCount = 0U;
    LiberationDirAllocation.StartCluster = 0U;
    LiberationDirAllocation.ClusterCount = 0U;

    InstallEspFiles = (BOOLEAN)(VolumeContents == LosFat32VolumeContentsEsp);
    InstallKernelData = (BOOLEAN)(VolumeContents == LosFat32VolumeContentsKernelData);
    ClusterBytes = (UINTN)Volume->SectorsPerCluster * LOS_SECTOR_SIZE_512;
    NextCluster = LOS_FAT32_ROOT_CLUSTER;

    RootAllocation = LosInstallerAllocateClusters(&NextCluster, 1U);
    EfiAllocation = LosInstallerAllocateClusters(&NextCluster, InstallEspFiles ? 1U : 0U);
    BootDirAllocation = LosInstallerAllocateClusters(&NextCluster, InstallEspFiles ? 1U : 0U);
    LiberationDirAllocation = LosInstallerAllocateClusters(&NextCluster, InstallKernelData ? 1U : 0U);

    if (InstallEspFiles)
    {
        LoaderAllocation = LosInstallerAllocateClusters(&NextCluster, (UINT32)((BootLoaderSize + ClusterBytes - 1U) / ClusterBytes));
        MonitorAllocation = LosInstallerAllocateClusters(&NextCluster, (UINT32)((MonitorSize + ClusterBytes - 1U) / ClusterBytes));
        BootInfoAllocation = LosInstallerAllocateClusters(&NextCluster, (UINT32)((BootInfoSize + ClusterBytes - 1U) / ClusterBytes));
    }

    if (InstallKernelData)
    {
        KernelAllocation = LosInstallerAllocateClusters(&NextCluster, (UINT32)((KernelSize + ClusterBytes - 1U) / ClusterBytes));
    }

    UsedClusterCount = NextCluster - 2U;
    if (UsedClusterCount >= Volume->ClusterCount)
    {
        return EFI_VOLUME_FULL;
    }

    Status = LosInstallerZeroLbaRange(SystemTable, BlockIo, Volume->StartLba, Volume->TotalSectors);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = LosInstallerAllocateZeroPool(SystemTable, LOS_SECTOR_SIZE_512, (void **)&BootSector);
    if (EFI_ERROR(Status) || BootSector == 0)
    {
        return Status;
    }

    Status = LosInstallerAllocateZeroPool(SystemTable, LOS_SECTOR_SIZE_512, (void **)&FsInfo);
    if (EFI_ERROR(Status) || FsInfo == 0)
    {
        SystemTable->BootServices->FreePool(BootSector);
        return Status;
    }

    Status = LosInstallerAllocateZeroPool(SystemTable, Volume->FatSectors * LOS_SECTOR_SIZE_512, (void **)&FatEntries);
    if (EFI_ERROR(Status) || FatEntries == 0)
    {
        SystemTable->BootServices->FreePool(FsInfo);
        SystemTable->BootServices->FreePool(BootSector);
        return Status;
    }

    Status = LosInstallerAllocateZeroPool(SystemTable, ClusterBytes, (void **)&DirectoryCluster);
    if (EFI_ERROR(Status) || DirectoryCluster == 0)
    {
        SystemTable->BootServices->FreePool(FatEntries);
        SystemTable->BootServices->FreePool(FsInfo);
        SystemTable->BootServices->FreePool(BootSector);
        return Status;
    }

    BootSector->JumpBoot[0] = 0xEBU;
    BootSector->JumpBoot[1] = 0x58U;
    BootSector->JumpBoot[2] = 0x90U;
    LosInstallerMemoryCopy(BootSector->OemName, "MSWIN4.1", 8U);
    BootSector->BytesPerSector = LOS_SECTOR_SIZE_512;
    BootSector->SectorsPerCluster = Volume->SectorsPerCluster;
    BootSector->ReservedSectorCount = Volume->ReservedSectors;
    BootSector->NumberOfFats = Volume->FatCount;
    BootSector->Media = 0xF8U;
    BootSector->SectorsPerTrack = 63U;
    BootSector->NumberOfHeads = 255U;
    BootSector->HiddenSectors = (UINT32)Volume->StartLba;
    BootSector->TotalSectors32 = Volume->TotalSectors;
    BootSector->FatSize32 = Volume->FatSectors;
    BootSector->RootCluster = Volume->RootCluster;
    BootSector->FileSystemInfo = 1U;
    BootSector->BackupBootSector = 6U;
    BootSector->DriveNumber = 0x80U;
    BootSector->BootSignature = 0x29U;
    BootSector->VolumeId = Volume->VolumeId;
    LosInstallerCopyVolumeLabel11(BootSector->VolumeLabel, Volume->VolumeLabel);
    LosInstallerMemoryCopy(BootSector->FileSystemType, "FAT32   ", 8U);
    BootSector->EndSignature = 0xAA55U;

    FsInfo->LeadSignature = 0x41615252U;
    FsInfo->StructureSignature = 0x61417272U;
    FsInfo->FreeCount = Volume->ClusterCount - UsedClusterCount;
    FsInfo->NextFree = NextCluster;
    FsInfo->TrailSignature = 0xAA550000U;

    FatEntries[0] = 0x0FFFFFF8U;
    FatEntries[1] = 0xFFFFFFFFU;
    LosInstallerSetFatChain(FatEntries, RootAllocation);
    if (InstallEspFiles)
    {
        LosInstallerSetFatChain(FatEntries, EfiAllocation);
        LosInstallerSetFatChain(FatEntries, BootDirAllocation);
        LosInstallerSetFatChain(FatEntries, LoaderAllocation);
        LosInstallerSetFatChain(FatEntries, MonitorAllocation);
        LosInstallerSetFatChain(FatEntries, BootInfoAllocation);
    }
    if (InstallKernelData)
    {
        LosInstallerSetFatChain(FatEntries, LiberationDirAllocation);
        LosInstallerSetFatChain(FatEntries, KernelAllocation);
    }

    Status = LosInstallerWriteBlocksChecked(BlockIo, Volume->StartLba, LOS_SECTOR_SIZE_512, BootSector);
    if (!EFI_ERROR(Status))
    {
        Status = LosInstallerWriteBlocksChecked(BlockIo, Volume->StartLba + 1ULL, LOS_SECTOR_SIZE_512, FsInfo);
    }
    if (!EFI_ERROR(Status))
    {
        Status = LosInstallerWriteBlocksChecked(BlockIo, Volume->StartLba + 6ULL, LOS_SECTOR_SIZE_512, BootSector);
    }
    if (!EFI_ERROR(Status))
    {
        Status = LosInstallerWriteBlocksChecked(BlockIo, Volume->StartLba + 7ULL, LOS_SECTOR_SIZE_512, FsInfo);
    }
    if (!EFI_ERROR(Status))
    {
        Status = LosInstallerWriteBlocksChecked(BlockIo, Volume->StartLba + (UINT64)Volume->ReservedSectors, Volume->FatSectors * LOS_SECTOR_SIZE_512, FatEntries);
    }
    if (!EFI_ERROR(Status))
    {
        Status = LosInstallerWriteBlocksChecked(
            BlockIo,
            Volume->StartLba + (UINT64)Volume->ReservedSectors + (UINT64)Volume->FatSectors,
            Volume->FatSectors * LOS_SECTOR_SIZE_512,
            FatEntries);
    }
    if (EFI_ERROR(Status))
    {
        SystemTable->BootServices->FreePool(DirectoryCluster);
        SystemTable->BootServices->FreePool(FatEntries);
        SystemTable->BootServices->FreePool(FsInfo);
        SystemTable->BootServices->FreePool(BootSector);
        return Status;
    }

    LosInstallerMemorySet(DirectoryCluster, 0, ClusterBytes);
    if (InstallEspFiles)
    {
        LosInstallerCreateShortEntry((LOS_FAT_DIRECTORY_ENTRY *)(void *)DirectoryCluster, (const UINT8 *)"EFI        ", 0x10U, EfiAllocation.StartCluster, 0U);
    }
    else if (InstallKernelData)
    {
        LOS_FAT_DIRECTORY_ENTRY *Entries;
        LOS_FAT_LFN_ENTRY *LfnEntries;
        UINTN LiberationLfnCount;
        UINTN LiberationShortEntryIndex;

        Entries = (LOS_FAT_DIRECTORY_ENTRY *)(void *)DirectoryCluster;
        LosInstallerFillShortName(ShortName, "LIBERATI", "");
        LiberationLfnCount = LosInstallerGetRequiredLfnEntryCount(LOS_TEXT("LIBERATION"));
        LfnEntries = (LOS_FAT_LFN_ENTRY *)(void *)&Entries[0];
        LosInstallerCreateLfnEntries(LfnEntries, LOS_TEXT("LIBERATION"), ShortName);
        LiberationShortEntryIndex = LiberationLfnCount;
        LosInstallerCreateShortEntry(&Entries[LiberationShortEntryIndex], ShortName, 0x10U, LiberationDirAllocation.StartCluster, 0U);
    }
    Status = LosInstallerWriteBlocksChecked(BlockIo, LosInstallerFat32ClusterToLba(Volume, RootAllocation.StartCluster), ClusterBytes, DirectoryCluster);
    if (EFI_ERROR(Status))
    {
        SystemTable->BootServices->FreePool(DirectoryCluster);
        SystemTable->BootServices->FreePool(FatEntries);
        SystemTable->BootServices->FreePool(FsInfo);
        SystemTable->BootServices->FreePool(BootSector);
        return Status;
    }

    if (InstallEspFiles)
    {
        LOS_FAT_DIRECTORY_ENTRY *Entries;

        LosInstallerMemorySet(DirectoryCluster, 0, ClusterBytes);
        Entries = (LOS_FAT_DIRECTORY_ENTRY *)(void *)DirectoryCluster;
        LosInstallerFillShortName(ShortName, ".", "");
        LosInstallerCreateShortEntry(&Entries[0], (const UINT8 *)".          ", 0x10U, EfiAllocation.StartCluster, 0U);
        LosInstallerCreateShortEntry(&Entries[1], (const UINT8 *)"..         ", 0x10U, RootAllocation.StartCluster, 0U);
        LosInstallerCreateShortEntry(&Entries[2], (const UINT8 *)"BOOT       ", 0x10U, BootDirAllocation.StartCluster, 0U);
        Status = LosInstallerWriteBlocksChecked(BlockIo, LosInstallerFat32ClusterToLba(Volume, EfiAllocation.StartCluster), ClusterBytes, DirectoryCluster);
        if (EFI_ERROR(Status))
        {
            SystemTable->BootServices->FreePool(DirectoryCluster);
            SystemTable->BootServices->FreePool(FatEntries);
            SystemTable->BootServices->FreePool(FsInfo);
            SystemTable->BootServices->FreePool(BootSector);
            return Status;
        }

        LosInstallerMemorySet(DirectoryCluster, 0, ClusterBytes);
        Entries = (LOS_FAT_DIRECTORY_ENTRY *)(void *)DirectoryCluster;
        {
            LOS_FAT_LFN_ENTRY *LfnEntries;
            UINTN MonitorLfnCount;
            UINTN MonitorShortEntryIndex;
            UINTN BootInfoEntryIndex;

            LosInstallerCreateShortEntry(&Entries[0], (const UINT8 *)".          ", 0x10U, BootDirAllocation.StartCluster, 0U);
            LosInstallerCreateShortEntry(&Entries[1], (const UINT8 *)"..         ", 0x10U, EfiAllocation.StartCluster, 0U);
            LosInstallerFillShortName(ShortName, "BOOTX64", "EFI");
            LosInstallerCreateShortEntry(&Entries[2], ShortName, 0x20U, LoaderAllocation.StartCluster, (UINT32)BootLoaderSize);
            LosInstallerFillShortName(ShortName, "MONITO~1", "EFI");
            MonitorLfnCount = LosInstallerGetRequiredLfnEntryCount(LOS_TEXT("MONITORX64.EFI"));
            LfnEntries = (LOS_FAT_LFN_ENTRY *)(void *)&Entries[3];
            LosInstallerCreateLfnEntries(LfnEntries, LOS_TEXT("MONITORX64.EFI"), ShortName);
            MonitorShortEntryIndex = 3U + MonitorLfnCount;
            LosInstallerCreateShortEntry(&Entries[MonitorShortEntryIndex], ShortName, 0x20U, MonitorAllocation.StartCluster, (UINT32)MonitorSize);
            BootInfoEntryIndex = MonitorShortEntryIndex + 1U;
            LosInstallerCreateShortEntry(&Entries[BootInfoEntryIndex], (const UINT8 *)"BOOTINFOTXT", 0x20U, BootInfoAllocation.StartCluster, (UINT32)BootInfoSize);
        }
        Status = LosInstallerWriteBlocksChecked(BlockIo, LosInstallerFat32ClusterToLba(Volume, BootDirAllocation.StartCluster), ClusterBytes, DirectoryCluster);
        if (EFI_ERROR(Status))
        {
            SystemTable->BootServices->FreePool(DirectoryCluster);
            SystemTable->BootServices->FreePool(FatEntries);
            SystemTable->BootServices->FreePool(FsInfo);
            SystemTable->BootServices->FreePool(BootSector);
            return Status;
        }

        Status = LosInstallerWriteFat32FileData(SystemTable, BlockIo, Volume, LoaderAllocation, BootLoaderBuffer, BootLoaderSize);
        if (!EFI_ERROR(Status))
        {
            Status = LosInstallerWriteFat32FileData(SystemTable, BlockIo, Volume, MonitorAllocation, MonitorBuffer, MonitorSize);
        }
        if (!EFI_ERROR(Status))
        {
            Status = LosInstallerWriteFat32FileData(SystemTable, BlockIo, Volume, BootInfoAllocation, BootInfoBuffer, BootInfoSize);
        }
        if (EFI_ERROR(Status))
        {
            SystemTable->BootServices->FreePool(DirectoryCluster);
            SystemTable->BootServices->FreePool(FatEntries);
            SystemTable->BootServices->FreePool(FsInfo);
            SystemTable->BootServices->FreePool(BootSector);
            return Status;
        }
    }

    if (InstallKernelData)
    {
        LOS_FAT_DIRECTORY_ENTRY *Entries;
        LOS_FAT_LFN_ENTRY *LfnEntries;
        UINTN KernelLfnCount;
        UINTN KernelShortEntryIndex;

        LosInstallerMemorySet(DirectoryCluster, 0, ClusterBytes);
        Entries = (LOS_FAT_DIRECTORY_ENTRY *)(void *)DirectoryCluster;
        LosInstallerCreateShortEntry(&Entries[0], (const UINT8 *)".          ", 0x10U, LiberationDirAllocation.StartCluster, 0U);
        LosInstallerCreateShortEntry(&Entries[1], (const UINT8 *)"..         ", 0x10U, RootAllocation.StartCluster, 0U);
        LosInstallerFillShortName(ShortName, "KERNEL~1", "ELF");
        KernelLfnCount = LosInstallerGetRequiredLfnEntryCount(LOS_TEXT("KERNELX64.ELF"));
        LfnEntries = (LOS_FAT_LFN_ENTRY *)(void *)&Entries[2];
        LosInstallerCreateLfnEntries(LfnEntries, LOS_TEXT("KERNELX64.ELF"), ShortName);
        KernelShortEntryIndex = 2U + KernelLfnCount;
        LosInstallerCreateShortEntry(&Entries[KernelShortEntryIndex], ShortName, 0x20U, KernelAllocation.StartCluster, (UINT32)KernelSize);
        Status = LosInstallerWriteBlocksChecked(BlockIo, LosInstallerFat32ClusterToLba(Volume, LiberationDirAllocation.StartCluster), ClusterBytes, DirectoryCluster);
        if (EFI_ERROR(Status))
        {
            SystemTable->BootServices->FreePool(DirectoryCluster);
            SystemTable->BootServices->FreePool(FatEntries);
            SystemTable->BootServices->FreePool(FsInfo);
            SystemTable->BootServices->FreePool(BootSector);
            return Status;
        }

        Status = LosInstallerWriteFat32FileData(SystemTable, BlockIo, Volume, KernelAllocation, KernelBuffer, KernelSize);
        if (EFI_ERROR(Status))
        {
            SystemTable->BootServices->FreePool(DirectoryCluster);
            SystemTable->BootServices->FreePool(FatEntries);
            SystemTable->BootServices->FreePool(FsInfo);
            SystemTable->BootServices->FreePool(BootSector);
            return Status;
        }
    }

    SystemTable->BootServices->FreePool(DirectoryCluster);
    SystemTable->BootServices->FreePool(FatEntries);
    SystemTable->BootServices->FreePool(FsInfo);
    SystemTable->BootServices->FreePool(BootSector);
    return EFI_SUCCESS;
}

EFI_STATUS LosInstallerInstallToRawDisk(
    EFI_SYSTEM_TABLE *SystemTable,
    const LOS_INSTALL_CANDIDATE *Candidate,
    const void *LoaderBuffer,
    UINTN LoaderSize,
    const void *MonitorBuffer,
    UINTN MonitorSize,
    const void *KernelBuffer,
    UINTN KernelSize,
    UINTN SelectedDiskNumber)
{
    LOS_INSTALL_LAYOUT Layout;
    LOS_FAT32_VOLUME EspVolume;
    LOS_FAT32_VOLUME DataVolume;
    CHAR16 BootInfoText[LOS_BOOT_INFO_FILE_CHARACTERS];
    EFI_STATUS Status;

    Status = LosInstallerCalculateInstallLayout(Candidate, &Layout);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    LosInstallerPrintInstallLayout(SystemTable, &Layout);

    LosInstallerMemorySet(BootInfoText, 0, sizeof(BootInfoText));
    LosInstallerCopyString16(BootInfoText, LOS_BOOT_INFO_FILE_CHARACTERS, LOS_TEXT("Booting from installed drive ["));
    BootInfoText[29] = (CHAR16)(L'0' + SelectedDiskNumber);
    BootInfoText[30] = L']';
    BootInfoText[31] = L'\r';
    BootInfoText[32] = L'\n';
    BootInfoText[33] = 0;

    Status = LosInstallerWriteProtectiveMbr(SystemTable, Candidate->BlockIo, Layout.TotalSectors);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = LosInstallerWriteGptTables(SystemTable, Candidate->BlockIo, &Layout);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = LosInstallerPrepareFat32Volume(&EspVolume, Layout.EspStartLba, Layout.EspSectors, LOS_TEXT("LIBESP"), (UINT32)Layout.EspStartLba);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = LosInstallerPrepareFat32Volume(&DataVolume, Layout.DataStartLba, Layout.DataSectors, LOS_TEXT("LIBDATA"), (UINT32)Layout.DataStartLba);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    LosInstallerPrint(SystemTable, LOS_TEXT("Formatting ESP as FAT32...\r\n"));
    Status = LosInstallerFormatFat32Volume(SystemTable, Candidate->BlockIo, &EspVolume, LoaderBuffer, LoaderSize, MonitorBuffer, MonitorSize, 0, 0U, BootInfoText, sizeof(BootInfoText), LosFat32VolumeContentsEsp);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    LosInstallerPrint(SystemTable, LOS_TEXT("Formatting Liberation data partition as FAT32...\r\n"));
    Status = LosInstallerFormatFat32Volume(SystemTable, Candidate->BlockIo, &DataVolume, 0, 0U, 0, 0U, KernelBuffer, KernelSize, 0, 0U, LosFat32VolumeContentsKernelData);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = LosInstallerFlushBlocksChecked(Candidate->BlockIo);
    return Status;
}

void LosInstallerPrintStatusError(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, EFI_STATUS Status)
{
    LosInstallerPrint(SystemTable, Prefix);
    LosInstallerPrintHex64(SystemTable, Status);
    LosInstallerPrint(SystemTable, LOS_TEXT("\r\n"));
}
