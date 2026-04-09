#!/usr/bin/env bash
# File Name: update.sh
# File Version: 0.3.11
# Author: OpenAI
# Email: dave66samaa@gmail.com
# Creation Timestamp: 2026-04-07T07:24:34Z
# Last Update Timestamp: 2026-04-07T12:35:00Z
# Operating System Name: Liberation OS
# Purpose: Automates Liberation OS build, packaging, runtime, or maintenance tasks.

set -euo pipefail

DownloadsDir="${HOME}/Downloads"
ArchivePattern="LOS-*-*-*.tar"
BaseDir="${HOME}/Dev/Los"
PreferredRepoDir="${BaseDir}/Src"
LegacyRepoDir="${BaseDir}/src"
TempDir="$(mktemp -d)"
ScriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UpdateScriptPath="${ScriptDir}/update.sh"
WrapperScriptPath="${ScriptDir}/Update.sh"

cleanup() {
    rm -rf "${TempDir}"
}
trap cleanup EXIT

RequireTool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[Liberation] Missing required tool: $1"
        exit 1
    fi
}

EnsureWrapperScript() {
    mkdir -p "$(dirname "${WrapperScriptPath}")"
    cat > "${WrapperScriptPath}" <<'WRAP'
#!/usr/bin/env bash
set -euo pipefail
ScriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${ScriptDir}/update.sh" "$@"
WRAP
    chmod 0755 "${WrapperScriptPath}"
}

RequireTool find
RequireTool tar
RequireTool rsync
RequireTool sort
RequireTool head
RequireTool cut
RequireTool git
RequireTool cmp
RequireTool install

mkdir -p "${BaseDir}"
EnsureWrapperScript

LatestTar="$({
    find "${DownloadsDir}" -maxdepth 1 -type f -name "${ArchivePattern}" -printf '%T@ %p\n' || true
} | sort -nr | head -n 1 | cut -d' ' -f2-)"

if [[ -z "${LatestTar}" ]]; then
    echo "[Liberation] No archive matching ${ArchivePattern} found in ${DownloadsDir}"
    exit 1
fi

ResolveRepoDir() {
    if [[ -e "${LegacyRepoDir}/.git" ]]; then
        printf '%s\n' "${LegacyRepoDir}"
        return
    fi

    if [[ -e "${PreferredRepoDir}/.git" ]]; then
        printf '%s\n' "${PreferredRepoDir}"
        return
    fi

    printf '%s\n' "${PreferredRepoDir}"
}

CleanBuildOutputs() {
    local RepoDir="$1"

    echo "[Liberation] Cleaning build output..."

    rm -rf "${RepoDir}/Build"
    mkdir -p "${RepoDir}/Build"

    rm -f "${RepoDir}/Image/EFI/BOOT/BOOTX64.EFI"
    rm -f "${RepoDir}/Image/EFI/BOOT/LOADERX64.EFI"
    rm -f "${RepoDir}/Image/EFI/BOOT/MONITORX64.EFI"
    rm -f "${RepoDir}/Image/EFI/BOOT/KERNELX64.ELF"
    rm -f "${RepoDir}/Image/EFI/BOOT/KERNELX64.BIN"
}

RemoveLegacySourceLayout() {
    local RepoDir="$1"

    rm -f "${RepoDir}/Source/Boot/Uefi/BootMain.c"
    rm -f "${RepoDir}/Source/Boot/Uefi/Efi.h"
    rm -f "${RepoDir}/Source/Src/Arch/X64/Purpose/Boot/C/BootMain.c"
    rm -f "${RepoDir}/Source/Src/Arch/X64/Purpose/Boot/H/Efi.h"

    rmdir "${RepoDir}/Source/Boot/Uefi" 2>/dev/null || true
    rmdir "${RepoDir}/Source/Boot" 2>/dev/null || true
    rmdir "${RepoDir}/Source/Src/Arch/X64/Purpose/Boot/C" 2>/dev/null || true
    rmdir "${RepoDir}/Source/Src/Arch/X64/Purpose/Boot/H" 2>/dev/null || true
    rmdir "${RepoDir}/Source/Src/Arch/X64/Purpose/Boot" 2>/dev/null || true
    rmdir "${RepoDir}/Source/Src/Arch/X64/Purpose" 2>/dev/null || true
}

