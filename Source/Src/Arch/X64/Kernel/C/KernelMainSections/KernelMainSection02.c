/*
 * File Name: KernelMainSection02.c
 * File Version: 0.0.3
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-10T20:25:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from KernelMain.c.
 */

void LosKernelIdleLoop(void)
{
    LosKernelSchedulerEnter();
}

static UINT64 BuildGdtEntry(UINT32 Base, UINT32 Limit, UINT8 Access, UINT8 Granularity)
{
    UINT64 Entry;

    Entry = 0ULL;
    Entry |= (UINT64)(Limit & 0xFFFFU);
    Entry |= ((UINT64)(Base & 0xFFFFU)) << 16;
    Entry |= ((UINT64)((Base >> 16) & 0xFFU)) << 32;
    Entry |= ((UINT64)Access) << 40;
    Entry |= ((UINT64)((Limit >> 16) & 0x0FU)) << 48;
    Entry |= ((UINT64)(Granularity & 0xF0U)) << 48;
    Entry |= ((UINT64)((Base >> 24) & 0xFFU)) << 56;
    return Entry;
}

static void BuildTssDescriptor(UINT64 Base, UINT32 Limit, UINT64 *LowEntry, UINT64 *HighEntry)
{
    UINT64 DescriptorLow;

    DescriptorLow = 0ULL;
    DescriptorLow |= (UINT64)(Limit & 0xFFFFU);
    DescriptorLow |= (Base & 0xFFFFULL) << 16;
    DescriptorLow |= ((Base >> 16) & 0xFFULL) << 32;
    DescriptorLow |= ((UINT64)LOS_GDT_TSS_ACCESS) << 40;
    DescriptorLow |= (((UINT64)Limit >> 16) & 0x0FULL) << 48;
    DescriptorLow |= ((Base >> 24) & 0xFFULL) << 56;

    if (LowEntry != 0)
    {
        *LowEntry = DescriptorLow;
    }
    if (HighEntry != 0)
    {
        *HighEntry = (Base >> 32) & 0xFFFFFFFFULL;
    }
}

void LosKernelSetInterruptStackTop(UINT64 StackTop)
{
    if (StackTop != 0ULL)
    {
        LosKernelTss.Rsp0 = StackTop;
    }
}

static void InstallGdt(void)
{
    LOS_KERNEL_ENTER();
    UINT16 TssSelector;

    LosGdt[0] = 0ULL;
    LosGdt[1] = BuildGdtEntry(0U, 0xFFFFFU, LOS_GDT_CODE_FLAGS, LOS_GDT_GRANULARITY);
    LosGdt[2] = BuildGdtEntry(0U, 0xFFFFFU, LOS_GDT_DATA_FLAGS, LOS_GDT_GRANULARITY);
    LosGdt[3] = BuildGdtEntry(0U, 0xFFFFFU, LOS_GDT_USER_CODE_FLAGS, LOS_GDT_GRANULARITY);
    LosGdt[4] = BuildGdtEntry(0U, 0xFFFFFU, LOS_GDT_USER_DATA_FLAGS, LOS_GDT_GRANULARITY);
    for (TssSelector = 5U; TssSelector < LOS_X64_GDT_ENTRY_COUNT; ++TssSelector)
    {
        LosGdt[TssSelector] = 0ULL;
    }

    LosKernelTss.Reserved0 = 0U;
    LosKernelTss.Rsp0 = (UINT64)(UINTN)LosX64GetKernelStackTop();
    LosKernelTss.Rsp1 = 0ULL;
    LosKernelTss.Rsp2 = 0ULL;
    LosKernelTss.Reserved1 = 0ULL;
    LosKernelTss.Ist1 = 0ULL;
    LosKernelTss.Ist2 = 0ULL;
    LosKernelTss.Ist3 = 0ULL;
    LosKernelTss.Ist4 = 0ULL;
    LosKernelTss.Ist5 = 0ULL;
    LosKernelTss.Ist6 = 0ULL;
    LosKernelTss.Ist7 = 0ULL;
    LosKernelTss.Reserved2 = 0ULL;
    LosKernelTss.Reserved3 = 0U;
    LosKernelTss.IoMapBase = (UINT16)sizeof(LosKernelTss);
    BuildTssDescriptor((UINT64)(UINTN)&LosKernelTss, (UINT32)(sizeof(LosKernelTss) - 1U), &LosGdt[5], &LosGdt[6]);

    LosGdtPointer.Limit = (UINT16)(sizeof(LosGdt) - 1U);
    LosGdtPointer.Base = (UINT64)(UINTN)&LosGdt[0];

    __asm__ __volatile__("lgdt %0" : : "m"(LosGdtPointer));
    __asm__ __volatile__(
        "pushq %0\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw %1, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        :
        : "i"((UINT64)LOS_X64_KERNEL_CODE_SELECTOR),
          "i"((UINT16)LOS_X64_KERNEL_DATA_SELECTOR)
        : "rax", "memory");

    TssSelector = (UINT16)LOS_X64_TSS_SELECTOR;
    __asm__ __volatile__("ltr %0" : : "rm"(TssSelector) : "memory");
}

const void *LosKernelGetGdtBase(void)
{
    LOS_KERNEL_ENTER();
    return &LosGdt[0];
}

