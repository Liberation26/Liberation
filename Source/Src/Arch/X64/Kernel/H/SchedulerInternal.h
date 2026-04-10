/*
 * File Name: SchedulerInternal.h
 * File Version: 0.3.14
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-10T19:20:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

#ifndef LOS_KERNEL_SCHEDULER_INTERNAL_H
#define LOS_KERNEL_SCHEDULER_INTERNAL_H

#include "Scheduler.h"
#include "VirtualMemory.h"

#define LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX 0xFFFFFFFFU
#define LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT 0xFFFFFFFFU
#define LOS_KERNEL_SCHEDULER_IDLE_PRIORITY 0U
#define LOS_KERNEL_SCHEDULER_HEARTBEAT_PRIORITY 4U
#define LOS_KERNEL_SCHEDULER_LIFECYCLE_PRIORITY 3U
#define LOS_KERNEL_SCHEDULER_BUSY_PRIORITY 2U
#define LOS_KERNEL_SCHEDULER_EPHEMERAL_PRIORITY 1U
#define LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_PRIORITY 1U
#define LOS_KERNEL_SCHEDULER_DEFAULT_QUANTUM_TICKS 1U
#define LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS 0ULL
#define LOS_KERNEL_SCHEDULER_AGING_INTERVAL_TICKS 100ULL
#define LOS_KERNEL_SCHEDULER_MAX_AGING_BOOST 4U
#define LOS_KERNEL_SCHEDULER_HEARTBEAT_PERIOD_TICKS 100ULL
#define LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS 25ULL
#define LOS_KERNEL_SCHEDULER_EPHEMERAL_LIFETIME_TICKS 150ULL
#define LOS_KERNEL_SCHEDULER_BUSY_REPORT_PERIOD_TICKS 500ULL
#define LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES 4ULL
#define LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES (LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES * 4096ULL)
#define LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_ENTRY_VIRTUAL_ADDRESS 0x0000000000400000ULL
#define LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_TOP_VIRTUAL_ADDRESS 0x000000007FFF0000ULL
#define LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_RFLAGS 0x0000000000000202ULL
#define LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_PAGES 4ULL
#define LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME_STACK_OFFSET_BYTES 8192ULL
#define LOS_KERNEL_SCHEDULER_USER_TRANSITION_CHAIN_STACK_OFFSET_BYTES 4096ULL

typedef struct __attribute__((packed))
{
    UINT64 Rip;
    UINT64 Cs;
    UINT64 Rflags;
    UINT64 Rsp;
    UINT64 Ss;
} LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME;

LOS_KERNEL_SCHEDULER_STATE *LosKernelSchedulerState(void);
BOOLEAN LosKernelSchedulerShouldEmitSerial(void);
void LosKernelSchedulerEmitTrace(const char *Text);
void LosKernelSchedulerEmitTraceOk(const char *Text);
void LosKernelSchedulerEmitTraceFail(const char *Text);
void LosKernelSchedulerEmitTraceHex64(const char *Prefix, UINT64 Value);
void LosKernelSchedulerEmitTraceUnsigned(const char *Prefix, UINT64 Value);

#define LosKernelTrace LosKernelSchedulerEmitTrace
#define LosKernelTraceOk LosKernelSchedulerEmitTraceOk
#define LosKernelTraceFail LosKernelSchedulerEmitTraceFail
#define LosKernelTraceHex64 LosKernelSchedulerEmitTraceHex64
#define LosKernelTraceUnsigned LosKernelSchedulerEmitTraceUnsigned

BOOLEAN LosKernelSchedulerCreateProcess(
    const char *Name,
    UINT32 Flags,
    UINT64 AddressSpaceId,
    UINT64 RootTablePhysicalAddress,
    UINT64 *ProcessId);
BOOLEAN LosKernelSchedulerMarkProcessTerminated(
    UINT64 ProcessId,
    UINT64 ExitStatus);
const LOS_KERNEL_SCHEDULER_PROCESS *LosKernelSchedulerGetCurrentProcess(void);
BOOLEAN LosKernelSchedulerHasActiveTransientProcess(void);
BOOLEAN LosKernelSchedulerPrepareUserTransitionScaffold(void);
BOOLEAN LosKernelSchedulerValidateUserTransitionScaffold(void);
BOOLEAN LosKernelSchedulerArmUserTransitionScaffold(void);
BOOLEAN LosKernelSchedulerRequestUserTransitionScaffold(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldEntryReady(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldDescriptorReady(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldFrameReady(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldTrampolineReady(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldBridgeReady(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldChainReady(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldContractReady(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldSealReady(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldHandoffReady(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldComplete(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldLive(void);
BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldLiveGateClosed(void);
BOOLEAN LosKernelSchedulerGuardUserTransitionScaffold(void);
BOOLEAN LosKernelSchedulerIsUserTransitionScaffoldBlocked(void);
void LosKernelSchedulerCleanupTerminatedProcesses(void);
void LosKernelSchedulerBindPendingProcessAddressSpaces(void);
BOOLEAN LosKernelSchedulerCreateTask(
    const char *Name,
    UINT64 ProcessId,
    UINT32 Flags,
    UINT32 Priority,
    UINT32 QuantumTicks,
    UINT64 WakePeriodTicks,
    LOS_KERNEL_SCHEDULER_THREAD_ROUTINE ThreadRoutine,
    void *Context,
    UINT64 *TaskId);
void LosKernelSchedulerWakeDueTasks(void);
void LosKernelSchedulerCleanupTerminatedTasks(void);
UINT32 LosKernelSchedulerSelectNextTaskIndex(void);
void LosKernelSchedulerActivateProcessAddressSpace(const LOS_KERNEL_SCHEDULER_PROCESS *Process);
void LosKernelSchedulerRestoreKernelAddressSpace(void);
void LosKernelSchedulerTraceProcess(const char *Prefix, const LOS_KERNEL_SCHEDULER_PROCESS *Process);
void LosKernelSchedulerTraceState(const char *Prefix);
void LosKernelSchedulerTraceTask(const char *Prefix, const LOS_KERNEL_SCHEDULER_TASK *Task);
void LosKernelSchedulerIdleThread(void *Context);
void LosKernelSchedulerHeartbeatThread(void *Context);
void LosKernelSchedulerLifecycleThread(void *Context);
void LosKernelSchedulerEphemeralThread(void *Context);
void LosKernelSchedulerUserTransitionTrapThread(void *Context);
void LosKernelSchedulerBusyThread(void *Context);
void LosKernelSchedulerThreadTrampoline(void);
void LosKernelSchedulerUserTransitionKernelEntry(void);
UINT64 LosKernelSchedulerPrepareUserTransitionIret(void);
void LosKernelSchedulerUserTransitionDispatchBridge(void);
UINT64 LosKernelSchedulerGetUserTransitionDispatchBridgeRuntimeAddress(void);
UINT64 LosKernelSchedulerGetUserTransitionKernelEntryRuntimeAddress(void);
void LosKernelSchedulerUserTransitionBridgeTrap(UINT64 KernelStackPointer);
void LosKernelSchedulerYieldCurrent(void);
void LosKernelSchedulerSleepCurrent(UINT64 TickCount);
void LosKernelSchedulerTerminateCurrent(void);
void LosKernelSchedulerSwitchContext(LOS_KERNEL_SCHEDULER_CONTEXT *From, const LOS_KERNEL_SCHEDULER_CONTEXT *To);

#endif
