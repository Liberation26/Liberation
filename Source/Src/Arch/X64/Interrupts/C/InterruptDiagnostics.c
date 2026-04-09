/*
 * File Name: InterruptDiagnostics.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "InterruptsInternal.h"

static UINT64 ReadCr2(void)
{
    LOS_KERNEL_ENTER();
    UINT64 Value;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(Value));
    return Value;
}

const char *LosX64GetExceptionName(UINT64 Vector)
{
    LOS_KERNEL_ENTER();
    static const char *const Names[LOS_X64_EXCEPTION_VECTOR_COUNT] =
    {
        "#DE Divide Error",
        "#DB Debug",
        "NMI Interrupt",
        "#BP Breakpoint",
        "#OF Overflow",
        "#BR BOUND Range Exceeded",
        "#UD Invalid Opcode",
        "#NM Device Not Available",
        "#DF Double Fault",
        "Coprocessor Segment Overrun",
        "#TS Invalid TSS",
        "#NP Segment Not Present",
        "#SS Stack-Segment Fault",
        "#GP General Protection Fault",
        "#PF Page Fault",
        "Reserved",
        "#MF x87 Floating-Point",
        "#AC Alignment Check",
        "#MC Machine Check",
        "#XM SIMD Floating-Point",
        "#VE Virtualization",
        "#CP Control Protection",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "#HV Hypervisor Injection",
        "#VC VMM Communication",
        "#SX Security",
        "Reserved"
    };

    if (Vector < LOS_X64_EXCEPTION_VECTOR_COUNT)
    {
        return Names[Vector];
    }

    return "Unknown Exception";
}

static void WritePageFaultDecode(UINT64 ErrorCode)
{
    LOS_KERNEL_ENTER();
    LosKernelSerialWriteText("[Kernel] Page-fault decode:");
    LosKernelSerialWriteText((ErrorCode & 0x001ULL) != 0ULL ? " present-violation" : " non-present");
    LosKernelSerialWriteText((ErrorCode & 0x002ULL) != 0ULL ? " write" : " read");
    LosKernelSerialWriteText((ErrorCode & 0x004ULL) != 0ULL ? " user" : " supervisor");
    if ((ErrorCode & 0x008ULL) != 0ULL)
    {
        LosKernelSerialWriteText(" reserved-bit");
    }
    if ((ErrorCode & 0x010ULL) != 0ULL)
    {
        LosKernelSerialWriteText(" instruction-fetch");
    }
    if ((ErrorCode & 0x020ULL) != 0ULL)
    {
        LosKernelSerialWriteText(" protection-key");
    }
    if ((ErrorCode & 0x040ULL) != 0ULL)
    {
        LosKernelSerialWriteText(" shadow-stack");
    }
    if ((ErrorCode & 0x080ULL) != 0ULL)
    {
        LosKernelSerialWriteText(" software-guard-extension");
    }
    LosKernelSerialWriteText("\n");
    LosKernelSerialWriteText("[Kernel] CR2: ");
    LosKernelSerialWriteHex64(ReadCr2());
    LosKernelSerialWriteText("\n");
}

void LosX64DescribeFault(UINT64 Vector, UINT64 ErrorCode)
{
    LOS_KERNEL_ENTER();
    LosKernelSerialWriteText("[Kernel] Fault decode: ");
    LosKernelSerialWriteText(LosX64GetExceptionName(Vector));
    LosKernelSerialWriteText("\n");

    if (Vector == 13ULL)
    {
        LosKernelSerialWriteText("[Kernel] General-protection selector/index bits: ");
        LosKernelSerialWriteHex64(ErrorCode);
        LosKernelSerialWriteText("\n");
    }
    else if (Vector == 14ULL)
    {
    LOS_KERNEL_ENTER();
        WritePageFaultDecode(ErrorCode);
    }
    else if (ErrorCode != 0ULL)
    {
    LOS_KERNEL_ENTER();
        LosKernelSerialWriteText("[Kernel] Raw error code: ");
        LosKernelSerialWriteHex64(ErrorCode);
        LosKernelSerialWriteText("\n");
    }
}

static void WriteRegisterLine(const char *Name, UINT64 ValueA, const char *NameB, UINT64 ValueB)
{
    LOS_KERNEL_ENTER();
    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteText(Name);
    LosKernelSerialWriteText("=");
    LosKernelSerialWriteHex64(ValueA);
    LosKernelSerialWriteText(" ");
    LosKernelSerialWriteText(NameB);
    LosKernelSerialWriteText("=");
    LosKernelSerialWriteHex64(ValueB);
    LosKernelSerialWriteText("\n");
}

void LosX64WriteRegisterDump(const LOS_X64_REGISTER_STATE *Registers, const LOS_X64_INTERRUPT_FRAME *Frame)
{
    LOS_KERNEL_ENTER();
    if (Registers == 0 || Frame == 0)
    {
        return;
    }

    WriteRegisterLine("RAX", Registers->Rax, "RBX", Registers->Rbx);
    WriteRegisterLine("RCX", Registers->Rcx, "RDX", Registers->Rdx);
    WriteRegisterLine("RSI", Registers->Rsi, "RDI", Registers->Rdi);
    WriteRegisterLine("RBP", Registers->Rbp, "R8", Registers->R8);
    WriteRegisterLine("R9", Registers->R9, "R10", Registers->R10);
    WriteRegisterLine("R11", Registers->R11, "R12", Registers->R12);
    WriteRegisterLine("R13", Registers->R13, "R14", Registers->R14);
    WriteRegisterLine("R15", Registers->R15, "RIP", Frame->Rip);
    WriteRegisterLine("CS", Frame->Cs, "RFLAGS", Frame->Rflags);
    if ((Frame->Cs & 0x3ULL) == 0x3ULL)
    {
        WriteRegisterLine("RSP", Frame->Rsp, "SS", Frame->Ss);
    }
}