UINT64 LosKernelGetGdtSize(void)
{
    LOS_KERNEL_ENTER();
    return (UINT64)sizeof(LosGdt);
}

const LOS_BOOT_CONTEXT *LosKernelGetBootContext(void)
{
    return LosKernelBootContext;
}

static UINT64 CountMemoryRegions(const LOS_BOOT_CONTEXT *BootContext)
{
    if (BootContext == 0 || BootContext->MemoryMapDescriptorSize == 0ULL)
    {
        return 0ULL;
    }

    return BootContext->MemoryMapSize / BootContext->MemoryMapDescriptorSize;
}

void LosKernelHigherHalfMain(const LOS_BOOT_CONTEXT *BootContext)
{
    LosKernelBootContext = BootContext;
    LosKernelSerialInit();
    LosKernelInitializeScreen(BootContext);
    LosDiagnosticsInitialize();
    LOS_KERNEL_ENTER();
    LosKernelSerialWriteText("Liberation Kernel\n");
    LosKernelTraceOk("Higher-half kernel entry active.");

    InstallGdt();
    LosKernelTraceOk("Minimal GDT installed.");

    LosX64InstallInterrupts();
    LosKernelTraceOk("Vector-specific exception IDT installed.");

    LosX64InitializeTimer();
    LosKernelTraceOk("Programmable interval timer started at 100 Hz.");

    LosKernelTraceOk("ExitBootServices complete. Kernel owns firmware memory map.");
    LosKernelSerialWriteText("[Kernel] The EFI monitor was handoff-only and does not remain a live UEFI application.\n");
    LosKernelSerialWriteText("[Kernel] Boot source: ");
    LosKernelSerialWriteUtf16(BootContext->BootSourceText);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] ");
    LosKernelSerialWriteUtf16(BootContext->KernelPartitionText);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Memory map bytes: ");
    LosKernelSerialWriteUnsigned(BootContext->MemoryMapSize);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Memory descriptor size: ");
    LosKernelSerialWriteUnsigned(BootContext->MemoryMapDescriptorSize);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Memory regions: ");
    LosKernelSerialWriteUnsigned(CountMemoryRegions(BootContext));
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Kernel image physical base: ");
    LosKernelSerialWriteHex64(BootContext->KernelImagePhysicalAddress);
    LosKernelSerialWriteText(" bytes=");
    LosKernelSerialWriteUnsigned(BootContext->KernelImageSize);
    LosKernelSerialWriteText("\n");
    LosKernelSerialWriteText("[Kernel] Boot context version: ");
    LosKernelSerialWriteUnsigned(BootContext->Version);
    LosKernelSerialWriteText(" load segments=");
    LosKernelSerialWriteUnsigned(BootContext->KernelLoadSegmentCount);
    LosKernelSerialWriteText("\n");

    LosKernelSerialWriteText("[Kernel] Boot capabilities blocks: ");
    LosKernelSerialWriteUnsigned((UINT64)BootContext->Capabilities.BlockCount);
    LosKernelSerialWriteText("\n");

    if ((BootContext->Flags & LOS_BOOT_CONTEXT_FLAG_MONITOR_HANDOFF_ONLY) != 0ULL)
    {
        LosKernelTraceOk("Any continuing monitoring now belongs to kernel-owned code or services.");
    }

    LosX64DescribeVirtualMemoryLayout();
    LosKernelTraceOk("Kernel now owns deliberate paging structures.");
    LosKernelTraceOk("Dedicated kernel stack mapping is active.");
    LosInitializeMemoryManagerBootstrap(BootContext);
    LosLaunchMemoryManagerBootstrap();
    {
        const LOS_MEMORY_MANAGER_BOOTSTRAP_INFO *MemoryManagerBootstrapInfo;

        MemoryManagerBootstrapInfo = LosGetMemoryManagerBootstrapInfo();
        LosKernelSerialWriteText("[OK] [Kernel] Bootstrap address space created.\n");
        LosKernelTraceOk("Bootstrap address space created.");
        LosKernelTraceUnsigned("Memory-manager bootstrap address-space id: ", 1ULL);
        if (MemoryManagerBootstrapInfo != 0)
        {
            LosKernelTraceHex64("Memory-manager bootstrap address-space object: ", MemoryManagerBootstrapInfo->ServiceAddressSpaceObjectPhysicalAddress);
            LosKernelTraceHex64("Memory-manager bootstrap address-space root: ", MemoryManagerBootstrapInfo->ServicePageMapLevel4PhysicalAddress);
        }
        else
        {
            LosKernelTraceHex64("Memory-manager bootstrap address-space object: ", 0ULL);
            LosKernelTraceHex64("Memory-manager bootstrap address-space root: ", 0ULL);
        }
    }

    LosKernelSchedulerInitialize();
    LosKernelSchedulerRegisterBootstrapTasks();

    LosKernelTraceUnsigned("Timer tick count before enabling interrupts: ", LosX64GetTimerTickCount());
    LosKernelEnableInterrupts();
    LosKernelTraceOk("Interrupts enabled. Entering the scheduler; timer proof and user-shell bootstrap continue there.");
    LosKernelIdleLoop();
}
