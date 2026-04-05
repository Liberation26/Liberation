# Kernel Scheduler

## Current State

LOS now has a kernel-internal scheduler instead of ending in a plain idle-only halt path.

The current scheduler is intentionally small:

- fixed-priority task selection
- one always-runnable idle task
- one periodic kernel heartbeat task
- timer-driven wake-up for periodic work
- explicit scheduler state and task objects in kernel memory
- scheduler ownership of the post-init idle path

This is a **step scheduler**.
Each task runs a short kernel step and returns to the scheduler.
That keeps the first implementation small and easy to verify while the rest of the task/thread model is still being built.

## What It Does Today

After kernel initialization completes:

1. interrupts are enabled
2. the scheduler enters its dispatch loop
3. the idle task sleeps with `hlt`
4. timer interrupts wake the CPU
5. periodic kernel work can be dispatched by priority

The heartbeat task proves that the scheduler is live and can dispatch non-idle work on a timer cadence.

## What It Does Not Yet Do

The current scheduler does **not** yet provide:

- saved CPU-context thread switching
- separate kernel thread stacks per runnable thread
- user-mode scheduling
- IPC block/wake integration
- timeout queues for endpoint waits
- SMP run queues

Those are the next scheduler stages, not part of this first scheduler delivery.

## Why This Split Is Useful

This step gives LOS a real scheduling authority now, without forcing a rushed thread/context-switch design into the tree.

It lets the kernel move forward in a clean order:

1. scheduler core
2. persistent thread objects and saved CPU context
3. kernel/user transition
4. IPC wake/block/timeouts
5. userland services running under the scheduler

## Immediate Next Steps

- replace step tasks with saved-context kernel threads
- add explicit thread and process objects
- add timer sleep and timeout queues
- let IPC paths block and wake scheduled tasks
- move the memory manager from hosted bootstrap steps to a real scheduled task
