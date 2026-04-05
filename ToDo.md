# ToDo

- Review the new `user-scaffold-chain-ready` diagnostic plus the chain-ready one-time log to confirm the blocked scaffold now carries a dedicated future dispatch-chain stack pointer before the live gate closes.
- Confirm the detailed scaffold process/task traces now report a non-zero `user-chain-sp=` value alongside the existing user entry, stack, selector, `RFLAGS`, `user-frame-sp=`, `user-kentry=`, and `user-bridge=` metadata.
- Verify the live-gate-closed one-time line still appears only after `frame-ready`, `trampoline-ready`, `bridge-ready`, and `chain-ready` have all completed, while `user-scaffold-live` remains `0`.
- Review the new `user-scaffold-bridge-ready` diagnostic plus the bridge-ready one-time log to confirm the blocked scaffold now carries a dedicated future dispatch-bridge address before the live gate closes.
- Confirm the detailed scaffold process/task traces now report a non-zero `user-bridge=` value alongside the existing user entry, stack, selector, `RFLAGS`, `user-frame-sp=`, and `user-kentry=` metadata.
- Verify the live-gate-closed one-time line still appears only after `frame-ready`, `trampoline-ready`, and `bridge-ready` have all completed, while `user-scaffold-live` remains `0`.
- Review the new `user-scaffold-trampoline-ready`, `user-scaffold-bridge-ready`, `user-scaffold-blocked`, `user-scaffold-reblocked`, and `user-scaffold-live` diagnostics to confirm the scaffold remains intentionally blocked and non-live after the future dispatch bridge is staged.
- Review the `user-scaffold-ready`, `user-scaffold-proc`, and `user-scaffold-task` diagnostics to confirm the blocked user-transition scaffold stays present while transient worker processes continue to be created and reaped.
- Review the scheduler `max-run-slice`, `max-ready-delay`, and `max-wake-delay` counters against the BusyWorker and ephemeral-worker logs.
- Review whether `stack-pool-used` stays bounded while transient workers continue to be created and reaped.
- Continue hardening scheduler/user transition work now that the scheduler can report runtime, dispatch latency, run-slice behaviour, explicit scaffold blocking state, selector metadata, a prepared user return-frame stack pointer, a dedicated non-live kernel-entry address, and a staged future dispatch bridge address.
- Wire the user-transition scaffold from bridge-ready/live-gated into a real ring transition path that calls `LosKernelSchedulerMarkUserTransitionScaffoldLive()` only when the actual `iretq` entry path exists.
