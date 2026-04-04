#!/usr/bin/env bash
set -euo pipefail

RootDir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SourceRoot="${RootDir}/Source/Src"
BootSourceDir="${SourceRoot}/Arch/X64/Boot/C"
BootHeaderDir="${SourceRoot}/Arch/X64/Boot/H"
PublicIncludeDir="${RootDir}/Source/Include/Public"
InstallerSourceDir="${SourceRoot}/Arch/X64/Installer/C"
InstallerHeaderDir="${SourceRoot}/Arch/X64/Installer/H"
MonitorSourceDir="${SourceRoot}/Arch/X64/Monitor/C"
MonitorHeaderDir="${SourceRoot}/Arch/X64/Monitor/H"
KernelSourceDir="${SourceRoot}/Arch/X64/Kernel/C"
KernelHeaderDir="${SourceRoot}/Arch/X64/Kernel/H"
KernelAsmDir="${SourceRoot}/Arch/X64/Kernel/Asm"
MemoryManagerServiceSourceDir="${SourceRoot}/Arch/X64/Services/MemoryManager/C"
MemoryManagerServiceHeaderDir="${SourceRoot}/Arch/X64/Services/MemoryManager/H"
MemoryManagerServiceLinkerScript="${SourceRoot}/Arch/X64/Services/MemoryManager/Ld/MemoryManagerX64.ld"
InterruptSourceDir="${SourceRoot}/Arch/X64/Interrupts/C"
InterruptHeaderDir="${SourceRoot}/Arch/X64/Interrupts/H"
InterruptAsmDir="${SourceRoot}/Arch/X64/Interrupts/Asm"
MemorySourceDir="${SourceRoot}/Arch/X64/Memory/C"
MemoryHeaderDir="${SourceRoot}/Arch/X64/Memory/H"
KernelLinkerScript="${SourceRoot}/Arch/X64/Kernel/Ld/KernelX64.ld"
BuildDir="${RootDir}/Build"
ImageDir="${RootDir}/Image"
BootDir="${ImageDir}/EFI/BOOT"
BootMode="${1:-iso}"
OsVersion="$(tr -d "\r\n" < "${RootDir}/VERSION")"

RequireTool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[Liberation] Missing required tool: $1"
        exit 1
    fi
}

SupportsUefiTarget() {
    clang --target=x86_64-unknown-uefi -### -x c /dev/null -o /dev/null >/dev/null 2>&1
}

ResolveBootMode() {
    case "$1" in
        dir|directory)
            printf '%s;%s\n' "LIBERATION_BOOT_FROM_DIRECTORY" "Directory"
            ;;
        hd|harddisk|hard-drive)
            printf '%s;%s\n' "LIBERATION_BOOT_FROM_HARD_DRIVE" "Hard Drive"
            ;;
        iso)
            printf '%s;%s\n' "LIBERATION_BOOT_FROM_ISO" "ISO"
            ;;
        *)
            echo "[Liberation] Unknown boot mode: $1"
            echo "[Liberation] Supported modes: dir, hd, iso"
            exit 1
            ;;
    esac
}


