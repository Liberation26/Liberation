/*
 * File Name: InterruptPolicy.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "InterruptsInternal.h"

extern void LosInterruptStub0(void);
extern void LosInterruptStub1(void);
extern void LosInterruptStub2(void);
extern void LosInterruptStub3(void);
extern void LosInterruptStub4(void);
extern void LosInterruptStub5(void);
extern void LosInterruptStub6(void);
extern void LosInterruptStub7(void);
extern void LosInterruptStub8(void);
extern void LosInterruptStub9(void);
extern void LosInterruptStub10(void);
extern void LosInterruptStub11(void);
extern void LosInterruptStub12(void);
extern void LosInterruptStub13(void);
extern void LosInterruptStub14(void);
extern void LosInterruptStub15(void);
extern void LosInterruptStub16(void);
extern void LosInterruptStub17(void);
extern void LosInterruptStub18(void);
extern void LosInterruptStub19(void);
extern void LosInterruptStub20(void);
extern void LosInterruptStub21(void);
extern void LosInterruptStub22(void);
extern void LosInterruptStub23(void);
extern void LosInterruptStub24(void);
extern void LosInterruptStub25(void);
extern void LosInterruptStub26(void);
extern void LosInterruptStub27(void);
extern void LosInterruptStub28(void);
extern void LosInterruptStub29(void);
extern void LosInterruptStub30(void);
extern void LosInterruptStub31(void);
extern void LosInterruptStub32(void);
extern void LosInterruptStub128(void);

static void SetIdtEntryWithAttributes(UINTN Index, void *Handler, UINT8 TypeAttributes)
{
    UINT64 Address;

    Address = (UINT64)(UINTN)Handler;
    LosX64Idt[Index].OffsetLow = (UINT16)(Address & 0xFFFFULL);
    LosX64Idt[Index].Selector = 0x08U;
    LosX64Idt[Index].Ist = 0U;
    LosX64Idt[Index].TypeAttributes = TypeAttributes;
    LosX64Idt[Index].OffsetMiddle = (UINT16)((Address >> 16) & 0xFFFFULL);
    LosX64Idt[Index].OffsetHigh = (UINT32)((Address >> 32) & 0xFFFFFFFFULL);
    LosX64Idt[Index].Reserved = 0U;
}

static void SetIdtEntry(UINTN Index, void *Handler)
{
    SetIdtEntryWithAttributes(Index, Handler, 0x8EU);
}

void LosX64PopulateExceptionEntries(void)
{
    static void *const Stubs[LOS_X64_EXCEPTION_VECTOR_COUNT] =
    {
        (void *)LosInterruptStub0,  (void *)LosInterruptStub1,
        (void *)LosInterruptStub2,  (void *)LosInterruptStub3,
        (void *)LosInterruptStub4,  (void *)LosInterruptStub5,
        (void *)LosInterruptStub6,  (void *)LosInterruptStub7,
        (void *)LosInterruptStub8,  (void *)LosInterruptStub9,
        (void *)LosInterruptStub10, (void *)LosInterruptStub11,
        (void *)LosInterruptStub12, (void *)LosInterruptStub13,
        (void *)LosInterruptStub14, (void *)LosInterruptStub15,
        (void *)LosInterruptStub16, (void *)LosInterruptStub17,
        (void *)LosInterruptStub18, (void *)LosInterruptStub19,
        (void *)LosInterruptStub20, (void *)LosInterruptStub21,
        (void *)LosInterruptStub22, (void *)LosInterruptStub23,
        (void *)LosInterruptStub24, (void *)LosInterruptStub25,
        (void *)LosInterruptStub26, (void *)LosInterruptStub27,
        (void *)LosInterruptStub28, (void *)LosInterruptStub29,
        (void *)LosInterruptStub30, (void *)LosInterruptStub31
    };
    UINTN Index;

    for (Index = 0U; Index < LOS_X64_EXCEPTION_VECTOR_COUNT; ++Index)
    {
        SetIdtEntry(Index, Stubs[Index]);
    }
}

void LosX64PopulateRuntimeEntries(void)
{
    SetIdtEntry(LOS_X64_PIC_TIMER_VECTOR, (void *)LosInterruptStub32);
    SetIdtEntryWithAttributes(LOS_X64_USER_TRANSITION_VECTOR, (void *)LosInterruptStub128, 0xEEU);
}
