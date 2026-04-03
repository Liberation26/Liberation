#include "MonitorInternal.h"

void LosMonitorPrint(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    if (SystemTable == 0 || SystemTable->ConOut == 0 || SystemTable->ConOut->OutputString == 0)
    {
        return;
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, (CHAR16 *)Text);
}

void LosMonitorTrace(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    LosMonitorPrint(SystemTable, LOS_TEXT("[Monitor] "));
    LosMonitorPrint(SystemTable, Text);
    LosMonitorPrint(SystemTable, LOS_TEXT("\r\n"));
}

void LosMonitorTracePath(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, const CHAR16 *Path)
{
    LosMonitorPrint(SystemTable, LOS_TEXT("[Monitor] "));
    LosMonitorPrint(SystemTable, Prefix);
    if (Path != 0)
    {
        LosMonitorPrint(SystemTable, Path);
    }
    else
    {
        LosMonitorPrint(SystemTable, LOS_TEXT("(null)"));
    }
    LosMonitorPrint(SystemTable, LOS_TEXT("\r\n"));
}

void LosMonitorTraceHex64(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, UINT64 Value)
{
    CHAR16 Buffer[19];
    UINTN Index;

    LosMonitorPrint(SystemTable, LOS_TEXT("[Monitor] "));
    LosMonitorPrint(SystemTable, Prefix);
    Buffer[0] = L'0';
    Buffer[1] = L'x';
    for (Index = 0; Index < 16U; ++Index)
    {
        UINTN Shift = (UINTN)((15U - Index) * 4U);
        UINT8 Nibble = (UINT8)((Value >> Shift) & 0xFULL);
        Buffer[2U + Index] = (CHAR16)((Nibble < 10U) ? (L'0' + Nibble) : (L'A' + (Nibble - 10U)));
    }
    Buffer[18] = 0;
    LosMonitorPrint(SystemTable, Buffer);
    LosMonitorPrint(SystemTable, LOS_TEXT("\r\n"));
}

void LosMonitorTraceStatus(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, EFI_STATUS Status)
{
    LosMonitorTraceHex64(SystemTable, Prefix, (UINT64)Status);
}


void LosMonitorAnnounceFunction(EFI_SYSTEM_TABLE *SystemTable, const char *FunctionName)
{
    CHAR16 Buffer[96];
    UINTN Index;

    if (SystemTable == 0 || FunctionName == 0)
    {
        return;
    }

    Buffer[0] = L'[';
    Buffer[1] = L'M';
    Buffer[2] = L'o';
    Buffer[3] = L'n';
    Buffer[4] = L'i';
    Buffer[5] = L't';
    Buffer[6] = L'o';
    Buffer[7] = L'r';
    Buffer[8] = L']';
    Buffer[9] = L' ';
    Buffer[10] = L'E';
    Buffer[11] = L'n';
    Buffer[12] = L't';
    Buffer[13] = L'e';
    Buffer[14] = L'r';
    Buffer[15] = L' ';
    Index = 16U;

    while (*FunctionName != '\0' && Index + 3U < (sizeof(Buffer) / sizeof(Buffer[0])))
    {
        Buffer[Index] = (CHAR16)(UINT8)(*FunctionName);
        ++Index;
        ++FunctionName;
    }

    Buffer[Index++] = L'\r';
    Buffer[Index++] = L'\n';
    Buffer[Index] = 0;
    LosMonitorPrint(SystemTable, Buffer);
}

void LosMonitorHaltForever(void)
{
    for (;;)
    {
        __asm__ __volatile__("hlt");
    }
}

void LosMonitorMemorySet(void *Destination, UINT8 Value, UINTN Size)
{
    UINT8 *Bytes = (UINT8 *)Destination;
    UINTN Index;
    for (Index = 0; Index < Size; ++Index)
    {
        Bytes[Index] = Value;
    }
}

void LosMonitorMemoryCopy(void *Destination, const void *Source, UINTN Size)
{
    UINT8 *DestinationBytes = (UINT8 *)Destination;
    const UINT8 *SourceBytes = (const UINT8 *)Source;
    UINTN Index;
    for (Index = 0; Index < Size; ++Index)
    {
        DestinationBytes[Index] = SourceBytes[Index];
    }
}

UINT64 LosMonitorAlignDown(UINT64 Value, UINT64 Alignment)
{
    return Value & ~(Alignment - 1ULL);
}

UINT64 LosMonitorAlignUp(UINT64 Value, UINT64 Alignment)
{
    return (Value + Alignment - 1ULL) & ~(Alignment - 1ULL);
}


void LosMonitorUtf16Copy(CHAR16 *Destination, UINTN DestinationCharacterCount, const CHAR16 *Source)
{
    UINTN Index;

    if (Destination == 0 || DestinationCharacterCount == 0U)
    {
        return;
    }

    Index = 0U;
    if (Source != 0)
    {
        while (Source[Index] != 0 && (Index + 1U) < DestinationCharacterCount)
        {
            Destination[Index] = Source[Index];
            ++Index;
        }
    }

    Destination[Index] = 0;
}
