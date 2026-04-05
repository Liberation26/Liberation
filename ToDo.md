# ToDo

- Review the new `user-scaffold-trampoline-ready` diagnostic plus the trampoline-ready one-time log to confirm the blocked scaffold now carries a dedicated kernel-entry return address before the live gate closes.
- Confirm the detailed scaffold process/task traces now report a non-zero `user-kentry=` value alongside the existing user entry, stack, selector, `RFLAGS`, and `user-frame-sp=` metadata.
- Verify the live-gate-closed one-time line still appears only after `frame-ready` and `trampoline-ready` have both completed, while `user-scaffold-live` remains `0`.
- Review the new `user-scaffold-frame-ready`, `user-scaffold-trampoline-ready`, `user-scaffold-blocked`, `user-scaffold-reblocked`, and `user-scaffold-live` diagnostics to confirm the scaffold remains intentionally blocked and non-live after the kernel-entry trampoline is staged.
- Review the `user-scaffold-ready`, `user-scaffold-proc`, and `user-scaffold-task` diagnostics to confirm the blocked user-transition scaffold stays present while transient worker processes continue to be created and reaped.
- Review the scheduler `max-run-slice`, `max-ready-delay`, and `max-wake-delay` counters against the BusyWorker and ephemeral-worker logs.
- Review whether `stack-pool-used` stays bounded while transient workers continue to be created and reaped.
- Continue hardening scheduler/user transition work now that the scheduler can report runtime, dispatch latency, run-slice behaviour, explicit scaffold blocking state, selector metadata, a prepared user return-frame stack pointer, and a dedicated non-live kernel-entry trampoline address.
- Wire the user-transition scaffold from trampoline-ready/live-gated into a real ring transition path that calls `LosKernelSchedulerMarkUserTransitionScaffoldLive()` only when `iretq` entry is implemented.
