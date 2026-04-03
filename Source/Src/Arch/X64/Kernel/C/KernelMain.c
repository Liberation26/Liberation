#include "KernelMain.h"
#include "Interrupts.h"
#include "VirtualMemory.h"

#define LOS_SERIAL_COM1_BASE 0x3F8U
#define LOS_GDT_CODE_FLAGS 0x9AU
#define LOS_GDT_DATA_FLAGS 0x92U
#define LOS_GDT_GRANULARITY 0xAFU

typedef struct __attribute__((packed))
{
    UINT16 Limit;
    UINT64 Base;
} LOS_DESCRIPTOR_POINTER;

UINT64 LosGdt[LOS_X64_GDT_ENTRY_COUNT];
static LOS_DESCRIPTOR_POINTER LosGdtPointer;

volatile UINT64 LosKernelRuntimeTracingEnabled __attribute__((section(".bootstrap.data"))) = 0ULL;

static inline void Out8(UINT16 Port, UINT8 Value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static inline UINT8 In8(UINT16 Port)
{
    UINT8 Value;
    __asm__ __volatile__("inb %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

void LosKernelSerialInit(void)
{
    Out8(LOS_SERIAL_COM1_BASE + 1U, 0x00U);
    Out8(LOS_SERIAL_COM1_BASE + 3U, 0x80U);
    Out8(LOS_SERIAL_COM1_BASE + 0U, 0x03U);
    Out8(LOS_SERIAL_COM1_BASE + 1U, 0x00U);
    Out8(LOS_SERIAL_COM1_BASE + 3U, 0x03U);
    Out8(LOS_SERIAL_COM1_BASE + 2U, 0xC7U);
    Out8(LOS_SERIAL_COM1_BASE + 4U, 0x0BU);
}

static void SerialWriteChar(char Character)
{
    while ((In8(LOS_SERIAL_COM1_BASE + 5U) & 0x20U) == 0U)
    {
    }

    Out8(LOS_SERIAL_COM1_BASE + 0U, (UINT8)Character);
}

void LosKernelSerialWriteText(const char *Text)
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
            SerialWriteChar('\r');
        }
        SerialWriteChar(Text[Index]);
    }
}

void LosKernelSerialWriteUnsigned(UINT64 Value)
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

void LosKernelSerialWriteHex64(UINT64 Value)
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

