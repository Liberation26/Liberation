#include "BootInternal.h"

void LosBootPrint(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    if (SystemTable == 0 || SystemTable->ConOut == 0 || SystemTable->ConOut->OutputString == 0)
    {
        return;
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, (CHAR16 *)Text);
}

void LosBootTrace(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    LosBootPrint(SystemTable, LOS_TEXT("[Boot] "));
    LosBootPrint(SystemTable, Text);
    LosBootPrint(SystemTable, LOS_TEXT("\r\n"));
}

void LosBootTracePath(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, const CHAR16 *Path)
{
    LosBootPrint(SystemTable, LOS_TEXT("[Boot] "));
    LosBootPrint(SystemTable, Prefix);
    if (Path != 0)
    {
        LosBootPrint(SystemTable, Path);
    }
    else
    {
        LosBootPrint(SystemTable, LOS_TEXT("(null)"));
    }
    LosBootPrint(SystemTable, LOS_TEXT("\r\n"));
}

void LosBootTraceHex64(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, UINT64 Value)
{
    LosBootPrint(SystemTable, LOS_TEXT("[Boot] "));
    LosBootPrint(SystemTable, Prefix);
    LosBootPrintHex64(SystemTable, Value);
    LosBootPrint(SystemTable, LOS_TEXT("\r\n"));
}

void LosBootTraceStatus(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, EFI_STATUS Status)
{
    LosBootPrint(SystemTable, LOS_TEXT("[Boot] "));
    LosBootPrint(SystemTable, Prefix);
    LosBootPrintHex64(SystemTable, Status);
    LosBootPrint(SystemTable, LOS_TEXT("\r\n"));
}


void LosBootAnnounceFunction(EFI_SYSTEM_TABLE *SystemTable, const char *FunctionName)
{
    CHAR16 Buffer[96];
    UINTN Index;

    if (SystemTable == 0 || FunctionName == 0)
    {
        return;
    }

    Buffer[0] = L'[';
    Buffer[1] = L'B';
    Buffer[2] = L'o';
    Buffer[3] = L'o';
    Buffer[4] = L't';
    Buffer[5] = L']';
    Buffer[6] = L' ';
    Buffer[7] = L'E';
    Buffer[8] = L'n';
    Buffer[9] = L't';
    Buffer[10] = L'e';
    Buffer[11] = L'r';
    Buffer[12] = L' ';
    Index = 13U;

    while (*FunctionName != '\0' && Index + 3U < (sizeof(Buffer) / sizeof(Buffer[0])))
    {
        Buffer[Index] = (CHAR16)(UINT8)(*FunctionName);
        ++Index;
        ++FunctionName;
    }

    Buffer[Index++] = L'\r';
    Buffer[Index++] = L'\n';
    Buffer[Index] = 0;
    LosBootPrint(SystemTable, Buffer);
}

void LosBootClear(EFI_SYSTEM_TABLE *SystemTable)
{
    LOS_BOOT_ENTER(SystemTable);
    if (SystemTable == 0 || SystemTable->ConOut == 0 || SystemTable->ConOut->ClearScreen == 0)
    {
        return;
    }

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
}

void LosBootPrintHex64(EFI_SYSTEM_TABLE *SystemTable, UINT64 Value)
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
    LosBootPrint(SystemTable, Buffer);
}

void LosBootPrintStatusError(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, EFI_STATUS Status)
{
    LOS_BOOT_ENTER(SystemTable);
    LosBootPrint(SystemTable, Prefix);
    LosBootPrintHex64(SystemTable, Status);
    LosBootPrint(SystemTable, LOS_TEXT("\r\n"));
}

#if !defined(LIBERATION_BOOT_FROM_ISO)
void LosBootHaltForever(void)
{
    for (;;)
    {
        __asm__ __volatile__("hlt");
    }
}

void LosBootMemorySet(void *Destination, UINT8 Value, UINTN Size)
{
    UINT8 *Bytes = (UINT8 *)Destination;
    UINTN Index;
    for (Index = 0; Index < Size; ++Index)
    {
        Bytes[Index] = Value;
    }
}

void LosBootMemoryCopy(void *Destination, const void *Source, UINTN Size)
{
    UINT8 *DestinationBytes = (UINT8 *)Destination;
    const UINT8 *SourceBytes = (const UINT8 *)Source;
    UINTN Index;
    for (Index = 0; Index < Size; ++Index)
    {
        DestinationBytes[Index] = SourceBytes[Index];
    }
}

UINT64 LosBootAlignDown(UINT64 Value, UINT64 Alignment)
{
    return Value & ~(Alignment - 1ULL);
}

UINT64 LosBootAlignUp(UINT64 Value, UINT64 Alignment)
{
    return (Value + Alignment - 1ULL) & ~(Alignment - 1ULL);
}
#endif
