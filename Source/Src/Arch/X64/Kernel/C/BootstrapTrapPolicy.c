/*
 * File Name: BootstrapTrapPolicy.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

#include "BootstrapTrapInternal.h"

extern void LosBootstrapInterruptStub0(void);
extern void LosBootstrapInterruptStub1(void);
extern void LosBootstrapInterruptStub2(void);
extern void LosBootstrapInterruptStub3(void);
extern void LosBootstrapInterruptStub4(void);
extern void LosBootstrapInterruptStub5(void);
extern void LosBootstrapInterruptStub6(void);
extern void LosBootstrapInterruptStub7(void);
extern void LosBootstrapInterruptStub8(void);
extern void LosBootstrapInterruptStub9(void);
extern void LosBootstrapInterruptStub10(void);
extern void LosBootstrapInterruptStub11(void);
extern void LosBootstrapInterruptStub12(void);
extern void LosBootstrapInterruptStub13(void);
extern void LosBootstrapInterruptStub14(void);
extern void LosBootstrapInterruptStub15(void);
extern void LosBootstrapInterruptStub16(void);
extern void LosBootstrapInterruptStub17(void);
extern void LosBootstrapInterruptStub18(void);
extern void LosBootstrapInterruptStub19(void);
extern void LosBootstrapInterruptStub20(void);
extern void LosBootstrapInterruptStub21(void);
extern void LosBootstrapInterruptStub22(void);
extern void LosBootstrapInterruptStub23(void);
extern void LosBootstrapInterruptStub24(void);
extern void LosBootstrapInterruptStub25(void);
extern void LosBootstrapInterruptStub26(void);
extern void LosBootstrapInterruptStub27(void);
extern void LosBootstrapInterruptStub28(void);
extern void LosBootstrapInterruptStub29(void);
extern void LosBootstrapInterruptStub30(void);
extern void LosBootstrapInterruptStub31(void);

static void *const LosBootstrapTrapStubs[LOS_X64_BOOTSTRAP_TRAP_EXCEPTION_VECTOR_COUNT] LOS_X64_BOOTSTRAP_RODATA =
{
    (void *)LosBootstrapInterruptStub0,  (void *)LosBootstrapInterruptStub1,
    (void *)LosBootstrapInterruptStub2,  (void *)LosBootstrapInterruptStub3,
    (void *)LosBootstrapInterruptStub4,  (void *)LosBootstrapInterruptStub5,
    (void *)LosBootstrapInterruptStub6,  (void *)LosBootstrapInterruptStub7,
    (void *)LosBootstrapInterruptStub8,  (void *)LosBootstrapInterruptStub9,
    (void *)LosBootstrapInterruptStub10, (void *)LosBootstrapInterruptStub11,
    (void *)LosBootstrapInterruptStub12, (void *)LosBootstrapInterruptStub13,
    (void *)LosBootstrapInterruptStub14, (void *)LosBootstrapInterruptStub15,
    (void *)LosBootstrapInterruptStub16, (void *)LosBootstrapInterruptStub17,
    (void *)LosBootstrapInterruptStub18, (void *)LosBootstrapInterruptStub19,
    (void *)LosBootstrapInterruptStub20, (void *)LosBootstrapInterruptStub21,
    (void *)LosBootstrapInterruptStub22, (void *)LosBootstrapInterruptStub23,
    (void *)LosBootstrapInterruptStub24, (void *)LosBootstrapInterruptStub25,
    (void *)LosBootstrapInterruptStub26, (void *)LosBootstrapInterruptStub27,
    (void *)LosBootstrapInterruptStub28, (void *)LosBootstrapInterruptStub29,
    (void *)LosBootstrapInterruptStub30, (void *)LosBootstrapInterruptStub31
};

static const char LosBootstrapTrapReporterInstalledMessage[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Minimal trap reporter installed.\n";

LOS_X64_BOOTSTRAP_SECTION
static void SetBootstrapIdtEntry(UINTN Index, void *Handler)
{
    UINT64 Address;

    Address = (UINT64)(UINTN)Handler;
    LosX64BootstrapIdt[Index].OffsetLow = (UINT16)(Address & 0xFFFFULL);
    LosX64BootstrapIdt[Index].Selector = LosX64BootstrapTrapCodeSelector;
    LosX64BootstrapIdt[Index].Ist = 0U;
    LosX64BootstrapIdt[Index].TypeAttributes = 0x8EU;
    LosX64BootstrapIdt[Index].OffsetMiddle = (UINT16)((Address >> 16) & 0xFFFFULL);
    LosX64BootstrapIdt[Index].OffsetHigh = (UINT32)((Address >> 32) & 0xFFFFFFFFULL);
    LosX64BootstrapIdt[Index].Reserved = 0U;
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64PopulateBootstrapTrapEntries(void)
{
    UINTN Index;

    for (Index = 0U; Index < LOS_X64_BOOTSTRAP_TRAP_EXCEPTION_VECTOR_COUNT; ++Index)
    {
        SetBootstrapIdtEntry(Index, LosBootstrapTrapStubs[Index]);
    }
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64LoadBootstrapTrapIdt(void)
{
    __asm__ __volatile__("lidt %0" : : "m"(LosX64BootstrapIdtPointer));
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64InstallBootstrapTrapReporter(void)
{
    LosX64BootstrapSerialInit();
    LosX64InitializeBootstrapTrapState();
    LosX64PopulateBootstrapTrapEntries();
    LosX64LoadBootstrapTrapIdt();
    LosX64BootstrapSerialWriteStatusTagOk();
    LosX64BootstrapSerialWriteText(LosBootstrapTrapReporterInstalledMessage);
}
