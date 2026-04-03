#include "InstallerInternal.h"

void LosInstallerPrint(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    if (SystemTable == 0 || SystemTable->ConOut == 0 || SystemTable->ConOut->OutputString == 0)
    {
        return;
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, (CHAR16 *)Text);
}

void LosInstallerHaltForever(void)
{
    for (;;)
    {
        __asm__ __volatile__("hlt");
    }
}

void LosInstallerMemorySet(void *Destination, UINT8 Value, UINTN Size)
{
    UINT8 *Bytes;
    UINTN Index;

    Bytes = (UINT8 *)Destination;
    for (Index = 0; Index < Size; ++Index)
    {
        Bytes[Index] = Value;
    }
}

void LosInstallerMemoryCopy(void *Destination, const void *Source, UINTN Size)
{
    UINT8 *DestBytes;
    const UINT8 *SourceBytes;
    UINTN Index;

    DestBytes = (UINT8 *)Destination;
    SourceBytes = (const UINT8 *)Source;
    for (Index = 0; Index < Size; ++Index)
    {
        DestBytes[Index] = SourceBytes[Index];
    }
}

UINTN LosInstallerStringLength16(const CHAR16 *Text)
{
    UINTN Length;

    if (Text == 0)
    {
        return 0;
    }

    Length = 0;
    while (Text[Length] != 0)
    {
        ++Length;
    }

    return Length;
}

void LosInstallerPrintChar(EFI_SYSTEM_TABLE *SystemTable, CHAR16 Character)
{
    CHAR16 Buffer[2];

    Buffer[0] = Character;
    Buffer[1] = 0;
    LosInstallerPrint(SystemTable, Buffer);
}

void LosInstallerPrintUnsigned(EFI_SYSTEM_TABLE *SystemTable, UINT64 Value)
{
    CHAR16 Buffer[32];
    UINTN Index;
    UINTN OutputIndex;

    if (Value == 0)
    {
        LosInstallerPrintChar(SystemTable, L'0');
        return;
    }

    Index = 0;
    while (Value != 0 && Index < (sizeof(Buffer) / sizeof(Buffer[0])) - 1U)
    {
        Buffer[Index++] = (CHAR16)(L'0' + (Value % 10ULL));
        Value /= 10ULL;
    }

    for (OutputIndex = 0; OutputIndex < Index / 2U; ++OutputIndex)
    {
        CHAR16 Temp = Buffer[OutputIndex];
        Buffer[OutputIndex] = Buffer[Index - 1U - OutputIndex];
        Buffer[Index - 1U - OutputIndex] = Temp;
    }

    Buffer[Index] = 0;
    LosInstallerPrint(SystemTable, Buffer);
}

void LosInstallerPrintHex64(EFI_SYSTEM_TABLE *SystemTable, UINT64 Value)
{
    CHAR16 Buffer[19];
    UINTN Index;

    Buffer[0] = L'0';
    Buffer[1] = L'x';
    for (Index = 0; Index < 16U; ++Index)
    {
        UINTN Shift = (UINTN)((15U - Index) * 4U);
        UINT8 Nibble = (UINT8)((Value >> Shift) & 0xFULL);
        Buffer[2U + Index] = (CHAR16)((Nibble < 10U) ? (L'0' + Nibble) : (L'A' + (Nibble - 10U)));
    }
    Buffer[18] = 0;
    LosInstallerPrint(SystemTable, Buffer);
}

void LosInstallerPrintMiB(EFI_SYSTEM_TABLE *SystemTable, UINT64 ByteCount)
{
    LosInstallerPrintUnsigned(SystemTable, ByteCount / (1024ULL * 1024ULL));
    LosInstallerPrint(SystemTable, LOS_TEXT(" MiB"));
}

void LosInstallerStallSeconds(EFI_SYSTEM_TABLE *SystemTable, UINTN Seconds)
{
    EFI_BOOT_SERVICES *BootServices;
    UINTN SecondIndex;

    if (SystemTable == 0)
    {
        return;
    }

    BootServices = SystemTable->BootServices;
    if (BootServices == 0 || BootServices->Stall == 0)
    {
        return;
    }

    for (SecondIndex = 0; SecondIndex < Seconds; ++SecondIndex)
    {
        BootServices->Stall(1000000U);
    }
}

