/*
 * File Name: InterruptDiagnostics.c
 * File Version: 0.3.12
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-10T17:20:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "InterruptsInternal.h"
#include "VirtualMemory.h"

#define LOS_X64_PAGE_PRESENT 0x001ULL
#define LOS_X64_PAGE_LARGE 0x080ULL
#define LOS_X64_PAGE_TABLE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#define LOS_X64_PAGE_2MB_ADDRESS_MASK 0x000FFFFFFFE00000ULL
#define LOS_X64_PAGE_1GB_ADDRESS_MASK 0x000FFFFFC0000000ULL
#define LOS_X64_RIP_DUMP_BYTE_COUNT 32U

static UINT64 ReadCr2(void)
{
    LOS_KERNEL_ENTER();
    UINT64 Value;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(Value));
    return Value;
}

static UINT64 ReadCr3(void)
{
    LOS_KERNEL_ENTER();
    UINT64 Value;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(Value));
    return Value;
}

static UINTN QueryPml4Index(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 39U) & 0x1FFULL);
}

static UINTN QueryPdptIndex(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 30U) & 0x1FFULL);
}

static UINTN QueryPdIndex(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 21U) & 0x1FFULL);
}

static UINTN QueryPtIndex(UINT64 VirtualAddress)
{
    return (UINTN)((VirtualAddress >> 12U) & 0x1FFULL);
}

static UINT64 *TranslatePageTable(UINT64 PhysicalAddress)
{
    LOS_KERNEL_ENTER();
    return (UINT64 *)LosX64GetDirectMapVirtualAddress(PhysicalAddress, 0x1000ULL);
}

static BOOLEAN ResolveCurrentVirtualToPhysical(UINT64 VirtualAddress, UINT64 *PhysicalAddress)
{
    UINT64 RootTablePhysicalAddress;
    UINT64 *PageMapLevel4;
    UINT64 CurrentEntry;
    UINT64 *PageDirectoryPointerTable;
    UINT64 *PageDirectory;
    UINT64 *PageTable;

    LOS_KERNEL_ENTER();
    if (PhysicalAddress != 0)
    {
        *PhysicalAddress = 0ULL;
    }

    RootTablePhysicalAddress = ReadCr3() & LOS_X64_PAGE_TABLE_ADDRESS_MASK;
    if (RootTablePhysicalAddress == 0ULL || PhysicalAddress == 0)
    {
        return 0;
    }

    PageMapLevel4 = TranslatePageTable(RootTablePhysicalAddress);
    if (PageMapLevel4 == 0)
    {
        return 0;
    }

    CurrentEntry = PageMapLevel4[QueryPml4Index(VirtualAddress)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }

    PageDirectoryPointerTable = TranslatePageTable(CurrentEntry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageDirectoryPointerTable == 0)
    {
        return 0;
    }

    CurrentEntry = PageDirectoryPointerTable[QueryPdptIndex(VirtualAddress)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }
    if ((CurrentEntry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        *PhysicalAddress = (CurrentEntry & LOS_X64_PAGE_1GB_ADDRESS_MASK) | (VirtualAddress & 0x3FFFFFFFULL);
        return 1;
    }

    PageDirectory = TranslatePageTable(CurrentEntry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageDirectory == 0)
    {
        return 0;
    }

    CurrentEntry = PageDirectory[QueryPdIndex(VirtualAddress)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }
    if ((CurrentEntry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        *PhysicalAddress = (CurrentEntry & LOS_X64_PAGE_2MB_ADDRESS_MASK) | (VirtualAddress & 0x1FFFFFULL);
        return 1;
    }

    PageTable = TranslatePageTable(CurrentEntry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageTable == 0)
    {
        return 0;
    }

    CurrentEntry = PageTable[QueryPtIndex(VirtualAddress)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0;
    }

    *PhysicalAddress = (CurrentEntry & LOS_X64_PAGE_TABLE_ADDRESS_MASK) | (VirtualAddress & 0xFFFULL);
    return 1;
}

static char HexDigit(UINT8 Value)
{
    return (char)(Value < 10U ? ('0' + Value) : ('A' + (Value - 10U)));
}

static void WriteHexByte(UINT8 Value)
{
    char Buffer[3];

    LOS_KERNEL_ENTER();
    Buffer[0] = HexDigit((UINT8)((Value >> 4U) & 0x0FU));
    Buffer[1] = HexDigit((UINT8)(Value & 0x0FU));
    Buffer[2] = '\0';
    LosKernelSerialWriteText(Buffer);
}

static UINT64 ResolveInterruptedStackPointer(const LOS_X64_INTERRUPT_FRAME *Frame)
{
    LOS_KERNEL_ENTER();
    if (Frame == 0)
    {
        return 0ULL;
    }

    if ((Frame->Cs & 0x3ULL) == 0x3ULL)
    {
        return Frame->Rsp;
    }

    return (UINT64)(UINTN)Frame + 24ULL;
}

static void WriteInstructionBytesAtRip(const LOS_X64_INTERRUPT_FRAME *Frame)
{
    UINT32 ByteIndex;
    UINT64 VirtualAddress;
    UINT64 PhysicalAddress;
    const volatile UINT8 *BytePointer;

    LOS_KERNEL_ENTER();
    if (Frame == 0 || Frame->Rip == 0ULL)
    {
        return;
    }

    LosKernelSerialWriteText("[Kernel] RIP bytes:");
    for (ByteIndex = 0U; ByteIndex < LOS_X64_RIP_DUMP_BYTE_COUNT; ++ByteIndex)
    {
        if ((ByteIndex % 16U) == 0U)
        {
            LosKernelSerialWriteText("\n[Kernel]   ");
            LosKernelSerialWriteHex64(Frame->Rip + (UINT64)ByteIndex);
            LosKernelSerialWriteText(": ");
        }
        else
        {
            LosKernelSerialWriteText(" ");
        }

        VirtualAddress = Frame->Rip + (UINT64)ByteIndex;
        if (!ResolveCurrentVirtualToPhysical(VirtualAddress, &PhysicalAddress))
        {
            LosKernelSerialWriteText("??");
            continue;
        }

        BytePointer = (const volatile UINT8 *)LosX64GetDirectMapVirtualAddress(PhysicalAddress, 1ULL);
        if (BytePointer == 0)
        {
            LosKernelSerialWriteText("??");
            continue;
        }

        WriteHexByte(*BytePointer);
    }
    LosKernelSerialWriteText("\n");
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
    LosKernelSerialWriteText("[Kernel] CR2: ");
    LosKernelSerialWriteHex64(ReadCr2());
    LosKernelSerialWriteText("\n");

    if (Vector == 13ULL)
    {
        LosKernelSerialWriteText("[Kernel] General-protection selector/index bits: ");
        LosKernelSerialWriteHex64(ErrorCode);
        LosKernelSerialWriteText("\n");
    }
    else if (Vector == 14ULL)
    {
        WritePageFaultDecode(ErrorCode);
    }
    else if (ErrorCode != 0ULL)
    {
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
    UINT64 InterruptedStackPointer;

    LOS_KERNEL_ENTER();
    if (Registers == 0 || Frame == 0)
    {
        return;
    }

    InterruptedStackPointer = ResolveInterruptedStackPointer(Frame);
    WriteRegisterLine("RIP", Frame->Rip, "CS", Frame->Cs);
    WriteRegisterLine("RFLAGS", Frame->Rflags, "RSP", InterruptedStackPointer);
    WriteRegisterLine("RBP", Registers->Rbp, "CR2", ReadCr2());
    WriteRegisterLine("RAX", Registers->Rax, "RBX", Registers->Rbx);
    WriteRegisterLine("RCX", Registers->Rcx, "RDX", Registers->Rdx);
    WriteRegisterLine("RSI", Registers->Rsi, "RDI", Registers->Rdi);
    WriteRegisterLine("R8", Registers->R8, "R9", Registers->R9);
    WriteRegisterLine("R10", Registers->R10, "R11", Registers->R11);
    WriteRegisterLine("R12", Registers->R12, "R13", Registers->R13);
    WriteRegisterLine("R14", Registers->R14, "R15", Registers->R15);
    if ((Frame->Cs & 0x3ULL) == 0x3ULL)
    {
        WriteRegisterLine("SS", Frame->Ss, "USER-RSP", Frame->Rsp);
    }

    WriteInstructionBytesAtRip(Frame);
}
