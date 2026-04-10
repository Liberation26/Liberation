/*
 * File Name: KernelMainSection01.c
 * File Version: 0.0.3
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-10T20:25:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from KernelMain.c.
 */

typedef struct __attribute__((packed))
{
    UINT16 Limit;
    UINT64 Base;
} LOS_DESCRIPTOR_POINTER;

typedef struct __attribute__((packed))
{
    UINT32 Reserved0;
    UINT64 Rsp0;
    UINT64 Rsp1;
    UINT64 Rsp2;
    UINT64 Reserved1;
    UINT64 Ist1;
    UINT64 Ist2;
    UINT64 Ist3;
    UINT64 Ist4;
    UINT64 Ist5;
    UINT64 Ist6;
    UINT64 Ist7;
    UINT64 Reserved2;
    UINT16 Reserved3;
    UINT16 IoMapBase;
} LOS_X64_TASK_STATE_SEGMENT;

UINT64 LosGdt[LOS_X64_GDT_ENTRY_COUNT];
static LOS_X64_TASK_STATE_SEGMENT LosKernelTss __attribute__((aligned(16)));
static LOS_DESCRIPTOR_POINTER LosGdtPointer;
static const LOS_BOOT_CONTEXT *LosKernelBootContext;

#define LOS_KERNEL_NOINSTRUMENT __attribute__((no_instrument_function))

volatile UINT64 LosKernelRuntimeTracingEnabled = 0ULL;

