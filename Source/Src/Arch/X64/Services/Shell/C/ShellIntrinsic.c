/*
 * File Name: ShellIntrinsic.c
 * File Version: 0.0.2
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T20:05:00Z
 * Last Update Timestamp: 2026-04-09T20:15:00Z
 * Operating System Name: Liberation OS
 * Purpose: Provides freestanding intrinsic implementations required by the shell service.
 */

#include "ShellMain.h"

__attribute__((weak)) void *memcpy(void *Destination, const void *Source, UINTN Size)
{
    UINT8 *DestinationBytes;
    const UINT8 *SourceBytes;
    UINTN Index;

    if (Destination == 0 || Source == 0)
    {
        return Destination;
    }

    DestinationBytes = (UINT8 *)Destination;
    SourceBytes = (const UINT8 *)Source;
    for (Index = 0U; Index < Size; ++Index)
    {
        DestinationBytes[Index] = SourceBytes[Index];
    }

    return Destination;
}

__attribute__((weak)) void *memmove(void *Destination, const void *Source, UINTN Size)
{
    UINT8 *DestinationBytes;
    const UINT8 *SourceBytes;
    UINTN Index;

    if (Destination == 0 || Source == 0 || Destination == Source)
    {
        return Destination;
    }

    DestinationBytes = (UINT8 *)Destination;
    SourceBytes = (const UINT8 *)Source;
    if (DestinationBytes < SourceBytes)
    {
        for (Index = 0U; Index < Size; ++Index)
        {
            DestinationBytes[Index] = SourceBytes[Index];
        }
    }
    else
    {
        for (Index = Size; Index != 0U; --Index)
        {
            DestinationBytes[Index - 1U] = SourceBytes[Index - 1U];
        }
    }

    return Destination;
}

__attribute__((weak)) void *memset(void *Destination, int Value, UINTN Size)
{
    UINT8 *DestinationBytes;
    UINTN Index;

    if (Destination == 0)
    {
        return Destination;
    }

    DestinationBytes = (UINT8 *)Destination;
    for (Index = 0U; Index < Size; ++Index)
    {
        DestinationBytes[Index] = (UINT8)Value;
    }

    return Destination;
}
