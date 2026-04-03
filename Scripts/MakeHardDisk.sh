#!/usr/bin/env bash
set -euo pipefail

RootDir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BuildDir="${RootDir}/Build"
ImageDir="${RootDir}/Image"
BootFile="${ImageDir}/EFI/BOOT/LOADERX64.EFI"
MonitorFile="${ImageDir}/EFI/BOOT/MONITORX64.EFI"
KernelFile="${ImageDir}/EFI/BOOT/KERNELX64.ELF"
StartupScript="${BuildDir}/startup.nsh"
EspFile="${BuildDir}/LiberationEsp.img"
DiskFile="${BuildDir}/LiberationDisk.img"

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

RequireTool truncate
RequireTool parted
RequireTool mkfs.fat
RequireTool mmd
RequireTool mcopy
RequireTool dd
RequireTool mkdir
RequireTool rm
RequireTool cat

if [[ ! -f "${BootFile}" ]]; then
    echo "[Liberation] Missing installed loader file: ${BootFile}"
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
rm -f "${EspFile}" "${DiskFile}"

echo "[Liberation] Creating EFI system partition image..."
truncate -s "${EspSizeMiB}M" "${EspFile}"
mkfs.fat -F 32 "${EspFile}" >/dev/null
mmd -i "${EspFile}" ::/EFI ::/EFI/BOOT
mcopy -i "${EspFile}" "${BootFile}" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "${EspFile}" "${MonitorFile}" ::/EFI/BOOT/MONITORX64.EFI
mcopy -i "${EspFile}" "${KernelFile}" ::/EFI/BOOT/KERNELX64.ELF
cat > "${StartupScript}" <<'EOS'
map -r
if exist fs0:\EFI\BOOT\BOOTX64.EFI then
  fs0:\EFI\BOOT\BOOTX64.EFI
endif
if exist fs1:\EFI\BOOT\BOOTX64.EFI then
  fs1:\EFI\BOOT\BOOTX64.EFI
endif
if exist fs2:\EFI\BOOT\BOOTX64.EFI then
  fs2:\EFI\BOOT\BOOTX64.EFI
endif
EOS
mcopy -i "${EspFile}" "${StartupScript}" ::/startup.nsh

echo "[Liberation] Creating GPT hard disk image..."
truncate -s "${DiskSizeMiB}M" "${DiskFile}"
parted -s "${DiskFile}" mklabel gpt
parted -s "${DiskFile}" unit MiB mkpart EFI FAT32 "${EspStartMiB}" "${EspEndMiB}"
parted -s "${DiskFile}" set 1 esp on
parted -s "${DiskFile}" set 1 boot on

echo "[Liberation] Writing EFI system partition into hard disk image..."
dd if="${EspFile}" of="${DiskFile}" bs=1M seek="${EspStartMiB}" conv=notrunc status=none

echo "[Liberation] UEFI hard disk image created: ${DiskFile}"
