#include "BootstrapTrapInternal.h"

LOS_X64_BOOTSTRAP_IDT_ENTRY LosX64BootstrapIdt[LOS_X64_BOOTSTRAP_TRAP_IDT_ENTRY_COUNT] LOS_X64_BOOTSTRAP_DATA;
LOS_X64_BOOTSTRAP_DESCRIPTOR_POINTER LosX64BootstrapIdtPointer __attribute__((section(".bootstrap.data")));
UINT16 LosX64BootstrapTrapCodeSelector __attribute__((section(".bootstrap.data"))) = 0U;
volatile UINT64 LosX64BootstrapTrapActive __attribute__((section(".bootstrap.data"))) = 0ULL;

LOS_X64_BOOTSTRAP_SECTION
void LosX64InitializeBootstrapTrapState(void)
{
    UINTN Index;

    for (Index = 0U; Index < LOS_X64_BOOTSTRAP_TRAP_IDT_ENTRY_COUNT; ++Index)
    {
        LosX64BootstrapIdt[Index].OffsetLow = 0U;
        LosX64BootstrapIdt[Index].Selector = 0U;
        LosX64BootstrapIdt[Index].Ist = 0U;
        LosX64BootstrapIdt[Index].TypeAttributes = 0U;
        LosX64BootstrapIdt[Index].OffsetMiddle = 0U;
        LosX64BootstrapIdt[Index].OffsetHigh = 0U;
        LosX64BootstrapIdt[Index].Reserved = 0U;
    }

    LosX64BootstrapIdtPointer.Limit = (UINT16)(sizeof(LosX64BootstrapIdt) - 1U);
    LosX64BootstrapIdtPointer.Base = (UINT64)(UINTN)&LosX64BootstrapIdt[0];
    __asm__ __volatile__("mov %%cs, %0" : "=r"(LosX64BootstrapTrapCodeSelector));
    LosX64BootstrapTrapActive = 0ULL;
}
