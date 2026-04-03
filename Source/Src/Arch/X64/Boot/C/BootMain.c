#include "BootMain.h"
#include "BootInternal.h"

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    LOS_BOOT_ENTER(SystemTable);
    if (SystemTable == 0 || SystemTable->ConOut == 0)
    {
        return EFI_SUCCESS;
    }

    if (SystemTable->ConOut->Reset != 0)
    {
        SystemTable->ConOut->Reset(SystemTable->ConOut, 1);
    }

    LosBootClear(SystemTable);
    LosBootStatusOk(SystemTable, LOS_TEXT("Liberation OS"));
    LosBootStatusOk(SystemTable, LOS_TEXT("BOOTX64.EFI Loader"));

#if defined(LIBERATION_BOOT_FROM_DIRECTORY)
    LosBootStatusOk(SystemTable, LOS_TEXT("Running From Directory"));
#elif defined(LIBERATION_BOOT_FROM_HARD_DRIVE)
    LosBootStatusOk(SystemTable, LOS_TEXT("Running From Hard Drive"));
#else
    LosBootStatusOk(SystemTable, LOS_TEXT("Running From ISO"));
#endif

#if defined(LIBERATION_BOOT_FROM_ISO)
    return LosRunInstaller(ImageHandle, SystemTable);
#else
    EFI_STATUS Status;

    LosBootStatusOk(SystemTable, LOS_TEXT("Starting kernel monitor EFI application."));
    Status = LosBootLaunchMonitor(ImageHandle, SystemTable, LosBootMonitorPath);
    if (EFI_ERROR(Status))
    {
        LosBootPrintStatusError(SystemTable, LOS_TEXT("Kernel monitor launch failed. Status "), Status);
        LosBootHaltForever();
    }

    LosBootStatusFail(SystemTable, LOS_TEXT("Kernel monitor returned unexpectedly."));
    LosBootHaltForever();
    return EFI_SUCCESS;
#endif
}
