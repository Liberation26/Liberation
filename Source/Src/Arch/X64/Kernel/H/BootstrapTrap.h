#ifndef LOS_X64_BOOTSTRAP_TRAP_H
#define LOS_X64_BOOTSTRAP_TRAP_H

#include "KernelMain.h"

typedef struct
{
    UINT64 R15;
    UINT64 R14;
    UINT64 R13;
    UINT64 R12;
    UINT64 R11;
    UINT64 R10;
    UINT64 R9;
    UINT64 R8;
    UINT64 Rdi;
    UINT64 Rsi;
    UINT64 Rbp;
    UINT64 Rdx;
    UINT64 Rcx;
    UINT64 Rbx;
    UINT64 Rax;
} LOS_X64_BOOTSTRAP_REGISTER_STATE;

typedef struct __attribute__((packed))
{
    UINT64 Rip;
    UINT64 Cs;
    UINT64 Rflags;
} LOS_X64_BOOTSTRAP_INTERRUPT_FRAME;

void LosX64InstallBootstrapTrapReporter(void);

#endif
