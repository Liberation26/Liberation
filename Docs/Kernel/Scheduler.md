## 0.4.82

- Kept the faster first-user-task ladder from 0.4.81, but stopped the lifecycle manager from spawning endless transient diagnostic processes after the first successful ring-3 proof return.
- That transient-process churn was what kept allocating fresh direct-claim stacks, which is why the console kept filling with repeated `Kernel scheduler using direct-claim stack pool.` lines after `Init command executed in ring 3 and trapped back into the kernel.`
- Direct-claim stack-pool confirmation is now emitted only on first use, so ordinary later task allocation no longer floods the screen with the same success line.

## 0.4.81

- Reduced the lifecycle-manager cadence from 250 ticks to 25 ticks so the first-user-task ladder no longer pauses for multiple seconds between each preparation stage. At 100 Hz this cuts each step from 2.5 seconds to 0.25 seconds, so `Interrupts enabled` to `seal-ready`/`handoff-ready` no longer feels stalled.
- Moved the synthetic first-user-task chain stack and initial `iretq` frame much deeper into the kernel task stack. This leaves several kilobytes of headroom for the first user-mode IRQ/trap entry and its C call chain before anything can touch the staged live-dispatch data.
- This specifically targets the remaining reset loop where the machine survived the real live mark, then died as soon as the first user-mode interrupt path started running on the task's kernel stack.

<!--
File Name: Scheduler.md
File Version: 0.3.11
Author: OpenAI
Email: dave66samaa@gmail.com
Creation Timestamp: 2026-04-07T08:42:21Z
Last Update Timestamp: 2026-04-07T12:35:00Z
Operating System Name: Liberation OS
Purpose: Documents Liberation OS design, behavior, usage, or integration details.
-->

## 0.3.4

- The first init-command handshake now carries explicit sender, receive, send-event, and receive-event endpoint ids in ABI version 6.
- Scheduler-side serial reporting now labels the init-command transport contract as ABI version 6 after the first ring-3 return.

## 0.3.3

- The first init-command handshake now assumes a richer fixed-size send packet and matching receive packet in ABI version 5.
- Scheduler-side serial reporting now labels the init-command transport contract as ABI version 5 after the first ring-3 return.

## 0.2.68

- `LosKernelSchedulerCreateTask()` now always creates a task through the ordinary ready-task path.
- The first-user-task bootstrap path still reblocks and annotates its special task immediately afterward, but that policy now lives in the user-transition preparation flow instead of being hidden inside generic task creation.
- This keeps normal scheduler task registration semantics consistent for all callers and makes special bootstrap behavior explicit at the call site.

The first live MM launch now carries explicit CPL3 proof instrumentation: right before `iretq`, the scheduler logs the exact frame and selectors it is about to dispatch, and on first user-mode entry the MM emits a serial proof line with live CS/CPL/RSP/RFLAGS values. This makes it obvious whether the first user task actually executed after the real ring transition.

The kernel-hosted scheduler now treats the launch ladder as the **first user task** lifecycle path. Remaining bootstrap-scaffold terminology is transitional documentation only and should keep shrinking as endpoint-backed userland MM startup becomes ordinary task launch.


Update 0.2.63
-------------

Scheduler-visible diagnostics now expose the live CPL3 ladder as `first-user-task-*` instead of `user-scaffold-*`, and the remaining lifecycle traces around image attach, stack allocation, live promotion, guarding, and completion now describe the launched object as the **first user task** once it enters the real dispatch path. This keeps the historical scaffold concept inside bootstrap internals while making serial/trace output match the kernel-hosted scheduler architecture.

## 0.2.61

Reduced remaining bootstrap-scaffold language in the kernel-hosted scheduler. Once the first runnable CPL3 object is prepared for real dispatch, lifecycle, dispatch, and diagnostics now describe it as the **first user task** rather than continuing to call the live path a scaffold.

This does not change the underlying bootstrap states or object names yet, but it does tighten the architectural boundary: scaffold terminology now belongs to preparation, while the live dispatch/return path is treated as the lifecycle of the first real user task.

Delivery packages continue to omit build outputs.

## 0.2.59

