/*
 * File Name: Diagnostics.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-10T18:55:00Z
 * Last Update Timestamp: 2026-04-10T20:10:00Z
 * Operating System Name: Liberation OS
 * Purpose: Centralises function-entry tracing and crash diagnostics for the x64 kernel.
 */

#include "Diagnostics.h"
#include "InterruptsInternal.h"

#define LOS_DIAGNOSTICS_NOINSTRUMENT __attribute__((no_instrument_function))
#define LOS_DIAGNOSTICS_BOOTSTRAP_TEXT __attribute__((section(".bootstrap.text")))
#define LOS_DIAGNOSTICS_BOOTSTRAP_DATA __attribute__((section(".bootstrap.data")))

static volatile UINT64 LosDiagnosticsTraceSequence LOS_DIAGNOSTICS_BOOTSTRAP_DATA = 0ULL;
static volatile UINT64 LosDiagnosticsTraceEntryCount LOS_DIAGNOSTICS_BOOTSTRAP_DATA = 0ULL;
static volatile UINT64 LosDiagnosticsTraceWriteIndex LOS_DIAGNOSTICS_BOOTSTRAP_DATA = 0ULL;
static volatile UINT64 LosDiagnosticsBusy LOS_DIAGNOSTICS_BOOTSTRAP_DATA = 0ULL;
static LOS_DIAGNOSTICS_TRACE_ENTRY LosDiagnosticsTraceEntries[LOS_DIAGNOSTICS_TRACE_ENTRY_COUNT] LOS_DIAGNOSTICS_BOOTSTRAP_DATA;

static void LOS_DIAGNOSTICS_NOINSTRUMENT LOS_DIAGNOSTICS_BOOTSTRAP_TEXT DiagnosticsWriteTraceLine(
    const char *Prefix,
    UINT64 Sequence,
    UINT64 Tick,
    UINT64 FunctionAddress,
    UINT64 CallerAddress)
{
    LosKernelSerialWriteText(Prefix);
    LosKernelSerialWriteText(" seq=");
    LosKernelSerialWriteUnsigned(Sequence);
    LosKernelSerialWriteText(" tick=");
    LosKernelSerialWriteUnsigned(Tick);
    LosKernelSerialWriteText(" function=");
    LosKernelSerialWriteHex64(FunctionAddress);
    LosKernelSerialWriteText(" caller=");
    LosKernelSerialWriteHex64(CallerAddress);
    LosKernelSerialWriteText("\n");
}

static void LOS_DIAGNOSTICS_NOINSTRUMENT LOS_DIAGNOSTICS_BOOTSTRAP_TEXT DiagnosticsRecordFunctionEntry(void *FunctionAddress, void *CallerAddress)
{
    UINT64 WriteIndex;
    UINT64 Sequence;
    UINT64 Tick;
    LOS_DIAGNOSTICS_TRACE_ENTRY *Entry;

    if (LosKernelRuntimeTracingEnabled == 0ULL || LosDiagnosticsBusy != 0ULL)
    {
        return;
    }

    LosDiagnosticsBusy = 1ULL;

    Sequence = LosDiagnosticsTraceSequence + 1ULL;
    LosDiagnosticsTraceSequence = Sequence;
    Tick = LosX64GetTimerTickCount();
    WriteIndex = LosDiagnosticsTraceWriteIndex;
    Entry = &LosDiagnosticsTraceEntries[WriteIndex % LOS_DIAGNOSTICS_TRACE_ENTRY_COUNT];
    Entry->Sequence = Sequence;
    Entry->Tick = Tick;
    Entry->FunctionAddress = (UINT64)(UINTN)FunctionAddress;
    Entry->CallerAddress = (UINT64)(UINTN)CallerAddress;
    LosDiagnosticsTraceWriteIndex = WriteIndex + 1ULL;
    if (LosDiagnosticsTraceEntryCount < LOS_DIAGNOSTICS_TRACE_ENTRY_COUNT)
    {
        LosDiagnosticsTraceEntryCount += 1ULL;
    }

    DiagnosticsWriteTraceLine(
        "[OK] [Diag] Enter",
        Sequence,
        Tick,
        Entry->FunctionAddress,
        Entry->CallerAddress);

    LosDiagnosticsBusy = 0ULL;
}

