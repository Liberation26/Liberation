#!/usr/bin/env bash
# File Name: BuildBoot.sh
# File Version: 0.4.32
# Author: OpenAI
# Email: dave66samaa@gmail.com
# Creation Timestamp: 2026-04-07T10:15:30Z
# Last Update Timestamp: 2026-04-09T18:45:00Z
# Operating System Name: Liberation OS
# Purpose: Automates Liberation OS build, packaging, runtime, or maintenance tasks.

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
CapabilitiesServiceSourceDir="${SourceRoot}/Arch/X64/Services/Capabilities/C"
CapabilitiesServiceHeaderDir="${SourceRoot}/Arch/X64/Services/Capabilities/H"
CapabilitiesServiceLinkerScript="${SourceRoot}/Arch/X64/Services/Capabilities/Ld/CapabilitiesX64.ld"
ShellServiceSourceDir="${SourceRoot}/Arch/X64/Services/Shell/C"
ShellServiceHeaderDir="${SourceRoot}/Arch/X64/Services/Shell/H"
ShellServiceAsmDir="${SourceRoot}/Arch/X64/Services/Shell/Asm"
ShellServiceLinkerScript="${SourceRoot}/Arch/X64/Services/Shell/Ld/ShellX64.ld"
LoginCommandSourceDir="${SourceRoot}/Userland/Commands/Login/C"
LoginCommandHeaderDir="${SourceRoot}/Userland/Commands/Login/H"
LoginCommandLinkerScript="${SourceRoot}/Userland/Commands/Login/Ld/Login.ld"
StringLibrarySourceDir="${SourceRoot}/Userland/Libraries/String/C"
StringLibraryHeaderDir="${SourceRoot}/Userland/Libraries/String/H"
StringLibraryLinkerScript="${SourceRoot}/Userland/Libraries/String/Ld/String.ld"
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
OsVersion="$(grep -v "^[[:space:]]*#" "${RootDir}/VERSION" | sed -e "/^[[:space:]]*$/d" | head -n 1 | tr -d "\r\n")"

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
            clang --target=x86_64-pc-win32-coff -nostdlib -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin -mno-red-zone -Wall -Wextra -Wpedantic -O0 -g0 -I"${BootHeaderDir}" -I"${MonitorHeaderDir}" -I"${PublicIncludeDir}" -c "${SourceFile}" -o "${BuildDir}/${BaseName}.obj"
            Objects+=("${BuildDir}/${BaseName}.obj")
        elif SupportsUefiTarget; then
            clang --target=x86_64-unknown-uefi -nostdlib -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin -mno-red-zone -Wall -Wextra -Wpedantic -O0 -g0 -I"${BootHeaderDir}" -I"${MonitorHeaderDir}" -I"${PublicIncludeDir}" -c "${SourceFile}" -o "${BuildDir}/${BaseName}.o"
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
mkdir -p "${ImageDir}/LIBERATION/COMMANDS"
mkdir -p "${ImageDir}/LIBERATION/LIBRARIES"

echo "[Liberation] Building memory-manager service ELF image..."
MemoryManagerServiceObjects=()
for SourceFile in "${MemoryManagerServiceSourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-MemoryManagerService"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -fshort-wchar         -mno-red-zone         -mgeneral-regs-only         -mcmodel=large         -fno-jump-tables         -Wall -Wextra -Wpedantic         -O0 -g0         -I"${BootHeaderDir}"         -I"${MemoryManagerServiceHeaderDir}"         -I"${PublicIncludeDir}"         -I"${MemoryHeaderDir}"         -I"${KernelHeaderDir}"         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    MemoryManagerServiceObjects+=("${BuildDir}/${BaseName}.o")
done

ld.lld     -m elf_x86_64     -nostdlib     -T "${MemoryManagerServiceLinkerScript}"     -o "${BuildDir}/MEMORYMGR.ELF"     "${MemoryManagerServiceObjects[@]}"

echo "[Liberation] Building capabilities service ELF image..."
CapabilitiesServiceObjects=()
for SourceFile in "${CapabilitiesServiceSourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-CapabilitiesService"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -fshort-wchar         -mno-red-zone         -mgeneral-regs-only         -mcmodel=large         -fno-jump-tables         -Wall -Wextra -Wpedantic         -O0 -g0         -I"${BootHeaderDir}"         -I"${CapabilitiesServiceHeaderDir}"         -I"${PublicIncludeDir}"         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    CapabilitiesServiceObjects+=("${BuildDir}/${BaseName}.o")
done

