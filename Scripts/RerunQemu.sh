#!/usr/bin/env bash
# File Name: RerunQemu.sh
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
OvmfCode="/usr/share/OVMF/OVMF_CODE_4M.fd"
OvmfVarsIso="${BuildDir}/OVMF_VARS_ISO_4M.fd"
OvmfVarsHd="${BuildDir}/OVMF_VARS_HD_4M.fd"
InstallTargetFile1="${BuildDir}/LiberationInstallTarget1.img"
InstallTargetFile2="${BuildDir}/LiberationInstallTarget2.img"
DiskFile="${BuildDir}/LiberationDisk.img"
InstallLogFile="${BuildDir}/RunISO-Installer.log"
HostLogFile="${BuildDir}/RerunQemu-Host.log"
SelectedBootDisk="1"

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
RequireTool tee
RequireTool grep

mkdir -p "${BuildDir}"
rm -f "${HostLogFile}"

if [[ ! -f "${OvmfCode}" ]]; then
    echo "[Liberation] Missing OVMF code file: ${OvmfCode}"
    exit 1
fi

if [[ -f "${InstallTargetFile1}" && -f "${InstallTargetFile2}" ]]; then
    if [[ -f "${InstallLogFile}" ]]; then
        if grep -q 'Selected target \[2\]' "${InstallLogFile}"; then
            SelectedBootDisk="2"
        elif grep -q 'Selected target \[1\]' "${InstallLogFile}"; then
            SelectedBootDisk="1"
        fi
    fi

    if [[ -f "${OvmfVarsIso}" ]]; then
        OvmfVarsLocal="${OvmfVarsIso}"
    else
        OvmfVarsLocal="${OvmfVarsHd}"
    fi

    echo "[Liberation] Re-running QEMU with the last installer hard drives attached..." | tee -a "${HostLogFile}"
    echo "[Liberation] Boot target disk: ${SelectedBootDisk}" | tee -a "${HostLogFile}"

    if [[ "${SelectedBootDisk}" == "2" ]]; then
        BootDriveArgs=(-drive if=virtio,format=raw,file="${InstallTargetFile2}" -drive if=virtio,format=raw,file="${InstallTargetFile1}")
    else
        BootDriveArgs=(-drive if=virtio,format=raw,file="${InstallTargetFile1}" -drive if=virtio,format=raw,file="${InstallTargetFile2}")
    fi

    TryMaximizeQemuWindow "Liberation ReRun"
    qemu-system-x86_64 \
        -machine q35,accel=kvm:tcg \
        -m 256M \
        -drive if=pflash,format=raw,readonly=on,file="${OvmfCode}" \
        -drive if=pflash,format=raw,file="${OvmfVarsLocal}" \
        "${BootDriveArgs[@]}" \
        -boot c \
        -name "Liberation ReRun" \
        -display gtk,gl=off,zoom-to-fit=on \
        -serial stdio 2>&1 | tee >(StripAnsiToFile "${HostLogFile}")
    exit 0
fi

if [[ -f "${DiskFile}" ]]; then
    if [[ -f "${OvmfVarsHd}" ]]; then
        OvmfVarsLocal="${OvmfVarsHd}"
    else
        OvmfVarsLocal="${OvmfVarsIso}"
    fi

    echo "[Liberation] Re-running QEMU with the last hard disk image attached..." | tee -a "${HostLogFile}"
    TryMaximizeQemuWindow "Liberation ReRun"
    qemu-system-x86_64 \
        -machine q35,accel=kvm:tcg \
        -m 256M \
        -drive if=pflash,format=raw,readonly=on,file="${OvmfCode}" \
        -drive if=pflash,format=raw,file="${OvmfVarsLocal}" \
        -device ich9-ahci,id=ahci \
        -drive if=none,id=BootDisk,format=raw,file="${DiskFile}" \
        -device ide-hd,drive=BootDisk,bus=ahci.0 \
        -boot c \
        -name "Liberation ReRun" \
        -display gtk,gl=off,zoom-to-fit=on \
        -serial stdio 2>&1 | tee >(StripAnsiToFile "${HostLogFile}")
    exit 0
fi

echo "[Liberation] No previous hard-disk images were found to re-run."
exit 1
