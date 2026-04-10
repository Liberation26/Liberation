/*
 * File Name: Diagnostics.h
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-10T18:55:00Z
 * Last Update Timestamp: 2026-04-10T18:55:00Z
 * Operating System Name: Liberation OS
 * Purpose: Centralises function-entry tracing and crash diagnostics for the x64 kernel.
 */

#ifndef LOS_X64_DIAGNOSTICS_H
#define LOS_X64_DIAGNOSTICS_H

#include "Interrupts.h"

#define LOS_DIAGNOSTICS_TRACE_ENTRY_COUNT 2048U
#define LOS_DIAGNOSTICS_CRASH_TRACE_LINES 128U

typedef struct
{
    UINT64 Sequence;
    UINT64 Tick;
    UINT64 FunctionAddress;
    UINT64 CallerAddress;
} LOS_DIAGNOSTICS_TRACE_ENTRY;

void LosDiagnosticsInitialize(void);
void LosDiagnosticsWriteInterruptCrashReport(
    const LOS_X64_REGISTER_STATE *Registers,
    UINT64 Vector,
    UINT64 ErrorCode,
    const LOS_X64_INTERRUPT_FRAME *Frame);

#endif
