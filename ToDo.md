- Verify one-bind-per-process behavior for memory-manager-created transient roots and then continue into the first user-mode transition path.
# ToDo

## Priority Order

- [x] Prove timer and interrupt stability
- [x] Add a real scheduler
- [x] Evolve the scheduler from step tasks into saved-context kernel threads
- [x] Add first-stage timer-driven preemption for kernel threads
- [x] Add persistent task and thread objects with saved CPU context and ownership/lifetime rules
- [x] Prevent starvation of lower-priority kernel work under sustained busy-thread load
- [x] Add first-stage process objects with address-space ownership metadata above threads
- [x] Bind real non-kernel address spaces to transient process objects
- [x] Require distinct address spaces for transient non-kernel scheduler processes instead of silently inheriting the kernel root
- [ ] Add a real user-mode transition path for first user tasks
- [ ] Add real IPC: send, receive, reply, notifications, blocking, wake, and timeouts
- [ ] Add capability enforcement tied to kernel objects and IPC operations
- [ ] Add fault and exception handling against tasks, not just machine halt paths
- [ ] Add a real executable and program loader path for user images
- [ ] Run `MEMORYMGR.ELF` as a real scheduled user task
- [ ] Move memory-manager request servicing from bootstrap dispatch to real endpoint delivery
- [ ] Keep the memory-manager address space alive after bootstrap
- [ ] Allow kernel and user tasks to talk to the memory manager through normal IPC
- [ ] Add endpoint objects beyond memory-manager bootstrap-only endpoints
- [ ] Add reply objects and notification objects
- [ ] Add object lifetime and cleanup rules for task death
- [ ] Add a syscall or trap boundary for entering kernel services from userland
- [ ] Add kernel-side namespace hooks for locating services and endpoints
- [ ] Add timer-based sleep, timeout, and wake scheduling for IPC waits as well as scheduler sleeps
- [ ] Add an init or service-manager process
- [ ] Add a process and task manager service
- [ ] Add a filesystem or VFS-facing service
- [ ] Add a driver host or driver service layer
- [ ] Add a console and terminal service
- [ ] Add a basic command interpreter or shell
- [ ] Add an identity and capability administration service
- [ ] Load services from the Liberation system area rather than special-case staging
- [ ] Define executable format policy for user programs
- [ ] Add service discovery and launch policy
- [ ] Add persistent filesystem-backed loading
- [ ] Add service restart policy for crashes
- [ ] Add shell commands for file, service, capability, and log operations
- [ ] Add a small startup manifest or init script

## Immediate Goal: Scheduler Hardening

Success criteria:

- [x] Spinner continues
- [x] Timer line goes live
- [x] Tick count increases
- [x] Serial heartbeat appears
- [x] No reboot or fault after interrupts are enabled
- [x] A non-yielding kernel thread can be preempted by the timer path
- [x] The heartbeat thread still runs while the busy worker spins forever

## Kernel Milestone: Minimum Real Kernel

- [x] Scheduler runs after kernel init instead of ending in a simple idle-only path
- [x] Kernel can create and schedule basic kernel threads
- [x] Kernel can preempt kernel threads from timer interrupts
- [x] Kernel can reclaim terminated kernel-thread objects and stacks safely
- [x] Lower-priority ready tasks can still reach dispatch under sustained higher-priority busy load
- [x] Kernel process objects exist above threads and can be reclaimed when transient work exits
- [ ] Kernel can enter and return from user mode safely
- [x] Scheduler activates the selected process root and restores the kernel root on return
- [x] Transient scheduler processes can own and later destroy distinct memory-manager address spaces
- [ ] Kernel IPC can block, wake, reply, and time out correctly
- [ ] Capabilities are enforced by object type and operation
- [ ] Exceptions and faults are reported against the owning task
- [ ] Executable loading is available for user programs

## First True Userland Milestone

- [ ] Memory manager runs as a real service under the scheduler
- [ ] Real endpoint delivery replaces bootstrap-only request dispatch
- [ ] Init or service manager starts and supervises services
- [ ] Service namespace and discovery are available
- [ ] Filesystem and driver-host path exists for loading and runtime work

## Usable System Milestone

- [ ] Console and terminal service available
- [ ] Shell available
- [ ] User commands available for files, services, logs, and capabilities
- [ ] Persistent loading from storage works
- [ ] Service crash and restart behaviour is defined
- Validate ephemeral process exit, reaping, and address-space destroy after the 0.2.9 process-creation gating change.
