#!/usr/bin/env bash
set -euo pipefail

RootDir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BuildDir="${RootDir}/Build"
ImageDir="${RootDir}/Image"
BootDir="${ImageDir}/EFI/BOOT"

RequireTool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[Liberation] Missing required tool: $1"
        exit 1
    fi
}

RequireTool rm
RequireTool mkdir


echo "[Liberation] Cleaning generated build output..."
rm -rf "${BuildDir}"
mkdir -p "${BuildDir}"

echo "[Liberation] Removing generated EFI payloads..."
rm -f \
    "${BootDir}/BOOTX64.EFI" \
    "${BootDir}/LOADERX64.EFI" \
    "${BootDir}/MONITORX64.EFI" \
    "${BootDir}/KERNELX64.ELF" \
    "${BootDir}/KERNELX64.BIN" \
    "${BootDir}/BOOTINFO.TXT" \
    "${BootDir}/Boot.psf" \
    "${ImageDir}/LIBERATION/KERNELX64.ELF"

echo "[Liberation] Clean complete. The next ISO run will rebuild all output from scratch."
