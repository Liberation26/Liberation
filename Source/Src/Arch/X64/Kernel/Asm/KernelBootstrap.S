.intel_syntax noprefix
.section .bootstrap.text, "ax"

.global LosKernelBootstrapEntry
.extern LosX64InitializeVirtualMemory
.extern LosX64GetKernelStackTop
.extern LosX64ValidateKernelStackMappingOrHalt
.extern LosKernelHigherHalfMain
.extern LosX64InstallBootstrapTrapReporter
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
    call LosX64InstallBootstrapTrapReporter
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