The first CPL3 dispatch path is now hardened against a direct return through `LosKernelSchedulerUserTransitionKernelEntry()`. Instead of treating that kernel-entry landing point as an impossible condition and halting forever, the scheduler now treats it as a controlled completion of the one-shot first-user-task bring-up path: the current user task is marked terminated, the scaffold is marked complete, the returned task/process are traced, and control switches cleanly back into the kernel scheduler.

Delivery packages continue to omit build outputs.

## 0.2.49

The memory manager is now pushed closer to being the authoritative owner of scaffold memory setup. Its service-side `AttachStagedImage` and `AllocateAddressSpaceStack` paths are idempotent, so a repeated scheduler probe no longer turns a previously successful image or stack attach into a hard conflict. The MM service can also answer `QueryMapping` from its own reserved-region metadata when page-table verification lags during hosted bootstrap, which keeps scheduler bring-up moving without bouncing back into kernel-side MM emulation.

Delivery packages continue to omit build outputs.

## 0.2.47

Scheduler scaffold bring-up now leans harder into the agreed split: the scheduler stays fully kernel-hosted and consumes memory state, while the memory manager remains the authority for image/stack metadata on an address space. The scheduler now prefers a direct walk of its owned scaffold page tables before asking the hosted bootstrap MM transport to confirm a mapping, and a conflicting `AttachStagedImage`/stack setup is treated as evidence to re-read the address-space object and reuse already-recorded image/stack metadata rather than spiraling into more attach retries.

Delivery packages also now omit build outputs so the target tar contains source/docs only.

## 0.2.46

LOS now treats the scheduler as a **fully kernel-hosted subsystem** rather than a future userland policy server. Task creation, runnable-state transitions, dispatch choice, CR3 activation on dispatch, kernel-stack ownership, timer-driven preemption, and the ring-transition entry/return substrate all remain scheduler responsibilities inside the kernel.

That clarifies the boundary for the current bring-up stage:

- the **scheduler** owns tasks, dispatch, and user-entry mechanics
- the **memory manager** owns address spaces, mappings, staged images, and user stacks
- the **kernel** continues to host the scheduler directly rather than acting as a generic worker for an external scheduling policy service

Follow-on cleanup should therefore remove bootstrap fallbacks and fixed scaffold assumptions, but it should not move scheduler policy into user space.

## 0.2.45

The scheduler scaffold bring-up now treats hosted-bootstrap mapping verification as a hybrid path rather than an all-or-nothing round-trip through the hosted memory-manager pump. When the bootstrap transport reports a `QueryMapping` miss for a user address space the kernel can already inspect directly, the scheduler now falls back to a local walk of the scaffold root page tables and accepts the mapping if the page is present there.

The scaffold setup path also now consumes the actual entry and stack metadata recorded on the address-space object after image attach / stack allocation instead of reusing fixed constants for every verification pass. That prevents a successful first attach from degenerating into repeated `AttachStagedImage` retries and conflict returns simply because the follow-up verification query did not come back through the hosted service path.

## 0.2.44

- Fixed the hosted-bootstrap fallback path used during scheduler scaffold bring-up. `QueryMapping` fallback can now actually execute when the hosted step does not publish a reply, instead of halting on the first failed hosted step.
- Local bootstrap-fallback accounting now records each completed request exactly once, which keeps `messages-completed` diagnostics aligned with the real number of serviced requests.
- Local `MapPages` and `UnmapPages` dispatch now resolve the target address-space object and pass its real `RootTablePhysicalAddress` into the x64 virtual-memory mapper, rather than accidentally passing the object physical address itself.
- Local address-space resolution for scaffold probes/attach/stack fallback now keys off a ready object with a valid root table instead of hard-rejecting future service-created objects that may not preserve bootstrap signature/version constants exactly.
- The dispatcher now guards null request/response pointers before initializing the response buffer, and hosted-service readiness now refuses re-entry while the hosted task is already marked `RUNNING`.

Hosted bootstrap now has kernel-side fallbacks for `AttachStagedImage` and `AllocateAddressSpaceStack` too. If the memory-manager hosted pump stops publishing a reply while the scheduler scaffold is staging the first user image or first user stack, the kernel now builds those mappings directly into the target address space so scaffold bring-up can continue toward `entry-ready`, `handoff-ready`, and the first real ring-3 proof path.

