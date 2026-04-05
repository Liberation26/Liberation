- Added an explicit `complete` scheduler-scaffold stage after `live-gate-closed` and delayed normal scheduler serial output until `seal-ready`, so scheduler logs now stay quiet during early scaffold assembly and then come alive only once the staged handoff metadata is fully formed.
- Added an explicit `handoff-ready` user-transition scaffold stage after `seal-ready`.
- The scheduler now records and verifies the final blocked scaffold task handoff stack pointer as non-zero `user-handoff-sp` metadata, matching the staged chain stack pointer and current saved execution stack pointer.
- Heartbeat output now exposes `user-scaffold-handoff-ready`, and detailed process/task traces now expose `user-handoff-sp=` so the serial log can prove the blocked scaffold carries a final saved handoff stack before the live gate closes.
- The live gate still remains closed; both live-gating and future `LIVE` promotion now require the prepared handoff-stack metadata.

# ToDo

- Review the new `user-scaffold-handoff-ready` diagnostic plus the handoff-ready one-time log to confirm the blocked scaffold now carries a verified non-zero `user-handoff-sp` value before the live gate closes.
- Confirm the detailed scaffold process/task traces now report a non-zero `user-handoff-sp=` value alongside the existing chain, contract, and seal metadata.
- Verify the live-gate-closed one-time line still appears only after `seal-ready` and `handoff-ready` have both completed, while `user-scaffold-live` remains `0`.

- Added an explicit `contract-ready` user-transition scaffold stage after `chain-ready`.
- The scheduler now re-reads the staged `iretq` frame plus bridge/kernel-entry chain, computes a frozen non-zero `user-contract` signature over that metadata, and records it on both the scaffold process and task.
- Heartbeat output now exposes `user-scaffold-contract-ready`, and detailed process/task traces now expose `user-contract=` so the serial log can prove that the staged handoff metadata was verified before the real ring-transition path exists.
- The live gate still remains closed; both live-gating and future `LIVE` promotion now require the prepared contract metadata.

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
- Finish the real kernel-to-user execution path on top of the now scheduler-complete scaffold: install a TSS/RSP0 path, map a real user code page plus stack into the owned user address space, switch the bridge from non-live scaffolding to an actual `iretq` entry, and only then allow `LosKernelSchedulerMarkUserTransitionScaffoldLive()` to dispatch user work.
- Wire the user-transition scaffold from bridge-ready/live-gated into a real ring transition path that calls `LosKernelSchedulerMarkUserTransitionScaffoldLive()` only when the actual `iretq` entry path exists.

- Added an explicit `seal-ready` user-transition scaffold stage after `contract-ready`.
- The scheduler now writes and verifies a non-zero `user-seal` marker on the staged dispatch chain, records it on both the scaffold process and task, and reports `user-scaffold-seal-ready` plus `user-seal=` in diagnostics.
- The live gate still remains closed; both live-gating and future `LIVE` promotion now require the prepared seal metadata.

# ToDo

- Review the new `user-scaffold-seal-ready` diagnostic plus the seal-ready one-time log to confirm the blocked scaffold now carries a verified non-zero `user-seal` marker before the live gate closes.
- Confirm the detailed scaffold process/task traces now report a non-zero `user-seal=` value alongside the existing chain and contract metadata.
- Verify the live-gate-closed one-time line still appears only after `contract-ready` and `seal-ready` have both completed, while `user-scaffold-live` remains `0`.
