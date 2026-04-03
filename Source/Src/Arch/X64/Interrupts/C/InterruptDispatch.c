#include "InterruptsInternal.h"

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
    LosX64LoadIdt();
}

void LosX64HandleException(
    const LOS_X64_REGISTER_STATE *Registers,
    UINT64 Vector,
    UINT64 ErrorCode,
    const LOS_X64_INTERRUPT_FRAME *Frame)
{
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