ld.lld     -m elf_x86_64     -nostdlib     -T "${CapabilitiesServiceLinkerScript}"     -o "${BuildDir}/CAPSMGR.ELF"     "${CapabilitiesServiceObjects[@]}"

echo "[Liberation] Embedding memory-manager service image into kernel-owned bootstrap package..."
(
    cd "${BuildDir}"
    cp "MEMORYMGR.ELF" "MemoryManagerServiceImage.bin"
    llvm-objcopy         --input-target=binary         --output-target=elf64-x86-64         --binary-architecture=i386:x86-64         --rename-section .data=.rodata,alloc,load,readonly,data,contents         --redefine-sym _binary_MemoryManagerServiceImage_bin_start=LosMemoryManagerServiceImageStart         --redefine-sym _binary_MemoryManagerServiceImage_bin_end=LosMemoryManagerServiceImageEnd         --redefine-sym _binary_MemoryManagerServiceImage_bin_size=LosMemoryManagerServiceImageSize         "MemoryManagerServiceImage.bin"         "MemoryManagerServiceImage.o"
)


echo "[Liberation] Embedding capabilities service image into kernel-owned bootstrap package..."
(
    cd "${BuildDir}"
    cp "CAPSMGR.ELF" "CapabilitiesServiceImage.bin"
    llvm-objcopy         --input-target=binary         --output-target=elf64-x86-64         --binary-architecture=i386:x86-64         --rename-section .data=.rodata,alloc,load,readonly,data,contents         --redefine-sym _binary_CapabilitiesServiceImage_bin_start=LosCapabilitiesServiceImageStart         --redefine-sym _binary_CapabilitiesServiceImage_bin_end=LosCapabilitiesServiceImageEnd         --redefine-sym _binary_CapabilitiesServiceImage_bin_size=LosCapabilitiesServiceImageSize         "CapabilitiesServiceImage.bin"         "CapabilitiesServiceImage.o"
)

echo "[Liberation] Building shell service ELF image..."
ShellServiceObjects=()
for SourceFile in "${ShellServiceSourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-ShellService"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -fshort-wchar         -mno-red-zone         -mgeneral-regs-only         -mcmodel=large         -fno-jump-tables         -Wall -Wextra -Wpedantic         -O0 -g0         -I"${BootHeaderDir}"         -I"${ShellServiceHeaderDir}"         -I"${PublicIncludeDir}"         -I"${LoginCommandHeaderDir}"         -I"${StringLibraryHeaderDir}"         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    ShellServiceObjects+=("${BuildDir}/${BaseName}.o")
done

for SourceFile in "${LoginCommandSourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-LoginCommandShellEmbed"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -fshort-wchar         -mno-red-zone         -mgeneral-regs-only         -mcmodel=large         -fno-jump-tables         -Wall -Wextra -Wpedantic         -O0 -g0         -DLOS_EMBED_USER_IMAGE_BOOTSTRAP         -I"${BootHeaderDir}"         -I"${ShellServiceHeaderDir}"         -I"${LoginCommandHeaderDir}"         -I"${PublicIncludeDir}"         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    ShellServiceObjects+=("${BuildDir}/${BaseName}.o")
done

for SourceFile in "${StringLibrarySourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-StringLibraryShellEmbed"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -fshort-wchar         -mno-red-zone         -mgeneral-regs-only         -mcmodel=large         -fno-jump-tables         -Wall -Wextra -Wpedantic         -O0 -g0         -DLOS_EMBED_USER_IMAGE_BOOTSTRAP         -I"${BootHeaderDir}"         -I"${ShellServiceHeaderDir}"         -I"${StringLibraryHeaderDir}"         -I"${PublicIncludeDir}"         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    ShellServiceObjects+=("${BuildDir}/${BaseName}.o")
done

