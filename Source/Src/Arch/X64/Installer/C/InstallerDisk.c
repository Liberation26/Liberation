/*
 * File Name: InstallerDisk.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "InstallerInternal.h"

EFI_STATUS LosInstallerLocateBlockIoHandles(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE **HandleBuffer,
    UINTN *HandleCount)
{
    EFI_STATUS Status;
    UINTN BufferSize;
    EFI_HANDLE *Buffer;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || HandleBuffer == 0 || HandleCount == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *HandleBuffer = 0;
    *HandleCount = 0;
    BufferSize = 0;
    Buffer = 0;

    Status = SystemTable->BootServices->LocateHandle(
        ByProtocol,
        (EFI_GUID *)&EfiBlockIoProtocolGuid,
        0,
        &BufferSize,
        0);
    if (Status != EFI_BUFFER_TOO_SMALL)
    {
        return Status;
    }

    Status = SystemTable->BootServices->AllocatePool(EfiLoaderData, BufferSize, (void **)&Buffer);
    if (EFI_ERROR(Status) || Buffer == 0)
    {
        return Status;
    }

    Status = SystemTable->BootServices->LocateHandle(
        ByProtocol,
        (EFI_GUID *)&EfiBlockIoProtocolGuid,
        0,
        &BufferSize,
        Buffer);
    if (EFI_ERROR(Status))
    {
        SystemTable->BootServices->FreePool(Buffer);
        return Status;
    }

    *HandleBuffer = Buffer;
    *HandleCount = BufferSize / sizeof(EFI_HANDLE);
    return EFI_SUCCESS;
}

UINT64 LosInstallerAlignUp(UINT64 Value, UINT64 Alignment)
{
    UINT64 Mask;

    if (Alignment == 0)
    {
        return Value;
    }

    Mask = Alignment - 1ULL;
    return (Value + Mask) & ~Mask;
}

UINT32 LosInstallerCalculateCrc32(const void *Buffer, UINTN BufferSize)
{
    const UINT8 *Bytes;
    UINT32 Crc;
    UINTN Index;
    UINTN BitIndex;

    Bytes = (const UINT8 *)Buffer;
    Crc = 0xFFFFFFFFU;
    for (Index = 0; Index < BufferSize; ++Index)
    {
        Crc ^= (UINT32)Bytes[Index];
        for (BitIndex = 0; BitIndex < 8U; ++BitIndex)
        {
            UINT32 Mask = (Crc & 1U) ? 0xFFFFFFFFU : 0U;
            Crc = (Crc >> 1U) ^ (0xEDB88320U & Mask);
        }
    }

    return ~Crc;
}

EFI_GUID LosInstallerCreateGuidFromSeed(UINT64 Seed)
{
    EFI_GUID Guid;
    UINT64 State;
    UINT8 *Bytes;
    UINTN Index;

    Bytes = (UINT8 *)&Guid;
    State = Seed ^ 0x9E3779B97F4A7C15ULL;
    for (Index = 0; Index < sizeof(EFI_GUID); ++Index)
    {
        State ^= State >> 12U;
        State ^= State << 25U;
        State ^= State >> 27U;
        State *= 2685821657736338717ULL;
        Bytes[Index] = (UINT8)(State >> 56U);
    }

    Guid.Data3 = (UINT16)((Guid.Data3 & 0x0FFFU) | 0x4000U);
    Guid.Data4[0] = (UINT8)((Guid.Data4[0] & 0x3FU) | 0x80U);
    return Guid;
}

void LosInstallerCopyPartitionName(CHAR16 *Destination, const CHAR16 *Source, UINTN Capacity)
{
    UINTN Index;

    for (Index = 0; Index < Capacity; ++Index)
    {
        if (Source != 0 && Source[Index] != 0)
        {
            Destination[Index] = Source[Index];
        }
        else
        {
            Destination[Index] = 0;
        }
    }
}

void LosInstallerCopyString16(CHAR16 *Destination, UINTN Capacity, const CHAR16 *Source)
{
    UINTN Index;

    if (Destination == 0 || Capacity == 0U)
    {
        return;
    }

    for (Index = 0; Index + 1U < Capacity; ++Index)
    {
        if (Source == 0 || Source[Index] == 0)
        {
            break;
        }

        Destination[Index] = Source[Index];
    }

    Destination[Index] = 0;
}

EFI_STATUS LosInstallerGetInstallCandidates(
    EFI_SYSTEM_TABLE *SystemTable,
    LOS_INSTALL_CANDIDATE *Candidates,
    UINTN CandidateCapacity,
    UINTN *CandidateCount)
{
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    EFI_STATUS Status;
    UINTN Index;
    UINTN Count;

    if (SystemTable == 0 || Candidates == 0 || CandidateCount == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *CandidateCount = 0;
    HandleBuffer = 0;
    HandleCount = 0;

    Status = LosInstallerLocateBlockIoHandles(SystemTable, &HandleBuffer, &HandleCount);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Count = 0;
    for (Index = 0; Index < HandleCount && Count < CandidateCapacity; ++Index)
    {
        EFI_BLOCK_IO_PROTOCOL *BlockIo;
        EFI_BLOCK_IO_MEDIA *Media;
        UINT64 TotalBlocks;
        UINT64 TotalBytes;

        BlockIo = 0;
        Status = SystemTable->BootServices->HandleProtocol(
            HandleBuffer[Index],
            (EFI_GUID *)&EfiBlockIoProtocolGuid,
            (void **)&BlockIo);
        if (EFI_ERROR(Status) || BlockIo == 0 || BlockIo->Media == 0)
        {
            continue;
        }

        Media = BlockIo->Media;
        if (!Media->MediaPresent || Media->LogicalPartition || Media->ReadOnly)
        {
            continue;
        }

        if (Media->BlockSize != LOS_SECTOR_SIZE_512)
        {
            continue;
        }

        TotalBlocks = (UINT64)Media->LastBlock + 1ULL;
        TotalBytes = TotalBlocks * (UINT64)Media->BlockSize;
        if (TotalBytes < (UINT64)LOS_MIN_TARGET_DISK_MIB * 1024ULL * 1024ULL)
        {
            continue;
        }

        Candidates[Count].Handle = HandleBuffer[Index];
        Candidates[Count].BlockIo = BlockIo;
        Candidates[Count].TotalBlocks = TotalBlocks;
        Candidates[Count].TotalBytes = TotalBytes;
        ++Count;
    }

    SystemTable->BootServices->FreePool(HandleBuffer);
    *CandidateCount = Count;
    return (Count == 0U) ? EFI_NOT_FOUND : EFI_SUCCESS;
}

void LosInstallerPrintCandidateLine(EFI_SYSTEM_TABLE *SystemTable, UINTN CandidateNumber, const LOS_INSTALL_CANDIDATE *Candidate)
{
    LosInstallerPrint(SystemTable, LOS_TEXT("  ["));
    LosInstallerPrintUnsigned(SystemTable, CandidateNumber);
    LosInstallerPrint(SystemTable, LOS_TEXT("] "));
    LosInstallerPrintMiB(SystemTable, Candidate->TotalBytes);
    LosInstallerPrint(SystemTable, LOS_TEXT(" target disk, block size "));
    LosInstallerPrintUnsigned(SystemTable, Candidate->BlockIo->Media->BlockSize);
    LosInstallerPrint(SystemTable, LOS_TEXT(" bytes"));
    if (Candidate->BlockIo->Media->RemovableMedia)
    {
        LosInstallerPrint(SystemTable, LOS_TEXT(", removable"));
    }
    else
    {
        LosInstallerPrint(SystemTable, LOS_TEXT(", fixed"));
    }
    LosInstallerPrint(SystemTable, LOS_TEXT("\r\n"));
}

EFI_STATUS LosInstallerReadSingleKey(EFI_SYSTEM_TABLE *SystemTable, EFI_INPUT_KEY *Key)
{
    EFI_STATUS Status;

    if (SystemTable == 0 || SystemTable->ConIn == 0 || Key == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    for (;;)
    {
        Status = SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, Key);
        if (Status == EFI_NOT_READY)
        {
            LosInstallerStallSeconds(SystemTable, 0U);
            if (SystemTable->BootServices != 0 && SystemTable->BootServices->Stall != 0)
            {
                SystemTable->BootServices->Stall(10000U);
            }
            continue;
        }

        return Status;
    }
}

EFI_STATUS LosInstallerChooseTargetDisk(
    EFI_SYSTEM_TABLE *SystemTable,
    const LOS_INSTALL_CANDIDATE *Candidates,
    UINTN CandidateCount,
    UINTN *SelectedIndex)
{
    EFI_INPUT_KEY Key;
    EFI_STATUS Status;
    UINTN Index;

    if (SystemTable == 0 || Candidates == 0 || SelectedIndex == 0 || CandidateCount == 0U)
    {
        return EFI_INVALID_PARAMETER;
    }

    LosInstallerPrint(SystemTable, LOS_TEXT("Available installation targets:\r\n"));
    for (Index = 0; Index < CandidateCount; ++Index)
    {
        LosInstallerPrintCandidateLine(SystemTable, Index + 1U, &Candidates[Index]);
    }
    LosInstallerPrint(SystemTable, LOS_TEXT("\r\n"));

    if (CandidateCount == 1U)
    {
        LosInstallerPrint(SystemTable, LOS_TEXT("Only one writable disk is available. Selecting target [1].\r\n"));
        *SelectedIndex = 0U;
        return EFI_SUCCESS;
    }

    LosInstallerPrint(SystemTable, LOS_TEXT("Press the target number to erase and install Liberation.\r\n"));
    for (;;)
    {
        Status = LosInstallerReadSingleKey(SystemTable, &Key);
        if (EFI_ERROR(Status))
        {
            return Status;
        }

        if (Key.UnicodeChar >= L'1' && Key.UnicodeChar < (CHAR16)(L'1' + CandidateCount) && CandidateCount <= 9U)
        {
            *SelectedIndex = (UINTN)(Key.UnicodeChar - L'1');
            LosInstallerPrint(SystemTable, LOS_TEXT("You pressed: "));
            LosInstallerPrintChar(SystemTable, Key.UnicodeChar);
            LosInstallerPrint(SystemTable, LOS_TEXT("\r\nSelected target ["));
            LosInstallerPrintUnsigned(SystemTable, *SelectedIndex + 1U);
            LosInstallerPrint(SystemTable, LOS_TEXT("]\r\n"));
            return EFI_SUCCESS;
        }
    }
}

