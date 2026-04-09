/*
 * File Name: InstallerMain.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements low-level functionality for Liberation OS.
 */

#include "InstallerInternal.h"

EFI_STATUS EFIAPI LosRunInstaller(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_FILE_PROTOCOL *SourceRoot;
    LOS_INSTALL_CANDIDATE Candidates[LOS_MAX_INSTALL_CANDIDATES];
    UINTN CandidateCount;
    UINTN SelectedIndex;
    void *LoaderBuffer;
    void *MonitorBuffer;
    void *KernelBuffer;
    UINTN LoaderSize;
    UINTN MonitorSize;
    UINTN KernelSize;
    EFI_STATUS Status;

    if (SystemTable == 0 || SystemTable->BootServices == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    LosInstallerPrint(SystemTable, LOS_TEXT("Starting EFI installer...\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("Installer source: ISO boot media\r\n\r\n"));

    LoadedImage = 0;
    SourceRoot = 0;
    CandidateCount = 0U;
    SelectedIndex = 0U;
    LoaderBuffer = 0;
    MonitorBuffer = 0;
    KernelBuffer = 0;
    LoaderSize = 0U;
    MonitorSize = 0U;
    KernelSize = 0U;
    LosInstallerMemorySet(Candidates, 0, sizeof(Candidates));

    Status = SystemTable->BootServices->HandleProtocol(
        ImageHandle,
        (EFI_GUID *)&EfiLoadedImageProtocolGuid,
        (void **)&LoadedImage);
    if (EFI_ERROR(Status) || LoadedImage == 0)
    {
        LosInstallerPrint(SystemTable, LOS_TEXT("Installer error: loaded image protocol unavailable.\r\n"));
        LosInstallerHaltForever();
    }

    Status = LosInstallerOpenRootForHandle(SystemTable, LoadedImage->DeviceHandle, &SourceRoot);
    if (EFI_ERROR(Status) || SourceRoot == 0)
    {
        LosInstallerPrint(SystemTable, LOS_TEXT("Installer error: could not open source volume.\r\n"));
        LosInstallerHaltForever();
    }

    LosInstallerPrint(SystemTable, LOS_TEXT("Reading installation payload from ISO...\r\n"));
    Status = LosInstallerReadFileIntoPool(
        SystemTable,
        SourceRoot,
        LOS_TEXT("\\EFI\\BOOT\\LOADERX64.EFI"),
        &LoaderBuffer,
        &LoaderSize);
    if (EFI_ERROR(Status) || LoaderBuffer == 0 || LoaderSize == 0U)
    {
        SourceRoot->Close(SourceRoot);
        LosInstallerPrintStatusError(SystemTable, LOS_TEXT("Installer error: missing LOADERX64.EFI on ISO. Status "), Status);
        LosInstallerHaltForever();
    }

    Status = LosInstallerReadFileIntoPool(
        SystemTable,
        SourceRoot,
        LOS_TEXT("\\EFI\\BOOT\\MONITORX64.EFI"),
        &MonitorBuffer,
        &MonitorSize);
    if (EFI_ERROR(Status) || MonitorBuffer == 0 || MonitorSize == 0U)
    {
        SystemTable->BootServices->FreePool(LoaderBuffer);
        SourceRoot->Close(SourceRoot);
        LosInstallerPrintStatusError(SystemTable, LOS_TEXT("Installer error: missing MONITORX64.EFI on ISO. Status "), Status);
        LosInstallerHaltForever();
    }

    Status = LosInstallerReadFileIntoPool(
        SystemTable,
        SourceRoot,
        LOS_TEXT("\\EFI\\BOOT\\KERNELX64.ELF"),
        &KernelBuffer,
        &KernelSize);
    SourceRoot->Close(SourceRoot);
    if (EFI_ERROR(Status) || KernelBuffer == 0 || KernelSize == 0U)
    {
        SystemTable->BootServices->FreePool(MonitorBuffer);
        SystemTable->BootServices->FreePool(LoaderBuffer);
        LosInstallerPrintStatusError(SystemTable, LOS_TEXT("Installer error: missing KERNELX64.ELF on ISO. Status "), Status);
        LosInstallerHaltForever();
    }

    LosInstallerPrint(SystemTable, LOS_TEXT("Enumerating writable block devices...\r\n"));
    Status = LosInstallerGetInstallCandidates(SystemTable, Candidates, LOS_MAX_INSTALL_CANDIDATES, &CandidateCount);
    if (EFI_ERROR(Status) || CandidateCount == 0U)
    {
        SystemTable->BootServices->FreePool(KernelBuffer);
        SystemTable->BootServices->FreePool(MonitorBuffer);
        SystemTable->BootServices->FreePool(LoaderBuffer);
        LosInstallerPrint(SystemTable, LOS_TEXT("Installer error: no writable target disks were found.\r\n"));
        LosInstallerHaltForever();
    }

    Status = LosInstallerChooseTargetDisk(SystemTable, Candidates, CandidateCount, &SelectedIndex);
    if (EFI_ERROR(Status))
    {
        SystemTable->BootServices->FreePool(KernelBuffer);
        SystemTable->BootServices->FreePool(MonitorBuffer);
        SystemTable->BootServices->FreePool(LoaderBuffer);
        LosInstallerPrintStatusError(SystemTable, LOS_TEXT("Installer error: could not choose target disk. Status "), Status);
        LosInstallerHaltForever();
    }

    Status = LosInstallerConfirmDestructiveInstall(SystemTable);
    if (EFI_ERROR(Status))
    {
        SystemTable->BootServices->FreePool(KernelBuffer);
        SystemTable->BootServices->FreePool(MonitorBuffer);
        SystemTable->BootServices->FreePool(LoaderBuffer);
        if (Status == EFI_ABORTED)
        {
            LosInstallerHaltForever();
        }
        LosInstallerPrintStatusError(SystemTable, LOS_TEXT("Installer error: confirmation failed. Status "), Status);
        LosInstallerHaltForever();
    }

    LosInstallerPrint(SystemTable, LOS_TEXT("Writing GPT partition table...\r\n"));
    Status = LosInstallerInstallToRawDisk(SystemTable, &Candidates[SelectedIndex], LoaderBuffer, LoaderSize, MonitorBuffer, MonitorSize, KernelBuffer, KernelSize, SelectedIndex + 1U);
    SystemTable->BootServices->FreePool(KernelBuffer);
    SystemTable->BootServices->FreePool(MonitorBuffer);
    SystemTable->BootServices->FreePool(LoaderBuffer);
    if (EFI_ERROR(Status))
    {
        LosInstallerPrintStatusError(SystemTable, LOS_TEXT("Installer error: installation failed. Status "), Status);
        LosInstallerHaltForever();
    }

    LosInstallerPrint(SystemTable, LOS_TEXT("\r\nInstallation completed successfully.\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("Installed layout:\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("  GPT disk with an EFI System Partition and a Liberation Data partition\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("  Both partitions are FAT32 for now\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("  ESP files:\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("    \\EFI\\BOOT\\BOOTX64.EFI\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("    \\EFI\\BOOT\\MONITORX64.EFI\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("    \\EFI\\BOOT\\BOOTINFO.TXT\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("  Liberation data files:\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("    \\LIBERATION\\KERNELX64.ELF\r\n\r\n"));
    LosInstallerPrint(SystemTable, LOS_TEXT("Requesting EFI reboot so QEMU can retry from the installed disk without the ISO.\r\n"));
    LosInstallerRequestColdReboot(SystemTable);
    return EFI_SUCCESS;
}
