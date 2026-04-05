# ToDo

- Review the new `user-scaffold-descriptor-ready` diagnostic plus the descriptor-ready one-time log to confirm the scaffold now carries future user CS/SS/RFLAGS values before the live gate closes.
- Confirm the detailed scaffold process/task traces now report `user-cs=0x000000000000001B`, `user-ss=0x0000000000000023`, and `user-rflags=0x0000000000000202`.
- Review the new `user-scaffold-blocked`, `user-scaffold-reblocked`, and `user-scaffold-live` diagnostics to confirm the scaffold remains intentionally blocked and non-live after descriptor-ready.
- Review the `user-scaffold-ready`, `user-scaffold-proc`, and `user-scaffold-task` diagnostics to confirm the blocked user-transition scaffold stays present while transient worker processes continue to be created and reaped.
- Review the scheduler `max-run-slice`, `max-ready-delay`, and `max-wake-delay` counters against the BusyWorker and ephemeral-worker logs.
- Review whether `stack-pool-used` stays bounded while transient workers continue to be created and reaped.
- Continue hardening scheduler/user transition work now that the scheduler can report runtime, dispatch latency, run-slice behaviour, explicit scaffold blocking state, and descriptor-ready user entry metadata.
- Wire the user-transition scaffold from descriptor-ready/live-gated into a real ring transition path that calls `LosKernelSchedulerMarkUserTransitionScaffoldLive()` only when `iretq` entry is implemented.
