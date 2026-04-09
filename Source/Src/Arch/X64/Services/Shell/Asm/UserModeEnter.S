/*
 * File Name: UserModeEnter.S
 * File Version: 0.4.33
 * Author: OpenAI
 * Creation Timestamp: 2026-04-09T17:15:00Z
 * Last Update Timestamp: 2026-04-09T17:15:00Z
 * Operating System Name: Liberation OS
 * Purpose: Provides the hardened shell-side ring-3 transition entry/return stubs.
 */

    .text
    .global LosShellEnterUserModeAsm
    .global LosShellUserModeReturnAsm

    /* LOS_USER_IMAGE_RING3_CONTEXT offsets */
    .set LOS_RING3_VERSION,                 0
    .set LOS_RING3_FLAGS,                   4
    .set LOS_RING3_USER_RIP,                8
    .set LOS_RING3_USER_RSP,                16
    .set LOS_RING3_USER_ARG,                24
    .set LOS_RING3_USER_CR3,                32
    .set LOS_RING3_USER_CS,                 40
    .set LOS_RING3_USER_DS,                 48
    .set LOS_RING3_USER_RFLAGS,             56
    .set LOS_RING3_KERNEL_RESUME_RIP,       64
    .set LOS_RING3_KERNEL_RESUME_RSP,       72
    .set LOS_RING3_COMPLETION_STATUS_ADDR,  80
    .set LOS_RING3_COMPLETION_RESULT_VALUE, 88

    .extern LosShellStageUserModeCompletion

LosShellEnterUserModeAsm:
    /* rdi -> LOS_USER_IMAGE_RING3_CONTEXT */
    mov $8, %rax
    test %rdi, %rdi
    jz .Lreturn

    mov LOS_RING3_USER_RIP(%rdi), %r8
    mov LOS_RING3_USER_RSP(%rdi), %r9
    mov LOS_RING3_USER_ARG(%rdi), %r10
    mov LOS_RING3_USER_CR3(%rdi), %r11
    mov LOS_RING3_USER_CS(%rdi), %r12
    mov LOS_RING3_USER_DS(%rdi), %r13
    mov LOS_RING3_USER_RFLAGS(%rdi), %r14

    test %r8, %r8
    jz .Lfail
    test %r9, %r9
    jz .Lfail
    test %r10, %r10
    jz .Lfail
    test %r11, %r11
    jz .Lfail
    test %r12, %r12
    jz .Lfail
    test %r13, %r13
    jz .Lfail

    /* Require 16-byte user stack alignment */
    test $0xF, %r9
    jnz .Lfail

    mov %r11, %cr3

    /* rdi becomes first argument to user entry */
    mov %r10, %rdi

    pushq %r13        /* SS    */
    pushq %r9         /* RSP   */
    pushq %r14        /* RFLAGS*/
    pushq %r12        /* CS    */
    pushq %r8         /* RIP   */
    xor %rax, %rax
    iretq

.Lfail:
    mov $8, %rax
.Lreturn:
    ret

LosShellUserModeReturnAsm:
    /* Kernel-side staged completion return helper */
    xor %rsi, %rsi
    xor %rdi, %rdi
    call LosShellStageUserModeCompletion
    xor %rax, %rax
    ret
