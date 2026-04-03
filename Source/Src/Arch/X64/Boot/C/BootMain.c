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
    LosBootPrint(SystemTable, LOS_TEXT("Liberation OS\r\n"));
    LosBootPrint(SystemTable, LOS_TEXT("BOOTX64.EFI Loader\r\n"));

#if defined(LIBERATION_BOOT_FROM_DIRECTORY)
    LosBootPrint(SystemTable, LOS_TEXT("Running From Directory\r\n"));
#elif defined(LIBERATION_BOOT_FROM_HARD_DRIVE)
    LosBootPrint(SystemTable, LOS_TEXT("Running From Hard Drive\r\n"));
#else
    LosBootPrint(SystemTable, LOS_TEXT("Running From ISO\r\n"));
#endif

    LosBootPrint(SystemTable, LOS_TEXT("\r\n"));

#if defined(LIBERATION_BOOT_FROM_ISO)
    return LosRunInstaller(ImageHandle, SystemTable);
#else
    EFI_STATUS Status;

    LosBootPrint(SystemTable, LOS_TEXT("Starting kernel monitor EFI application...\r\n"));
    Status = LosBootLaunchMonitor(ImageHandle, SystemTable, LosBootMonitorPath);
    if (EFI_ERROR(Status))
    {
        LosBootPrintStatusError(SystemTable, LOS_TEXT("Kernel monitor launch failed. Status "), Status);
        LosBootHaltForever();
    }

    LosBootPrint(SystemTable, LOS_TEXT("Kernel monitor returned unexpectedly.\r\n"));
    LosBootHaltForever();
    return EFI_SUCCESS;
#endif
}
