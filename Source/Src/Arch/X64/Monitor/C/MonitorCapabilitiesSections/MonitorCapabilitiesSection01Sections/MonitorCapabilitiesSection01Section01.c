/*
 * File Name: MonitorCapabilitiesSection01Section01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MonitorCapabilitiesSection01.c.
 */

static const CHAR16 *const LosMonitorCapabilitiesPath = LOS_TEXT("\\EFI\\BOOT\\CAPABILITIES.CFG");

static BOOLEAN LosMonitorAsciiIsSpace(char Value)
{
    return (BOOLEAN)(Value == ' ' || Value == '\t' || Value == '\r' || Value == '\n');
}

static BOOLEAN LosMonitorAsciiIsDigit(char Value)
{
    return (BOOLEAN)(Value >= '0' && Value <= '9');
}

static BOOLEAN LosMonitorAsciiIsHexDigit(char Value)
{
    return (BOOLEAN)(LosMonitorAsciiIsDigit(Value) ||
                     (Value >= 'a' && Value <= 'f') ||
                     (Value >= 'A' && Value <= 'F'));
}

static UINT32 LosMonitorAsciiHexValue(char Value)
{
    if (Value >= '0' && Value <= '9')
    {
        return (UINT32)(Value - '0');
    }
    if (Value >= 'a' && Value <= 'f')
    {
        return (UINT32)(10 + (Value - 'a'));
    }
    return (UINT32)(10 + (Value - 'A'));
}

static void LosMonitorCopyAscii(char *Destination, UINTN Capacity, const char *Source)
{
    UINTN Index;

    if (Destination == 0 || Capacity == 0U)
    {
        return;
    }

    for (Index = 0U; Index < Capacity; ++Index)
    {
        Destination[Index] = 0;
    }

    if (Source == 0)
    {
        return;
    }

    for (Index = 0U; Index + 1U < Capacity && Source[Index] != 0; ++Index)
    {
        Destination[Index] = Source[Index];
    }
}

static BOOLEAN LosMonitorTextEqual(const char *Left, const char *Right)
{
    UINTN Index;

    if (Left == 0 || Right == 0)
    {
        return 0;
    }

    for (Index = 0U;; ++Index)
    {
        if (Left[Index] != Right[Index])
        {
            return 0;
        }
        if (Left[Index] == 0)
        {
            return 1;
        }
    }
}

static BOOLEAN LosMonitorReadToken(const char **Cursor, const char *LineEnd, char *Destination, UINTN Capacity)
{
    const char *Start;
    UINTN Length;
    UINTN Index;

    if (Cursor == 0 || *Cursor == 0 || Destination == 0 || Capacity < 2U)
    {
        return 0;
    }

    while (*Cursor < LineEnd && LosMonitorAsciiIsSpace(**Cursor) != 0U)
    {
        *Cursor += 1;
    }

    if (*Cursor >= LineEnd || **Cursor == '#' || **Cursor == ';')
    {
        return 0;
    }

    Start = *Cursor;
    while (*Cursor < LineEnd && LosMonitorAsciiIsSpace(**Cursor) == 0U && **Cursor != '#')
    {
        *Cursor += 1;
    }

    Length = (UINTN)(*Cursor - Start);
    if (Length == 0U || Length + 1U > Capacity)
    {
        return 0;
    }

    for (Index = 0U; Index < Length; ++Index)
    {
        Destination[Index] = Start[Index];
    }
    Destination[Length] = 0;
    return 1;
}

static BOOLEAN LosMonitorParseUnsignedToken(const char *Token, UINT32 *Value)
{
    UINTN Index;
    UINT32 Base;
    UINT32 Accumulator;

    if (Token == 0 || Value == 0 || Token[0] == 0)
    {
        return 0;
    }

    Base = 10U;
    Index = 0U;
    if (Token[0] == '0' && (Token[1] == 'x' || Token[1] == 'X'))
    {
        Base = 16U;
        Index = 2U;
    }

    Accumulator = 0U;
    for (; Token[Index] != 0; ++Index)
    {
        char Digit;

        Digit = Token[Index];
        if (Base == 16U)
        {
            if (LosMonitorAsciiIsHexDigit(Digit) == 0U)
            {
                return 0;
            }
            Accumulator = (Accumulator * 16U) + LosMonitorAsciiHexValue(Digit);
        }
        else
        {
            if (LosMonitorAsciiIsDigit(Digit) == 0U)
            {
                return 0;
            }
            Accumulator = (Accumulator * 10U) + (UINT32)(Digit - '0');
        }
    }

    *Value = Accumulator;
    return 1;
}

