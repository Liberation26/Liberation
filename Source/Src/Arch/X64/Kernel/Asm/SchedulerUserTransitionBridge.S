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
