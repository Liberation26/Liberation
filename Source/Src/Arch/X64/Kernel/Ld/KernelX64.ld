OUTPUT_FORMAT(elf64-x86-64)
ENTRY(LosKernelBootstrapEntry)

KERNEL_PHYS_BASE = 0x00100000;
KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;
PAGE_SIZE = 0x1000;

SECTIONS
{
    . = KERNEL_PHYS_BASE;

    .bootstrap.text : ALIGN(PAGE_SIZE)
    {
        *(.bootstrap.text .bootstrap.text.*)
        *(.bootstrap.rodata .bootstrap.rodata.*)
    }

    .bootstrap.data : ALIGN(PAGE_SIZE)
    {
        *(.bootstrap.data .bootstrap.data.*)
        *(.bootstrap.bss .bootstrap.bss.*)
    }
    __LosKernelBootstrapEnd = ALIGN(PAGE_SIZE);

    . = KERNEL_VIRT_BASE;

    .text : AT(((LOADADDR(.bootstrap.data) + SIZEOF(.bootstrap.data) + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE) ALIGN(PAGE_SIZE)
    {
        __LosKernelHigherHalfTextStart = .;
        *(.text .text.*)
        *(.ltext .ltext.*)
        *(.rodata .rodata.*)
        *(.lrodata .lrodata.*)
        __LosKernelHigherHalfTextEnd = .;
    }
    __LosKernelHigherHalfTextLoadStart = LOADADDR(.text);
    __LosKernelHigherHalfTextLoadEnd = LOADADDR(.text) + SIZEOF(.text);

    .data : AT(((LOADADDR(.text) + SIZEOF(.text) + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE) ALIGN(PAGE_SIZE)
    {
        __LosKernelHigherHalfDataStart = .;
        *(.data .data.*)
        *(.ldata .ldata.*)
        __LosKernelHigherHalfDataEnd = .;
    }
    __LosKernelHigherHalfDataLoadStart = LOADADDR(.data);
    __LosKernelHigherHalfDataLoadEnd = LOADADDR(.data) + SIZEOF(.data);

    .bss : AT(((LOADADDR(.data) + SIZEOF(.data) + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE) ALIGN(PAGE_SIZE)
    {
        __LosKernelHigherHalfBssStart = .;
        *(.bss .bss.*)
        *(.lbss .lbss.*)
        *(COMMON)
        __LosKernelHigherHalfBssEnd = .;
    }
    __LosKernelHigherHalfBssLoadStart = LOADADDR(.bss);
    __LosKernelHigherHalfBssLoadEnd = LOADADDR(.bss) + SIZEOF(.bss);

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.llvm_addrsig)
    }
}
