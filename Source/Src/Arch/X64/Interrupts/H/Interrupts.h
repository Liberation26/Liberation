/*
 * File Name: Interrupts.h
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#ifndef LOS_X64_INTERRUPTS_H
#define LOS_X64_INTERRUPTS_H

#include "KernelMain.h"

typedef struct
{
    UINT64 R15;
    UINT64 R14;
    UINT64 R13;
    UINT64 R12;
    UINT64 R11;
    UINT64 R10;
    UINT64 R9;
    UINT64 R8;
    UINT64 Rdi;
    UINT64 Rsi;
    UINT64 Rbp;
    UINT64 Rdx;
    UINT64 Rcx;
    UINT64 Rbx;
    UINT64 Rax;
} LOS_X64_REGISTER_STATE;

typedef struct __attribute__((packed))
{
    UINT64 Rip;
    UINT64 Cs;
    UINT64 Rflags;
    UINT64 Rsp;
    UINT64 Ss;
} LOS_X64_INTERRUPT_FRAME;

void LosX64InstallInterrupts(void);
void LosX64InitializeTimer(void);
UINT64 LosX64GetTimerTickCount(void);
BOOLEAN LosX64HasObservedTimerInterrupt(void);
void LosX64AcknowledgePicInterrupt(UINT64 Vector);
void LosX64HandleInterrupt(
    const LOS_X64_REGISTER_STATE *Registers,
    UINT64 Vector,
    UINT64 ErrorCode,
    const LOS_X64_INTERRUPT_FRAME *Frame);
const void *LosX64GetIdtBase(void);
UINT64 LosX64GetIdtSize(void);

#endif
