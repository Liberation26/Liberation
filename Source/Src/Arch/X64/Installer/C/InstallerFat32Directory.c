#include "InstallerInternal.h"

void LosInstallerCopyVolumeLabel11(UINT8 *Destination, const CHAR16 *Source)
{
    UINTN Index;

    for (Index = 0; Index < 11U; ++Index)
    {
        if (Source != 0 && Source[Index] != 0)
        {
            Destination[Index] = (UINT8)Source[Index];
        }
        else
        {
            Destination[Index] = (UINT8)' ';
        }
    }
}

void LosInstallerFillShortName(UINT8 *Destination, const char *Base, const char *Extension)
{
    UINTN Index;

    for (Index = 0; Index < 8U; ++Index)
    {
        Destination[Index] = (UINT8)((Base[Index] != 0) ? Base[Index] : ' ');
    }

    for (Index = 0; Index < 3U; ++Index)
    {
        Destination[8U + Index] = (UINT8)((Extension[Index] != 0) ? Extension[Index] : ' ');
    }
}

void LosInstallerCreateShortEntry(
    LOS_FAT_DIRECTORY_ENTRY *Entry,
    const UINT8 *Name,
    UINT8 Attributes,
    UINT32 FirstCluster,
    UINT32 FileSize)
{
    LosInstallerMemorySet(Entry, 0, sizeof(*Entry));
    LosInstallerMemoryCopy(Entry->Name, Name, 11U);
    Entry->Attributes = Attributes;
    Entry->FirstClusterHigh = (UINT16)(FirstCluster >> 16U);
    Entry->FirstClusterLow = (UINT16)(FirstCluster & 0xFFFFU);
    Entry->FileSize = FileSize;
}

UINT8 LosInstallerCalculateShortNameChecksum(const UINT8 *ShortName)
{
    UINT8 Checksum;
    UINTN Index;

    Checksum = 0U;
    for (Index = 0; Index < 11U; ++Index)
    {
        Checksum = (UINT8)(((Checksum & 1U) ? 0x80U : 0U) + (Checksum >> 1U) + ShortName[Index]);
    }

    return Checksum;
}

void LosInstallerFillLfnNameFields(LOS_FAT_LFN_ENTRY *Entry, const CHAR16 *Name, UINTN StartIndex, UINTN Length)
{
    UINTN Index;
    UINT16 *Targets[13];

    Targets[0] = &Entry->Name1[0];
    Targets[1] = &Entry->Name1[1];
    Targets[2] = &Entry->Name1[2];
    Targets[3] = &Entry->Name1[3];
    Targets[4] = &Entry->Name1[4];
    Targets[5] = &Entry->Name2[0];
    Targets[6] = &Entry->Name2[1];
    Targets[7] = &Entry->Name2[2];
    Targets[8] = &Entry->Name2[3];
    Targets[9] = &Entry->Name2[4];
    Targets[10] = &Entry->Name2[5];
    Targets[11] = &Entry->Name3[0];
    Targets[12] = &Entry->Name3[1];

    for (Index = 0; Index < 13U; ++Index)
    {
        UINTN NameIndex;

        NameIndex = StartIndex + Index;
        if (NameIndex < Length)
        {
            *Targets[Index] = (UINT16)Name[NameIndex];
        }
        else if (NameIndex == Length)
        {
            *Targets[Index] = 0x0000U;
        }
        else
        {
            *Targets[Index] = 0xFFFFU;
        }
    }
}

UINTN LosInstallerGetRequiredLfnEntryCount(const CHAR16 *Name)
{
    UINTN Length;

    Length = LosInstallerStringLength16(Name);
    return (Length + 12U) / 13U;
}

void LosInstallerCreateLfnEntries(LOS_FAT_LFN_ENTRY *Entries, const CHAR16 *Name, const UINT8 *ShortName)
{
    UINTN EntryCount;
    UINTN EntryIndex;
    UINTN NameLength;
    UINT8 Checksum;

    NameLength = LosInstallerStringLength16(Name);
    EntryCount = LosInstallerGetRequiredLfnEntryCount(Name);
    Checksum = LosInstallerCalculateShortNameChecksum(ShortName);

    for (EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
    {
        UINTN Sequence;
        UINTN StartIndex;
        LOS_FAT_LFN_ENTRY *Entry;

        Sequence = EntryCount - EntryIndex;
        StartIndex = (EntryCount - 1U - EntryIndex) * 13U;
        Entry = &Entries[EntryIndex];

        LosInstallerMemorySet(Entry, 0, sizeof(*Entry));
        Entry->Order = (UINT8)Sequence;
        if (EntryIndex == 0U)
        {
            Entry->Order = (UINT8)(Entry->Order | 0x40U);
        }
        Entry->Attributes = 0x0FU;
        Entry->Type = 0U;
        Entry->Checksum = Checksum;
        LosInstallerFillLfnNameFields(Entry, Name, StartIndex, NameLength);
    }
}
