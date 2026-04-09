/*
 * File Name: BootConsole.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "BootInternal.h"

static void LosBootSetTextAttribute(EFI_SYSTEM_TABLE *SystemTable, UINTN Attribute)
{
    if (SystemTable == 0 || SystemTable->ConOut == 0 || SystemTable->ConOut->SetAttribute == 0)
    {
        return;
    }

    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, Attribute);
}

static void LosBootPrintColoredPrefix(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, UINTN PrefixAttribute)
{
    LosBootSetTextAttribute(SystemTable, PrefixAttribute);
    LosBootPrint(SystemTable, Prefix);
    LosBootSetTextAttribute(SystemTable, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

void LosBootPrint(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    if (SystemTable == 0 || SystemTable->ConOut == 0 || SystemTable->ConOut->OutputString == 0)
    {
        return;
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, (CHAR16 *)Text);
}

void LosBootStatusOk(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    LosBootPrintColoredPrefix(SystemTable, LOS_TEXT("[OK] "), EFI_TEXT_ATTR(EFI_LIGHTGREEN, EFI_BLACK));
    LosBootPrint(SystemTable, Text);
    LosBootPrint(SystemTable, LOS_TEXT("\r\n"));
}

void LosBootStatusFail(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    LosBootPrintColoredPrefix(SystemTable, LOS_TEXT("[FAIL] "), EFI_TEXT_ATTR(EFI_LIGHTRED, EFI_BLACK));
    LosBootPrint(SystemTable, Text);
    LosBootPrint(SystemTable, LOS_TEXT("\r\n"));
}

void LosBootTrace(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Text)
{
    (void)SystemTable;
    (void)Text;
}

void LosBootTracePath(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, const CHAR16 *Path)
{
    (void)SystemTable;
    (void)Prefix;
    (void)Path;
}

void LosBootTraceHex64(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, UINT64 Value)
{
    (void)SystemTable;
    (void)Prefix;
    (void)Value;
}

void LosBootTraceStatus(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Prefix, EFI_STATUS Status)
{
    (void)SystemTable;
    (void)Prefix;
    (void)Status;
}

void LosBootAnnounceFunction(EFI_SYSTEM_TABLE *SystemTable, const char *FunctionName)
{
    (void)SystemTable;
    (void)FunctionName;
}

void LosBootClear(EFI_SYSTEM_TABLE *SystemTable)
{
    LOS_BOOT_ENTER(SystemTable);
    if (SystemTable == 0 || SystemTable->ConOut == 0 || SystemTable->ConOut->ClearScreen == 0)
    {
        return;
    }

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    LosBootSetTextAttribute(SystemTable, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
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
    LosBootPrintColoredPrefix(SystemTable, LOS_TEXT("[FAIL] "), EFI_TEXT_ATTR(EFI_LIGHTRED, EFI_BLACK));
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
