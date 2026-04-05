# ToDo

- Review the `user-scaffold-ready`, `user-scaffold-proc`, and `user-scaffold-task` diagnostics to confirm the blocked user-transition scaffold stays present while transient worker processes continue to be created and reaped.
- Review the new scheduler `max-run-slice`, `max-ready-delay`, and `max-wake-delay` counters against the BusyWorker and ephemeral-worker logs.
- Review whether `stack-pool-used` stays bounded while transient workers continue to be created and reaped.
- Continue hardening scheduler/user transition work now that the scheduler can report runtime, dispatch latency, and run-slice behaviour.
- Wire the user-transition scaffold launch-requested stage into a real ring-transition entry path.

- Wire the user-transition scaffold from launch-requested/entry-ready into a real ring transition path that marks the task live only when `iretq` entry is implemented.
