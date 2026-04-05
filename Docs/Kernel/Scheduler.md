# Kernel Scheduler

## Current State

LOS now has a kernel-internal scheduler with **timer-driven preemptive kernel threads**, **reclaimable task objects**, a **first-stage process layer above threads**, and a **first-stage starvation-relief policy** for lower-priority ready work.

The scheduler is still intentionally small, but it now provides:

- fixed-priority task selection with ready-time aging for starvation relief
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
- first-stage process objects with process ids, owner process ids, thread counts, and address-space metadata
- deferred reclamation of terminated thread stacks, task slots, and now transient process objects from the scheduler loop
- ready-time tracking plus bounded aging so lower-priority ready tasks can still reach dispatch under sustained busy-thread load

## What It Does Today

After kernel initialization completes:

1. interrupts are enabled
2. the scheduler enters its dispatch loop
3. the scheduler switches onto dedicated kernel-thread stacks
4. timer interrupts advance global tick time and wake sleeping threads
5. a running thread can be preempted from the timer interrupt path even if it never yields voluntarily
6. the heartbeat thread continues to run even while the busy worker spins forever without calling yield or sleep
7. the lifecycle manager can create a short-lived worker, let it run even while a higher-priority busy worker is spinning, let it exit, and the scheduler then reclaims its stack and task slot for reuse

This means LOS now has a preemptive kernel-thread substrate, explicit task-lifetime rules, and a first process object layer that can own groups of threads.

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

## How Starvation Relief Works In This Stage

The live serial log exposed a real policy bug: a continuously ready higher-priority busy thread could keep a lower-priority ready thread from ever being chosen, even though timer preemption was working correctly.

To fix that, ready tasks now carry a `ReadySinceTick` timestamp. During task selection, the scheduler computes a small bounded aging boost from how long each ready task has been waiting. That boost only affects dispatch choice; it does not permanently rewrite the task's base priority.

This gives LOS a simple first-stage starvation control policy:

- base priority still decides the normal dispatch order
- timer preemption still stops one busy thread from running forever uninterrupted
- long-waiting lower-priority tasks can now eventually outrank a frequently re-queued busy thread long enough to run
- diagnostics expose `ready-since` and `starvation-relief` counters so the serial log shows when this rescue path is active

## What Process Ownership Adds In This Stage

Processes are still kernel-owned bookkeeping objects at this point, but they now exist separately from threads.

Each process carries:

- process id
- owner process id
- generation number
- thread count
- address-space id metadata
- root-table metadata
- cleanup state and exit status

The kernel bootstrap work now runs inside a persistent `KernelProcess`, while the lifecycle manager spawns short-lived `EphemeralProcess` objects that each own one ephemeral worker thread. When the worker exits, the scheduler reclaims the thread and then reclaims the now-empty transient process object.

That gives LOS a real place to hang later address-space ownership, fault accounting, IPC ownership, and user-mode launch state without pretending that a thread alone is the whole execution object.

## What It Still Does Not Do

The current scheduler still does **not** yet provide:

- separate process objects above the thread layer
- user-mode scheduling or ring transitions
- IPC block/wake integration
- endpoint wait queues and timeout objects
- SMP run queues or cross-core reschedule IPIs
- process/address-space ownership above the thread owner field

So this stage is now a **small preemptive kernel-thread scheduler with first-stage process ownership and lifecycle cleanup**, not yet a full user/process scheduler.

## Why This Stage Matters

This gives LOS the first real preemptive thread substrate inside the kernel together with reusable task objects:

1. per-thread stacks exist
2. thread context is preserved across dispatches
3. sleeping work wakes on timer ticks
4. non-cooperative work can no longer monopolize the CPU forever
5. terminated thread resources can now be reclaimed instead of being leaked forever
6. transient process objects can now be reclaimed once their last thread exits
7. later IPC blocking, timeout delivery, address-space ownership, and user-mode entry can build on a real preemptive base with explicit task and process lifetime rules

## Immediate Next Steps

- add a process/address-space ownership layer above threads
- add safe kernel-to-user transition and return paths
- let IPC paths block and wake scheduled threads
- move the memory manager from hosted bootstrap steps to a real scheduled task
