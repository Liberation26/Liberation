#ifndef LOS_X64_BOOTSTRAP_TRAP_INTERNAL_H
#define LOS_X64_BOOTSTRAP_TRAP_INTERNAL_H

#include "BootstrapTrap.h"
#include "VirtualMemoryInternal.h"

#define LOS_X64_BOOTSTRAP_TRAP_IDT_ENTRY_COUNT 256U
#define LOS_X64_BOOTSTRAP_TRAP_EXCEPTION_VECTOR_COUNT 32U

typedef struct __attribute__((packed))
{
    UINT16 Limit;
    UINT64 Base;
} LOS_X64_BOOTSTRAP_DESCRIPTOR_POINTER;

typedef struct __attribute__((packed))
{
    UINT16 OffsetLow;
    UINT16 Selector;
    UINT8 Ist;
    UINT8 TypeAttributes;
    UINT16 OffsetMiddle;
    UINT32 OffsetHigh;
    UINT32 Reserved;
} LOS_X64_BOOTSTRAP_IDT_ENTRY;

static inline void LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapSerialInit(void)
{
    LosX64BootstrapOut8(0x3F8U + 1U, 0x00U);
    LosX64BootstrapOut8(0x3F8U + 3U, 0x80U);
    LosX64BootstrapOut8(0x3F8U + 0U, 0x03U);
    LosX64BootstrapOut8(0x3F8U + 1U, 0x00U);
    LosX64BootstrapOut8(0x3F8U + 3U, 0x03U);
    LosX64BootstrapOut8(0x3F8U + 2U, 0xC7U);
    LosX64BootstrapOut8(0x3F8U + 4U, 0x0BU);
}

static inline void LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapSerialWriteUnsigned(UINT64 Value)
{
    char Buffer[32];
    UINTN Index;

    if (Value == 0ULL)
    {
        LosX64BootstrapSerialWriteChar('0');
        return;
    }

    Index = 0U;
    while (Value > 0ULL && Index < sizeof(Buffer))
    {
        Buffer[Index] = (char)('0' + (Value % 10ULL));
        Value /= 10ULL;
        ++Index;
    }

    while (Index > 0U)
    {
        --Index;
        LosX64BootstrapSerialWriteChar(Buffer[Index]);
    }
}

void LosX64InitializeBootstrapTrapState(void);
void LosX64PopulateBootstrapTrapEntries(void);
void LosX64LoadBootstrapTrapIdt(void);
void LosX64BootstrapHandleTrap(
    const LOS_X64_BOOTSTRAP_REGISTER_STATE *Registers,
    UINT64 Vector,
    UINT64 ErrorCode,
    const LOS_X64_BOOTSTRAP_INTERRUPT_FRAME *Frame,
    UINT64 InterruptedStackPointer);

extern LOS_X64_BOOTSTRAP_IDT_ENTRY LosX64BootstrapIdt[LOS_X64_BOOTSTRAP_TRAP_IDT_ENTRY_COUNT];
extern LOS_X64_BOOTSTRAP_DESCRIPTOR_POINTER LosX64BootstrapIdtPointer;
extern UINT16 LosX64BootstrapTrapCodeSelector;
extern volatile UINT64 LosX64BootstrapTrapActive;

#endif
