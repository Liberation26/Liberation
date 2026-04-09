/*
 * File Name: SchedulerUserTransitionBridge.S
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-07T12:35:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

.intel_syntax noprefix
.section .text, "ax"

.global LosKernelSchedulerUserTransitionDispatchBridge
.type LosKernelSchedulerUserTransitionDispatchBridge, @function

.extern LosKernelSchedulerPrepareUserTransitionIret

LosKernelSchedulerUserTransitionDispatchBridge:
    call LosKernelSchedulerPrepareUserTransitionIret
    test rax, rax
    jz .Lhang
    mov rsp, rax
    iretq

.Lhang:
    cli
    hlt
    jmp .Lhang
