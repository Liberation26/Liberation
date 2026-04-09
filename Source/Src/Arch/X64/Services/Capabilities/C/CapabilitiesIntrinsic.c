/*
 * File Name: CapabilitiesIntrinsic.c
 * File Version: 0.3.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T08:35:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Provides freestanding intrinsic implementations required by the capabilities service.
 */

#include "CapabilitiesMain.h"

void *memcpy(void *Destination, const void *Source, UINTN Size)
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
