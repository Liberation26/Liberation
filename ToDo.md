# ToDo

- Review the new `user-scaffold-blocked` and `user-scaffold-reblocked` diagnostics to confirm the scaffold stays intentionally blocked after `entry-ready` and that the new guard remains quiet during normal runs.
- Review the `user-scaffold-ready`, `user-scaffold-proc`, and `user-scaffold-task` diagnostics to confirm the blocked user-transition scaffold stays present while transient worker processes continue to be created and reaped.
- Review the new scheduler `max-run-slice`, `max-ready-delay`, and `max-wake-delay` counters against the BusyWorker and ephemeral-worker logs.
- Review whether `stack-pool-used` stays bounded while transient workers continue to be created and reaped.
- Continue hardening scheduler/user transition work now that the scheduler can report runtime, dispatch latency, run-slice behaviour, and explicit scaffold blocking state.
- Wire the user-transition scaffold from entry-ready/live-gated into a real ring transition path that calls `LosKernelSchedulerMarkUserTransitionScaffoldLive()` only when `iretq` entry is implemented.
