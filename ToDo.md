# ToDo

- Review the new `user-scaffold-frame-ready` diagnostic plus the frame-ready one-time log to confirm the scaffold now carries a prepared `iretq` return frame template before the live gate closes.
- Confirm the detailed scaffold process/task traces now report a non-zero `user-frame-sp=` value alongside the existing user entry, stack, selector, and `RFLAGS` metadata.
- Review the new `user-scaffold-descriptor-ready`, `user-scaffold-frame-ready`, `user-scaffold-blocked`, `user-scaffold-reblocked`, and `user-scaffold-live` diagnostics to confirm the scaffold remains intentionally blocked and non-live after frame-ready.
- Review the `user-scaffold-ready`, `user-scaffold-proc`, and `user-scaffold-task` diagnostics to confirm the blocked user-transition scaffold stays present while transient worker processes continue to be created and reaped.
- Review the scheduler `max-run-slice`, `max-ready-delay`, and `max-wake-delay` counters against the BusyWorker and ephemeral-worker logs.
- Review whether `stack-pool-used` stays bounded while transient workers continue to be created and reaped.
- Continue hardening scheduler/user transition work now that the scheduler can report runtime, dispatch latency, run-slice behaviour, explicit scaffold blocking state, selector metadata, and a prepared user return-frame stack pointer.
- Wire the user-transition scaffold from frame-ready/live-gated into a real ring transition path that calls `LosKernelSchedulerMarkUserTransitionScaffoldLive()` only when `iretq` entry is implemented.
