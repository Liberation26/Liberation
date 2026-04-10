/*
 * File Name: KernelBootstrap.S
 * File Version: 0.3.12
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-10T20:25:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

.intel_syntax noprefix
.section .bootstrap.text, "ax"

.global LosKernelBootstrapEntry
.extern LosX64InitializeVirtualMemory
.extern LosX64GetKernelStackTop
.extern LosX64ValidateKernelStackMappingOrHalt
.extern LosKernelHigherHalfMain
.extern LosX64GetBootstrapTransitionStackTop
.extern LosX64ReportHigherHalfEntryJump

LosKernelBootstrapEntry:
    cli
    cld
    mov r12, rcx
    test r12, r12
    jnz 1f
    mov r12, rdi
1:
    call LosX64GetBootstrapTransitionStackTop
    mov rsp, rax
    xor rbp, rbp
    mov rdi, r12
    call LosX64InitializeVirtualMemory
    call LosX64ValidateKernelStackMappingOrHalt
    movabs rax, 0xFFFF800000000000
    add r12, rax
    mov rdi, r12
    call LosX64ReportHigherHalfEntryJump
    call LosX64GetKernelStackTop
    mov rsp, rax
    xor rbp, rbp
    mov rdi, r12
    .att_syntax
    movabs $LosKernelHigherHalfMain, %rax
    .intel_syntax noprefix
    jmp rax
