/*
 * File Name: InstallerFat32File.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "InstallerInternal.h"

EFI_STATUS LosInstallerWriteFat32FileData(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    const LOS_FAT32_VOLUME *Volume,
    LOS_FAT32_ALLOCATION Allocation,
    const void *Buffer,
    UINTN BufferSize)
{
    EFI_STATUS Status;
    UINTN ClusterBytes;
    UINT8 *ClusterBuffer;
    UINT32 ClusterIndex;
    UINTN CopiedBytes;

    ClusterBytes = (UINTN)Volume->SectorsPerCluster * LOS_SECTOR_SIZE_512;
    Status = LosInstallerAllocateZeroPool(SystemTable, ClusterBytes, (void **)&ClusterBuffer);
    if (EFI_ERROR(Status) || ClusterBuffer == 0)
    {
        return Status;
    }

    CopiedBytes = 0U;
    for (ClusterIndex = 0; ClusterIndex < Allocation.ClusterCount; ++ClusterIndex)
    {
        UINTN ThisCopy;
        UINT64 Lba;

        LosInstallerMemorySet(ClusterBuffer, 0, ClusterBytes);
        ThisCopy = BufferSize - CopiedBytes;
        if (ThisCopy > ClusterBytes)
        {
            ThisCopy = ClusterBytes;
        }
        if (ThisCopy != 0U)
        {
            LosInstallerMemoryCopy(ClusterBuffer, (const UINT8 *)Buffer + CopiedBytes, ThisCopy);
            CopiedBytes += ThisCopy;
        }

        Lba = LosInstallerFat32ClusterToLba(Volume, Allocation.StartCluster + ClusterIndex);
        Status = LosInstallerWriteBlocksChecked(BlockIo, Lba, ClusterBytes, ClusterBuffer);
        if (EFI_ERROR(Status))
        {
            SystemTable->BootServices->FreePool(ClusterBuffer);
            return Status;
        }
    }

    SystemTable->BootServices->FreePool(ClusterBuffer);
    return EFI_SUCCESS;
}
