#!/usr/bin/env bash
# File Name: RunDir.sh
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
OvmfCode="/usr/share/OVMF/OVMF_CODE_4M.fd"
OvmfVarsSource="/usr/share/OVMF/OVMF_VARS_4M.fd"
OvmfVarsLocal="${BuildDir}/OVMF_VARS_DIR_4M.fd"
StartupScript="${ImageDir}/startup.nsh"
BootInfoFile="${BootDir}/BOOTINFO.TXT"
HostLogFile="${BuildDir}/RunDir-Host.log"

StripAnsiToFile() {
    local OutputFile="$1"

    if command -v perl >/dev/null 2>&1; then
        perl -pe 's/\e\[[0-9;]*m//g' >> "${OutputFile}"
    else
        cat >> "${OutputFile}"
    fi
}

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

echo "[Liberation] Starting directory build and run..."

TryMaximizeQemuWindow() {
    local WindowTitle="$1"
    local Attempt

    if ! command -v wmctrl >/dev/null 2>&1; then
        return
    fi

    (
        for Attempt in $(seq 1 50); do
            sleep 0.2
            if wmctrl -r "${WindowTitle}" -b add,maximized_vert,maximized_horz >/dev/null 2>&1; then
                exit 0
            fi
        done
    ) &
}
RequireTool qemu-system-x86_64
RequireTool cp
RequireTool mkdir
RequireTool tee
RequireTool cat
RequireTool rm
RequireTool python3

mkdir -p "${BuildDir}"
rm -f "${HostLogFile}"

if [[ ! -f "${OvmfCode}" ]]; then
    echo "[Liberation] Missing OVMF code file: ${OvmfCode}"
    exit 1
fi

if [[ ! -f "${OvmfVarsSource}" ]]; then
    echo "[Liberation] Missing OVMF vars file: ${OvmfVarsSource}"
    exit 1
fi

echo "[Liberation] Resetting local writable OVMF vars file..."
rm -f "${OvmfVarsLocal}"
cp "${OvmfVarsSource}" "${OvmfVarsLocal}"

"${RootDir}/Scripts/BuildBoot.sh" dir

mkdir -p "${BootDir}" "${LiberationDir}"
cp "${BootDir}/KERNELX64.ELF" "${LiberationDir}/KERNELX64.ELF"
WriteUtf16LeTextFile "${BootInfoFile}" $'Booting from directory media\r\n'

echo "[Liberation] Writing startup.nsh for directory boot fallback..."
rm -f "${StartupScript}"
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

echo "[Liberation] Launching QEMU from EFI directory as removable USB media..." | tee -a "${HostLogFile}"
TryMaximizeQemuWindow "Liberation Directory Boot"
qemu-system-x86_64 \
    -machine q35,accel=kvm:tcg \
    -m 256M \
    -drive if=pflash,format=raw,readonly=on,file="${OvmfCode}" \
    -drive if=pflash,format=raw,file="${OvmfVarsLocal}" \
    -device qemu-xhci,id=xhci \
    -drive if=none,id=BootDir,format=raw,file=fat:rw:${ImageDir} \
    -device usb-storage,drive=BootDir,bus=xhci.0,removable=true \
    -name "Liberation Directory Boot" \
    -display gtk,gl=off,zoom-to-fit=on \
    -serial stdio 2>&1 | tee >(StripAnsiToFile "${HostLogFile}")
