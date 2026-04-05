.intel_syntax noprefix
.section .text, "ax"

.global LosKernelSchedulerUserTransitionDispatchBridge
.type LosKernelSchedulerUserTransitionDispatchBridge, @function

LosKernelSchedulerUserTransitionDispatchBridge:
    mov rdi, rsp
    call LosKernelSchedulerUserTransitionBridgeTrap

.Lhang:
    cli
    hlt
    jmp .Lhang