Hosted bootstrap now has a kernel-side `QueryMapping` fallback as well: if the memory-manager hosted pump does not publish a reply while the scheduler is probing scaffold mappings, the kernel resolves the address-space object directly and walks the target page tables itself. That keeps the image/stack presence checks moving instead of fatal-halting on a missing hosted reply.

Hosted memory-manager exchanges used during scaffold preparation are serialized with interrupts disabled, and the memory-manager `QueryMapping` probe is allowed to report `not found` for unmapped user virtual addresses rather than hard-failing the service. This keeps the scaffold image/stack mapping phase stable while the memory manager is still running through the hosted bootstrap pump.

# 0.2.40

Added the first **real x64 ring-transition substrate** for the scheduler user scaffold. LOS now installs a real TSS descriptor in the GDT, loads `TR`, refreshes `TSS.RSP0` with the current kernel-thread stack top before user dispatch, maps a staged user ELF plus a real user stack into the scaffold process's owned address space through the memory manager, and switches the scheduler bridge from staged metadata to a real `iretq` entry.

Interrupt vector `128` is now installed as a DPL3 gate so that first user image can trap back into ring 0 on the TSS-provided kernel stack. The interrupt path recognizes that user-origin return, terminates the scaffold task, requests reschedule, and lets the ordinary reap path clean it up. This proves the real kernel->user->kernel transition substrate needed before resumable syscalls, timer-driven user preemption, and general loader-backed user tasks can be layered on top.

Added an explicit **complete** stage after **live-gate-closed** for the first-user-task bring-up path. Normal scheduler serial output now stays quiet until the scaffold reaches **seal-ready**, so the first scheduler-side serial lines appear only once the staged user handoff metadata is materially in place. After the live gate closes, the scaffold now advances once more to a scheduler-side **complete** state, heartbeat/state diagnostics expose `user-scaffold-complete`, and the scaffold process/task report `user-state=15` to show that scheduler-side staging is finished even though a future real TSS-backed ring transition is still required before the task can be marked LIVE.

Added an explicit **chain-ready** stage after **bridge-ready** for the first-user-task bring-up path. The scheduler now stages a dedicated non-live dispatch chain on the blocked scaffold task's kernel stack, records that prepared chain stack pointer as `user-chain-sp` on the scaffold process/task objects, and exposes `user-scaffold-chain-ready` in heartbeat output. The live gate now closes only after that chain metadata is present, so the future real ring-transition path has a preserved bridge -> kernel-entry stack chain ready before anything can be marked live.

# 0.2.33

Added an explicit **trampoline-ready** stage after **frame-ready** for the first-user-task bring-up path. The scheduler now rewrites the blocked scaffold task's first kernel return address to a dedicated non-live kernel-entry helper, records that address as `user-kentry` on both the scaffold process/task objects, and exposes `user-scaffold-trampoline-ready` in the heartbeat. The live gate now closes only after that trampoline metadata is present, so the future real ring-transition path has a dedicated kernel-entry landing point ready before anything can be marked live.

# 0.2.32

Added an explicit **frame-ready** stage after **descriptor-ready** for the first-user-task bring-up path. The scheduler now writes a future `iretq` return-frame template onto the blocked scaffold task's kernel stack and records the prepared frame stack pointer as `user-frame-sp` on both the scaffold process/task traces. Scheduler heartbeats now expose `user-scaffold-frame-ready`, and the live-gate/live-marking helpers now require that non-zero prepared frame metadata to exist before any future real ring-transition handoff can mark the scaffold live.

# 0.2.31

Added an explicit **descriptor-ready** stage after **entry-ready** for the first-user-task bring-up path. The kernel GDT now carries user code/data descriptors, the scheduler records future user `CS`, `SS`, and `RFLAGS` values on the scaffold process/task objects, and scheduler heartbeats now expose `user-scaffold-descriptor-ready`. The scaffold still stops at the live gate and remains blocked until a future real ring-transition path marks it live.

# 0.2.30

