# Kernel Scheduler

## Current State

LOS now has a kernel-internal scheduler with **timer-driven preemptive kernel threads** and **reclaimable task objects**.

The scheduler is still intentionally small, but it now provides:

- fixed-priority task selection
- one always-runnable idle thread
- one periodic heartbeat kernel thread
- one non-cooperative busy worker thread to prove involuntary preemption
- one lifecycle-manager kernel thread that spawns short-lived work to prove task creation and teardown
- dedicated kernel stack per scheduled thread
- saved execution context for scheduler and threads
- timer-driven wake-up for sleeping work
- interrupt-driven quantum expiry and return-to-scheduler preemption
- scheduler ownership of the post-init idle path
- scheduler initialization moved to the post-bootstrap stage, after the memory-manager bootstrap address space exists
- early boot fallback stacks reserved inside the kernel image so scheduler bring-up does not halt if physical-frame claiming is temporarily unavailable
- per-task ownership, generation, termination, and deferred cleanup bookkeeping
- deferred reclamation of terminated thread stacks and task slots from the scheduler loop

## What It Does Today

After kernel initialization completes:

1. interrupts are enabled
2. the scheduler enters its dispatch loop
3. the scheduler switches onto dedicated kernel-thread stacks
4. timer interrupts advance global tick time and wake sleeping threads
5. a running thread can be preempted from the timer interrupt path even if it never yields voluntarily
6. the heartbeat thread continues to run even while the busy worker spins forever without calling yield or sleep
7. the lifecycle manager can create a short-lived worker, let it exit, and the scheduler then reclaims its stack and task slot for reuse

This means LOS now has both a preemptive kernel-thread substrate and the first real task-lifetime rules above it.

## How Preemption Works In This Stage

The current design keeps the scheduler itself in kernel context and uses the timer interrupt path to trigger preemption.

When the PIT interrupt fires:

- the scheduler tick count is advanced
- due sleeping threads are marked ready
- the current thread's quantum is decremented
- if rescheduling is needed, the interrupt path switches from the running thread back into the scheduler context before the interrupt returns

That gives LOS a minimal but real involuntary kernel preemption path without yet needing full user-mode scheduling.

## How Task Lifetime Works In This Stage

Each task now carries its own identity and ownership metadata:

- task id
- owner task id
- generation number
- exit status
- cleanup-pending state
- stack ownership metadata

When a thread terminates:

1. the running task is marked terminated
2. cleanup is deferred until control is safely back in the scheduler loop
3. the scheduler reclaims the task's stack resources
4. the scheduler zeroes the task slot and makes it reusable for later creations

That keeps lifetime transitions explicit and avoids trying to free a task's own stack while that task is still executing on it.

## What It Still Does Not Do

The current scheduler still does **not** yet provide:

- separate process objects above the thread layer
- user-mode scheduling or ring transitions
- IPC block/wake integration
- endpoint wait queues and timeout objects
- SMP run queues or cross-core reschedule IPIs
- process/address-space ownership above the thread owner field

So this stage is now a **small preemptive kernel-thread scheduler with first-stage task lifecycle cleanup**, not yet a full process scheduler.

## Why This Stage Matters

This gives LOS the first real preemptive thread substrate inside the kernel together with reusable task objects:

1. per-thread stacks exist
2. thread context is preserved across dispatches
3. sleeping work wakes on timer ticks
4. non-cooperative work can no longer monopolize the CPU forever
5. terminated thread resources can now be reclaimed instead of being leaked forever
6. later IPC blocking, timeout delivery, and user-mode entry can build on a real preemptive base with explicit task lifetime rules

## Immediate Next Steps

- add a process/address-space ownership layer above threads
- add safe kernel-to-user transition and return paths
- let IPC paths block and wake scheduled threads
- move the memory manager from hosted bootstrap steps to a real scheduled task
