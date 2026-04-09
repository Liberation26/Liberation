/*
 * File Name: MonitorMain.h
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements monitor-stage functionality for Liberation OS.
 */

#ifndef LOS_MONITOR_MAIN_H
#define LOS_MONITOR_MAIN_H

#include "Efi.h"

EFI_STATUS EFIAPI LosRunKernelMonitor(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

#endif
