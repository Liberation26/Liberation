#ifndef LOS_KERNEL_SCHEDULER_H
#define LOS_KERNEL_SCHEDULER_H

#include "KernelMain.h"

#define LOS_KERNEL_SCHEDULER_MAX_TASKS 8U
#define LOS_KERNEL_SCHEDULER_MAX_PROCESSES 8U
#define LOS_KERNEL_SCHEDULER_INVALID_TASK_ID 0ULL
#define LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID 0ULL

#define LOS_KERNEL_SCHEDULER_TASK_STATE_UNUSED 0U
#define LOS_KERNEL_SCHEDULER_TASK_STATE_READY 1U
#define LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING 2U
#define LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED 3U
#define LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED 4U

#define LOS_KERNEL_SCHEDULER_TASK_FLAG_IDLE 0x00000001U
#define LOS_KERNEL_SCHEDULER_TASK_FLAG_PERIODIC 0x00000002U

#define LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED 0U
#define LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY 1U
#define LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED 2U

#define LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL 0x00000001U
#define LOS_KERNEL_SCHEDULER_PROCESS_FLAG_TRANSIENT 0x00000002U
#define LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT 0x00000004U

#define LOS_KERNEL_SCHEDULER_BLOCK_REASON_NONE 0U
#define LOS_KERNEL_SCHEDULER_BLOCK_REASON_WAIT_PERIOD 1U
#define LOS_KERNEL_SCHEDULER_BLOCK_REASON_YIELD 2U
#define LOS_KERNEL_SCHEDULER_BLOCK_REASON_TERMINATED 3U
#define LOS_KERNEL_SCHEDULER_BLOCK_REASON_PREEMPTED 4U

#define LOS_KERNEL_SCHEDULER_SIGNATURE 0x52454C5544454843ULL
#define LOS_KERNEL_SCHEDULER_VERSION 5U

typedef void (*LOS_KERNEL_SCHEDULER_THREAD_ROUTINE)(void *Context);

typedef struct
{
    UINT64 StackPointer;
    UINT64 Rbx;
    UINT64 Rbp;
    UINT64 R12;
    UINT64 R13;
    UINT64 R14;
    UINT64 R15;
    UINT64 Rflags;
} LOS_KERNEL_SCHEDULER_CONTEXT;

typedef struct
{
    UINT64 Signature;
    UINT32 Version;
    UINT32 State;
    UINT64 ProcessId;
    UINT64 OwnerProcessId;
    UINT64 Generation;
    const char *Name;
    UINT32 Flags;
    UINT32 ThreadCount;
    UINT64 AddressSpaceId;
    UINT64 RootTablePhysicalAddress;
    UINT64 CreatedTick;
    UINT64 TerminatedTick;
    UINT64 ExitStatus;
    UINT32 CleanupPending;
    UINT32 Reserved0;
} LOS_KERNEL_SCHEDULER_PROCESS;

typedef struct
{
    UINT64 Signature;
    UINT32 Version;
    UINT32 State;
    UINT64 TaskId;
    UINT64 OwnerTaskId;
    UINT64 ProcessId;
    UINT64 Generation;
    const char *Name;
    UINT32 Flags;
    UINT32 Priority;
    UINT32 QuantumTicks;
    UINT32 RemainingQuantumTicks;
    UINT64 WakePeriodTicks;
    UINT64 NextWakeTick;
    UINT64 DispatchCount;
    UINT64 TotalTicks;
    UINT64 LastRunTick;
    UINT64 LastBlockReason;
    UINT64 ReadySinceTick;
    UINT64 PreemptionCount;
    UINT64 ExitStatus;
    UINT32 CleanupPending;
    UINT32 BootstrapStackSlot;
    LOS_KERNEL_SCHEDULER_THREAD_ROUTINE ThreadRoutine;
    void *Context;
    LOS_KERNEL_SCHEDULER_CONTEXT ExecutionContext;
    UINT64 StackPhysicalAddress;
    UINT64 StackBaseVirtualAddress;
    UINT64 StackTopVirtualAddress;
    UINT64 StackSizeBytes;
} LOS_KERNEL_SCHEDULER_TASK;

typedef struct
{
    UINT64 Signature;
    UINT32 Version;
    UINT32 Online;
    UINT64 TickCount;
    UINT64 DispatchCount;
    UINT64 CurrentTaskId;
    UINT64 CurrentProcessId;
    UINT64 NextTaskId;
    UINT64 NextProcessId;
    UINT64 KernelProcessId;
    UINT32 TaskCount;
    UINT32 ProcessCount;
    UINT32 CurrentTaskIndex;
    UINT32 LastSelectedIndex;
    UINT32 ReschedulePending;
    UINT32 InScheduler;
    UINT32 Reserved0;
    UINT64 InterruptPreemptionCount;
    UINT64 StarvationReliefDispatchCount;
    UINT64 CreatedTaskCount;
    UINT64 TerminatedTaskCount;
    UINT64 ReapedTaskCount;
    UINT64 CreatedProcessCount;
    UINT64 TerminatedProcessCount;
    UINT64 ReapedProcessCount;
    UINT64 ActiveRootTablePhysicalAddress;
    UINT64 AddressSpaceSwitchCount;
    UINT64 AddressSpaceReuseCount;
    LOS_KERNEL_SCHEDULER_CONTEXT SchedulerContext;
    LOS_KERNEL_SCHEDULER_PROCESS Processes[LOS_KERNEL_SCHEDULER_MAX_PROCESSES];
    LOS_KERNEL_SCHEDULER_TASK Tasks[LOS_KERNEL_SCHEDULER_MAX_TASKS];
} LOS_KERNEL_SCHEDULER_STATE;

void LosKernelSchedulerInitialize(void);
void LosKernelSchedulerRegisterBootstrapTasks(void);
void LosKernelSchedulerEnter(void);
void LosKernelSchedulerOnTimerTick(void);
void LosKernelSchedulerRequestReschedule(void);
void LosKernelSchedulerPreemptIfNeededFromInterrupt(void);
BOOLEAN LosKernelSchedulerIsOnline(void);
UINT64 LosKernelSchedulerGetTickCount(void);
const LOS_KERNEL_SCHEDULER_TASK *LosKernelSchedulerGetCurrentTask(void);
const LOS_KERNEL_SCHEDULER_STATE *LosKernelSchedulerGetState(void);

#endif