Added an explicit **non-live scaffold guard** for the user-transition ladder. Scheduler heartbeats now expose `user-scaffold-blocked` and `user-scaffold-reblocked`, and the lifecycle thread now force-reblocks the scaffold task if anything accidentally makes it ready before the future real ring-transition path marks it live. That keeps the scaffold visible, proves it remains intentionally blocked after `entry-ready`, and turns accidental premature readiness into an explicit diagnostic instead of a silent risk.

# 0.2.29

Added an explicit **live-gate-closed** stop after the first-user-task bring-up path reaches **entry-ready**. The lifecycle thread now emits a one-time `Scheduler first-user-task bring-up path live gate closed...` line, scheduler diagnostics expose `user-scaffold-live` and `user-live-gate-closed`, and a dedicated `LosKernelSchedulerMarkUserTransitionScaffoldLive()` helper is now in place for the future real ring-transition entry path to flip the scaffold to LIVE without reopening dispatch early.

# 0.2.28

Added an explicit **entry-ready** stage to the first-user-task bring-up path. After launch is requested, the scaffold now advances to an entry-ready state once the blocked user task and its distinct address space still carry non-zero user entry and user-stack values. Scheduler diagnostics now expose `user-scaffold-entry-ready`, and dispatch eligibility stays closed to user-mode tasks until a future real ring-transition path marks them live.

Added a first-stage **first-user-task bring-up path**. The scheduler lifecycle path now prepares a dedicated `FirstUserTaskProcess` with its own distinct address space plus a blocked `FirstUserTaskTask` that records the future user entry and user-stack top without attempting a ring transition yet. Scheduler-wide diagnostics now expose `user-scaffold-ready`, `user-scaffold-prepared`, `user-scaffold-proc`, and `user-scaffold-task`, while task/process traces now include the recorded user entry, user stack, and user-transition state. That gives LOS a concrete launch object to harden before the first real kernel-to-user transfer path is wired.

## 0.2.20

Added first-stage scheduler runtime accounting. The scheduler now records per-task and per-process dispatch counts plus consumed run ticks, and the scheduler heartbeat/state diagnostics now expose scheduler-wide `idle-ticks` and `busy-ticks`. That makes the serial log useful for answering whether time is actually being spent in the idle thread, the busy worker, or transient work before the first user-mode transition path is added.

## 0.2.19

Fixed the direct-claim scheduler stack-pool release path. Reaped direct-claim tasks were clearing the generic bootstrap-fallback slot bitmap first because both paths reused the same slot field, which left the direct-claim slot marked busy and caused `stack-pool-used` to climb even though the transient worker had already been reaped. Cleanup now releases slots according to the recorded stack source, so direct-claim stack-pool accounting drops back after reap and the pool can be reused without drifting toward false exhaustion.

## 0.2.18

Scheduler threads now prefer a dedicated **kernel direct-claim stack pool** reserved once during scheduler initialization. That pool is backed by kernel-claimed frames and reused slot-by-slot as transient workers are created and reaped, so LOS no longer has to keep ordinary scheduler work on the embedded bootstrap fallback stacks just because hosted `AllocateFrames` replies are still being held back. The embedded bootstrap stack array now remains as an emergency fallback only.

## 0.2.17

Scheduler-created transient threads now stay on bootstrap fallback stacks even after the hosted memory-manager attach completes. The hosted `AllocateFrames` request path can still lose replies once transient distinct-root process activation is live, so this stage deliberately keeps scheduler-owned stacks off that path while distinct process address spaces continue to be created and destroyed through the memory manager.

## 0.2.16

Scheduler bootstrap threads now stay on bootstrap fallback stacks until the scheduler is online and the memory-manager attach handshake is complete. That prevents early scheduler initialization from issuing premature hosted `AllocateFrames` requests before the hosted service path is stable enough to guarantee a real reply.

## 0.2.15

- Scheduler thread stacks now come from the memory manager whenever the bootstrap transport is ready.
- If the transport is not ready, scheduler threads fall back to bootstrap stacks instead of using untracked direct frame claims.
- Task cleanup now only sends `FreeFrames` for memory-manager-owned stacks, which fixes the `freeing-unowned-pages` hard-fail seen when transient workers exited and the scheduler tried to reclaim their stacks.

