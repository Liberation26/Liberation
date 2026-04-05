# ToDo

- Review the new scheduler `idle-ticks`, `busy-ticks`, and per-task/process runtime counters against the BusyWorker and ephemeral-worker logs.
- Continue hardening scheduler/user transition work now that the scheduler has direct-claim stack-pool reuse and first-stage runtime accounting.
