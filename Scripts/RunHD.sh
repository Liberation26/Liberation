#!/usr/bin/env bash
# File Name: RunHD.sh
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
DiskFile="${BuildDir}/LiberationDisk.img"
OvmfCode="/usr/share/OVMF/OVMF_CODE_4M.fd"
OvmfVarsSource="/usr/share/OVMF/OVMF_VARS_4M.fd"
OvmfVarsLocal="${BuildDir}/OVMF_VARS_HD_4M.fd"
HostLogFile="${BuildDir}/RunHD-Host.log"

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

echo "[Liberation] Starting hard-disk build and run..."

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
RequireTool rm

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

"${RootDir}/Scripts/BuildBoot.sh" hd
"${RootDir}/Scripts/MakeHardDisk.sh"

if [[ ! -f "${DiskFile}" ]]; then
    echo "[Liberation] Missing hard disk image after build: ${DiskFile}"
    exit 1
fi

echo "[Liberation] Launching QEMU from UEFI hard disk image..." | tee -a "${HostLogFile}"
TryMaximizeQemuWindow "Liberation Hard Disk"
qemu-system-x86_64 \
    -machine q35,accel=kvm:tcg \
    -m 256M \
    -drive if=pflash,format=raw,readonly=on,file="${OvmfCode}" \
    -drive if=pflash,format=raw,file="${OvmfVarsLocal}" \
    -device ich9-ahci,id=ahci \
    -drive if=none,id=BootDisk,format=raw,file="${DiskFile}" \
    -device ide-hd,drive=BootDisk,bus=ahci.0 \
    -boot c \
    -name "Liberation Hard Disk" \
    -display gtk,gl=off,zoom-to-fit=on \
    -serial stdio 2>&1 | tee >(StripAnsiToFile "${HostLogFile}")
