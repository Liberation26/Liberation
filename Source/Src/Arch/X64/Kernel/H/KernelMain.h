#ifndef LOS_KERNEL_MAIN_H
#define LOS_KERNEL_MAIN_H

#include "Efi.h"

#define LOS_BOOT_CONTEXT_TEXT_CHARACTERS 128U
#define LOS_BOOT_CONTEXT_SIGNATURE 0x544F4F424F534F4CULL
#define LOS_BOOT_CONTEXT_VERSION 7U
#define LOS_BOOT_CONTEXT_FLAG_MEMORY_MANAGER_IMAGE_VALID 0x0000000000000004ULL
#define LOS_BOOT_CONTEXT_FLAG_MONITOR_HANDOFF_ONLY 0x0000000000000001ULL
#define LOS_BOOT_CONTEXT_FLAG_KERNEL_SEGMENTS_VALID 0x0000000000000002ULL
#define LOS_BOOT_CONTEXT_MAX_LOAD_SEGMENTS 8U
#define LOS_ELF_PROGRAM_FLAG_EXECUTE 0x1U
#define LOS_ELF_PROGRAM_FLAG_WRITE 0x2U
#define LOS_ELF_PROGRAM_FLAG_READ 0x4U
#define LOS_X64_GDT_ENTRY_COUNT 7U
#define LOS_X64_KERNEL_CODE_SELECTOR 0x08U
#define LOS_X64_KERNEL_DATA_SELECTOR 0x10U
#define LOS_X64_USER_CODE_SELECTOR 0x1BU
#define LOS_X64_USER_DATA_SELECTOR 0x23U
#define LOS_X64_TSS_SELECTOR 0x28U

typedef struct
{
    UINT64 VirtualAddress;
    UINT64 PhysicalAddress;
    UINT64 FileSize;
    UINT64 MemorySize;
    UINT64 Flags;
    UINT64 Reserved;
} LOS_BOOT_CONTEXT_LOAD_SEGMENT;

typedef struct
{
    UINT64 Signature;
    UINT32 Version;
    UINT32 Reserved;
    UINT64 Flags;
    UINT64 BootContextAddress;
    UINT64 BootContextSize;
    UINT64 KernelImagePhysicalAddress;
    UINT64 KernelImageSize;
    UINT64 MemoryMapAddress;
    UINT64 MemoryMapSize;
    UINT64 MemoryMapBufferSize;
    UINT64 MemoryMapDescriptorSize;
    UINT64 MemoryMapDescriptorVersion;
    UINT64 MemoryRegionCount;
    UINT64 FrameBufferPhysicalAddress;
    UINT64 FrameBufferSize;
    UINT32 FrameBufferWidth;
    UINT32 FrameBufferHeight;
    UINT32 FrameBufferPixelsPerScanLine;
    UINT32 FrameBufferPixelFormat;
    UINT64 KernelFontPhysicalAddress;
    UINT64 KernelFontSize;
    UINT64 MemoryManagerImagePhysicalAddress;
    UINT64 MemoryManagerImageSize;
    UINT64 KernelLoadSegmentCount;
    LOS_BOOT_CONTEXT_LOAD_SEGMENT KernelLoadSegments[LOS_BOOT_CONTEXT_MAX_LOAD_SEGMENTS];
    CHAR16 BootSourceText[LOS_BOOT_CONTEXT_TEXT_CHARACTERS];
    CHAR16 KernelPartitionText[LOS_BOOT_CONTEXT_TEXT_CHARACTERS];
} LOS_BOOT_CONTEXT;

void LosKernelSerialInit(void);
void LosKernelSerialWriteText(const char *Text);
void LosKernelSerialWriteUnsigned(UINT64 Value);
void LosKernelSerialWriteHex64(UINT64 Value);
void LosKernelSerialWriteUtf16(const CHAR16 *Text);
void LosKernelTrace(const char *Text);
void LosKernelTraceOk(const char *Text);
void LosKernelTraceFail(const char *Text);
void LosKernelTraceHex64(const char *Prefix, UINT64 Value);
void LosKernelTraceUnsigned(const char *Prefix, UINT64 Value);
void LosKernelInitializeScreen(const LOS_BOOT_CONTEXT *BootContext);
void LosKernelStatusScreenWriteOk(const char *Text);
void LosKernelStatusScreenWriteFail(const char *Text);
void LosKernelScreenUpdateSpinner(UINT64 TickCount);
void LosKernelScreenUpdateTimer(UINT64 TickCount, UINT64 TargetHz, BOOLEAN InterruptsLive);
void LosKernelHaltForever(void);
void LosKernelEnableInterrupts(void);
void LosKernelIdleLoop(void);
void LosKernelAnnounceFunction(const char *FunctionName);
void LosKernelSetInterruptStackTop(UINT64 StackTop);

extern UINT64 LosGdt[LOS_X64_GDT_ENTRY_COUNT];
const void *LosKernelGetGdtBase(void);
UINT64 LosKernelGetGdtSize(void);
extern volatile UINT64 LosKernelRuntimeTracingEnabled;
#define LOS_KERNEL_ENTER() do { if (LosKernelRuntimeTracingEnabled != 0ULL) { LosKernelAnnounceFunction(__func__); } } while (0)

void LosKernelHigherHalfMain(const LOS_BOOT_CONTEXT *BootContext);

#endif