BuildMonitorApplication() {
    local OutputFile="$1"
    local Objects=()
    local SourceFile

    for SourceFile in "${MonitorSourceDir}"/*.c; do
        local BaseName="$(basename "${SourceFile}" .c)"
        if command -v lld-link >/dev/null 2>&1; then
            clang --target=x86_64-pc-win32-coff -nostdlib -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin -mno-red-zone -Wall -Wextra -Wpedantic -O0 -g0 -I"${BootHeaderDir}" -I"${MonitorHeaderDir}" -c "${SourceFile}" -o "${BuildDir}/${BaseName}.obj"
            Objects+=("${BuildDir}/${BaseName}.obj")
        elif SupportsUefiTarget; then
            clang --target=x86_64-unknown-uefi -nostdlib -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin -mno-red-zone -Wall -Wextra -Wpedantic -O0 -g0 -I"${BootHeaderDir}" -I"${MonitorHeaderDir}" -c "${SourceFile}" -o "${BuildDir}/${BaseName}.o"
            Objects+=("${BuildDir}/${BaseName}.o")
        else
            echo "[Liberation] No supported UEFI build path found. Install lld-link or a working clang UEFI target."
            exit 1
        fi
    done

    if command -v lld-link >/dev/null 2>&1; then
        lld-link /nologo /subsystem:efi_application /entry:EfiMain /nodefaultlib "/out:${OutputFile}" "${Objects[@]}"
    else
        clang --target=x86_64-unknown-uefi -fuse-ld=lld -nostdlib "${Objects[@]}" -o "${OutputFile}"
    fi
}

BuildEfiApplication() {
    local BootMacro="$1"
    local OutputFile="$2"
    local ObjectSuffix="$3"
    local Objects=()
    local SourceFile

    for SourceFile in "${BootSourceDir}"/*.c "${InstallerSourceDir}"/*.c; do
        local BaseName="$(basename "${SourceFile}" .c)-${ObjectSuffix}"
        if command -v lld-link >/dev/null 2>&1; then
            clang --target=x86_64-pc-win32-coff -nostdlib -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin -mno-red-zone -Wall -Wextra -Wpedantic -O0 -g0 -D"${BootMacro}" -I"${BootHeaderDir}" -I"${InstallerHeaderDir}" -c "${SourceFile}" -o "${BuildDir}/${BaseName}.obj"
            Objects+=("${BuildDir}/${BaseName}.obj")
        elif SupportsUefiTarget; then
            clang --target=x86_64-unknown-uefi -nostdlib -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin -mno-red-zone -Wall -Wextra -Wpedantic -O0 -g0 -D"${BootMacro}" -I"${BootHeaderDir}" -I"${InstallerHeaderDir}" -c "${SourceFile}" -o "${BuildDir}/${BaseName}.o"
            Objects+=("${BuildDir}/${BaseName}.o")
        else
            echo "[Liberation] No supported UEFI build path found. Install lld-link or a working clang UEFI target."
            exit 1
        fi
    done

    if command -v lld-link >/dev/null 2>&1; then
        lld-link /nologo /subsystem:efi_application /entry:EfiMain /nodefaultlib "/out:${OutputFile}" "${Objects[@]}"
    else
        clang --target=x86_64-unknown-uefi -fuse-ld=lld -nostdlib "${Objects[@]}" -o "${OutputFile}"
    fi
}

IFS=';' read -r BootMacro BootLabel <<< "$(ResolveBootMode "${BootMode}")"

echo "[Liberation] Building BOOTX64.EFI for ${BootLabel}..."
RequireTool clang
RequireTool cp
RequireTool mkdir
RequireTool ld.lld
RequireTool llvm-objcopy
if [[ -z "${OsVersion}" ]]; then
    echo "[Liberation] VERSION file is empty."
    exit 1
fi
mkdir -p "${BuildDir}"
mkdir -p "${BootDir}"
mkdir -p "${ImageDir}/LIBERATION"
mkdir -p "${ImageDir}/LIBERATION/FONTS"
mkdir -p "${ImageDir}/LIBERATION/SERVICES"

echo "[Liberation] Building memory-manager service ELF image..."
MemoryManagerServiceObjects=()
for SourceFile in "${MemoryManagerServiceSourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-MemoryManagerService"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -fshort-wchar         -mno-red-zone         -mgeneral-regs-only         -mcmodel=large         -fno-jump-tables         -Wall -Wextra -Wpedantic         -O0 -g0         -I"${BootHeaderDir}"         -I"${MemoryManagerServiceHeaderDir}"         -I"${PublicIncludeDir}"         -I"${MemoryHeaderDir}"         -I"${KernelHeaderDir}"         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    MemoryManagerServiceObjects+=("${BuildDir}/${BaseName}.o")
done

ld.lld     -m elf_x86_64     -nostdlib     -T "${MemoryManagerServiceLinkerScript}"     -o "${BuildDir}/MEMORYMGR.ELF"     "${MemoryManagerServiceObjects[@]}"

echo "[Liberation] Embedding memory-manager service image into kernel-owned bootstrap package..."
(
    cd "${BuildDir}"
    cp "MEMORYMGR.ELF" "MemoryManagerServiceImage.bin"
    llvm-objcopy         --input-target=binary         --output-target=elf64-x86-64         --binary-architecture=i386:x86-64         --rename-section .data=.rodata,alloc,load,readonly,data,contents         --redefine-sym _binary_MemoryManagerServiceImage_bin_start=LosMemoryManagerServiceImageStart         --redefine-sym _binary_MemoryManagerServiceImage_bin_end=LosMemoryManagerServiceImageEnd         --redefine-sym _binary_MemoryManagerServiceImage_bin_size=LosMemoryManagerServiceImageSize         "MemoryManagerServiceImage.bin"         "MemoryManagerServiceImage.o"
)

echo "[Liberation] Building ELF X64 kernel image..."
KernelObjects=()
for SourceFile in "${KernelSourceDir}"/*.c "${InterruptSourceDir}"/*.c "${MemorySourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-Kernel"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -fshort-wchar         -mno-red-zone         -mgeneral-regs-only         -mcmodel=large         -fno-jump-tables         -Wall -Wextra -Wpedantic         -O0 -g0         -DLIBERATION_VERSION_STRING="${OsVersion}"         -I"${BootHeaderDir}"         -I"${KernelHeaderDir}"         -I"${InterruptHeaderDir}"         -I"${MemoryHeaderDir}"         -I"${PublicIncludeDir}"         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    KernelObjects+=("${BuildDir}/${BaseName}.o")
done

clang     --target=x86_64-unknown-none-elf     -ffreestanding     -fno-stack-protector     -fno-builtin     -fno-pic     -fno-pie     -mno-red-zone     -Wall -Wextra -Wpedantic     -O0 -g0     -c "${InterruptAsmDir}/InterruptStubs.S"     -o "${BuildDir}/InterruptStubs.o"
KernelObjects+=("${BuildDir}/InterruptStubs.o")

for SourceFile in "${KernelAsmDir}"/*.S; do
    BaseName="$(basename "${SourceFile}" .S)-Kernel"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -mno-red-zone         -mcmodel=large         -Wall -Wextra -Wpedantic         -O0 -g0         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    KernelObjects+=("${BuildDir}/${BaseName}.o")
done

KernelObjects+=("${BuildDir}/MemoryManagerServiceImage.o")

ld.lld     -m elf_x86_64     -nostdlib     -T "${KernelLinkerScript}"     -o "${BuildDir}/KernelX64.elf"     "${KernelObjects[@]}"

echo "[Liberation] Building installed-system EFI loader..."
BuildEfiApplication "LIBERATION_BOOT_FROM_HARD_DRIVE" "${BuildDir}/LOADERX64.EFI" "Loader"

echo "[Liberation] Building kernel monitor EFI application..."
BuildMonitorApplication "${BuildDir}/MONITORX64.EFI"

echo "[Liberation] Building media EFI application..."
BuildEfiApplication "${BootMacro}" "${BuildDir}/BOOTX64.EFI" "Boot"

echo "[Liberation] Copying EFI payloads into EFI boot path..."
cp "${BuildDir}/BOOTX64.EFI" "${BootDir}/BOOTX64.EFI"
cp "${BuildDir}/LOADERX64.EFI" "${BootDir}/LOADERX64.EFI"
cp "${BuildDir}/MONITORX64.EFI" "${BootDir}/MONITORX64.EFI"
cp "${BuildDir}/KernelX64.elf" "${BootDir}/KERNELX64.ELF"
cp "${BuildDir}/KernelX64.elf" "${ImageDir}/LIBERATION/KERNELX64.ELF"
cp "${BuildDir}/MEMORYMGR.ELF" "${ImageDir}/LIBERATION/SERVICES/MEMORYMGR.ELF"
if [[ -f "${ImageDir}/LIBERATION/FONTS/Boot.psf" ]]; then
    cp "${ImageDir}/LIBERATION/FONTS/Boot.psf" "${BootDir}/Boot.psf"
fi