# Scheduler

## 0.2.57

The scheduler no longer closes the old non-live gate after the first-user-task bring-up path reaches **handoff-ready**. Instead, the lifecycle diagnostics thread now promotes the scaffold to **LIVE** by calling `LosKernelSchedulerMarkUserTransitionScaffoldLive()`, which requeues the prepared blocked task for dispatch through the real scheduler user-transition bridge. This turns the staged handoff metadata into the first actual CPL3 `iretq` attempt instead of stopping at a scheduler-only **complete** marker.

## 0.2.40

- Wired the first **real x64 ring-transition substrate** into the scheduler user scaffold. LOS now installs a proper TSS descriptor in the GDT, loads `TR`, and refreshes `TSS.RSP0` with the current kernel-thread stack top before dispatching the scaffold to user mode.
- The scheduler scaffold now maps a staged user ELF image and a real user stack into the scaffold process's owned address space through the memory manager instead of only carrying placeholder metadata.
- The scheduler's user-transition bridge now performs an actual `iretq` into ring 3 using the prepared user frame, rather than stopping at a non-live staging chain.
- Interrupt vector `128` is now installed as a DPL3 gate so the first user image can trap back into ring 0 on the TSS-provided kernel stack. The interrupt path recognizes that user-origin trap, marks the scaffold task terminated, requests reschedule, and lets the ordinary reap path clean it up.
- This stage proves the real kernel->user->kernel transition substrate needed by the scheduler, user tasks, and future memory-manager-backed user launches. The next step is to preserve/resume user trapframes instead of treating the first user return as a one-shot proof.

## 0.2.22

- Added first-stage **run-slice accounting** for tasks, processes, and the scheduler as a whole. LOS now records `last-run-slice` and `max-run-slice` so the serial log can show the longest observed uninterrupted run segment for BusyWorker, transient workers, and the scheduler overall.
- Fixed the scheduler heartbeat summary so it now actually emits `max-ready-delay` and `max-wake-delay` alongside the new `max-run-slice` field. That makes the lightweight heartbeat line match the richer scheduler state trace more closely.

## 0.2.21

- Added dispatch-latency accounting for tasks, processes, and the scheduler as a whole. LOS now records the worst observed `max-ready-delay` and `max-wake-delay` values so the serial log can show not just CPU usage totals, but also how long work waited before it was actually dispatched.

- 0.2.13: Added a one-shot pending-wake dispatch path plus a short resume window for freshly woken tasks. A task that wakes from scheduler sleep now keeps an explicit wake-pending marker until it is actually dispatched, and its first post-wake run can receive a slightly longer quantum so resume -> exit paths stay visible even under BusyWorker load.

## 0.2.12 update

- Woken scheduler tasks now get a short-lived wake dispatch boost so freshly resumed work is less likely to sit behind the busy worker for many extra quanta after its wake tick arrives.
- Added `wakeups` and `wake-dispatch` diagnostics plus per-task `last-wake` tracing, so the serial log can now show whether a task woke and whether the selector chose it because of the wake boost.
- This should make the transient worker path easier to prove end-to-end: enter -> sleep -> wake -> resume -> exit -> reap.

## 0.2.11 update

- Scheduler context switching now preserves the full general-purpose register set, not just the callee-saved subset.
- This hardens both voluntary sleep/yield resumes and timer-interrupt preemption resumes so resumed code is less likely to observe clobbered register state.
- Ephemeral workers now emit an explicit `resumed` trace after waking, which should make it obvious in the serial log whether the wake -> resume -> exit -> reap path is healthy.

# Kernel Scheduler

## Current State

LOS now has a kernel-internal scheduler with **timer-driven preemptive kernel threads**, **reclaimable task objects**, a **first-stage process layer above threads**, a **first-stage starvation-relief policy** for lower-priority ready work, **scheduler-managed process root activation**, and **distinct memory-manager-created address spaces bound to transient scheduler processes** so dispatch can switch between genuinely different CR3 roots even before user mode exists. Transient non-kernel scheduler processes now **require** that distinct address-space bind; if the bind is unavailable, creation is rejected instead of silently leaving the process on the inherited kernel root. The lifecycle test path is now also **serialized to one transient owned-root process at a time**, so LOS does not keep issuing new distinct-root process-creation attempts while an earlier transient process is still alive or waiting to be reaped.

