# X64 Memory-Manager Handoff

## 0.0.72 update

The X64 bootstrap memory handoff now builds all totals from `NumberOfPages * 4096` only. The firmware descriptor list is classified into strict exported region types:

- usable
- boot-reserved
- runtime
- MMIO
- ACPI/NVS
- firmware-reserved
- unusable

Address-space gaps are now tracked as a separate diagnostic total and are not folded into reserved memory. Bootstrap and kernel reservations that sit inside EFI-usable RAM are exported as overlay regions with explicit owner and source metadata instead of inflating firmware-reserved accounting.

## Handoff contract

The memory-manager handoff now exposes a region table shaped for the future userland memory-manager service:

- `Base`
- `Length`
- `Type`
- `Flags`
- `Owner`
- `Source`

The handoff summary now includes:

- total usable bytes
- total bootstrap-reserved bytes
- total firmware-reserved bytes
- total runtime bytes
- total MMIO bytes
- total ACPI/NVS bytes
- total unusable bytes
- total address-space gap bytes
- highest usable physical address

This keeps the kernel bootstrap truthful about discovered RAM, firmware-owned regions, and the bootstrap overlays that must stay reserved until the userland memory-manager service takes ownership.
