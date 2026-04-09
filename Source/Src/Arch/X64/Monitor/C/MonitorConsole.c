/*
 * File Name: MonitorConsole.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements monitor-stage functionality for Liberation OS.
 */

#include "MonitorInternal.h"

static void LosMonitorSetTextAttribute(EFI_SYSTEM_TABLE *SystemTable, UINTN Attribute)
{
    if (SystemTable == 0 || SystemTable->ConOut == 0 || SystemTable->ConOut->SetAttribute == 0)
    {
        return;
    }

    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, Attribute);
}

static void LosMonitorPrintColoredPrefix(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, UINTN PrefixAttribute)
{
    LosMonitorSetTextAttribute(SystemTable, PrefixAttribute);
    LosMonitorPrint(SystemTable, Prefix);
    LosMonitorSetTextAttribute(SystemTable, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

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
    LosMonitorPrintColoredPrefix(SystemTable, LOS_TEXT("[OK] "), EFI_TEXT_ATTR(EFI_LIGHTGREEN, EFI_BLACK));
    LosMonitorPrint(SystemTable, Text);
    LosMonitorPrint(SystemTable, LOS_TEXT("\r\n"));
}

void LosMonitorStatusFail(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    LosMonitorPrintColoredPrefix(SystemTable, LOS_TEXT("[FAIL] "), EFI_TEXT_ATTR(EFI_LIGHTRED, EFI_BLACK));
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
