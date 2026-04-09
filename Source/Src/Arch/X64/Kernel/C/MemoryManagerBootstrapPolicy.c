/*
 * File Name: MemoryManagerBootstrapPolicy.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

#include "MemoryManagerBootstrapInternal.h"

#define LOS_ELF_MAGIC_0 0x7FU
#define LOS_ELF_MAGIC_1 0x45U
#define LOS_ELF_MAGIC_2 0x4CU
#define LOS_ELF_MAGIC_3 0x46U
#define LOS_ELF_CLASS_64 2U
#define LOS_ELF_DATA_LITTLE_ENDIAN 1U
#define LOS_ELF_MACHINE_X86_64 0x3EU
#define LOS_ELF_TYPE_EXEC 2U

typedef struct __attribute__((packed))
{
    UINT8 Ident[16];
    UINT16 Type;
    UINT16 Machine;
    UINT32 Version;
    UINT64 Entry;
    UINT64 ProgramHeaderOffset;
    UINT64 SectionHeaderOffset;
    UINT32 Flags;
    UINT16 HeaderSize;
    UINT16 ProgramHeaderEntrySize;
    UINT16 ProgramHeaderCount;
    UINT16 SectionHeaderEntrySize;
    UINT16 SectionHeaderCount;
    UINT16 SectionNameStringIndex;
} LOS_MEMORY_MANAGER_ELF64_HEADER;

BOOLEAN LosMemoryManagerBootstrapOperationSupported(UINT32 Operation)
{
    switch (Operation)
    {
        case LOS_MEMORY_MANAGER_OPERATION_BOOTSTRAP_ATTACH:
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_FREE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_CREATE_ADDRESS_SPACE:
        case LOS_MEMORY_MANAGER_OPERATION_DESTROY_ADDRESS_SPACE:
        case LOS_MEMORY_MANAGER_OPERATION_MAP_PAGES:
        case LOS_MEMORY_MANAGER_OPERATION_UNMAP_PAGES:
        case LOS_MEMORY_MANAGER_OPERATION_PROTECT_PAGES:
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MAPPING:
        case LOS_MEMORY_MANAGER_OPERATION_ATTACH_STAGED_IMAGE:
        case LOS_MEMORY_MANAGER_OPERATION_ALLOCATE_ADDRESS_SPACE_STACK:
        case LOS_MEMORY_MANAGER_OPERATION_QUERY_MEMORY_REGIONS:
        case LOS_MEMORY_MANAGER_OPERATION_RESERVE_FRAMES:
        case LOS_MEMORY_MANAGER_OPERATION_CLAIM_FRAMES:
            return 1;
        default:
            return 0;
    }
}

BOOLEAN LosMemoryManagerBootstrapValidateServiceImage(void)
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *State;
    const LOS_MEMORY_MANAGER_ELF64_HEADER *Header;

    State = LosMemoryManagerBootstrapState();
    if (State->ServiceImageVirtualAddress == 0ULL || State->Info.ServiceImageSize < sizeof(LOS_MEMORY_MANAGER_ELF64_HEADER))
    {
        return 0;
    }

    Header = (const LOS_MEMORY_MANAGER_ELF64_HEADER *)(UINTN)State->ServiceImageVirtualAddress;
    if (Header->Ident[0] != LOS_ELF_MAGIC_0 ||
        Header->Ident[1] != LOS_ELF_MAGIC_1 ||
        Header->Ident[2] != LOS_ELF_MAGIC_2 ||
        Header->Ident[3] != LOS_ELF_MAGIC_3 ||
        Header->Ident[4] != LOS_ELF_CLASS_64 ||
        Header->Ident[5] != LOS_ELF_DATA_LITTLE_ENDIAN ||
        Header->Machine != LOS_ELF_MACHINE_X86_64 ||
        Header->Type != LOS_ELF_TYPE_EXEC ||
        Header->ProgramHeaderCount == 0U)
    {
        return 0;
    }

    State->Info.ServiceEntryVirtualAddress = Header->Entry;
    LosMemoryManagerBootstrapSetFlag(LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_SERVICE_IMAGE_READY);
    LosMemoryManagerBootstrapTransitionTo(LOS_MEMORY_MANAGER_BOOTSTRAP_STATE_IMAGE_READY);
    return 1;
}