The scheduler is still intentionally small, but it is now explicitly the complete kernel-hosted task authority for this stage. It provides:

- fixed-priority task selection with ready-time aging for starvation relief
- one always-runnable idle thread
- one periodic heartbeat kernel thread
- one non-cooperative busy worker thread to prove involuntary preemption
- one lifecycle-manager kernel thread that spawns short-lived work to prove task creation and teardown
- dedicated kernel stack per scheduled thread
- scheduler-owned direct-claim stack pool reserved once at initialization and reused across transient task lifetimes
- saved execution context for scheduler and threads
- timer-driven wake-up for sleeping work
- interrupt-driven quantum expiry and return-to-scheduler preemption
- scheduler ownership of the post-init idle path
- scheduler initialization moved to the post-bootstrap stage, after the memory-manager bootstrap address space exists
- early boot fallback stacks reserved inside the kernel image so scheduler bring-up does not halt if physical-frame claiming is temporarily unavailable
- hosted transient thread stacks currently stay off the hosted `AllocateFrames` path; scheduler threads now prefer the direct-claim kernel stack pool, with the bootstrap fallback array retained only as emergency backup
- per-task ownership, generation, termination, and deferred cleanup bookkeeping
- first-stage process objects with process ids, owner process ids, thread counts, and address-space metadata
- inherited process root-table tracking so transient processes no longer carry a zero root while they still share the kernel mappings
- memory-manager-backed distinct address-space binding for transient processes, including address-space-object tracking and cleanup-time destroy requests
- scheduler-side CR3 activation accounting on dispatch and restore back to the kernel process root on return to the scheduler
- deferred reclamation of terminated thread stacks, task slots, and now transient process objects from the scheduler loop
- ready-time tracking plus bounded aging so lower-priority ready tasks can still reach dispatch under sustained busy-thread load

## Lifecycle Serialization In This Stage

The live serial log showed that LOS could create the first transient owned-root process, but then the lifecycle thread kept trying to create more while that earlier transient process was still active. That produced noisy rejection logs and made it harder to isolate the real remaining bug.

To tighten that path, the lifecycle manager now waits until no transient process is still ready, initializing, or pending cleanup before it attempts to create the next transient owned-root process. This keeps the scheduler honest about the current boundary: one transient distinct-root worker at a time until wake, exit, reap, and address-space teardown are all proven cleanly.

## What It Does Today

After kernel initialization completes:

1. interrupts are enabled
2. the scheduler enters its dispatch loop
3. the scheduler switches onto dedicated kernel-thread stacks
4. timer interrupts advance global tick time and wake sleeping threads
5. a running thread can be preempted from the timer interrupt path even if it never yields voluntarily
6. the heartbeat thread continues to run even while the busy worker spins forever without calling yield or sleep
7. the lifecycle manager can create a short-lived worker, let it run even while a higher-priority busy worker is spinning, let it exit, and the scheduler then reclaims its stack and task slot for reuse

This means LOS now has a preemptive kernel-thread substrate, explicit task-lifetime rules, a first process object layer that can own groups of threads, explicit root-table activation points in the dispatch path, and live transient processes that can carry their own memory-manager-created roots instead of always inheriting the kernel root.

## How Preemption Works In This Stage

The current design keeps the scheduler itself entirely in kernel context and uses the timer interrupt path to trigger preemption. The kernel is the complete host for scheduling; there is no separate userland scheduler-policy service in this stage.

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
- an inherited-root marker when it is intentionally sharing an existing root rather than owning a distinct one yet

The kernel bootstrap work now runs inside a persistent `KernelProcess`, while the lifecycle manager spawns short-lived `EphemeralProcess` objects that each own one ephemeral worker thread. When the worker exits, the scheduler reclaims the thread and then reclaims the now-empty transient process object.

