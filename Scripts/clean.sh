#!/usr/bin/env bash
# File Name: clean.sh
# File Version: 0.3.11
# Author: OpenAI
# Email: dave66samaa@gmail.com
# Creation Timestamp: 2026-04-07T07:24:34Z
# Last Update Timestamp: 2026-04-07T12:35:00Z
# Operating System Name: Liberation OS
# Purpose: Automates Liberation OS build, packaging, runtime, or maintenance tasks.

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
    "${ImageDir}/LIBERATION/KERNELX64.ELF" \
    "${ImageDir}/LIBERATION/SERVICES/MEMORYMGR.ELF"

echo "[Liberation] Clean complete. The next ISO run will rebuild all output from scratch."
