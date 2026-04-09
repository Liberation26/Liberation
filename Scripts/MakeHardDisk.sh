#!/usr/bin/env bash
# File Name: MakeHardDisk.sh
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
LiberationDir="${ImageDir}/LIBERATION"
LoaderFile="${BootDir}/LOADERX64.EFI"
MonitorFile="${BootDir}/MONITORX64.EFI"
KernelFile="${LiberationDir}/KERNELX64.ELF"
BootFontFile="${LiberationDir}/FONTS/Boot.psf"
MemoryManagerServiceFile="${LiberationDir}/SERVICES/MEMORYMGR.ELF"
EspFile="${BuildDir}/LiberationEsp.img"
DiskFile="${BuildDir}/LiberationDisk.img"
BootInfoFile="${BuildDir}/BOOTINFO.TXT"

DiskSizeMiB=64
EspSizeMiB=62
EspStartMiB=1
EspEndMiB=63

RequireTool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[Liberation] Missing required tool: $1"
        exit 1
    fi
}

WriteUtf16LeTextFile() {
    local OutputFile="$1"
    local Text="$2"
    python3 - "$OutputFile" "$Text" <<'PY'
import sys
from pathlib import Path
Path(sys.argv[1]).write_bytes(sys.argv[2].encode('utf-16le'))
PY
}

RequireTool truncate
RequireTool parted
RequireTool mkfs.fat
RequireTool mmd
RequireTool mcopy
RequireTool dd
RequireTool mkdir
RequireTool rm
RequireTool python3

if [[ ! -f "${LoaderFile}" ]]; then
    echo "[Liberation] Missing installed loader file: ${LoaderFile}"
    exit 1
fi

if [[ ! -f "${MonitorFile}" ]]; then
    echo "[Liberation] Missing monitor file: ${MonitorFile}"
    exit 1
fi

if [[ ! -f "${KernelFile}" ]]; then
    echo "[Liberation] Missing kernel file: ${KernelFile}"
    exit 1
fi

mkdir -p "${BuildDir}"
rm -f "${EspFile}" "${DiskFile}" "${BootInfoFile}"

WriteUtf16LeTextFile "${BootInfoFile}" $'Booting from direct hard-disk media\r\n'

echo "[Liberation] Creating EFI system partition image..."
truncate -s "${EspSizeMiB}M" "${EspFile}"
mkfs.fat -F 32 "${EspFile}" >/dev/null
mmd -i "${EspFile}" ::/EFI ::/EFI/BOOT ::/LIBERATION
if [[ -d "${LiberationDir}/FONTS" ]]; then
    mmd -i "${EspFile}" ::/LIBERATION/FONTS
fi
if [[ -d "${LiberationDir}/SERVICES" ]]; then
    mmd -i "${EspFile}" ::/LIBERATION/SERVICES
fi
mcopy -i "${EspFile}" "${LoaderFile}" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "${EspFile}" "${MonitorFile}" ::/EFI/BOOT/MONITORX64.EFI
mcopy -i "${EspFile}" "${KernelFile}" ::/EFI/BOOT/KERNELX64.ELF
mcopy -i "${EspFile}" "${KernelFile}" ::/LIBERATION/KERNELX64.ELF
mcopy -i "${EspFile}" "${BootInfoFile}" ::/EFI/BOOT/BOOTINFO.TXT
if [[ -f "${BootFontFile}" ]]; then
    mcopy -i "${EspFile}" "${BootFontFile}" ::/EFI/BOOT/Boot.psf
    mcopy -i "${EspFile}" "${BootFontFile}" ::/LIBERATION/FONTS/Boot.psf
fi
if [[ -f "${MemoryManagerServiceFile}" ]]; then
    mcopy -i "${EspFile}" "${MemoryManagerServiceFile}" ::/LIBERATION/SERVICES/MEMORYMGR.ELF
fi

echo "[Liberation] Creating GPT hard disk image..."
truncate -s "${DiskSizeMiB}M" "${DiskFile}"
parted -s "${DiskFile}" mklabel gpt
parted -s "${DiskFile}" unit MiB mkpart EFI FAT32 "${EspStartMiB}" "${EspEndMiB}"
parted -s "${DiskFile}" set 1 esp on
parted -s "${DiskFile}" set 1 boot on

echo "[Liberation] Writing EFI system partition into hard disk image..."
dd if="${EspFile}" of="${DiskFile}" bs=1M seek="${EspStartMiB}" conv=notrunc status=none

echo "[Liberation] UEFI hard disk image created: ${DiskFile}"
