# ToDo

- Review the `user-scaffold-ready`, `user-scaffold-proc`, and `user-scaffold-task` diagnostics to confirm the blocked user-transition scaffold stays present while transient worker processes continue to be created and reaped.
- Review the new scheduler `max-run-slice`, `max-ready-delay`, and `max-wake-delay` counters against the BusyWorker and ephemeral-worker logs.
- Review whether `stack-pool-used` stays bounded while transient workers continue to be created and reaped.
- Continue hardening scheduler/user transition work now that the scheduler can report runtime, dispatch latency, and run-slice behaviour.
- Wire the user-transition scaffold from entry-ready/live-gated into a real ring transition path that calls `LosKernelSchedulerMarkUserTransitionScaffoldLive()` only when `iretq` entry is implemented.
