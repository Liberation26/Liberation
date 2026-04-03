#include "InterruptsInternal.h"

LOS_X64_IDT_ENTRY LosX64Idt[LOS_X64_IDT_ENTRY_COUNT];
LOS_X64_DESCRIPTOR_POINTER LosX64IdtPointer;

void LosX64InitializeIdtState(void)
{
    LOS_KERNEL_ENTER();
    UINTN Index;

    for (Index = 0U; Index < LOS_X64_IDT_ENTRY_COUNT; ++Index)
    {
        LosX64Idt[Index].OffsetLow = 0U;
        LosX64Idt[Index].Selector = 0x08U;
        LosX64Idt[Index].Ist = 0U;
        LosX64Idt[Index].TypeAttributes = 0x8EU;
        LosX64Idt[Index].OffsetMiddle = 0U;
        LosX64Idt[Index].OffsetHigh = 0U;
        LosX64Idt[Index].Reserved = 0U;
    }

    LosX64IdtPointer.Limit = (UINT16)(sizeof(LosX64Idt) - 1U);
    LosX64IdtPointer.Base = (UINT64)(UINTN)&LosX64Idt[0];
}

const void *LosX64GetIdtBase(void)
{
    LOS_KERNEL_ENTER();
    return &LosX64Idt[0];
}

UINT64 LosX64GetIdtSize(void)
{
    LOS_KERNEL_ENTER();
    return (UINT64)sizeof(LosX64Idt);
}
