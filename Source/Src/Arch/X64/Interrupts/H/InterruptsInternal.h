#ifndef LOS_X64_INTERRUPTS_INTERNAL_H
#define LOS_X64_INTERRUPTS_INTERNAL_H

#include "Interrupts.h"

#define LOS_X64_IDT_ENTRY_COUNT 256U
#define LOS_X64_EXCEPTION_VECTOR_COUNT 32U
#define LOS_X64_PIC_MASTER_VECTOR_BASE 32U
#define LOS_X64_PIC_SLAVE_VECTOR_BASE 40U
#define LOS_X64_PIC_TIMER_VECTOR LOS_X64_PIC_MASTER_VECTOR_BASE
#define LOS_X64_PIC_SPURIOUS_MASTER_VECTOR 39U
#define LOS_X64_PIC_SPURIOUS_SLAVE_VECTOR 47U
#define LOS_X64_USER_TRANSITION_VECTOR 128U

typedef struct __attribute__((packed))
{
    UINT16 Limit;
    UINT64 Base;
} LOS_X64_DESCRIPTOR_POINTER;

typedef struct __attribute__((packed))
{
    UINT16 OffsetLow;
    UINT16 Selector;
    UINT8 Ist;
    UINT8 TypeAttributes;
    UINT16 OffsetMiddle;
    UINT32 OffsetHigh;
    UINT32 Reserved;
} LOS_X64_IDT_ENTRY;

void LosX64InitializeIdtState(void);
void LosX64PopulateExceptionEntries(void);
void LosX64LoadIdt(void);
void LosX64PopulateRuntimeEntries(void);
const char *LosX64GetExceptionName(UINT64 Vector);
void LosX64DescribeFault(UINT64 Vector, UINT64 ErrorCode);
void LosX64WriteRegisterDump(const LOS_X64_REGISTER_STATE *Registers, const LOS_X64_INTERRUPT_FRAME *Frame);

extern LOS_X64_IDT_ENTRY LosX64Idt[LOS_X64_IDT_ENTRY_COUNT];
extern LOS_X64_DESCRIPTOR_POINTER LosX64IdtPointer;

#endif
