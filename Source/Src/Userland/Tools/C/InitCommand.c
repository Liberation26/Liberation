/*
 * File Name: InitCommand.c
 * File Version: 0.4.10
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T11:02:18Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS userland component.
 */



#include "InitCommand.h"
#include "ShellMain.h"

#define LOS_ELF_MAGIC_0 0x7FU
#define LOS_ELF_MAGIC_1 0x45U
#define LOS_ELF_MAGIC_2 0x4CU
#define LOS_ELF_MAGIC_3 0x46U
#define LOS_ELF_CLASS_64 2U
#define LOS_ELF_DATA_LITTLE_ENDIAN 1U
#define LOS_ELF_MACHINE_X86_64 0x3EU
#define LOS_ELF_TYPE_EXEC 2U


#include "InitCommandSections/InitCommandSection01.c"
#include "InitCommandSections/InitCommandSection02.c"
#include "InitCommandSections/InitCommandSection03.c"
#include "InitCommandSections/InitCommandSection04.c"
