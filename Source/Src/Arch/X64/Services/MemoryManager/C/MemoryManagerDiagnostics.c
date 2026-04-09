/*
 * File Name: MemoryManagerDiagnostics.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-07T12:35:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#include "MemoryManagerMainInternal.h"

void LosMemoryManagerRecordAttachDiagnostic(LOS_MEMORY_MANAGER_TASK_OBJECT *TaskObject, UINT64 Stage, UINT64 Detail)
{
    if (TaskObject == 0)
    {
        return;
    }

    TaskObject->LastRequestId = Stage;
    TaskObject->Heartbeat = Detail;
}

static inline void ServiceOut8(UINT16 Port, UINT8 Value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static inline UINT8 ServiceIn8(UINT16 Port)
{
    UINT8 Value;

    __asm__ __volatile__("inb %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

void LosMemoryManagerServiceSerialInit(void)
{
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 1U, 0x00U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 3U, 0x80U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 0U, 0x03U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 1U, 0x00U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 3U, 0x03U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 2U, 0xC7U);
    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 4U, 0x0BU);
}

void LosMemoryManagerServiceSerialWriteChar(char Character)
{
    while ((ServiceIn8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 5U) & 0x20U) == 0U)
    {
    }

    ServiceOut8(LOS_MEMORY_MANAGER_SERVICE_SERIAL_COM1_BASE + 0U, (UINT8)Character);
}

void LosMemoryManagerServiceSerialWriteText(const char *Text)
{
    UINTN Index;

    if (Text == 0)
    {
        return;
    }

    for (Index = 0U; Text[Index] != '\0'; ++Index)
    {
        if (Text[Index] == '\n')
        {
            LosMemoryManagerServiceSerialWriteChar('\r');
        }
        LosMemoryManagerServiceSerialWriteChar(Text[Index]);
    }
}

void LosMemoryManagerServiceSerialWriteHex64(UINT64 Value)
{
    UINTN Shift;

    LosMemoryManagerServiceSerialWriteText("0x");
    for (Shift = 16U; Shift > 0U; --Shift)
    {
        UINT8 Nibble;

        Nibble = (UINT8)((Value >> ((Shift - 1U) * 4U)) & 0xFULL);
        LosMemoryManagerServiceSerialWriteChar((char)(Nibble < 10U ? ('0' + Nibble) : ('A' + (Nibble - 10U))));
    }
}

void LosMemoryManagerServiceSerialWriteLine(const char *Text)
{
    LosMemoryManagerServiceSerialWriteText(Text);
    LosMemoryManagerServiceSerialWriteText("\n");
}

void LosMemoryManagerServiceSerialWriteUnsigned(UINT64 Value)
{
    char Buffer[32];
    UINTN Index;

    if (Value == 0ULL)
    {
        LosMemoryManagerServiceSerialWriteChar('0');
        return;
    }

    Index = 0U;
    while (Value != 0ULL && Index < (sizeof(Buffer) / sizeof(Buffer[0])))
    {
        Buffer[Index] = (char)('0' + (Value % 10ULL));
        Value /= 10ULL;
        ++Index;
    }

    while (Index > 0U)
    {
        --Index;
        LosMemoryManagerServiceSerialWriteChar(Buffer[Index]);
    }
}

void LosMemoryManagerServiceSerialWriteNamedHex(const char *Name, UINT64 Value)
{
    LosMemoryManagerServiceSerialWriteText("[MemManager] ");
    LosMemoryManagerServiceSerialWriteText(Name);
    LosMemoryManagerServiceSerialWriteText(": ");
    LosMemoryManagerServiceSerialWriteHex64(Value);
    LosMemoryManagerServiceSerialWriteText("\n");
}

void LosMemoryManagerServiceSerialWriteNamedUnsigned(const char *Name, UINT64 Value)
{
    LosMemoryManagerServiceSerialWriteText("[MemManager] ");
    LosMemoryManagerServiceSerialWriteText(Name);
    LosMemoryManagerServiceSerialWriteText(": ");
    LosMemoryManagerServiceSerialWriteUnsigned(Value);
    LosMemoryManagerServiceSerialWriteText("\n");
}

void LosMemoryManagerHardFail(const char *RuleName, UINT64 Value0, UINT64 Value1, UINT64 Value2)
{
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;

    LosMemoryManagerServiceSerialWriteText("[MemManager][HARD-FAIL] ");
    LosMemoryManagerServiceSerialWriteText(RuleName != 0 ? RuleName : "unknown");
    LosMemoryManagerServiceSerialWriteText(" v0=");
    LosMemoryManagerServiceSerialWriteHex64(Value0);
    LosMemoryManagerServiceSerialWriteText(" v1=");
    LosMemoryManagerServiceSerialWriteHex64(Value1);
    LosMemoryManagerServiceSerialWriteText(" v2=");
    LosMemoryManagerServiceSerialWriteHex64(Value2);
    LosMemoryManagerServiceSerialWriteText("\n");

    State = LosMemoryManagerServiceState();
    if (State != 0 && State->TaskObject != 0)
    {
        State->TaskObject->LastRequestId = 0xFFFFFFFFFFFFFF00ULL;
        State->TaskObject->Heartbeat = Value0;
    }

    for (;; )
    {
        __asm__ __volatile__("cli; hlt" : : : "memory");
    }
}

const char *LosMemoryManagerBootstrapAttachResultName(UINT32 BootstrapResult)
{
    switch (BootstrapResult)
    {
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_READY:
            return "ready";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ALREADY_ATTACHED:
            return "already-attached";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_INVALID_REQUEST:
            return "invalid-request";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_VERSION_MISMATCH:
            return "version-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_LAUNCH_BLOCK_MISMATCH:
            return "launch-block-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ENDPOINT_MISMATCH:
            return "endpoint-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_MAILBOX_MISMATCH:
            return "mailbox-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_ROOT_MISMATCH:
            return "service-root-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_IMAGE_MISMATCH:
            return "service-image-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_SERVICE_STATE_INVALID:
            return "service-state-invalid";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_OPERATION_SET_INVALID:
            return "operation-set-invalid";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_ENTRY_MISMATCH:
            return "entry-mismatch";
        case LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_STACK_TOP_MISMATCH:
            return "stack-top-mismatch";
        default:
            return "unknown";
    }
}

void LosMemoryManagerZeroMemory(void *Buffer, UINTN ByteCount)
{
    UINT8 *Bytes;
    UINTN Index;

    if (Buffer == 0)
    {
        return;
    }

    Bytes = (UINT8 *)Buffer;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        Bytes[Index] = 0U;
    }
}

void LosMemoryManagerCopyBytes(void *Destination, const void *Source, UINTN ByteCount)
{
    UINT8 *DestinationBytes;
    const UINT8 *SourceBytes;
    UINTN Index;

    if (Destination == 0 || Source == 0)
    {
        return;
    }

    DestinationBytes = (UINT8 *)Destination;
    SourceBytes = (const UINT8 *)Source;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        DestinationBytes[Index] = SourceBytes[Index];
    }
}

UINT64 LosMemoryManagerResolveDirectMapOffset(const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock, UINT64 LaunchBlockAddress)
{
    if (LaunchBlock == 0 || LaunchBlock->LaunchBlockPhysicalAddress == 0ULL || LaunchBlockAddress < LaunchBlock->LaunchBlockPhysicalAddress)
    {
        return 0ULL;
    }

    return LaunchBlockAddress - LaunchBlock->LaunchBlockPhysicalAddress;
}

void *LosMemoryManagerTranslateBootstrapAddress(UINT64 DirectMapOffset, UINT64 PhysicalAddress)
{
    if (PhysicalAddress == 0ULL)
    {
        return 0;
    }

    return (void *)(UINTN)(DirectMapOffset + PhysicalAddress);
}

void LosMemoryManagerRecordEntryBreadcrumbFromLaunchBlock(UINT64 LaunchBlockAddress, UINT64 Stage, UINT64 Value)
{
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    UINT64 DirectMapOffset;
    LOS_MEMORY_MANAGER_TASK_OBJECT *TaskObject;

    if (LaunchBlockAddress == 0ULL)
    {
        return;
    }

    LaunchBlock = (const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *)(UINTN)LaunchBlockAddress;
    if (LaunchBlock == 0)
    {
        return;
    }

    DirectMapOffset = LosMemoryManagerResolveDirectMapOffset(LaunchBlock, LaunchBlockAddress);
    if (DirectMapOffset == 0ULL)
    {
        return;
    }

    TaskObject = (LOS_MEMORY_MANAGER_TASK_OBJECT *)LosMemoryManagerTranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceTaskObjectPhysicalAddress);
    if (TaskObject == 0)
    {
        return;
    }

    TaskObject->LastRequestId = Stage;
    TaskObject->Heartbeat = Value;
}