void LosInstallerPrintRebootCountdown(EFI_SYSTEM_TABLE *SystemTable, UINTN SecondsRemaining)
{
    LosInstallerPrint(SystemTable, LOS_TEXT("\rRebooting in "));
    LosInstallerPrintUnsigned(SystemTable, SecondsRemaining);
    LosInstallerPrint(SystemTable, LOS_TEXT(" seconds...   "));
}

void LosInstallerRequestColdReboot(EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_RUNTIME_SERVICES *RuntimeServices;
    UINTN SecondsRemaining;

    if (SystemTable == 0)
    {
        LosInstallerHaltForever();
    }

    for (SecondsRemaining = LOS_REBOOT_COUNTDOWN_SECONDS; SecondsRemaining > 0U; --SecondsRemaining)
    {
        LosInstallerPrintRebootCountdown(SystemTable, SecondsRemaining);
        LosInstallerStallSeconds(SystemTable, 1U);
    }

    LosInstallerPrint(SystemTable, LOS_TEXT("\rRebooting now.          \r\n"));

    RuntimeServices = (EFI_RUNTIME_SERVICES *)SystemTable->RuntimeServices;
    if (RuntimeServices != 0 && RuntimeServices->ResetSystem != 0)
    {
        RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, 0);
    }

    LosInstallerHaltForever();
}

EFI_STATUS LosInstallerOpenRootForHandle(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE DeviceHandle,
    EFI_FILE_PROTOCOL **Root)
{
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_STATUS Status;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || Root == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *Root = 0;
    FileSystem = 0;
    Status = SystemTable->BootServices->HandleProtocol(
        DeviceHandle,
        (EFI_GUID *)&EfiSimpleFileSystemProtocolGuid,
        (void **)&FileSystem);
    if (EFI_ERROR(Status) || FileSystem == 0)
    {
        return Status;
    }

    Status = FileSystem->OpenVolume(FileSystem, Root);
    return Status;
}

EFI_STATUS LosInstallerReadFileIntoPool(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_FILE_PROTOCOL *Root,
    const CHAR16 *Path,
    void **Buffer,
    UINTN *BufferSize)
{
    EFI_FILE_PROTOCOL *File;
    EFI_STATUS Status;
    UINT8 FileInfoBuffer[LOS_FILE_INFO_BUFFER_SIZE];
    UINTN FileInfoSize;
    EFI_FILE_INFO *FileInfo;
    void *AllocatedBuffer;
    UINTN ReadSize;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || Root == 0 || Path == 0 || Buffer == 0 || BufferSize == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    *Buffer = 0;
    *BufferSize = 0;
    File = 0;
    AllocatedBuffer = 0;

    Status = Root->Open(Root, &File, (CHAR16 *)Path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || File == 0)
    {
        return Status;
    }

    FileInfoSize = sizeof(FileInfoBuffer);
    Status = File->GetInfo(File, (EFI_GUID *)&EfiFileInfoGuid, &FileInfoSize, FileInfoBuffer);
    if (EFI_ERROR(Status) || FileInfoSize < sizeof(EFI_FILE_INFO))
    {
        File->Close(File);
        return Status;
    }

    FileInfo = (EFI_FILE_INFO *)(void *)FileInfoBuffer;
    if (FileInfo->FileSize == 0)
    {
        File->Close(File);
        return EFI_NOT_FOUND;
    }

    Status = SystemTable->BootServices->AllocatePool(
        EfiLoaderData,
        (UINTN)FileInfo->FileSize,
        &AllocatedBuffer);
    if (EFI_ERROR(Status) || AllocatedBuffer == 0)
    {
        File->Close(File);
        return Status;
    }

    ReadSize = (UINTN)FileInfo->FileSize;
    Status = File->Read(File, &ReadSize, AllocatedBuffer);
    File->Close(File);
    if (EFI_ERROR(Status) || ReadSize != (UINTN)FileInfo->FileSize)
    {
        SystemTable->BootServices->FreePool(AllocatedBuffer);
        return EFI_OUT_OF_RESOURCES;
    }

    *Buffer = AllocatedBuffer;
    *BufferSize = (UINTN)FileInfo->FileSize;
    return EFI_SUCCESS;
}