void LosKernelSerialWriteUtf16(const CHAR16 *Text)
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
        if (Character == L'\n')
        {
            SerialWriteChar('\r');
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

void LosKernelHaltForever(void)
{
    for (;;)
    {
        __asm__ __volatile__("cli; hlt");
    }
}

void LosKernelEnableInterrupts(void)
{
    __asm__ __volatile__("sti" : : : "memory");
}

void LosKernelIdleLoop(void)
{
    LosKernelTraceOk("Kernel idle loop entered. CPU will sleep until interrupts arrive.");
    for (;;)
    {
        __asm__ __volatile__("hlt" : : : "memory");
    }
}

static UINT64 BuildGdtEntry(UINT32 Base, UINT32 Limit, UINT8 Access, UINT8 Granularity)
{
    UINT64 Entry;

    Entry = 0ULL;
    Entry |= (UINT64)(Limit & 0xFFFFU);
    Entry |= ((UINT64)(Base & 0xFFFFU)) << 16;
    Entry |= ((UINT64)((Base >> 16) & 0xFFU)) << 32;
    Entry |= ((UINT64)Access) << 40;
    Entry |= ((UINT64)((Limit >> 16) & 0x0FU)) << 48;
    Entry |= ((UINT64)(Granularity & 0xF0U)) << 48;
    Entry |= ((UINT64)((Base >> 24) & 0xFFU)) << 56;
    return Entry;
}

static void InstallGdt(void)
{
    LOS_KERNEL_ENTER();
    LosGdt[0] = 0ULL;
    LosGdt[1] = BuildGdtEntry(0U, 0xFFFFFU, LOS_GDT_CODE_FLAGS, LOS_GDT_GRANULARITY);
    LosGdt[2] = BuildGdtEntry(0U, 0xFFFFFU, LOS_GDT_DATA_FLAGS, LOS_GDT_GRANULARITY);

    LosGdtPointer.Limit = (UINT16)(sizeof(LosGdt) - 1U);
    LosGdtPointer.Base = (UINT64)(UINTN)&LosGdt[0];

    __asm__ __volatile__("lgdt %0" : : "m"(LosGdtPointer));
    __asm__ __volatile__(
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        :
        :
        : "rax", "memory");
}

const void *LosKernelGetGdtBase(void)
{
    LOS_KERNEL_ENTER();
    return &LosGdt[0];
}

UINT64 LosKernelGetGdtSize(void)
{
    LOS_KERNEL_ENTER();
    return (UINT64)sizeof(LosGdt);
}

static UINT64 CountMemoryRegions(const LOS_BOOT_CONTEXT *BootContext)
{
    if (BootContext == 0 || BootContext->MemoryMapDescriptorSize == 0ULL)
    {
        return 0ULL;
    }

    return BootContext->MemoryMapSize / BootContext->MemoryMapDescriptorSize;
}

void LosKernelHigherHalfMain(const LOS_BOOT_CONTEXT *BootContext)
{
    LosKernelSerialInit();
    LosKernelInitializeScreen(BootContext);
    LosKernelRuntimeTracingEnabled = 1ULL;
    LOS_KERNEL_ENTER();
    LosKernelSerialWriteText("Liberation Kernel\n");
    LosKernelTraceOk("Higher-half kernel entry active.");

    InstallGdt();
    LosKernelTraceOk("Minimal GDT installed.");

    LosX64InstallInterrupts();
    LosKernelTraceOk("Vector-specific exception IDT installed.");

    LosX64InitializeTimer();
    LosKernelTraceOk("Programmable interval timer started at 100 Hz.");

    LosKernelTraceOk("ExitBootServices complete. Kernel owns firmware memory map.");
    LosKernelSerialWriteText("[Kernel] The EFI monitor was handoff-only and does not remain a live UEFI application.\n");
    LosKernelSerialWriteText("[Kernel] Boot source: ");
    LosKernelSerialWriteUtf16(BootContext->BootSourceText);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteUtf16(BootContext->KernelPartitionText);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Memory map bytes: ");
    LosKernelSerialWriteUnsigned(BootContext->MemoryMapSize);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Memory descriptor size: ");
    LosKernelSerialWriteUnsigned(BootContext->MemoryMapDescriptorSize);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Memory regions: ");
    LosKernelSerialWriteUnsigned(CountMemoryRegions(BootContext));
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Kernel image physical base: ");
    LosKernelSerialWriteHex64(BootContext->KernelImagePhysicalAddress);
    LosKernelSerialWriteText(" bytes=");
    LosKernelSerialWriteUnsigned(BootContext->KernelImageSize);
    LosKernelSerialWriteText("\n");
    LosKernelSerialWriteText("[Kernel] Boot context version: ");
    LosKernelSerialWriteUnsigned(BootContext->Version);
    LosKernelSerialWriteText(" load segments=");
    LosKernelSerialWriteUnsigned(BootContext->KernelLoadSegmentCount);
    LosKernelSerialWriteText("\n");

    if ((BootContext->Flags & LOS_BOOT_CONTEXT_FLAG_MONITOR_HANDOFF_ONLY) != 0ULL)
    {
        LosKernelTraceOk("Any continuing monitoring now belongs to kernel-owned code or services.");
    }

    LosX64DescribeVirtualMemoryLayout();
    LosX64DescribePhysicalMemoryState();
    LosX64DescribeMemoryManagerHandoff();
    LosKernelTraceOk("Kernel now owns deliberate paging structures.");
    LosKernelTraceOk("Dedicated kernel stack mapping is active.");
    LosKernelTraceOk("QueryMemoryRegions, ReserveFrames, and ClaimFrames are ready for the future userland memory manager.");
    LosKernelTraceOk("MapPages and UnmapPages are ready for explicit address-space work.");
    LosKernelTraceOk("Memory manager remains a userland service.");
    LosKernelTraceUnsigned("Timer tick count before enabling interrupts: ", LosX64GetTimerTickCount());
    LosKernelScreenUpdateTimer(LosX64GetTimerTickCount(), 100ULL, 0U);
    LosKernelEnableInterrupts();
    LosKernelTraceOk("Interrupts enabled. Waiting for timer IRQ heartbeat while remaining in the idle path.");
    LosKernelIdleLoop();
}
