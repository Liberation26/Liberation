.intel_syntax noprefix
.section .text

.global LosCommonInterruptEntry
.extern LosX64HandleInterrupt

.macro PUSH_REGS
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax
.endm

.macro POP_REGS
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
.endm

LosCommonInterruptEntry:
    cld
    PUSH_REGS
    mov rdi, rsp
    mov rsi, [rsp + 120]
    mov rdx, [rsp + 128]
    lea rcx, [rsp + 136]
    call LosX64HandleInterrupt
    POP_REGS
    add rsp, 16
    iretq

.macro ISR_NOERR num
    .global LosInterruptStub\num
LosInterruptStub\num:
    push 0
    push \num
    jmp LosCommonInterruptEntry
.endm

.macro ISR_ERR num
    .global LosInterruptStub\num
LosInterruptStub\num:
    push \num
    jmp LosCommonInterruptEntry
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

ISR_NOERR 32

ISR_NOERR 128
