/*
 * File Name: CapabilitiesX64.ld
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T09:50:51Z
 * Last Update Timestamp: 2026-04-07T12:35:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

ENTRY(LosCapabilitiesServiceBootstrapEntry)

SECTIONS
{
    . = 0x0000000000410000;

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
