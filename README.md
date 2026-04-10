- 0.4.105: fixed the first user-task ring-3 handoff to use a dedicated chain stack slot and runtime bridge/kernel-entry addresses, and tightened scaffold validation before live dispatch.

LOS 0.4.105

This update removes bootstrap-only diagnostics from the early x64 kernel handoff path, stops compiler-wide function instrumentation in the kernel build, and keeps the real kernel-side crash reporter in place for later runtime faults.
