# ToDo

- Review the new scheduler `max-run-slice`, `max-ready-delay`, and `max-wake-delay` counters against the BusyWorker and ephemeral-worker logs.
- Review whether `stack-pool-used` stays bounded while transient workers continue to be created and reaped.
- Continue hardening scheduler/user transition work now that the scheduler can report runtime, dispatch latency, and run-slice behaviour.