When a process is created without its own root yet, the scheduler now inherits the creator root and records that fact explicitly. For transient non-kernel processes, the scheduler then immediately asks the hosted memory manager to create a distinct address-space object, replaces the inherited root with the returned root-table physical address, and records the owning address-space object for later teardown. During dispatch, the scheduler activates the selected process root before entering the thread context and restores the kernel-process root when control returns to the scheduler loop. When a transient process is reaped, the scheduler now asks the memory manager to destroy the owned address-space object before freeing the process slot.

If that distinct bind cannot be completed for a transient process that requires its own address space, the scheduler now rejects the process creation rather than pretending that the process boundary exists while it is still running on the inherited kernel root. The main scheduler loop also retries pending binds for any non-kernel process that still carries the inherited-root marker.

That gives LOS a real place to hang later address-space ownership, fault accounting, IPC ownership, and user-mode launch state without pretending that a thread alone is the whole execution object.

## What It Still Does Not Do

The current scheduler still does **not** yet provide:

- user-mode scheduling or ring transitions
- IPC block/wake integration
- endpoint wait queues and timeout objects
- SMP run queues or cross-core reschedule IPIs
- ring transitions into user mode using those distinct user process address spaces

So this stage is now a **small preemptive kernel-thread scheduler with first-stage process ownership, lifecycle cleanup, and dispatch-time root activation**, not yet a full user/process scheduler.

## Why This Stage Matters

This gives LOS the first real preemptive thread substrate inside the kernel together with reusable task objects:

1. per-thread stacks exist
2. thread context is preserved across dispatches
3. sleeping work wakes on timer ticks
4. non-cooperative work can no longer monopolize the CPU forever
5. terminated thread resources can now be reclaimed instead of being leaked forever
6. transient process objects can now be reclaimed once their last thread exits
7. dispatch now has an explicit place to install a process root table before the thread runs
8. transient scheduler processes can now own a distinct memory-manager-created address space and tear it down again on cleanup
9. later IPC blocking, timeout delivery, executable mapping, and user-mode entry can build on a real preemptive base with explicit task and process lifetime rules

## Immediate Next Steps

- add safe kernel-to-user transition and return paths on top of the now-distinct process roots
- map a first executable image and user stack into one of those process objects
- let IPC paths block and wake scheduled threads
- move the memory manager from hosted bootstrap steps to a real scheduled task

## 0.2.14

- The memory-manager bootstrap readiness check now treats all states at or beyond `READY` as scheduler-usable. That matters because a hosted service request advances the bootstrap state to `SERVICE_ONLINE`; without this, scheduler cleanup could create and run a transient process successfully, then fail to free its terminated thread stack or destroy its owned address space during reap.
- Added `Scripts/Run.sh` as a single entry wrapper that clears the screen, runs `update.sh`, and then launches directory, hard-disk, or ISO boot depending on whether the user passes `D`, `H`, or `I`.

## 0.2.8

- Transient non-kernel process address-space binding is now serialized per process. The scheduler marks a process as `bind-in-progress` before sending `CreateAddressSpace`, so the lifecycle thread and the scheduler bind pass cannot both issue separate create requests for the same process.
- Scheduler diagnostics now expose `bind-in-progress`, `bind-count`, and `bind-deferred`, making it possible to prove in the serial log that a process root was bound once rather than racing and leaking the first address-space object.
- This still is not user-mode execution yet. It is process/root ownership hardening ahead of the first kernel-to-user transition.
## 0.2.9 update

- Distinct-root transient processes remain in an internal creating state until their address-space bind succeeds.
- The scheduler no longer exposes half-created transient processes to the main bind-pending sweep.
- Deferred address-space-bind logging was removed from the hot scheduler loop to avoid serial spam; the deferred counter remains.



- Added a launch-requested stage to the first-user-task bring-up path. The scaffold remains blocked, but now records that the kernel has accepted the next entry request before any future ring-transition implementation.


## 0.2.36

- Added a `contract-ready` first-user-task bring-up path stage after `chain-ready`.
- The scheduler now verifies the staged frame and saved dispatch chain, then freezes a non-zero `user-contract` signature across that metadata so the serial log can prove the future handoff contract is internally consistent before a real ring transition exists.
- Diagnostics now report `user-contract=` in detailed task/process traces and `user-scaffold-contract-ready=` in heartbeat output.
- The live gate still remains closed until the real ring-transition entry path exists, and both live-gating and future `LIVE` promotion now require the prepared contract metadata.

