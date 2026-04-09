/*
 * File Name: KernelScreen.c
 * File Version: 0.3.15
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */



#include "KernelMain.h"
#include "VirtualMemory.h"

#define LOS_KERNEL_SCREEN_VIRTUAL_BASE 0xFFFF900000000000ULL
#define LOS_KERNEL_SCREEN_DEFAULT_CELL_WIDTH 8U
#define LOS_KERNEL_SCREEN_DEFAULT_CELL_HEIGHT 8U
#define LOS_KERNEL_SCREEN_DEFAULT_GLYPH_WIDTH 5U
#define LOS_KERNEL_SCREEN_DEFAULT_GLYPH_HEIGHT 7U
#define LOS_KERNEL_PAGE_WRITABLE 0x002ULL
#define LOS_KERNEL_PAGE_WRITE_THROUGH 0x008ULL
#define LOS_KERNEL_PAGE_CACHE_DISABLE 0x010ULL
#define LOS_PSF2_MAGIC 0x864AB572U


#include "KernelScreenSections/KernelScreenSection01.c"
#include "KernelScreenSections/KernelScreenSection02.c"
#include "KernelScreenSections/KernelScreenSection03.c"
#include "KernelScreenSections/KernelScreenSection04.c"
