LOS 0.4.8

This update pivots the shell toward actual command input. Internal commands execute inside the shell service, while external names resolve to command images under `\LIBERATION\COMMANDS`.


Update 0.4.11: the shell now requires an external login command before opening the normal command path.


This package updates shell login so CAPSMGR decides whether a named user is permitted to log in via the `session.login` capability.


0.4.12 adds the external string library used by the shell after login.


0.4.13 adds a generic user-image loader call shape so shell libraries and commands can share one runtime dispatch path.

0.4.20 wires login and string image calls through one generic shell-side bootstrap image dispatcher while keeping the user-image ABI in place for the later real loader path.

0.4.21 adds a disk-backed shell image-loader front end: the shell now reads ELF images from disk, validates them, and prepares a structured execution context for a real loader hook before falling back to bootstrap dispatch.

0.4.27 adds a shell-side direct execution bridge that materializes PT_LOAD segments into a staged runtime image, zero-fills segment tails, and emits execution-entry/return debug tracing.

0.4.28 adds the first isolated user-execution path: a memory-manager-owned user address-space descriptor, staged PT_LOAD mapping into per-process buffers, ring-3 transition frame definitions, and a shell-side preference for isolated execution before falling back to the direct bridge.

0.4.29 extends isolated user execution with memory-manager-owned mapping records, staged call-block ownership, and a real shell-side user-mode handoff wrapper.

0.4.30 adds a kernel resume handoff frame for ring-3 execution, call-block completion staging, and memory-manager-owned completion copy-back support.

0.4.31 makes disk-backed ELF loading the normal shell path by embedding installed user-image binaries into the shell service, serving them through the file-read hook, and removing bootstrap fallback from normal command and library dispatch.

0.4.32 moves mapped user-address-space ownership into the memory manager by adding explicit address-space descriptors, mapping tables, activation hooks, and copy-back completion handling owned by the memory-manager runtime.

0.4.33 hardens the ring-3 handoff by fixing the user-mode context offsets, passing the user call-block pointer correctly in RDI, validating selectors and stack alignment before iretq, and adding an explicit kernel-side return completion path.

0.4.34 removes bootstrap fallback from login and string handling by making the shell require installed disk-backed LOGIN.ELF and STRING.ELF images, treating missing or failed loads as hard shell-call failures.

0.4.35 unifies command and library invocation around the same LOS_USER_IMAGE_CALL ABI and stages completion/result copy-back into shell-owned buffers before writing results to shell-visible memory.