CommitAndPush() {
    local RepoDir="$1"
    local ArchiveName="$2"
    local CommitMessage="Update from ${ArchiveName}"
    local BranchName

    if [[ ! -e "${RepoDir}/.git" ]]; then
        echo "[Liberation] No .git entry found in ${RepoDir}; skipping GitHub push."
        return
    fi

    if ! git -C "${RepoDir}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        echo "[Liberation] ${RepoDir} is not a valid Git working tree; skipping GitHub push."
        return
    fi

    echo "[Liberation] Staging changes..."
    git -C "${RepoDir}" add -A

    if git -C "${RepoDir}" diff --cached --quiet; then
        echo "[Liberation] No Git changes detected after update."
        return
    fi

    echo "[Liberation] Creating commit: ${CommitMessage}"
    git -C "${RepoDir}" commit -m "${CommitMessage}"

    if git -C "${RepoDir}" remote get-url origin >/dev/null 2>&1; then
        BranchName="$(git -C "${RepoDir}" branch --show-current || true)"
        echo "[Liberation] Pushing changes to GitHub..."
        if [[ -n "${BranchName}" ]]; then
            git -C "${RepoDir}" push -u origin "${BranchName}"
        else
            git -C "${RepoDir}" push
        fi
    else
        echo "[Liberation] No origin remote configured; commit created locally only."
    fi
}

SelfUpdateIfNeeded() {
    local IncomingUpdateScript="$1"

    if [[ ! -f "${IncomingUpdateScript}" ]]; then
        return
    fi

    mkdir -p "$(dirname "${UpdateScriptPath}")"

    if [[ ! -f "${UpdateScriptPath}" ]] || ! cmp -s "${IncomingUpdateScript}" "${UpdateScriptPath}"; then
        echo "[Liberation] Updating update.sh..."
        install -m 0755 "${IncomingUpdateScript}" "${UpdateScriptPath}"
        rm -f "${IncomingUpdateScript}"
        EnsureWrapperScript
        echo "[Liberation] update.sh has been updated in ${UpdateScriptPath}. Run ./Scripts/update.sh again to apply the archive contents."
        exit 0
    fi

    rm -f "${IncomingUpdateScript}"
    EnsureWrapperScript
}

RepoDir="$(ResolveRepoDir)"
ArchiveName="$(basename "${LatestTar}")"

echo "[Liberation] Using archive: ${LatestTar}"
echo "[Liberation] Extracting archive..."
tar -xf "${LatestTar}" -C "${TempDir}"

if [[ ! -d "${TempDir}/FullSource" ]]; then
    echo "[Liberation] Missing FullSource in archive."
    exit 1
fi

if [[ ! -d "${TempDir}/ChangedFiles" ]]; then
    echo "[Liberation] Missing ChangedFiles in archive."
    exit 1
fi

SelfUpdateIfNeeded "${TempDir}/ChangedFiles/Scripts/update.sh"
rm -f "${TempDir}/ChangedFiles/Scripts/Update.sh"

if [[ ! -e "${RepoDir}/.git" ]]; then
    echo "[Liberation] No .git entry found in ${RepoDir}."
    echo "[Liberation] Copying FullSource into ${RepoDir}/"
    mkdir -p "${RepoDir}"
    rsync -a --delete "${TempDir}/FullSource/" "${RepoDir}/"
    EnsureWrapperScript
    echo "[Liberation] Initial source deployment complete."
    exit 0
fi

echo "[Liberation] Git repository detected in ${RepoDir}."
echo "[Liberation] Copying ChangedFiles into ${RepoDir}/"
rsync -a "${TempDir}/ChangedFiles/" "${RepoDir}/"

RemoveLegacySourceLayout "${RepoDir}"
CleanBuildOutputs "${RepoDir}"
EnsureWrapperScript
CommitAndPush "${RepoDir}" "${ArchiveName}"

echo "[Liberation] Update complete."
