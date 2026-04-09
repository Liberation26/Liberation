/*
 * File Name: VirtualMemoryInternal.h
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#ifndef LOS_X64_VIRTUAL_MEMORY_INTERNAL_H
#define LOS_X64_VIRTUAL_MEMORY_INTERNAL_H

#include "VirtualMemory.h"
#include "Interrupts.h"

#define LOS_X64_PAGE_PRESENT 0x001ULL
#define LOS_X64_PAGE_WRITABLE 0x002ULL
#define LOS_X64_PAGE_USER 0x004ULL
#define LOS_X64_PAGE_WRITE_THROUGH 0x008ULL
#define LOS_X64_PAGE_CACHE_DISABLE 0x010ULL
#define LOS_X64_PAGE_ACCESSED 0x020ULL
#define LOS_X64_PAGE_DIRTY 0x040ULL
#define LOS_X64_PAGE_LARGE 0x080ULL
#define LOS_X64_PAGE_GLOBAL 0x100ULL
#define LOS_X64_PAGE_NX 0x8000000000000000ULL
#define LOS_X64_PAGE_TABLE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#define LOS_X64_CR0_WP 0x00010000ULL
#define LOS_X64_CR4_PAE 0x00000020ULL
#define LOS_X64_CR4_PGE 0x00000080ULL
#define LOS_X64_EFER_MSR 0xC0000080U
#define LOS_X64_EFER_NXE 0x00000800ULL
#define LOS_X64_EFER_SCE 0x00000001ULL
#define LOS_X64_BOOTSTRAP_IDENTITY_BYTES 0x00200000ULL
#define LOS_X64_HIGHER_HALF_BASE 0xFFFF800000000000ULL
#define LOS_X64_KERNEL_WINDOW_BASE 0xFFFFFF0000000000ULL
#define LOS_X64_KERNEL_STACK_BASE 0xFFFFFFFFC0000000ULL
#define LOS_X64_KERNEL_STACK_GUARD_PAGES 1ULL
#define LOS_X64_KERNEL_STACK_COMMITTED_PAGES 8ULL
#define LOS_X64_KERNEL_STACK_SIZE_BYTES (LOS_X64_KERNEL_STACK_COMMITTED_PAGES * 4096ULL)
#define LOS_X64_KERNEL_STACK_TOP (LOS_X64_KERNEL_STACK_BASE + ((LOS_X64_KERNEL_STACK_GUARD_PAGES + LOS_X64_KERNEL_STACK_COMMITTED_PAGES) * 4096ULL))
#define LOS_X64_PAGE_TABLE_POOL_PAGES 512U
#define LOS_X64_MAX_PHYSICAL_MEMORY_DESCRIPTORS 512U
#define LOS_X64_MAX_PHYSICAL_FRAME_REGIONS 4096U
#define LOS_X64_MAX_MEMORY_REGIONS 4096U
#define LOS_X64_BOOTSTRAP_TRANSITION_STACK_PAGES 8U
#define LOS_X64_BOOTSTRAP_TRANSITION_STACK_BYTES (LOS_X64_BOOTSTRAP_TRANSITION_STACK_PAGES * 4096U)
#define LOS_X64_BOOTSTRAP_SECTION __attribute__((section(".bootstrap.text")))
#define LOS_X64_BOOTSTRAP_DATA __attribute__((section(".bootstrap.data"), aligned(4096)))
#define LOS_X64_BOOTSTRAP_RODATA __attribute__((section(".bootstrap.rodata")))

typedef struct
{
    UINT64 BaseAddress;
    UINT64 Length;
    UINT32 Type;
    UINT32 Flags;
    UINT64 Attributes;
} LOS_X64_PHYSICAL_MEMORY_DESCRIPTOR;

static inline void LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapOut8(UINT16 Port, UINT8 Value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static inline UINT8 LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapIn8(UINT16 Port)
{
    UINT8 Value;
    __asm__ __volatile__("inb %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

static inline void LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapSerialWriteChar(char Character)
{
    while ((LosX64BootstrapIn8(0x3F8U + 5U) & 0x20U) == 0U)
    {
    }

    LosX64BootstrapOut8(0x3F8U + 0U, (UINT8)Character);
}

static inline void LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapSerialWriteText(const char *Text)
{
    UINTN Index;

    if (Text == 0)
    {
        return;
    }

    for (Index = 0U; Text[Index] != '\0'; ++Index)
    {
        if (Text[Index] == '\n')
        {
            LosX64BootstrapSerialWriteChar('\r');
        }
        LosX64BootstrapSerialWriteChar(Text[Index]);
    }
}

static inline void LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapSerialWriteAnsiColor(UINT8 Code)
{
    LosX64BootstrapSerialWriteChar(0x1BU);
    LosX64BootstrapSerialWriteChar('[');
    if (Code == 0U)
    {
        LosX64BootstrapSerialWriteChar('0');
        LosX64BootstrapSerialWriteChar('m');
        return;
    }

    if (Code >= 100U)
    {
        LosX64BootstrapSerialWriteChar((char)('0' + (Code / 100U)));
        Code = (UINT8)(Code % 100U);
    }
    if (Code >= 10U)
    {
        LosX64BootstrapSerialWriteChar((char)('0' + (Code / 10U)));
    }
    LosX64BootstrapSerialWriteChar((char)('0' + (Code % 10U)));
    LosX64BootstrapSerialWriteChar('m');
}

static inline void LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapSerialWriteStatusTagOk(void)
{
    LosX64BootstrapSerialWriteAnsiColor(32U);
    LosX64BootstrapSerialWriteChar('[');
    LosX64BootstrapSerialWriteChar('O');
    LosX64BootstrapSerialWriteChar('K');
    LosX64BootstrapSerialWriteChar(']');
    LosX64BootstrapSerialWriteAnsiColor(0U);
    LosX64BootstrapSerialWriteChar(' ');
}

static inline void LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapSerialWriteStatusTagFail(void)
{
    LosX64BootstrapSerialWriteAnsiColor(31U);
    LosX64BootstrapSerialWriteChar('[');
    LosX64BootstrapSerialWriteChar('F');
    LosX64BootstrapSerialWriteChar('A');
    LosX64BootstrapSerialWriteChar('I');
    LosX64BootstrapSerialWriteChar('L');
    LosX64BootstrapSerialWriteChar(']');
    LosX64BootstrapSerialWriteAnsiColor(0U);
    LosX64BootstrapSerialWriteChar(' ');
}

static inline void LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapSerialWriteHex64(UINT64 Value)
{
    int Shift;
    UINT8 Nibble;
    char Character;

    LosX64BootstrapSerialWriteChar('0');
    LosX64BootstrapSerialWriteChar('x');
    for (Shift = 60; Shift >= 0; Shift -= 4)
    {
        Nibble = (UINT8)((Value >> (UINTN)Shift) & 0xFULL);
        Character = (Nibble < 10U) ? (char)('0' + Nibble) : (char)('A' + (Nibble - 10U));
        LosX64BootstrapSerialWriteChar(Character);
    }
}

static inline void LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapSerialWriteLineHex64(const char *Prefix, UINT64 Value)
{
    LosX64BootstrapSerialWriteText(Prefix);
    LosX64BootstrapSerialWriteHex64(Value);
    LosX64BootstrapSerialWriteChar('\n');
}

static inline void LOS_X64_BOOTSTRAP_SECTION LosX64BootstrapHaltForever(void)
{
    for (;;)
    {
        __asm__ __volatile__("cli; hlt");
    }
}

typedef struct
{
    UINT64 BootstrapIdentityBase;
    UINT64 BootstrapIdentitySize;
    UINT64 HigherHalfDirectMapBase;
    UINT64 HigherHalfDirectMapSize;
    UINT64 KernelWindowBase;
    UINT64 KernelWindowSize;
    UINT64 KernelStackBase;
    UINT64 KernelStackSize;
    UINT64 KernelStackTop;
    UINT64 HighestDiscoveredPhysicalAddress;
} LOS_X64_VIRTUAL_MEMORY_LAYOUT;

void LosX64InitializeVirtualMemoryState(const LOS_BOOT_CONTEXT *BootContext);
void LosX64BuildPhysicalMemoryState(const LOS_BOOT_CONTEXT *BootContext);
void LosX64BuildVirtualMemoryPolicy(const LOS_BOOT_CONTEXT *BootContext);
void LosX64TakeVirtualMemoryOwnership(void);
const LOS_X64_VIRTUAL_MEMORY_LAYOUT *LosX64GetVirtualMemoryLayout(void);

UINT64 *LosX64GetPageMapLevel4(void);
UINT64 *LosX64GetIdentityDirectoryPointer(void);
UINT64 *LosX64GetIdentityDirectory(void);
UINT64 *LosX64GetKernelWindowDirectoryPointer(void);
UINT64 *LosX64GetKernelWindowDirectory(void);
UINT64 *LosX64AllocatePageTablePage(void);
void *LosX64GetBootstrapTransitionStackTop(void);
UINT64 LosX64GetBootstrapTransitionStackBase(void);
UINT64 LosX64GetBootstrapTransitionStackSize(void);
UINT64 LosX64GetBootstrapPageTableStorageBase(void);
UINT64 LosX64GetBootstrapPageTableStorageSize(void);
UINT64 LosX64GetKernelStackBackingBase(void);
UINT64 LosX64GetKernelStackBackingSize(void);
BOOLEAN LosX64InstallKernelStackMapping(void);
void LosX64ValidateKernelStackMappingOrHalt(void);
UINTN LosX64GetPageTablePoolUsedCount(void);

UINTN LosX64GetPhysicalMemoryDescriptorCount(void);
const LOS_X64_PHYSICAL_MEMORY_DESCRIPTOR *LosX64GetPhysicalMemoryDescriptor(UINTN Index);
BOOLEAN LosX64IsPhysicalRangeDiscovered(UINT64 PhysicalAddress, UINT64 Length);
BOOLEAN LosX64IsPhysicalRangeDirectMapCandidate(UINT64 PhysicalAddress, UINT64 Length);
BOOLEAN LosX64TryTranslateKernelVirtualToPhysical(UINT64 VirtualAddress, UINT64 *PhysicalAddress);
UINT64 LosX64GetCurrentPageMapLevel4PhysicalAddress(void);
void *LosX64GetDirectMapVirtualAddress(UINT64 PhysicalAddress, UINT64 Length);
BOOLEAN LosX64MapVirtualRangeUnchecked(UINT64 VirtualAddress, UINT64 PhysicalAddress, UINTN PageCount, UINT64 Flags);

extern char __LosKernelBootstrapEnd[];
extern char __LosKernelHigherHalfTextStart[];
extern char __LosKernelHigherHalfTextEnd[];
extern char __LosKernelHigherHalfTextLoadStart[];
extern char __LosKernelHigherHalfTextLoadEnd[];
extern char __LosKernelHigherHalfDataStart[];
extern char __LosKernelHigherHalfDataEnd[];
extern char __LosKernelHigherHalfDataLoadStart[];
extern char __LosKernelHigherHalfDataLoadEnd[];
extern char __LosKernelHigherHalfBssStart[];
extern char __LosKernelHigherHalfBssEnd[];
extern char __LosKernelHigherHalfBssLoadStart[];
extern char __LosKernelHigherHalfBssLoadEnd[];

#endif
