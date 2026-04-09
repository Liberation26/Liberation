#!/usr/bin/env bash
# File Name: MakeInstallTarget.sh
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
TargetDiskFile1="${BuildDir}/LiberationInstallTarget1.img"
TargetDiskFile2="${BuildDir}/LiberationInstallTarget2.img"
LegacyTargetDiskFile="${BuildDir}/LiberationInstallTarget.img"

DiskSizeMiB1=100
DiskSizeMiB2=120

RequireTool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[Liberation] Missing required tool: $1"
        exit 1
    fi
}

RequireTool truncate
RequireTool mkdir
RequireTool rm

mkdir -p "${BuildDir}"
rm -f "${TargetDiskFile1}" "${TargetDiskFile2}" "${LegacyTargetDiskFile}"

echo "[Liberation] Creating blank installer target disk images..."
truncate -s "${DiskSizeMiB1}M" "${TargetDiskFile1}"
truncate -s "${DiskSizeMiB2}M" "${TargetDiskFile2}"

echo "[Liberation] Installer target image created: ${TargetDiskFile1} (${DiskSizeMiB1} MiB)"
echo "[Liberation] Installer target image created: ${TargetDiskFile2} (${DiskSizeMiB2} MiB)"
echo "[Liberation] The ISO EFI installer will partition and format the selected disk itself."
