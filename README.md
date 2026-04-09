LOS 0.4.85

This update fixes the two issues that were most obvious in the latest boot screenshot. The kernel screen now uses a larger default font again so the framebuffer console is readable at a glance, and init no longer returns straight back to the kernel after the first ring-3 proof path succeeds. Instead, once the memory manager is already online and CAPSMGR plus SHELL have been bootstrapped, init stays resident in a quiet parked state.

That resident init state is still not the finished design. The current tree is still using bootstrap-callable service bring-up, not the final separate scheduled-service launch path. The intended end state remains: the memory manager is already running before init, init is the first userland session manager, it launches CAPSMGR and the shell as real long-lived user services, and the shell invokes LOGIN externally when no user session is active.