## 0.2.34

- Added a `bridge-ready` first-user-task bring-up path stage after `trampoline-ready`.
- The blocked scaffold task now carries a dedicated future dispatch-bridge entry address in addition to the staged kernel-entry and user return-frame metadata.
- Diagnostics now report `user-bridge=` in detailed task/process traces and `user-scaffold-bridge-ready=` in heartbeat output.
- The live gate still stays closed until the real ring-transition entry path exists, so this remains scaffold hardening rather than actual user-mode dispatch.


## 0.2.37

- Added a `seal-ready` first-user-task bring-up path stage after `contract-ready`.
- The scheduler now writes and verifies a non-zero `user-seal` value on the staged dispatch chain so the blocked scaffold carries an explicit sealed handoff marker before the live gate closes.
- Diagnostics now report `user-seal=` in detailed task/process traces and `user-scaffold-seal-ready=` in heartbeat output.
- The live gate still remains closed until the real ring-transition entry path exists, and both live-gating and future `LIVE` promotion now require the prepared seal metadata.


## 0.2.38

- Added a `handoff-ready` first-user-task bring-up path stage after `seal-ready`.
- The scheduler now freezes the final blocked-task handoff stack pointer as non-zero `user-handoff-sp` metadata after verifying it still matches the staged chain stack pointer and current saved execution stack pointer.
- Diagnostics now report `user-handoff-sp=` in detailed task/process traces and `user-scaffold-handoff-ready=` in heartbeat output.
- The live gate still remains closed until the real ring-transition entry path exists, and both live-gating and future `LIVE` promotion now require the prepared handoff stack metadata.


## 0.2.49 integration note
The kernel-hosted scheduler now treats MM-recorded image and stack metadata as authoritative during scaffold verification, while the memory manager returns success for already-established image/stack state so bootstrap retries do not degenerate into attach conflicts.


- 0.2.50: tightened MM-owned scaffold image/stack metadata reuse and page-rounded image accounting; scheduler final verification now accepts MM-recorded mappings before retrying scaffold creation.


## 0.2.51
- made MM-owned scaffold image and stack metadata authoritative during bootstrap retries
- AttachStagedImage and AllocateAddressSpaceStack now repair from reserved-region records before reporting conflicts
- QueryMapping now prefers MM-owned reserved-region answers before relying on the live page-table walk
- scheduler accepts successful MM image/stack metadata directly instead of forcing an immediate re-query before advancing


## 0.2.52
- hardened MM-owned scaffold recovery so `AttachStagedImage` and `AllocateAddressSpaceStack` can repair address-space metadata from live mappings when earlier bootstrap bookkeeping lagged
- MM now treats already-mapped contiguous image and stack ranges as reusable success instead of falling back to conflict on retry paths
- kept scheduler kernel-hosted while pushing more memory truth back under the MM service

0.2.54:
- hardened the memory-manager address-space metadata path so image/stack reserved-region records are repaired from MM-owned metadata and reused for later mapping queries
- MM query, image attach, and stack allocation now keep reserved-region truth synchronized with recorded image/stack state


0.2.57: the scheduler now promotes the handoff-ready first-user-task bring-up path to LIVE and dispatches it through the real `iretq` bridge instead of closing the old non-live gate.
0.2.56: scheduler scaffold bring-up now accepts coherent MM-returned image/stack metadata directly, while MM success replies are normalized from recorded address-space state so image/stack results stay authoritative across bootstrap retries.


### 0.2.60
The kernel-hosted scheduler now treats the first CPL3 dispatch as the first user task lifecycle path rather than a permanent scaffold concept. Diagnostics and completion reporting were updated accordingly.


0.2.67: Added MM endpoint-ready and first endpoint-reply proof instrumentation for the first user-mode service path.


## 0.2.67 update

The first-user-task MM launch path now requires authoritative MM endpoint replies. Kernel bootstrap local rescue handling for QueryMapping / AttachStagedImage / AllocateAddressSpaceStack is no longer used, and scheduler mapping verification no longer falls back to kernel page-table walks or recorded metadata synthesis.