static void LOS_DIAGNOSTICS_NOINSTRUMENT LOS_DIAGNOSTICS_BOOTSTRAP_TEXT DiagnosticsWriteRecentTrace(void)
{
    UINT64 EntryCount;
    UINT64 WriteIndex;
    UINT64 ToWrite;
    UINT64 Offset;

    EntryCount = LosDiagnosticsTraceEntryCount;
    WriteIndex = LosDiagnosticsTraceWriteIndex;

    LosKernelSerialWriteText("[FAIL] [Diag] Trace entries captured=");
    LosKernelSerialWriteUnsigned(EntryCount);
    LosKernelSerialWriteText(" total-sequence=");
    LosKernelSerialWriteUnsigned(LosDiagnosticsTraceSequence);
    LosKernelSerialWriteText("\n");

    if (EntryCount == 0ULL)
    {
        LosKernelSerialWriteText("[FAIL] [Diag] No recorded function-entry trace is available.\n");
        return;
    }

    ToWrite = EntryCount;
    if (ToWrite > LOS_DIAGNOSTICS_CRASH_TRACE_LINES)
    {
        ToWrite = LOS_DIAGNOSTICS_CRASH_TRACE_LINES;
    }

    LosKernelSerialWriteText("[FAIL] [Diag] Recent function entries follow (newest first).\n");
    for (Offset = 0ULL; Offset < ToWrite; ++Offset)
    {
        UINT64 RingIndex;
        const LOS_DIAGNOSTICS_TRACE_ENTRY *Entry;

        RingIndex = (WriteIndex - 1ULL - Offset) % LOS_DIAGNOSTICS_TRACE_ENTRY_COUNT;
        Entry = &LosDiagnosticsTraceEntries[RingIndex];
        DiagnosticsWriteTraceLine(
            "[FAIL] [Diag] Entry",
            Entry->Sequence,
            Entry->Tick,
            Entry->FunctionAddress,
            Entry->CallerAddress);
    }
}

void LOS_DIAGNOSTICS_NOINSTRUMENT LOS_DIAGNOSTICS_BOOTSTRAP_TEXT LosDiagnosticsInitialize(void)
{
    UINT64 Index;

    LosDiagnosticsTraceSequence = 0ULL;
    LosDiagnosticsTraceEntryCount = 0ULL;
    LosDiagnosticsTraceWriteIndex = 0ULL;
    LosDiagnosticsBusy = 0ULL;

    for (Index = 0ULL; Index < LOS_DIAGNOSTICS_TRACE_ENTRY_COUNT; ++Index)
    {
        LosDiagnosticsTraceEntries[Index].Sequence = 0ULL;
        LosDiagnosticsTraceEntries[Index].Tick = 0ULL;
        LosDiagnosticsTraceEntries[Index].FunctionAddress = 0ULL;
        LosDiagnosticsTraceEntries[Index].CallerAddress = 0ULL;
    }
}

void LOS_DIAGNOSTICS_NOINSTRUMENT LOS_DIAGNOSTICS_BOOTSTRAP_TEXT __cyg_profile_func_enter(void *FunctionAddress, void *CallerAddress)
{
    DiagnosticsRecordFunctionEntry(FunctionAddress, CallerAddress);
}

void LOS_DIAGNOSTICS_NOINSTRUMENT LOS_DIAGNOSTICS_BOOTSTRAP_TEXT __cyg_profile_func_exit(void *FunctionAddress, void *CallerAddress)
{
    LOS_UNUSED_PARAMETER(FunctionAddress);
    LOS_UNUSED_PARAMETER(CallerAddress);
}

void LOS_DIAGNOSTICS_NOINSTRUMENT LOS_DIAGNOSTICS_BOOTSTRAP_TEXT LosDiagnosticsWriteInterruptCrashReport(
    const LOS_X64_REGISTER_STATE *Registers,
    UINT64 Vector,
    UINT64 ErrorCode,
    const LOS_X64_INTERRUPT_FRAME *Frame)
{
    LosKernelRuntimeTracingEnabled = 0ULL;
    LosDiagnosticsBusy = 1ULL;

    LosKernelSerialWriteText("[FAIL] [Kernel] Exception vector ");
    LosKernelSerialWriteUnsigned(Vector);
    LosKernelSerialWriteText(": ");
    LosKernelSerialWriteText(LosX64GetExceptionName(Vector));
    LosKernelSerialWriteText("\n");
    LosKernelSerialWriteText("[FAIL] [Kernel] Error code: ");
    LosKernelSerialWriteHex64(ErrorCode);
    LosKernelSerialWriteText("\n");

    LosX64DescribeFault(Vector, ErrorCode);
    LosX64WriteRegisterDump(Registers, Frame);
    DiagnosticsWriteRecentTrace();
}
