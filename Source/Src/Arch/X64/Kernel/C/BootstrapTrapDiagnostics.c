/*
 * File Name: BootstrapTrapDiagnostics.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

#include "BootstrapTrapInternal.h"

static const char LosBootstrapUnknownException[] LOS_X64_BOOTSTRAP_RODATA = "Unknown Exception";
static const char LosBootstrapException0[] LOS_X64_BOOTSTRAP_RODATA = "#DE Divide Error";
static const char LosBootstrapException1[] LOS_X64_BOOTSTRAP_RODATA = "#DB Debug";
static const char LosBootstrapException2[] LOS_X64_BOOTSTRAP_RODATA = "NMI Interrupt";
static const char LosBootstrapException3[] LOS_X64_BOOTSTRAP_RODATA = "#BP Breakpoint";
static const char LosBootstrapException4[] LOS_X64_BOOTSTRAP_RODATA = "#OF Overflow";
static const char LosBootstrapException5[] LOS_X64_BOOTSTRAP_RODATA = "#BR Bound Range Exceeded";
static const char LosBootstrapException6[] LOS_X64_BOOTSTRAP_RODATA = "#UD Invalid Opcode";
static const char LosBootstrapException7[] LOS_X64_BOOTSTRAP_RODATA = "#NM Device Not Available";
static const char LosBootstrapException8[] LOS_X64_BOOTSTRAP_RODATA = "#DF Double Fault";
static const char LosBootstrapException9[] LOS_X64_BOOTSTRAP_RODATA = "Coprocessor Segment Overrun";
static const char LosBootstrapException10[] LOS_X64_BOOTSTRAP_RODATA = "#TS Invalid TSS";
static const char LosBootstrapException11[] LOS_X64_BOOTSTRAP_RODATA = "#NP Segment Not Present";
static const char LosBootstrapException12[] LOS_X64_BOOTSTRAP_RODATA = "#SS Stack Fault";
static const char LosBootstrapException13[] LOS_X64_BOOTSTRAP_RODATA = "#GP General Protection Fault";
static const char LosBootstrapException14[] LOS_X64_BOOTSTRAP_RODATA = "#PF Page Fault";
static const char LosBootstrapException15[] LOS_X64_BOOTSTRAP_RODATA = "Reserved";
static const char LosBootstrapException16[] LOS_X64_BOOTSTRAP_RODATA = "#MF x87 Floating Point";
static const char LosBootstrapException17[] LOS_X64_BOOTSTRAP_RODATA = "#AC Alignment Check";
static const char LosBootstrapException18[] LOS_X64_BOOTSTRAP_RODATA = "#MC Machine Check";
static const char LosBootstrapException19[] LOS_X64_BOOTSTRAP_RODATA = "#XM SIMD Floating Point";
static const char LosBootstrapException20[] LOS_X64_BOOTSTRAP_RODATA = "#VE Virtualization";
static const char LosBootstrapException21[] LOS_X64_BOOTSTRAP_RODATA = "#CP Control Protection";
static const char LosBootstrapException22[] LOS_X64_BOOTSTRAP_RODATA = "Reserved";
static const char LosBootstrapException23[] LOS_X64_BOOTSTRAP_RODATA = "Reserved";
static const char LosBootstrapException24[] LOS_X64_BOOTSTRAP_RODATA = "Reserved";
static const char LosBootstrapException25[] LOS_X64_BOOTSTRAP_RODATA = "Reserved";
static const char LosBootstrapException26[] LOS_X64_BOOTSTRAP_RODATA = "Reserved";
static const char LosBootstrapException27[] LOS_X64_BOOTSTRAP_RODATA = "Reserved";
static const char LosBootstrapException28[] LOS_X64_BOOTSTRAP_RODATA = "#HV Hypervisor Injection";
static const char LosBootstrapException29[] LOS_X64_BOOTSTRAP_RODATA = "#VC VMM Communication";
static const char LosBootstrapException30[] LOS_X64_BOOTSTRAP_RODATA = "#SX Security";
static const char LosBootstrapException31[] LOS_X64_BOOTSTRAP_RODATA = "Reserved";

static const char *const LosBootstrapExceptionNames[LOS_X64_BOOTSTRAP_TRAP_EXCEPTION_VECTOR_COUNT] LOS_X64_BOOTSTRAP_RODATA =
{
    LosBootstrapException0,  LosBootstrapException1,  LosBootstrapException2,  LosBootstrapException3,
    LosBootstrapException4,  LosBootstrapException5,  LosBootstrapException6,  LosBootstrapException7,
    LosBootstrapException8,  LosBootstrapException9,  LosBootstrapException10, LosBootstrapException11,
    LosBootstrapException12, LosBootstrapException13, LosBootstrapException14, LosBootstrapException15,
    LosBootstrapException16, LosBootstrapException17, LosBootstrapException18, LosBootstrapException19,
    LosBootstrapException20, LosBootstrapException21, LosBootstrapException22, LosBootstrapException23,
    LosBootstrapException24, LosBootstrapException25, LosBootstrapException26, LosBootstrapException27,
    LosBootstrapException28, LosBootstrapException29, LosBootstrapException30, LosBootstrapException31
};

static const char LosBootstrapPrefix[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] ";
static const char LosBootstrapEquals[] LOS_X64_BOOTSTRAP_RODATA = "=";
static const char LosBootstrapSpace[] LOS_X64_BOOTSTRAP_RODATA = " ";
static const char LosBootstrapPageFaultDecodePrefix[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Page-fault decode:";
static const char LosBootstrapPresentViolation[] LOS_X64_BOOTSTRAP_RODATA = " present-violation";
static const char LosBootstrapNonPresent[] LOS_X64_BOOTSTRAP_RODATA = " non-present";
static const char LosBootstrapWrite[] LOS_X64_BOOTSTRAP_RODATA = " write";
static const char LosBootstrapRead[] LOS_X64_BOOTSTRAP_RODATA = " read";
static const char LosBootstrapUser[] LOS_X64_BOOTSTRAP_RODATA = " user";
static const char LosBootstrapSupervisor[] LOS_X64_BOOTSTRAP_RODATA = " supervisor";
static const char LosBootstrapReservedBit[] LOS_X64_BOOTSTRAP_RODATA = " reserved-bit";
static const char LosBootstrapInstructionFetch[] LOS_X64_BOOTSTRAP_RODATA = " instruction-fetch";
static const char LosBootstrapProtectionKey[] LOS_X64_BOOTSTRAP_RODATA = " protection-key";
static const char LosBootstrapShadowStack[] LOS_X64_BOOTSTRAP_RODATA = " shadow-stack";
static const char LosBootstrapSoftwareGuardExtension[] LOS_X64_BOOTSTRAP_RODATA = " software-guard-extension";
static const char LosBootstrapRecursiveTrap[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Recursive bootstrap trap. Halting.\n";
static const char LosBootstrapTrapCaptured[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Post-ExitBootServices trap captured.\n";
static const char LosBootstrapVectorPrefix[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Vector: ";
static const char LosBootstrapErrorCodePrefix[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Error code: ";
static const char LosBootstrapCr2Prefix[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] CR2: ";
static const char LosBootstrapCr0Prefix[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] CR0: ";
static const char LosBootstrapCr3Prefix[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] CR3: ";
static const char LosBootstrapCr4Prefix[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] CR4: ";
static const char LosBootstrapRipName[] LOS_X64_BOOTSTRAP_RODATA = "RIP";
static const char LosBootstrapCsName[] LOS_X64_BOOTSTRAP_RODATA = "CS";
static const char LosBootstrapRflagsName[] LOS_X64_BOOTSTRAP_RODATA = "RFLAGS";
static const char LosBootstrapRspName[] LOS_X64_BOOTSTRAP_RODATA = "RSP";
static const char LosBootstrapRaxName[] LOS_X64_BOOTSTRAP_RODATA = "RAX";
static const char LosBootstrapRbxName[] LOS_X64_BOOTSTRAP_RODATA = "RBX";
static const char LosBootstrapRcxName[] LOS_X64_BOOTSTRAP_RODATA = "RCX";
static const char LosBootstrapRdxName[] LOS_X64_BOOTSTRAP_RODATA = "RDX";
static const char LosBootstrapRsiName[] LOS_X64_BOOTSTRAP_RODATA = "RSI";
static const char LosBootstrapRdiName[] LOS_X64_BOOTSTRAP_RODATA = "RDI";
static const char LosBootstrapRbpName[] LOS_X64_BOOTSTRAP_RODATA = "RBP";
static const char LosBootstrapR8Name[] LOS_X64_BOOTSTRAP_RODATA = "R8";
static const char LosBootstrapR9Name[] LOS_X64_BOOTSTRAP_RODATA = "R9";
static const char LosBootstrapR10Name[] LOS_X64_BOOTSTRAP_RODATA = "R10";
static const char LosBootstrapR11Name[] LOS_X64_BOOTSTRAP_RODATA = "R11";
static const char LosBootstrapR12Name[] LOS_X64_BOOTSTRAP_RODATA = "R12";
static const char LosBootstrapR13Name[] LOS_X64_BOOTSTRAP_RODATA = "R13";
static const char LosBootstrapR14Name[] LOS_X64_BOOTSTRAP_RODATA = "R14";
static const char LosBootstrapR15Name[] LOS_X64_BOOTSTRAP_RODATA = "R15";
static const char LosBootstrapTrapCodeSelectorName[] LOS_X64_BOOTSTRAP_RODATA = "TrapCodeSelector";
static const char LosBootstrapReporterHalted[] LOS_X64_BOOTSTRAP_RODATA = "[KernelBootstrap] Bootstrap trap reporter halted the machine.\n";

LOS_X64_BOOTSTRAP_SECTION
static UINT64 ReadCr0(void)
{
    UINT64 Value;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(Value));
    return Value;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 ReadCr2(void)
{
    UINT64 Value;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(Value));
    return Value;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 ReadCr3(void)
{
    UINT64 Value;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(Value));
    return Value;
}

LOS_X64_BOOTSTRAP_SECTION
static UINT64 ReadCr4(void)
{
    UINT64 Value;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(Value));
    return Value;
}

LOS_X64_BOOTSTRAP_SECTION
static const char *GetBootstrapExceptionName(UINT64 Vector)
{
    if (Vector < LOS_X64_BOOTSTRAP_TRAP_EXCEPTION_VECTOR_COUNT)
    {
        return LosBootstrapExceptionNames[Vector];
    }

    return LosBootstrapUnknownException;
}

LOS_X64_BOOTSTRAP_SECTION
static void WriteRegisterLine(const char *NameA, UINT64 ValueA, const char *NameB, UINT64 ValueB)
{
    LosX64BootstrapSerialWriteText(LosBootstrapPrefix);
    LosX64BootstrapSerialWriteText(NameA);
    LosX64BootstrapSerialWriteText(LosBootstrapEquals);
    LosX64BootstrapSerialWriteHex64(ValueA);
    LosX64BootstrapSerialWriteText(LosBootstrapSpace);
    LosX64BootstrapSerialWriteText(NameB);
    LosX64BootstrapSerialWriteText(LosBootstrapEquals);
    LosX64BootstrapSerialWriteHex64(ValueB);
    LosX64BootstrapSerialWriteChar('\n');
}

LOS_X64_BOOTSTRAP_SECTION
static void WritePageFaultDecode(UINT64 ErrorCode)
{
    LosX64BootstrapSerialWriteText(LosBootstrapPageFaultDecodePrefix);
    LosX64BootstrapSerialWriteText((ErrorCode & 0x001ULL) != 0ULL ? LosBootstrapPresentViolation : LosBootstrapNonPresent);
    LosX64BootstrapSerialWriteText((ErrorCode & 0x002ULL) != 0ULL ? LosBootstrapWrite : LosBootstrapRead);
    LosX64BootstrapSerialWriteText((ErrorCode & 0x004ULL) != 0ULL ? LosBootstrapUser : LosBootstrapSupervisor);
    if ((ErrorCode & 0x008ULL) != 0ULL)
    {
        LosX64BootstrapSerialWriteText(LosBootstrapReservedBit);
    }
    if ((ErrorCode & 0x010ULL) != 0ULL)
    {
        LosX64BootstrapSerialWriteText(LosBootstrapInstructionFetch);
    }
    if ((ErrorCode & 0x020ULL) != 0ULL)
    {
        LosX64BootstrapSerialWriteText(LosBootstrapProtectionKey);
    }
    if ((ErrorCode & 0x040ULL) != 0ULL)
    {
        LosX64BootstrapSerialWriteText(LosBootstrapShadowStack);
    }
    if ((ErrorCode & 0x080ULL) != 0ULL)
    {
        LosX64BootstrapSerialWriteText(LosBootstrapSoftwareGuardExtension);
    }
    LosX64BootstrapSerialWriteChar('\n');
}

LOS_X64_BOOTSTRAP_SECTION
void LosX64BootstrapHandleTrap(
    const LOS_X64_BOOTSTRAP_REGISTER_STATE *Registers,
    UINT64 Vector,
    UINT64 ErrorCode,
    const LOS_X64_BOOTSTRAP_INTERRUPT_FRAME *Frame,
    UINT64 InterruptedStackPointer)
{
    __asm__ __volatile__("cli" : : : "memory");

    if (LosX64BootstrapTrapActive != 0ULL)
    {
        LosX64BootstrapSerialWriteStatusTagFail();
        LosX64BootstrapSerialWriteText(LosBootstrapRecursiveTrap);
        LosX64BootstrapHaltForever();
    }

    LosX64BootstrapTrapActive = 1ULL;
    LosX64BootstrapSerialInit();
    LosX64BootstrapSerialWriteStatusTagFail();
    LosX64BootstrapSerialWriteText(LosBootstrapTrapCaptured);
    LosX64BootstrapSerialWriteText(LosBootstrapVectorPrefix);
    LosX64BootstrapSerialWriteUnsigned(Vector);
    LosX64BootstrapSerialWriteText(LosBootstrapSpace);
    LosX64BootstrapSerialWriteText(GetBootstrapExceptionName(Vector));
    LosX64BootstrapSerialWriteChar('\n');
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapErrorCodePrefix, ErrorCode);

    if (Vector == 14ULL)
    {
        WritePageFaultDecode(ErrorCode);
        LosX64BootstrapSerialWriteLineHex64(LosBootstrapCr2Prefix, ReadCr2());
    }

    LosX64BootstrapSerialWriteLineHex64(LosBootstrapCr0Prefix, ReadCr0());
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapCr3Prefix, ReadCr3());
    LosX64BootstrapSerialWriteLineHex64(LosBootstrapCr4Prefix, ReadCr4());

    if (Frame != 0)
    {
        WriteRegisterLine(LosBootstrapRipName, Frame->Rip, LosBootstrapCsName, Frame->Cs);
        WriteRegisterLine(LosBootstrapRflagsName, Frame->Rflags, LosBootstrapRspName, InterruptedStackPointer);
    }

    if (Registers != 0)
    {
        WriteRegisterLine(LosBootstrapRaxName, Registers->Rax, LosBootstrapRbxName, Registers->Rbx);
        WriteRegisterLine(LosBootstrapRcxName, Registers->Rcx, LosBootstrapRdxName, Registers->Rdx);
        WriteRegisterLine(LosBootstrapRsiName, Registers->Rsi, LosBootstrapRdiName, Registers->Rdi);
        WriteRegisterLine(LosBootstrapRbpName, Registers->Rbp, LosBootstrapR8Name, Registers->R8);
        WriteRegisterLine(LosBootstrapR9Name, Registers->R9, LosBootstrapR10Name, Registers->R10);
        WriteRegisterLine(LosBootstrapR11Name, Registers->R11, LosBootstrapR12Name, Registers->R12);
        WriteRegisterLine(LosBootstrapR13Name, Registers->R13, LosBootstrapR14Name, Registers->R14);
        WriteRegisterLine(LosBootstrapR15Name, Registers->R15, LosBootstrapTrapCodeSelectorName, LosX64BootstrapTrapCodeSelector);
    }

    LosX64BootstrapSerialWriteStatusTagFail();
    LosX64BootstrapSerialWriteText(LosBootstrapReporterHalted);
    LosX64BootstrapHaltForever();
}
