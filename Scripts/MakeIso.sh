#!/usr/bin/env bash
set -euo pipefail

RootDir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BuildDir="${RootDir}/Build"
ImageDir="${RootDir}/Image"
IsoRootDir="${BuildDir}/IsoRoot"
IsoFile="${BuildDir}/Liberation.iso"
EspFile="${IsoRootDir}/Efiboot.img"
BootFile="${ImageDir}/EFI/BOOT/BOOTX64.EFI"
LoaderFile="${ImageDir}/EFI/BOOT/LOADERX64.EFI"
MonitorFile="${ImageDir}/EFI/BOOT/MONITORX64.EFI"
KernelFile="${ImageDir}/EFI/BOOT/KERNELX64.ELF"

RequireTool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[Liberation] Missing required tool: $1"
        exit 1
    fi
}

RequireTool xorriso
RequireTool mkfs.fat
RequireTool mmd
RequireTool mcopy
RequireTool dd
RequireTool mkdir
RequireTool rm
RequireTool cp

if [[ ! -f "${BootFile}" ]]; then
    echo "[Liberation] Missing boot file: ${BootFile}"
    exit 1
fi

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

rm -rf "${IsoRootDir}"
mkdir -p "${IsoRootDir}/EFI/BOOT"
rm -f "${IsoFile}"

echo "[Liberation] Copying BOOTX64.EFI, LOADERX64.EFI, MONITORX64.EFI, and KERNELX64.ELF into ISO tree..."
cp "${BootFile}" "${IsoRootDir}/EFI/BOOT/BOOTX64.EFI"
cp "${LoaderFile}" "${IsoRootDir}/EFI/BOOT/LOADERX64.EFI"
cp "${MonitorFile}" "${IsoRootDir}/EFI/BOOT/MONITORX64.EFI"
cp "${KernelFile}" "${IsoRootDir}/EFI/BOOT/KERNELX64.ELF"

echo "[Liberation] Creating EFI boot image for ISO..."
dd if=/dev/zero of="${EspFile}" bs=1M count=8 status=none
mkfs.fat -F 12 "${EspFile}" >/dev/null
mmd -i "${EspFile}" ::/EFI ::/EFI/BOOT
mcopy -i "${EspFile}" "${BootFile}" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "${EspFile}" "${LoaderFile}" ::/EFI/BOOT/LOADERX64.EFI
mcopy -i "${EspFile}" "${MonitorFile}" ::/EFI/BOOT/MONITORX64.EFI
mcopy -i "${EspFile}" "${KernelFile}" ::/EFI/BOOT/KERNELX64.ELF

echo "[Liberation] Building UEFI ISO image..."
xorriso -as mkisofs \
    -R -J \
    -V "LIBERATION" \
    -eltorito-alt-boot \
    -e Efiboot.img \
    -no-emul-boot \
    -o "${IsoFile}" \
    "${IsoRootDir}" >/dev/null

echo "[Liberation] ISO created: ${IsoFile}"
