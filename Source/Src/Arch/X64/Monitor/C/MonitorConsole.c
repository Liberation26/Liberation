#include "MonitorInternal.h"

void LosMonitorPrint(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    if (SystemTable == 0 || SystemTable->ConOut == 0 || SystemTable->ConOut->OutputString == 0)
    {
        return;
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, (CHAR16 *)Text);
}

void LosMonitorStatusOk(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    LosMonitorPrint(SystemTable, LOS_TEXT("[OK] "));
    LosMonitorPrint(SystemTable, Text);
    LosMonitorPrint(SystemTable, LOS_TEXT("\r\n"));
}

void LosMonitorStatusFail(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    LosMonitorPrint(SystemTable, LOS_TEXT("[FAIL] "));
    LosMonitorPrint(SystemTable, Text);
    LosMonitorPrint(SystemTable, LOS_TEXT("\r\n"));
}

void LosMonitorTrace(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    (void)SystemTable;
    (void)Text;
}

void LosMonitorTracePath(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, const CHAR16 *Path)
{
    (void)SystemTable;
    (void)Prefix;
    (void)Path;
}

void LosMonitorTraceHex64(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, UINT64 Value)
{
    (void)SystemTable;
    (void)Prefix;
    (void)Value;
}

void LosMonitorTraceStatus(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, EFI_STATUS Status)
{
    (void)SystemTable;
    (void)Prefix;
    (void)Status;
}

void LosMonitorAnnounceFunction(EFI_SYSTEM_TABLE *SystemTable, const char *FunctionName)
{
    (void)SystemTable;
    (void)FunctionName;
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
