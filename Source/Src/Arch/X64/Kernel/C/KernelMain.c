/*
 * File Name: KernelMain.c
 * File Version: 0.3.13
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T10:54:19Z
 * Last Update Timestamp: 2026-04-10T19:10:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */



#include "KernelMain.h"
#include "Interrupts.h"
#include "VirtualMemory.h"
#include "MemoryManagerBootstrap.h"
#include "Scheduler.h"
#include "Diagnostics.h"

#define LOS_SERIAL_COM1_BASE 0x3F8U
#define LOS_GDT_CODE_FLAGS 0x9AU
#define LOS_GDT_DATA_FLAGS 0x92U
#define LOS_GDT_USER_CODE_FLAGS 0xFAU
#define LOS_GDT_USER_DATA_FLAGS 0xF2U
#define LOS_GDT_GRANULARITY 0xAFU
#define LOS_GDT_TSS_ACCESS 0x89U


#include "KernelMainSections/KernelMainSection01.c"
#include "KernelMainSections/KernelMainSection02.c"
