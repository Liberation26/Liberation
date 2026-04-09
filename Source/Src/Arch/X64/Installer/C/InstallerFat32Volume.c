/*
 * File Name: InstallerFat32Volume.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "InstallerInternal.h"

UINT8 LosInstallerChooseFat32SectorsPerCluster(UINT32 TotalSectors)
{
    if (TotalSectors < 524288U)
    {
        return 1U;
    }

    if (TotalSectors < 16777216U)
    {
        return 8U;
    }

    return 16U;
}

EFI_STATUS LosInstallerPrepareFat32Volume(LOS_FAT32_VOLUME *Volume, UINT64 StartLba, UINT64 SectorCount, const CHAR16 *Label, UINT32 Seed)
{
    UINT32 TotalSectors;
    UINT32 FatSectors;
    UINT32 ClusterCount;
    UINT32 NewFatSectors;
    UINT32 DataSectors;

    if (Volume == 0 || SectorCount > 0xFFFFFFFFULL || SectorCount < 65536ULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    TotalSectors = (UINT32)SectorCount;
    Volume->StartLba = StartLba;
    Volume->TotalSectors = TotalSectors;
    Volume->ReservedSectors = LOS_FAT32_RESERVED_SECTORS;
    Volume->FatCount = LOS_FAT32_FAT_COUNT;
    Volume->RootCluster = LOS_FAT32_ROOT_CLUSTER;
    Volume->SectorsPerCluster = LosInstallerChooseFat32SectorsPerCluster(TotalSectors);
    Volume->VolumeId = Seed;
    LosInstallerCopyPartitionName(Volume->VolumeLabel, Label, 11U);
    Volume->VolumeLabel[11] = 0;

    FatSectors = 1U;
    for (;;)
    {
        DataSectors = TotalSectors - (UINT32)Volume->ReservedSectors - ((UINT32)Volume->FatCount * FatSectors);
        ClusterCount = DataSectors / (UINT32)Volume->SectorsPerCluster;
        NewFatSectors = (UINT32)(((UINT64)(ClusterCount + 2U) * 4ULL + 511ULL) / 512ULL);
        if (NewFatSectors <= FatSectors)
        {
            break;
        }
        FatSectors = NewFatSectors;
    }

    DataSectors = TotalSectors - (UINT32)Volume->ReservedSectors - ((UINT32)Volume->FatCount * FatSectors);
    ClusterCount = DataSectors / (UINT32)Volume->SectorsPerCluster;

    if (ClusterCount < 65525U)
    {
        return EFI_UNSUPPORTED;
    }

    Volume->FatSectors = FatSectors;
    Volume->ClusterCount = ClusterCount;
    Volume->DataStartSector = (UINT32)Volume->ReservedSectors + ((UINT32)Volume->FatCount * FatSectors);
    Volume->TotalClustersAvailable = ClusterCount;
    return EFI_SUCCESS;
}

UINT64 LosInstallerFat32ClusterToLba(const LOS_FAT32_VOLUME *Volume, UINT32 Cluster)
{
    return Volume->StartLba + (UINT64)Volume->DataStartSector + ((UINT64)(Cluster - 2U) * (UINT64)Volume->SectorsPerCluster);
}

LOS_FAT32_ALLOCATION LosInstallerAllocateClusters(UINT32 *NextCluster, UINT32 RequestedClusters)
{
    LOS_FAT32_ALLOCATION Allocation;

    Allocation.StartCluster = *NextCluster;
    Allocation.ClusterCount = RequestedClusters;
    *NextCluster += RequestedClusters;
    return Allocation;
}

void LosInstallerSetFatChain(UINT32 *FatEntries, LOS_FAT32_ALLOCATION Allocation)
{
    UINT32 Index;

    for (Index = 0; Index < Allocation.ClusterCount; ++Index)
    {
        UINT32 Cluster = Allocation.StartCluster + Index;
        if (Index + 1U == Allocation.ClusterCount)
        {
            FatEntries[Cluster] = 0x0FFFFFFFU;
        }
        else
        {
            FatEntries[Cluster] = Cluster + 1U;
        }
    }
}