static UINT64 LosMonitorHashText(const char *Text)
{
    UINT64 Hash;
    UINTN Index;

    Hash = 1469598103934665603ULL;
    if (Text == 0)
    {
        return Hash;
    }

    for (Index = 0U; Text[Index] != 0; ++Index)
    {
        Hash ^= (UINT8)Text[Index];
        Hash *= 1099511628211ULL;
    }

    return Hash;
}

static UINT64 LosMonitorComposeCapabilityId(UINT32 CapabilityClass, UINT32 BlockIndex, UINT32 GrantIndex)
{
    return (((UINT64)LOS_CAPABILITIES_SERVICE_VERSION) << 56) |
           (((UINT64)CapabilityClass) << 48) |
           (((UINT64)(BlockIndex + 1U)) << 16) |
           (UINT64)(GrantIndex + 1U);
}

static UINT64 LosMonitorComposeGrantId(UINT32 BlockIndex, UINT32 GrantIndex)
{
    return (((UINT64)(BlockIndex + 1U)) << 32) | (UINT64)(GrantIndex + 1U);
}

static UINT64 LosMonitorComposeEventId(UINT32 EventIndex)
{
    return (((UINT64)LOS_CAPABILITIES_SERVICE_VERSION) << 56) | (UINT64)(EventIndex + 1U);
}

static UINT32 LosMonitorPrincipalTypeFromToken(const char *Token)
{
    if (LosMonitorTextEqual(Token, "user"))
    {
        return LOS_CAPABILITIES_PRINCIPAL_TYPE_USER;
    }
    if (LosMonitorTextEqual(Token, "profile"))
    {
        return LOS_CAPABILITIES_PRINCIPAL_TYPE_PROFILE;
    }
    if (LosMonitorTextEqual(Token, "service"))
    {
        return LOS_CAPABILITIES_PRINCIPAL_TYPE_SERVICE;
    }
    if (LosMonitorTextEqual(Token, "task"))
    {
        return LOS_CAPABILITIES_PRINCIPAL_TYPE_TASK;
    }
    if (LosMonitorTextEqual(Token, "session"))
    {
        return LOS_CAPABILITIES_PRINCIPAL_TYPE_SESSION;
    }
    return 0U;
}

static BOOLEAN LosMonitorAppendGrantImportEvent(LOS_BOOT_CONTEXT *BootContext,
                                                const LOS_CAPABILITY_GRANT_BLOCK *Block,
                                                const LOS_CAPABILITY_GRANT_ENTRY *Grant)
{
    LOS_CAPABILITY_GRANT_EVENT *Event;
    UINT32 EventIndex;

    if (BootContext == 0 || Block == 0 || Grant == 0)
    {
        return 0;
    }
    if (BootContext->Capabilities.EventCount >= LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_EVENTS)
    {
        return 0;
    }

    EventIndex = BootContext->Capabilities.EventCount;
    Event = &BootContext->Capabilities.Events[EventIndex];
    Event->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Event->EventType = LOS_CAPABILITIES_GRANT_EVENT_IMPORTED;
    Event->ActorType = LOS_CAPABILITIES_AUTHORISER_BOOT_POLICY;
    Event->Reserved = 0U;
    Event->EventId = LosMonitorComposeEventId(EventIndex);
    Event->GrantId = Grant->GrantId;
    Event->EventAtUtc = 0ULL;
    Event->PrincipalId = Block->PrincipalId;
    LosMonitorCopyAscii(Event->PrincipalName, LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH, Block->PrincipalName);
    LosMonitorCopyAscii(Event->CapabilityNamespace, LOS_CAPABILITIES_SERVICE_NAMESPACE_LENGTH, Grant->Namespace);
    LosMonitorCopyAscii(Event->CapabilityName, LOS_CAPABILITIES_SERVICE_NAME_LENGTH, Grant->Name);
    LosMonitorCopyAscii(Event->ActorName, LOS_CAPABILITIES_AUTHORISER_NAME_LENGTH, "bootstrap_policy");
    BootContext->Capabilities.EventCount += 1U;
    return 1;
}
