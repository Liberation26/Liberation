/*
 * File Name: MemoryManagerBootstrapDispatch.c
 * File Version: 0.3.23
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */



#include "MemoryManagerBootstrapInternal.h"

#define LOS_MEMORY_MANAGER_BOOTSTRAP_LOCAL_STACK_BASE 0x0000000000800000ULL
#define LOS_MEMORY_MANAGER_BOOTSTRAP_LOCAL_STACK_GAP_BYTES 0x0000000000010000ULL
#define LOS_MEMORY_MANAGER_BOOTSTRAP_LOCAL_LOW_HALF_LIMIT 0x0000800000000000ULL

#define LOS_X64_PAGE_PRESENT 0x001ULL
#define LOS_X64_PAGE_WRITABLE 0x002ULL
#define LOS_X64_PAGE_USER 0x004ULL
#define LOS_X64_PAGE_NX 0x8000000000000000ULL

#define LOS_ELF_MAGIC_0 0x7FU
#define LOS_ELF_MAGIC_1 0x45U
#define LOS_ELF_MAGIC_2 0x4CU
#define LOS_ELF_MAGIC_3 0x46U
#define LOS_ELF_CLASS_64 2U
#define LOS_ELF_DATA_LITTLE_ENDIAN 1U
#define LOS_ELF_MACHINE_X86_64 0x3EU
#define LOS_ELF_TYPE_EXEC 2U
#define LOS_ELF_PROGRAM_HEADER_TYPE_LOAD 1U
#define LOS_ELF_PROGRAM_HEADER_FLAG_EXECUTE 0x1U
#define LOS_ELF_PROGRAM_HEADER_FLAG_WRITE 0x2U


#include "MemoryManagerBootstrapDispatchSections/MemoryManagerBootstrapDispatchSection01.c"
#include "MemoryManagerBootstrapDispatchSections/MemoryManagerBootstrapDispatchSection02.c"
#include "MemoryManagerBootstrapDispatchSections/MemoryManagerBootstrapDispatchSection03.c"
#include "MemoryManagerBootstrapDispatchSections/MemoryManagerBootstrapDispatchSection04.c"
#include "MemoryManagerBootstrapDispatchSections/MemoryManagerBootstrapDispatchSection05.c"
#include "MemoryManagerBootstrapDispatchSections/MemoryManagerBootstrapDispatchSection06.c"
