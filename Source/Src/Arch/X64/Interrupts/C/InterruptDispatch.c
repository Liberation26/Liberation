/*
 * File Name: InterruptDispatch.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "Scheduler.h"
#include "InterruptsInternal.h"

#define LOS_X64_PIC1_COMMAND_PORT 0x20U
#define LOS_X64_PIC1_DATA_PORT 0x21U
#define LOS_X64_PIC2_COMMAND_PORT 0xA0U
#define LOS_X64_PIC2_DATA_PORT 0xA1U
#define LOS_X64_PIC_COMMAND_EOI 0x20U
#define LOS_X64_PIC_ICW1_INIT 0x10U
#define LOS_X64_PIC_ICW1_ICW4 0x01U
#define LOS_X64_PIC_ICW4_8086 0x01U
#define LOS_X64_PIT_COMMAND_PORT 0x43U
#define LOS_X64_PIT_CHANNEL0_PORT 0x40U
#define LOS_X64_PIT_FREQUENCY_HZ 1193182U
#define LOS_X64_TIMER_TARGET_HZ 100U

static volatile UINT64 LosX64TimerTickCount = 0ULL;
static volatile UINT64 LosX64TimerInterruptObserved = 0ULL;

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

static inline void IoWait(void)
{
    Out8(0x80U, 0U);
}

static void RemapPic(void)
{
    UINT8 MasterMask;
    UINT8 SlaveMask;

    MasterMask = In8(LOS_X64_PIC1_DATA_PORT);
    SlaveMask = In8(LOS_X64_PIC2_DATA_PORT);

    Out8(LOS_X64_PIC1_COMMAND_PORT, LOS_X64_PIC_ICW1_INIT | LOS_X64_PIC_ICW1_ICW4);
    IoWait();
    Out8(LOS_X64_PIC2_COMMAND_PORT, LOS_X64_PIC_ICW1_INIT | LOS_X64_PIC_ICW1_ICW4);
    IoWait();

    Out8(LOS_X64_PIC1_DATA_PORT, LOS_X64_PIC_MASTER_VECTOR_BASE);
    IoWait();
    Out8(LOS_X64_PIC2_DATA_PORT, LOS_X64_PIC_SLAVE_VECTOR_BASE);
    IoWait();

    Out8(LOS_X64_PIC1_DATA_PORT, 4U);
    IoWait();
    Out8(LOS_X64_PIC2_DATA_PORT, 2U);
    IoWait();

    Out8(LOS_X64_PIC1_DATA_PORT, LOS_X64_PIC_ICW4_8086);
    IoWait();
    Out8(LOS_X64_PIC2_DATA_PORT, LOS_X64_PIC_ICW4_8086);
    IoWait();

    Out8(LOS_X64_PIC1_DATA_PORT, MasterMask);
    Out8(LOS_X64_PIC2_DATA_PORT, SlaveMask);
}

static void ConfigurePit(void)
{
    UINT16 Divisor;

    Divisor = (UINT16)(LOS_X64_PIT_FREQUENCY_HZ / LOS_X64_TIMER_TARGET_HZ);
    Out8(LOS_X64_PIT_COMMAND_PORT, 0x36U);
    Out8(LOS_X64_PIT_CHANNEL0_PORT, (UINT8)(Divisor & 0x00FFU));
    Out8(LOS_X64_PIT_CHANNEL0_PORT, (UINT8)((Divisor >> 8) & 0x00FFU));
}

void LosX64AcknowledgePicInterrupt(UINT64 Vector)
{
    if (Vector >= LOS_X64_PIC_SLAVE_VECTOR_BASE && Vector < (LOS_X64_PIC_SLAVE_VECTOR_BASE + 8U))
    {
        Out8(LOS_X64_PIC2_COMMAND_PORT, LOS_X64_PIC_COMMAND_EOI);
    }

    if (Vector >= LOS_X64_PIC_MASTER_VECTOR_BASE && Vector < (LOS_X64_PIC_MASTER_VECTOR_BASE + 8U))
    {
        Out8(LOS_X64_PIC1_COMMAND_PORT, LOS_X64_PIC_COMMAND_EOI);
    }
}

void LosX64LoadIdt(void)
{
    LOS_KERNEL_ENTER();
    __asm__ __volatile__("lidt %0" : : "m"(LosX64IdtPointer));
}

void LosX64InstallInterrupts(void)
{
    LOS_KERNEL_ENTER();
    LosX64InitializeIdtState();
    LosX64PopulateExceptionEntries();
    LosX64PopulateRuntimeEntries();
    LosX64LoadIdt();
}

void LosX64InitializeTimer(void)
{
    UINT8 MasterMask;
    UINT8 SlaveMask;

    LOS_KERNEL_ENTER();
    LosX64TimerTickCount = 0ULL;
    LosX64TimerInterruptObserved = 0ULL;
    RemapPic();
    ConfigurePit();

    MasterMask = (UINT8)(In8(LOS_X64_PIC1_DATA_PORT) & (UINT8)~0x01U);
    SlaveMask = 0xFFU;
    Out8(LOS_X64_PIC1_DATA_PORT, MasterMask);
    Out8(LOS_X64_PIC2_DATA_PORT, SlaveMask);

    LosKernelSerialWriteText("[Kernel] PIC remapped. PIT channel 0 running at 100 Hz on vector ");
    LosKernelSerialWriteUnsigned(LOS_X64_PIC_TIMER_VECTOR);
    LosKernelSerialWriteText(".\n");
    LosKernelScreenUpdateTimer(0ULL, LOS_X64_TIMER_TARGET_HZ, 0U);
}

UINT64 LosX64GetTimerTickCount(void)
{
    return LosX64TimerTickCount;
}

BOOLEAN LosX64HasObservedTimerInterrupt(void)
{
    return LosX64TimerInterruptObserved != 0ULL ? 1U : 0U;
}

void LosX64HandleInterrupt(
    const LOS_X64_REGISTER_STATE *Registers,
    UINT64 Vector,
    UINT64 ErrorCode,
    const LOS_X64_INTERRUPT_FRAME *Frame)
{
    BOOLEAN FromUserMode;
    UINT64 UserStackPointer;

    (void)Registers;

    FromUserMode = (Frame != 0 && (Frame->Cs & 0x3ULL) == 0x3ULL) ? 1U : 0U;
    UserStackPointer = (FromUserMode != 0U && Frame != 0) ? Frame->Rsp : 0ULL;
    if (FromUserMode != 0U &&
        LosKernelSchedulerHandleUserModeInterrupt(Vector, ErrorCode, Frame->Rip, UserStackPointer) != 0U)
    {
        if (Vector >= LOS_X64_PIC_MASTER_VECTOR_BASE && Vector < (LOS_X64_PIC_SLAVE_VECTOR_BASE + 8U))
        {
            LosX64AcknowledgePicInterrupt(Vector);
        }
        LosKernelSchedulerPreemptIfNeededFromInterrupt();
        return;
    }

    if (Vector == LOS_X64_PIC_TIMER_VECTOR)
    {
        LosX64TimerTickCount += 1ULL;
        LosX64TimerInterruptObserved = 1ULL;
        LosKernelSchedulerOnTimerTick();
        LosX64AcknowledgePicInterrupt(Vector);
        LosKernelScreenUpdateSpinner(LosX64TimerTickCount >> 3U);
        if ((LosX64TimerTickCount % 10ULL) == 0ULL)
        {
            LosKernelScreenUpdateTimer(LosX64TimerTickCount, LOS_X64_TIMER_TARGET_HZ, 1U);
        }
        if ((LosX64TimerTickCount % (UINT64)LOS_X64_TIMER_TARGET_HZ) == 0ULL)
        {
            LosKernelSerialWriteText("[Kernel] Timer heartbeat ticks=");
            LosKernelSerialWriteUnsigned(LosX64TimerTickCount);
            LosKernelSerialWriteText(" hz=");
            LosKernelSerialWriteUnsigned(LOS_X64_TIMER_TARGET_HZ);
            LosKernelSerialWriteText(" vector=");
            LosKernelSerialWriteUnsigned(LOS_X64_PIC_TIMER_VECTOR);
            LosKernelSerialWriteText("\n");
        }
        LosKernelSchedulerPreemptIfNeededFromInterrupt();
        return;
    }

    if (Vector >= LOS_X64_EXCEPTION_VECTOR_COUNT)
    {
        LosKernelSerialWriteText("[Kernel] Unhandled runtime interrupt vector ");
        LosKernelSerialWriteUnsigned(Vector);
        LosKernelSerialWriteText("\n");
        LosX64AcknowledgePicInterrupt(Vector);
        return;
    }

    LosKernelSerialWriteText("[Kernel] Exception vector ");
    LosKernelSerialWriteUnsigned(Vector);
    LosKernelSerialWriteText(": ");
    LosKernelSerialWriteText(LosX64GetExceptionName(Vector));
    LosKernelSerialWriteText("\n");
    LosKernelSerialWriteText("[Kernel] Error code: ");
    LosKernelSerialWriteHex64(ErrorCode);
    LosKernelSerialWriteText("\n");
    LosX64DescribeFault(Vector, ErrorCode);
    LosX64WriteRegisterDump(Registers, Frame);
    LosKernelHaltForever();
}