static inline void LOS_KERNEL_NOINSTRUMENT Out8(UINT16 Port, UINT8 Value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static inline UINT8 LOS_KERNEL_NOINSTRUMENT In8(UINT16 Port)
{
    UINT8 Value;
    __asm__ __volatile__("inb %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

void LOS_KERNEL_NOINSTRUMENT LosKernelSerialInit(void)
{
    Out8(LOS_SERIAL_COM1_BASE + 1U, 0x00U);
    Out8(LOS_SERIAL_COM1_BASE + 3U, 0x80U);
    Out8(LOS_SERIAL_COM1_BASE + 0U, 0x03U);
    Out8(LOS_SERIAL_COM1_BASE + 1U, 0x00U);
    Out8(LOS_SERIAL_COM1_BASE + 3U, 0x03U);
    Out8(LOS_SERIAL_COM1_BASE + 2U, 0xC7U);
    Out8(LOS_SERIAL_COM1_BASE + 4U, 0x0BU);
}

static char LosKernelTraceLineBuffer[1024];
static UINTN LosKernelTraceLineLength;

static void LOS_KERNEL_NOINSTRUMENT SerialWriteRawChar(char Character)
{
    while ((In8(LOS_SERIAL_COM1_BASE + 5U) & 0x20U) == 0U)
    {
    }

    Out8(LOS_SERIAL_COM1_BASE + 0U, (UINT8)Character);
}

static BOOLEAN LOS_KERNEL_NOINSTRUMENT LosKernelShouldEmitSerialLine(const char *Buffer, UINTN Length)
{
    UINTN Index;

    if (Buffer == 0 || Length == 0U)
    {
        return 0U;
    }

    for (Index = 0U; Index + 3U < Length; ++Index)
    {
        if (Buffer[Index] == '[' && Buffer[Index + 1U] == 'O' && Buffer[Index + 2U] == 'K' && Buffer[Index + 3U] == ']')
        {
            return 1U;
        }
    }

    for (Index = 0U; Index + 5U < Length; ++Index)
    {
        if (Buffer[Index] == '[' && Buffer[Index + 1U] == 'F' && Buffer[Index + 2U] == 'A' && Buffer[Index + 3U] == 'I' && Buffer[Index + 4U] == 'L' && Buffer[Index + 5U] == ']')
        {
            return 1U;
        }
    }

    for (Index = 0U; Index + 10U < Length; ++Index)
    {
        if (Buffer[Index] == '[' && Buffer[Index + 1U] == 'H' && Buffer[Index + 2U] == 'A' && Buffer[Index + 3U] == 'R' && Buffer[Index + 4U] == 'D' && Buffer[Index + 5U] == '-' && Buffer[Index + 6U] == 'F' && Buffer[Index + 7U] == 'A' && Buffer[Index + 8U] == 'I' && Buffer[Index + 9U] == 'L' && Buffer[Index + 10U] == ']')
        {
            return 1U;
        }
    }

    return 0U;
}

static void LOS_KERNEL_NOINSTRUMENT LosKernelFlushSerialLineBuffer(BOOLEAN AppendNewline)
{
    UINTN Index;

    if (LosKernelShouldEmitSerialLine(LosKernelTraceLineBuffer, LosKernelTraceLineLength) == 0U)
    {
        LosKernelTraceLineLength = 0U;
        return;
    }

    for (Index = 0U; Index < LosKernelTraceLineLength; ++Index)
    {
        SerialWriteRawChar(LosKernelTraceLineBuffer[Index]);
    }

    if (AppendNewline != 0U)
    {
        SerialWriteRawChar('\r');
        SerialWriteRawChar('\n');
    }

    LosKernelTraceLineLength = 0U;
}

static void LOS_KERNEL_NOINSTRUMENT SerialWriteChar(char Character)
{
    if (Character == '\r')
    {
        return;
    }

    if (Character == '\n')
    {
        LosKernelFlushSerialLineBuffer(1U);
        return;
    }

    if (LosKernelTraceLineLength >= (sizeof(LosKernelTraceLineBuffer) - 1U))
    {
        LosKernelTraceLineLength = 0U;
    }

    LosKernelTraceLineBuffer[LosKernelTraceLineLength] = Character;
    ++LosKernelTraceLineLength;
    LosKernelTraceLineBuffer[LosKernelTraceLineLength] = '\0';
}

void LOS_KERNEL_NOINSTRUMENT LosKernelSerialWriteText(const char *Text)
{
    UINTN Index;

    if (Text == 0)
    {
        return;
    }

    for (Index = 0U; Text[Index] != '\0'; ++Index)
    {
        SerialWriteChar(Text[Index]);
    }
}

void LOS_KERNEL_NOINSTRUMENT LosKernelSerialWriteUnsigned(UINT64 Value)
{
    char Buffer[32];
    UINTN Index;

    if (Value == 0ULL)
    {
        SerialWriteChar('0');
        return;
    }

    Index = 0U;
    while (Value > 0ULL && Index < sizeof(Buffer))
    {
        Buffer[Index] = (char)('0' + (Value % 10ULL));
        Value /= 10ULL;
        ++Index;
    }

    while (Index > 0U)
    {
        --Index;
        SerialWriteChar(Buffer[Index]);
    }
}

void LOS_KERNEL_NOINSTRUMENT LosKernelSerialWriteHex64(UINT64 Value)
{
    UINTN Shift;

    LosKernelSerialWriteText("0x");
    for (Shift = 16U; Shift > 0U; --Shift)
    {
        UINT8 Nibble;
        Nibble = (UINT8)((Value >> ((Shift - 1U) * 4U)) & 0xFULL);
        SerialWriteChar((char)(Nibble < 10U ? ('0' + Nibble) : ('A' + (Nibble - 10U))));
    }
}

void LOS_KERNEL_NOINSTRUMENT LosKernelSerialWriteUtf16(const CHAR16 *Text)
{
    UINTN Index;

    if (Text == 0)
    {
        return;
    }

    for (Index = 0U; Text[Index] != 0; ++Index)
    {
        CHAR16 Character;
        Character = Text[Index];
        if (Character == L'\r')
        {
            continue;
        }
        SerialWriteChar((char)(Character <= 0x7FU ? Character : '?'));
    }
}

static void SerialWriteAnsiColor(UINT8 Code)
{
    if (Code == 0U)
    {
        LosKernelSerialWriteText("\x1B[0m");
        return;
    }

    LosKernelSerialWriteText("\x1B[");
    if (Code >= 100U)
    {
        SerialWriteChar((char)('0' + (Code / 100U)));
        Code = (UINT8)(Code % 100U);
    }
    if (Code >= 10U)
    {
        SerialWriteChar((char)('0' + (Code / 10U)));
    }
    SerialWriteChar((char)('0' + (Code % 10U)));
    SerialWriteChar('m');
}

static void SerialWriteStatusTagOk(void)
{
    SerialWriteAnsiColor(32U);
    LosKernelSerialWriteText("[OK]");
    SerialWriteAnsiColor(0U);
    SerialWriteChar(' ');
}

static void SerialWriteStatusTagFail(void)
{
    SerialWriteAnsiColor(31U);
    LosKernelSerialWriteText("[FAIL]");
    SerialWriteAnsiColor(0U);
    SerialWriteChar(' ');
}

void LosKernelTrace(const char *Text)
{
    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Text);
    LosKernelSerialWriteText("\n");
}

void LosKernelTraceOk(const char *Text)
{
    LosKernelStatusScreenWriteOk(Text);
    SerialWriteStatusTagOk();
    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Text);
    LosKernelSerialWriteText("\n");
}

void LosKernelTraceFail(const char *Text)
{
    LosKernelStatusScreenWriteFail(Text);
    SerialWriteStatusTagFail();
    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Text);
    LosKernelSerialWriteText("\n");
}

void LosKernelTraceHex64(const char *Prefix, UINT64 Value)
{
    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Prefix);
    LosKernelSerialWriteHex64(Value);
    LosKernelSerialWriteText("\n");
}

void LosKernelTraceUnsigned(const char *Prefix, UINT64 Value)
{
    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Prefix);
    LosKernelSerialWriteUnsigned(Value);
    LosKernelSerialWriteText("\n");
}

void LosKernelAnnounceFunction(const char *FunctionName)
{
    LosKernelSerialWriteText("[Kernel] Enter ");
    LosKernelSerialWriteText(FunctionName);
    LosKernelSerialWriteText("\n");
}

void LOS_KERNEL_NOINSTRUMENT LosKernelHaltForever(void)
{
    for (;;)
    {
        __asm__ __volatile__("cli; hlt");
    }
}

void LOS_KERNEL_NOINSTRUMENT LosKernelEnableInterrupts(void)
{
    __asm__ __volatile__("sti" : : : "memory");
}
