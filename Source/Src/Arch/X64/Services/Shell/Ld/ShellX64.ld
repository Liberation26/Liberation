/*
 * File Name: ShellX64.ld
 * File Version: 0.4.0
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-08T15:00:00Z
 * Last Update Timestamp: 2026-04-08T15:00:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

ENTRY(LosShellServiceBootstrapEntry)

SECTIONS
{
    . = 0x0000000000420000;

    .text ALIGN(0x1000) :
    {
        *(.text*)
        *(.rodata*)
    }

    .data ALIGN(0x1000) :
    {
        *(.data*)
    }

    .bss ALIGN(0x1000) :
    {
        *(COMMON)
        *(.bss*)
    }
}
