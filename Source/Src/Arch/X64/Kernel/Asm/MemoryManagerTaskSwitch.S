.intel_syntax noprefix
.section .text, "ax"

.global LosMemoryManagerBootstrapTransferToServiceTask
.type LosMemoryManagerBootstrapTransferToServiceTask, @function

LosMemoryManagerBootstrapTransferToServiceTask:
    push r15
    push r14
    mov r14, rsp
    mov r15, cr3
    mov cr3, rdi
    mov rax, rsi
    mov rsp, rdx
    and rsp, -16
    sub rsp, 8
    mov rdi, rcx
    mov rsi, rcx
    mov rdx, rcx
    call rax
    mov cr3, r15
    mov rsp, r14
    pop r14
    pop r15
    ret
