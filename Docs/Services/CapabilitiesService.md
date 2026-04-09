<!--
File Name: CapabilitiesService.md
File Version: 0.3.13
Author: OpenAI
Email: dave66samaa@gmail.com
Creation Timestamp: 2026-04-07T11:02:18Z
Last Update Timestamp: 2026-04-07T13:15:00Z
Operating System Name: Liberation OS
Purpose: Documents Liberation OS design, behavior, usage, or integration details.
-->

# Capabilities Service

This delivery corrects the ownership model for first-service startup: the kernel does not directly run the capabilities service. Instead, the kernel hands InitCommand a service request that tells it to load and run `\LIBERATION\SERVICES\CAPSMGR.ELF`.

What exists now:
- `Config/Services/Capabilities.cfg`
- `Source/Include/Public/CapabilitiesServiceAbi.h`
- `Source/Include/Public/InitCommandAbi.h`
- `Source/Src/Arch/X64/Services/Capabilities/*`
- `Image/LIBERATION/SERVICES/CAPSMGR.ELF` built by `Scripts/BuildBoot.sh`
- `Image/EFI/BOOT/CAPABILITIES.CFG` copied from the config directory at build time

ESP bootstrap flow:
- monitor opens `\EFI\BOOT\CAPABILITIES.CFG` from the ESP
- monitor parses `block ...`, repeated `grant ...`, `assign ...`, and `endblock`
- parsed grant entries are normalized into `LOS_CAPABILITIES_BOOTSTRAP_CONTEXT`
- the kernel receives that structured snapshot through `LOS_BOOT_CONTEXT`
- the kernel-to-init contract now includes a `LOS_INIT_COMMAND_SERVICE_REQUEST`
- InitCommand is instructed to load and run `\LIBERATION\SERVICES\CAPSMGR.ELF`
- the capabilities service entry accepts an imported bootstrap context in `RDI` and flattens the active grants into its registry for query and enumeration

Bootstrap context model now contains:
- grant blocks describing profile-owned capability arrays
- assignment records mapping principals like users, services, and tasks onto profile names
- grant event records showing imported bootstrap grant events and actor provenance

Current scope:
- ESP-backed profile/grant bootstrap source
- bounded block, assignment, and event arrays suitable for early boot ABI use
- deterministic capability ids, grant ids, and event ids assigned during monitor parse
- kernel-visible structured grant snapshot and init-visible service request contract
- service-side import path for kernel-provided structured bootstrap policy
- InitCommand reporting for profiles, assignments, imported grant events, and the CAPSMGR load request

What this is **not** yet:
- not yet a persistent runtime ledger with append-only history records
- not yet proven at boot as a fully working init-driven ELF loader path
- not yet backed by endpoint transport or kernel-issued grant/revoke/suspend requests
- not yet authoritative for task capability enforcement


## 0.3.14 init launch contract

The kernel now embeds `CAPSMGR.ELF` into the bootstrap package and supplies that image to `InitCommand` as the first service request image payload. `InitCommand` validates the embedded ELF image before attempting the first service launch, so the control model is explicitly **kernel requests / init loads and runs** rather than kernel direct execution.


## Secure endpoint bootstrap

`CAPSMGR` is now requested through an init-owned service request that carries a `LOS_SECURE_ENDPOINT_POLICY`. The current bootstrap contract sets the capabilities manager channel to encrypted mutual-auth mode with replay protection and session derivation. This update lands the ABI and validation path only. It does **not** yet implement full cryptographic handshakes or payload encryption at runtime.


<!-- Update: In 0.3.18 CAPSMGR now keeps a runtime grant ledger and event ledger, supports grant/revoke/suspend/restore primitives, and performs a startup self-test after init brings it online. -->

Init-owned bootstrap launch in 0.3.17:
- kernel supplies embedded CAPSMGR image, bootstrap context, and service-state address
- init invokes the callable bootstrap entry directly
- CAPSMGR performs one-shot bootstrap import, marks state ONLINE, and returns to init
- init treats ONLINE state as proof that the capability system actually ran


Runtime-authority work in 0.3.18:
- keeps a runtime grant table separate from the imported bootstrap snapshot
- keeps a runtime event ledger with imported, created, suspended, restored, and revoked events
- adds mutation request/result ABI structs for grant, revoke, suspend, and restore
- runs a startup self-test that proves query, grant, suspend, restore, and revoke through CAPSMGR itself


## Launch authority and bootstrap policy

As of version 0.3.22, InitCommand will only honour the kernel service request for `CAPSMGR.ELF` when the imported bootstrap capability policy assigns the `init` task to a profile that holds both `service.start` and `service.bootstrap.import`. This makes the first CAPSMGR launch capability-gated rather than unconditional.

The recommended early profiles are:

- `service.init` for the init task
- `service.capability_manager` for the capability manager service

The capability manager registry also seeds `service.start`, `service.bootstrap.import`, and `capability.query` so runtime checks can be expressed in the same namespace model used by the ESP policy.

- 0.3.22: route memory.allocate and memory.query requests through CAPSMGR before the memory manager will service them.

## 0.3.27 transport export and binding

`CAPSMGR` now publishes a transport export block that contains the live request and response endpoint metadata plus mailbox addresses and a transport generation. Clients such as the memory manager bind against that exported transport instead of depending on the internal service state layout. A stale binding is detected by generation mismatch and the client must rebind before retrying the request.



## 0.3.27 named transport connection

`CAPSMGR` now publishes a named transport export for service `capsmgr`, and clients connect through a transport connection request rather than rebinding directly against a private state layout or an anonymous export alone. The connection request carries the caller service name, required endpoint role, and required transport flags. The capability service validates the target service name and transport shape before returning a binding.

This is still early-boot shared-memory transport, but it is now shaped like a real inter-service connect step: **discover service name -> validate endpoint contract -> bind transport -> exchange messages**.


## Transport integrity
- authenticated request and response tags are now computed per transport session
- monotonic request and response sequences are now checked to reject replayed messages
- current transport protection uses per-session keyed SipHash tags over the mailbox message envelope


## 0.3.30 session establishment
- transport connect now binds explicit endpoint identity for the caller and service
- connect performs a lightweight session-establishment exchange using client nonce, server nonce, and derived per-session auth keys
- requests are accepted only for the active established session, with identity-bound transport flags exported by CAPSMGR


## 0.3.31 transport security
- control-class endpoint transport now advertises a minimum confidential security mode.
- clients request a target endpoint class and desired security mode during connect.
- CAPSMGR rejects connects that do not satisfy endpoint-class policy or minimum security mode.
- request and response payloads are now transformed in-session before transit and restored after authenticated receipt.
- this confidentiality layer is policy-driven per endpoint class and currently enabled for the CAPSMGR control endpoint.
