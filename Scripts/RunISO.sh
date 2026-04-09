#!/usr/bin/env bash
# File Name: RunISO.sh
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
IsoFile="${BuildDir}/Liberation.iso"
InstallTargetFile1="${BuildDir}/LiberationInstallTarget1.img"
InstallTargetFile2="${BuildDir}/LiberationInstallTarget2.img"
InstallLogFile="${BuildDir}/RunISO-Installer.log"
BootLogFile="${BuildDir}/RunISO-Boot.log"
HostLogFile="${BuildDir}/RunISO-Host.log"
SelectedBootDisk="1"
OvmfCode="/usr/share/OVMF/OVMF_CODE_4M.fd"
OvmfVarsSource="/usr/share/OVMF/OVMF_VARS_4M.fd"
OvmfVarsLocal="${BuildDir}/OVMF_VARS_ISO_4M.fd"

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

echo "[Liberation] Starting ISO build and run..."
RequireTool qemu-system-x86_64
RequireTool cp
RequireTool mkdir
RequireTool tee
RequireTool grep
RequireTool tail

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


mkdir -p "${BuildDir}"

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

"${RootDir}/Scripts/BuildBoot.sh" iso
"${RootDir}/Scripts/MakeIso.sh"
"${RootDir}/Scripts/MakeInstallTarget.sh"

if [[ ! -f "${IsoFile}" ]]; then
    echo "[Liberation] Missing ISO image after build: ${IsoFile}"
    exit 1
fi

if [[ ! -f "${InstallTargetFile1}" || ! -f "${InstallTargetFile2}" ]]; then
    echo "[Liberation] Missing installer target images after build."
    exit 1
fi

rm -f "${InstallLogFile}" "${BootLogFile}" "${HostLogFile}"

echo "[Liberation] Launching QEMU from UEFI ISO image with two blank installer target disks..." | tee -a "${HostLogFile}"
TryMaximizeQemuWindow "Liberation ISO Installer"
qemu-system-x86_64     -machine q35,accel=kvm:tcg     -m 256M     -drive if=pflash,format=raw,readonly=on,file="${OvmfCode}"     -drive if=pflash,format=raw,file="${OvmfVarsLocal}"     -drive if=none,id=BootIso,media=cdrom,format=raw,file="${IsoFile}"     -device ide-cd,drive=BootIso,bus=ide.1     -drive if=virtio,format=raw,file="${InstallTargetFile1}"     -drive if=virtio,format=raw,file="${InstallTargetFile2}"     -boot d     -action reboot=shutdown     -name "Liberation ISO Installer"     -display gtk,gl=off,zoom-to-fit=on     -serial stdio 2>&1 | tee >(StripAnsiToFile "${InstallLogFile}") >(StripAnsiToFile "${HostLogFile}")

echo "[Liberation] ISO installer phase exited after guest reboot." | tee -a "${HostLogFile}"
if [[ -f "${InstallLogFile}" ]]; then
    if grep -q 'Selected target \[2\]' "${InstallLogFile}"; then
        SelectedBootDisk="2"
    elif grep -q 'Selected target \[1\]' "${InstallLogFile}"; then
        SelectedBootDisk="1"
    fi
fi

echo "[Liberation] Installer selected target disk ${SelectedBootDisk}." | tee -a "${HostLogFile}"
if [[ "${SelectedBootDisk}" == "2" ]]; then
    RelaunchDisk="${InstallTargetFile2}"
else
    RelaunchDisk="${InstallTargetFile1}"
fi

echo "[Liberation] Relaunching QEMU from installed target disk ${SelectedBootDisk} only..." | tee -a "${HostLogFile}"
TryMaximizeQemuWindow "Liberation Installed Disk"
qemu-system-x86_64     -machine q35,accel=kvm:tcg     -m 256M     -drive if=pflash,format=raw,readonly=on,file="${OvmfCode}"     -drive if=pflash,format=raw,file="${OvmfVarsLocal}"     -drive if=virtio,format=raw,file="${RelaunchDisk}"     -boot c     -name "Liberation Installed Disk"     -display gtk,gl=off,zoom-to-fit=on     -serial stdio 2>&1 | tee >(StripAnsiToFile "${BootLogFile}") >(StripAnsiToFile "${HostLogFile}")
