.intel_syntax noprefix
.section .text, "ax"

.global LosKernelSchedulerSwitchContext
.type LosKernelSchedulerSwitchContext, @function

LosKernelSchedulerSwitchContext:
    mov [rdi + 0], rsp
    mov [rdi + 8], rbx
    mov [rdi + 16], rbp
    mov [rdi + 24], r12
    mov [rdi + 32], r13
    mov [rdi + 40], r14
    mov [rdi + 48], r15
    mov [rdi + 56], rax
    mov [rdi + 64], rcx
    mov [rdi + 72], rdx
    mov [rdi + 80], rsi
    mov [rdi + 88], rdi
    mov [rdi + 96], r8
    mov [rdi + 104], r9
    mov [rdi + 112], r10
    mov [rdi + 120], r11
    pushfq
    pop rax
    mov [rdi + 128], rax
    cli

    mov rdx, rsi
    mov rsp, [rdx + 0]
    mov rbx, [rdx + 8]
    mov rbp, [rdx + 16]
    mov r12, [rdx + 24]
    mov r13, [rdx + 32]
    mov r14, [rdx + 40]
    mov r15, [rdx + 48]
    mov rax, [rdx + 56]
    mov rcx, [rdx + 64]
    mov rsi, [rdx + 80]
    mov rdi, [rdx + 88]
    mov r8, [rdx + 96]
    mov r9, [rdx + 104]
    mov r10, [rdx + 112]
    mov r11, [rdx + 120]
    push qword ptr [rdx + 128]
    popfq
    mov rdx, [rdx + 72]
    ret
