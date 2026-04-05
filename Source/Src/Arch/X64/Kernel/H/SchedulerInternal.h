#ifndef LOS_KERNEL_SCHEDULER_INTERNAL_H
#define LOS_KERNEL_SCHEDULER_INTERNAL_H

#include "Scheduler.h"
#include "VirtualMemory.h"

#define LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX 0xFFFFFFFFU
#define LOS_KERNEL_SCHEDULER_IDLE_PRIORITY 0U
#define LOS_KERNEL_SCHEDULER_HEARTBEAT_PRIORITY 4U
#define LOS_KERNEL_SCHEDULER_DEFAULT_QUANTUM_TICKS 1U
#define LOS_KERNEL_SCHEDULER_HEARTBEAT_PERIOD_TICKS 100ULL
#define LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES 4ULL
#define LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES (LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES * 4096ULL)

LOS_KERNEL_SCHEDULER_STATE *LosKernelSchedulerState(void);
BOOLEAN LosKernelSchedulerCreateTask(
    const char *Name,
    UINT32 Flags,
    UINT32 Priority,
    UINT32 QuantumTicks,
    UINT64 WakePeriodTicks,
    LOS_KERNEL_SCHEDULER_THREAD_ROUTINE ThreadRoutine,
    void *Context,
    UINT64 *TaskId);
void LosKernelSchedulerWakeDueTasks(void);
UINT32 LosKernelSchedulerSelectNextTaskIndex(void);
void LosKernelSchedulerTraceState(const char *Prefix);
void LosKernelSchedulerTraceTask(const char *Prefix, const LOS_KERNEL_SCHEDULER_TASK *Task);
void LosKernelSchedulerIdleThread(void *Context);
void LosKernelSchedulerHeartbeatThread(void *Context);
void LosKernelSchedulerThreadTrampoline(void);
void LosKernelSchedulerYieldCurrent(void);
void LosKernelSchedulerSleepCurrent(UINT64 TickCount);
void LosKernelSchedulerTerminateCurrent(void);
void LosKernelSchedulerSwitchContext(LOS_KERNEL_SCHEDULER_CONTEXT *From, const LOS_KERNEL_SCHEDULER_CONTEXT *To);

#endif
