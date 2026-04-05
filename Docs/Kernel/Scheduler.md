# Kernel Scheduler

## Current State

LOS now has a kernel-internal scheduler with **saved-context kernel threads**.

The current scheduler is still intentionally small, but it has moved beyond the earlier step-only model:

- fixed-priority task selection
- one always-runnable idle thread
- one heartbeat kernel thread
- dedicated kernel stack per scheduled thread
- saved execution context for scheduler and threads
- timer-driven wake-up for sleeping work
- scheduler ownership of the post-init idle path
- scheduler initialization moved to the post-bootstrap stage, after the memory-manager bootstrap address space exists
- early boot fallback stacks reserved inside the kernel image so scheduler bring-up does not halt if physical-frame claiming is temporarily unavailable

## What It Does Today

After kernel initialization completes:

1. interrupts are enabled
2. the scheduler enters its dispatch loop
3. the scheduler switches onto a dedicated idle-thread stack
4. timer interrupts wake the CPU
5. runnable threads can yield or sleep and later resume from their saved context

The heartbeat thread now proves more than basic dispatch. It proves that LOS can switch onto a separate kernel-thread stack, block that thread on a timer-based sleep, and later resume it from saved scheduler context.

## What It Still Does Not Do

The current scheduler still does **not** yet provide:

- involuntary preemption of running kernel threads
- separate process objects above the thread layer
- user-mode scheduling
- IPC block/wake integration
- timeout queues for endpoint waits
- SMP run queues

So this stage is now a **cooperative saved-context kernel-thread scheduler**, not yet a full preemptive tasking system.

## Why This Stage Matters

This gives LOS the first real thread substrate inside the kernel:

1. per-thread stacks exist
2. context is preserved across yields and sleeps
3. the scheduler can resume work instead of re-entering a one-shot step routine
4. later user-mode and IPC work can build on a real thread base instead of a synthetic dispatch loop

## Immediate Next Steps

- add true timer-driven preemption at safe scheduler boundaries
- add explicit thread objects distinct from higher-level task/process ownership
- add timer timeout queues for waits and sleeps
- let IPC paths block and wake scheduled threads
- move the memory manager from hosted bootstrap steps to a real scheduled task