for SourceFile in "${ShellServiceAsmDir}"/*.S; do
    if [ ! -e "${SourceFile}" ]; then
        continue
    fi
    BaseName="$(basename "${SourceFile}" .S)-ShellService"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -mno-red-zone         -mcmodel=large         -Wall -Wextra -Wpedantic         -O0 -g0         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    ShellServiceObjects+=("${BuildDir}/${BaseName}.o")
done

ld.lld     -m elf_x86_64     -nostdlib     -T "${ShellServiceLinkerScript}"     -o "${BuildDir}/SHELLX64.ELF"     "${ShellServiceObjects[@]}"

echo "[Liberation] Embedding shell service image into kernel-owned bootstrap package..."
(
    cd "${BuildDir}"
    cp "SHELLX64.ELF" "ShellServiceImage.bin"
    llvm-objcopy         --input-target=binary         --output-target=elf64-x86-64         --binary-architecture=i386:x86-64         --rename-section .data=.rodata,alloc,load,readonly,data,contents         --redefine-sym _binary_ShellServiceImage_bin_start=LosShellServiceImageStart         --redefine-sym _binary_ShellServiceImage_bin_end=LosShellServiceImageEnd         --redefine-sym _binary_ShellServiceImage_bin_size=LosShellServiceImageSize         "ShellServiceImage.bin"         "ShellServiceImage.o"
)


echo "[Liberation] Building external string library ELF image..."
StringLibraryObjects=()
for SourceFile in "${StringLibrarySourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-StringLibrary"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -fshort-wchar         -mno-red-zone         -mgeneral-regs-only         -mcmodel=large         -fno-jump-tables         -Wall -Wextra -Wpedantic         -O0 -g0         -I"${BootHeaderDir}"         -I"${StringLibraryHeaderDir}"         -I"${PublicIncludeDir}"         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    StringLibraryObjects+=("${BuildDir}/${BaseName}.o")
done

ld.lld     -m elf_x86_64     -nostdlib     --build-id=none     -T "${StringLibraryLinkerScript}"     -o "${BuildDir}/STRING.ELF"     "${StringLibraryObjects[@]}"

echo "[Liberation] Building external login command ELF image..."
LoginCommandObjects=()
for SourceFile in "${LoginCommandSourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-LoginCommand"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -fshort-wchar         -mno-red-zone         -mgeneral-regs-only         -mcmodel=large         -fno-jump-tables         -Wall -Wextra -Wpedantic         -O0 -g0         -I"${BootHeaderDir}"         -I"${LoginCommandHeaderDir}"         -I"${PublicIncludeDir}"         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    LoginCommandObjects+=("${BuildDir}/${BaseName}.o")
done

ld.lld     -m elf_x86_64     -nostdlib     --build-id=none     -T "${LoginCommandLinkerScript}"     -o "${BuildDir}/LOGIN.ELF"     "${LoginCommandObjects[@]}"


echo "[Liberation] Embedding installed user images for disk-backed shell loading..."
(
    cd "${BuildDir}"
    cp "LOGIN.ELF" "InstalledLoginImage.bin"
    llvm-objcopy \
        --input-target=binary \
        --output-target=elf64-x86-64 \
        --binary-architecture=i386:x86-64 \
        --rename-section .data=.rodata,alloc,load,readonly,data,contents \
        --redefine-sym _binary_InstalledLoginImage_bin_start=LosInstalledLoginImageStart \
        --redefine-sym _binary_InstalledLoginImage_bin_end=LosInstalledLoginImageEnd \
        --redefine-sym _binary_InstalledLoginImage_bin_size=LosInstalledLoginImageSize \
        "InstalledLoginImage.bin" \
        "InstalledLoginImage.o"

    cp "STRING.ELF" "InstalledStringImage.bin"
    llvm-objcopy \
        --input-target=binary \
        --output-target=elf64-x86-64 \
        --binary-architecture=i386:x86-64 \
        --rename-section .data=.rodata,alloc,load,readonly,data,contents \
        --redefine-sym _binary_InstalledStringImage_bin_start=LosInstalledStringImageStart \
        --redefine-sym _binary_InstalledStringImage_bin_end=LosInstalledStringImageEnd \
        --redefine-sym _binary_InstalledStringImage_bin_size=LosInstalledStringImageSize \
        "InstalledStringImage.bin" \
        "InstalledStringImage.o"
)

echo "[Liberation] Rebuilding shell service image with installed disk-backed user images..."
ShellServiceObjects=()
for SourceFile in "${ShellServiceSourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-ShellServiceDisk"
    clang \
        --target=x86_64-unknown-none-elf \
        -ffreestanding \
        -fno-stack-protector \
        -fno-builtin \
        -fno-pic \
        -fno-pie \
        -fshort-wchar \
        -mno-red-zone \
        -mgeneral-regs-only \
        -mcmodel=large \
        -fno-jump-tables \
        -Wall -Wextra -Wpedantic \
        -O0 -g0 \
        -I"${BootHeaderDir}" \
        -I"${ShellServiceHeaderDir}" \
        -I"${PublicIncludeDir}" \
        -I"${LoginCommandHeaderDir}" \
        -I"${StringLibraryHeaderDir}" \
        -c "${SourceFile}" \
        -o "${BuildDir}/${BaseName}.o"
    ShellServiceObjects+=("${BuildDir}/${BaseName}.o")
done

for SourceFile in "${ShellServiceAsmDir}"/*.S; do
    if [ ! -e "${SourceFile}" ]; then
        continue
    fi
    BaseName="$(basename "${SourceFile}" .S)-ShellServiceDisk"
    clang \
        --target=x86_64-unknown-none-elf \
        -ffreestanding \
        -fno-stack-protector \
        -fno-builtin \
        -fno-pic \
        -fno-pie \
        -mno-red-zone \
        -mcmodel=large \
        -Wall -Wextra -Wpedantic \
        -O0 -g0 \
        -c "${SourceFile}" \
        -o "${BuildDir}/${BaseName}.o"
    ShellServiceObjects+=("${BuildDir}/${BaseName}.o")
done

ShellServiceObjects+=("${BuildDir}/InstalledLoginImage.o")
ShellServiceObjects+=("${BuildDir}/InstalledStringImage.o")

ld.lld \
    -m elf_x86_64 \
    -nostdlib \
    -T "${ShellServiceLinkerScript}" \
    -o "${BuildDir}/SHELLX64.ELF" \
    "${ShellServiceObjects[@]}"

echo "[Liberation] Re-embedding shell service image into kernel-owned bootstrap package..."
(
    cd "${BuildDir}"
    cp "SHELLX64.ELF" "ShellServiceImage.bin"
    llvm-objcopy \
        --input-target=binary \
        --output-target=elf64-x86-64 \
        --binary-architecture=i386:x86-64 \
        --rename-section .data=.rodata,alloc,load,readonly,data,contents \
        --redefine-sym _binary_ShellServiceImage_bin_start=LosShellServiceImageStart \
        --redefine-sym _binary_ShellServiceImage_bin_end=LosShellServiceImageEnd \
        --redefine-sym _binary_ShellServiceImage_bin_size=LosShellServiceImageSize \
        "ShellServiceImage.bin" \
        "ShellServiceImage.o"
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

echo "[Liberation] Building kernel-callable shell bootstrap entry..."
for SourceFile in "${ShellServiceSourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-ShellBootstrapKernel"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -fshort-wchar         -mno-red-zone         -mgeneral-regs-only         -mcmodel=large         -fno-jump-tables         -Wall -Wextra -Wpedantic         -O0 -g0         -I"${BootHeaderDir}"         -I"${ShellServiceHeaderDir}"         -I"${PublicIncludeDir}"         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    KernelObjects+=("${BuildDir}/${BaseName}.o")
done

echo "[Liberation] Building kernel-callable capabilities bootstrap entry..."
for SourceFile in "${CapabilitiesServiceSourceDir}"/*.c; do
    BaseName="$(basename "${SourceFile}" .c)-CapabilitiesBootstrapKernel"
    clang         --target=x86_64-unknown-none-elf         -ffreestanding         -fno-stack-protector         -fno-builtin         -fno-pic         -fno-pie         -fshort-wchar         -mno-red-zone         -mgeneral-regs-only         -mcmodel=large         -fno-jump-tables         -Wall -Wextra -Wpedantic         -O0 -g0         -I"${BootHeaderDir}"         -I"${CapabilitiesServiceHeaderDir}"         -I"${PublicIncludeDir}"         -c "${SourceFile}"         -o "${BuildDir}/${BaseName}.o"
    KernelObjects+=("${BuildDir}/${BaseName}.o")
done

KernelObjects+=("${BuildDir}/MemoryManagerServiceImage.o")
KernelObjects+=("${BuildDir}/CapabilitiesServiceImage.o")
KernelObjects+=("${BuildDir}/ShellServiceImage.o")

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
cp "${BuildDir}/CAPSMGR.ELF" "${ImageDir}/LIBERATION/SERVICES/CAPSMGR.ELF"
cp "${BuildDir}/SHELLX64.ELF" "${ImageDir}/LIBERATION/SERVICES/SHELLX64.ELF"
mkdir -p "${ImageDir}/LIBERATION/COMMANDS"
mkdir -p "${ImageDir}/LIBERATION/LIBRARIES"
cp "${BuildDir}/LOGIN.ELF" "${ImageDir}/LIBERATION/COMMANDS/LOGIN.ELF"
cp "${BuildDir}/STRING.ELF" "${ImageDir}/LIBERATION/LIBRARIES/STRING.ELF"
if [[ -f "${RootDir}/Config/Services/Capabilities.cfg" ]]; then
    cp "${RootDir}/Config/Services/Capabilities.cfg" "${BootDir}/CAPABILITIES.CFG"
fi
if [[ -f "${ImageDir}/LIBERATION/FONTS/Boot.psf" ]]; then
    cp "${ImageDir}/LIBERATION/FONTS/Boot.psf" "${BootDir}/Boot.psf"
fi
