# Kernel Scheduler

## Current State

LOS now has a kernel-internal scheduler with **timer-driven preemptive kernel threads**.

The scheduler is still intentionally small, but it now provides:

- fixed-priority task selection
- one always-runnable idle thread
- one periodic heartbeat kernel thread
- one non-cooperative busy worker thread to prove involuntary preemption
- dedicated kernel stack per scheduled thread
- saved execution context for scheduler and threads
- timer-driven wake-up for sleeping work
- interrupt-driven quantum expiry and return-to-scheduler preemption
- scheduler ownership of the post-init idle path
- scheduler initialization moved to the post-bootstrap stage, after the memory-manager bootstrap address space exists
- early boot fallback stacks reserved inside the kernel image so scheduler bring-up does not halt if physical-frame claiming is temporarily unavailable

## What It Does Today

After kernel initialization completes:

1. interrupts are enabled
2. the scheduler enters its dispatch loop
3. the scheduler switches onto dedicated kernel-thread stacks
4. timer interrupts advance global tick time and wake sleeping threads
5. a running thread can now be preempted from the timer interrupt path even if it never yields voluntarily
6. the heartbeat thread continues to run even while the busy worker spins forever without calling yield or sleep

This means LOS has now crossed from a cooperative thread model into a first-stage preemptive one.

## How Preemption Works In This Stage

The current design keeps the scheduler itself in kernel context and uses the timer interrupt path to trigger preemption.

When the PIT interrupt fires:

- the scheduler tick count is advanced
- due sleeping threads are marked ready
- the current thread's quantum is decremented
- if rescheduling is needed, the interrupt path switches from the running thread back into the scheduler context before the interrupt returns

That gives LOS a minimal but real involuntary kernel preemption path without yet needing full user-mode scheduling.

## What It Still Does Not Do

The current scheduler still does **not** yet provide:

- separate process objects above the thread layer
- user-mode scheduling or ring transitions
- IPC block/wake integration
- endpoint wait queues and timeout objects
- SMP run queues or cross-core reschedule IPIs
- per-thread ownership, teardown, and reclamation rules beyond the current bootstrap task model

So this stage is now a **small preemptive kernel-thread scheduler**, not yet a full process scheduler.

## Why This Stage Matters

This gives LOS the first real preemptive thread substrate inside the kernel:

1. per-thread stacks exist
2. thread context is preserved across dispatches
3. sleeping work wakes on timer ticks
4. non-cooperative work can no longer monopolize the CPU forever
5. later IPC blocking, timeout delivery, and user-mode entry can build on a real preemptive base

## Immediate Next Steps

- add explicit persistent thread objects with ownership and cleanup rules
- add a process/address-space ownership layer above threads
- add safe kernel-to-user transition and return paths
- let IPC paths block and wake scheduled threads
- move the memory manager from hosted bootstrap steps to a real scheduled task
