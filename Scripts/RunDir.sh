#!/usr/bin/env bash
set -euo pipefail

RootDir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BuildDir="${RootDir}/Build"
ImageDir="${RootDir}/Image"
OvmfCode="/usr/share/OVMF/OVMF_CODE_4M.fd"
OvmfVarsSource="/usr/share/OVMF/OVMF_VARS_4M.fd"
OvmfVarsLocal="${BuildDir}/OVMF_VARS_DIR_4M.fd"
StartupScript="${ImageDir}/startup.nsh"
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

echo "[Liberation] Starting directory build and run..."
RequireTool qemu-system-x86_64
RequireTool cp
RequireTool mkdir
RequireTool tee
RequireTool cat
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

if [[ ! -f "${OvmfVarsLocal}" ]]; then
    echo "[Liberation] Creating local writable OVMF vars file..."
    cp "${OvmfVarsSource}" "${OvmfVarsLocal}"
fi

"${RootDir}/Scripts/BuildBoot.sh" dir

echo "[Liberation] Writing startup.nsh for directory boot fallback..."
rm -f "${StartupScript}"
cat > "${StartupScript}" <<'EOF'
if exist fs0:\EFI\BOOT\BOOTX64.EFI then
  fs0:\EFI\BOOT\BOOTX64.EFI
endif
if exist fs1:\EFI\BOOT\BOOTX64.EFI then
  fs1:\EFI\BOOT\BOOTX64.EFI
endif
if exist fs2:\EFI\BOOT\BOOTX64.EFI then
  fs2:\EFI\BOOT\BOOTX64.EFI
endif
EOF

echo "[Liberation] Launching QEMU from EFI directory as removable USB media..." | tee -a "${HostLogFile}"
qemu-system-x86_64 \
    -machine q35,accel=kvm:tcg \
    -m 256M \
    -drive if=pflash,format=raw,readonly=on,file="${OvmfCode}" \
    -drive if=pflash,format=raw,file="${OvmfVarsLocal}" \
    -device qemu-xhci,id=xhci \
    -drive if=none,id=BootDir,format=raw,file=fat:rw:${ImageDir} \
    -device usb-storage,drive=BootDir,bus=xhci.0,removable=true \
    -display gtk,gl=off \
    -serial stdio 2>&1 | tee >(StripAnsiToFile "${HostLogFile}")
