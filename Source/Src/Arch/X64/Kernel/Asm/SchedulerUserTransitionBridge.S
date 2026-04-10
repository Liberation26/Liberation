/*
 * File Name: SchedulerUserTransitionBridge.S
 * File Version: 0.3.12
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-10T18:45:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

.intel_syntax noprefix
.section .text, "ax"

.global LosKernelSchedulerUserTransitionDispatchBridge
.type LosKernelSchedulerUserTransitionDispatchBridge, @function

.extern LosKernelSchedulerPrepareUserTransitionIret

LosKernelSchedulerUserTransitionDispatchBridge:
    mov rbx, rsp
    and rsp, -16
    call LosKernelSchedulerPrepareUserTransitionIret
    mov rsp, rbx
    test rax, rax
    jz .Lhang
    mov rsp, rax
    iretq

.Lhang:
    cli
    hlt
    jmp .Lhang
