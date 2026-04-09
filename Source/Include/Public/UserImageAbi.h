/*
 * File Name: UserImageAbi.h
 * File Version: 0.4.33
 * Author: OpenAI
 * Creation Timestamp: 2026-04-08T20:10:00Z
 * Last Update Timestamp: 2026-04-09T17:15:00Z
 * Operating System Name: Liberation OS
 * Purpose: Declares the generic callable user-image ABI used by shell runtime dispatch.
 */

#ifndef LOS_PUBLIC_USER_IMAGE_ABI_H
#define LOS_PUBLIC_USER_IMAGE_ABI_H

#include "Efi.h"

#define LOS_USER_IMAGE_CALL_VERSION 1U
#define LOS_USER_IMAGE_CALL_PATH_LENGTH 128U
#define LOS_USER_IMAGE_MAX_STAGED_IMAGE_SIZE (256U * 1024U)

#define LOS_USER_IMAGE_CALL_SIGNATURE 0x55494D4743414C4CULL

#define LOS_USER_IMAGE_CALL_KIND_NONE    0U
#define LOS_USER_IMAGE_CALL_KIND_COMMAND 1U
#define LOS_USER_IMAGE_CALL_KIND_LIBRARY 2U

#define LOS_USER_IMAGE_CALL_STATUS_SUCCESS            0U
#define LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER  1U
#define LOS_USER_IMAGE_CALL_STATUS_UNSUPPORTED        2U
#define LOS_USER_IMAGE_CALL_STATUS_NOT_FOUND          3U
#define LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED      4U
#define LOS_USER_IMAGE_CALL_STATUS_ACCESS_DENIED      5U
#define LOS_USER_IMAGE_CALL_STATUS_TRUNCATED          6U
#define LOS_USER_IMAGE_CALL_STATUS_ISOLATION_UNAVAILABLE 7U
#define LOS_USER_IMAGE_CALL_STATUS_TRANSITION_FAILED   8U

#define LOS_USER_IMAGE_CALL_FLAG_NONE          0U
#define LOS_USER_IMAGE_CALL_FLAG_RESPONSE_ONLY 0x00000001U

#define LOS_USER_IMAGE_FILE_STATUS_SUCCESS            0U
#define LOS_USER_IMAGE_FILE_STATUS_INVALID_PARAMETER  1U
#define LOS_USER_IMAGE_FILE_STATUS_NOT_FOUND          2U
#define LOS_USER_IMAGE_FILE_STATUS_BUFFER_TOO_SMALL   3U
#define LOS_USER_IMAGE_FILE_STATUS_IO_ERROR           4U
#define LOS_USER_IMAGE_FILE_STATUS_UNSUPPORTED        5U

#define LOS_ELF_MAGIC_0 0x7FU
#define LOS_ELF_MAGIC_1 0x45U
#define LOS_ELF_MAGIC_2 0x4CU
#define LOS_ELF_MAGIC_3 0x46U
#define LOS_ELF_CLASS_64 2U
#define LOS_ELF_DATA_LITTLE_ENDIAN 1U
#define LOS_ELF_MACHINE_X86_64 0x3EU
#define LOS_ELF_TYPE_EXEC 2U
#define LOS_ELF_PROGRAM_TYPE_NULL 0U
#define LOS_ELF_PROGRAM_TYPE_LOAD 1U
#define LOS_ELF_PROGRAM_FLAG_EXECUTE 0x1U
#define LOS_ELF_PROGRAM_FLAG_WRITE   0x2U
#define LOS_ELF_PROGRAM_FLAG_READ    0x4U

#define LOS_USER_IMAGE_EXECUTION_MODE_BOOTSTRAP 0U
#define LOS_USER_IMAGE_EXECUTION_MODE_DISK      1U
#define LOS_USER_IMAGE_EXECUTION_MODE_RING3     2U

#define LOS_USER_IMAGE_RING3_FLAG_NONE          0U
#define LOS_USER_IMAGE_RING3_FLAG_IF_ENABLED       0x00000001U
#define LOS_USER_IMAGE_RING3_FLAG_VALIDATE_FRAME   0x00000002U
#define LOS_USER_IMAGE_RING3_FLAG_RETURN_STAGED    0x00000004U
#define LOS_USER_IMAGE_MAPPING_FLAG_READ        0x00000001U
#define LOS_USER_IMAGE_MAPPING_FLAG_WRITE       0x00000002U
#define LOS_USER_IMAGE_MAPPING_FLAG_EXECUTE     0x00000004U
#define LOS_USER_IMAGE_MAPPING_FLAG_USER        0x00000008U
#define LOS_USER_IMAGE_MAPPING_FLAG_PRESENT     0x00000010U

#define LOS_USER_IMAGE_MAPPING_KIND_IMAGE       1U
#define LOS_USER_IMAGE_MAPPING_KIND_STACK       2U
#define LOS_USER_IMAGE_MAPPING_KIND_CALL_BLOCK  3U
#define LOS_USER_IMAGE_MAPPING_KIND_PAGING      4U

#define LOS_USER_IMAGE_MAX_MAPPING_RECORDS      16U


typedef struct
{
    UINT32 Version;
    UINT32 CallKind;
    UINT32 Flags;
    UINT32 Reserved0;
    UINT64 Signature;
    UINT64 RequestAddress;
    UINT64 RequestSize;
    UINT64 ResponseAddress;
    UINT64 ResponseSize;
    UINT64 ResultAddress;
    UINT64 ResultSize;
    char Path[LOS_USER_IMAGE_CALL_PATH_LENGTH];
} LOS_USER_IMAGE_CALL;

typedef struct
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
    UINT16 SectionNameIndex;
} LOS_USER_IMAGE_ELF64_HEADER;

typedef struct
{
    UINT32 Type;
    UINT32 Flags;
    UINT64 Offset;
    UINT64 VirtualAddress;
    UINT64 PhysicalAddress;
    UINT64 FileSize;
    UINT64 MemorySize;
    UINT64 Alignment;
} LOS_USER_IMAGE_ELF64_PROGRAM_HEADER;

typedef struct
{
    UINT64 VirtualAddress;
    UINT64 PhysicalAddress;
    UINT64 Size;
    UINT32 Flags;
    UINT32 Kind;
} LOS_USER_IMAGE_MAPPING_RECORD;

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT64 AddressSpaceId;
    UINT64 RootTablePhysicalAddress;
    UINT64 MappingRecordAddress;
    UINT32 MappingCount;
    UINT32 Reserved0;
} LOS_USER_IMAGE_ADDRESS_SPACE_DESCRIPTOR;

typedef struct
{
    UINT32 Version;
    UINT32 ExecutionMode;
    UINT64 CallAddress;
    UINT64 ImageAddress;
    UINT64 ImageSize;
    UINT64 EntryAddress;
    UINT64 StackAddress;
    UINT64 StackSize;
} LOS_USER_IMAGE_EXECUTION_CONTEXT;

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT64 AddressSpaceId;
    UINT64 RootTablePhysicalAddress;
    UINT64 UserImageBaseAddress;
    UINT64 UserImageSize;
    UINT64 UserEntryAddress;
    UINT64 UserStackBaseAddress;
    UINT64 UserStackSize;
    UINT64 UserStackPointer;
    UINT64 CallBlockUserAddress;
    UINT64 CallBlockKernelAddress;
    UINT64 MappingRecordAddress;
    UINT32 MappingCount;
    UINT32 Reserved0;
    LOS_USER_IMAGE_ADDRESS_SPACE_DESCRIPTOR AddressSpace;
} LOS_USER_IMAGE_ISOLATED_SPACE;

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT64 UserInstructionPointer;
    UINT64 UserStackPointer;
    UINT64 UserCallArgument;
    UINT64 PageMapLevel4PhysicalAddress;
    UINT64 UserCodeSelector;
    UINT64 UserDataSelector;
    UINT64 UserRflags;
    UINT64 KernelResumeInstructionPointer;
    UINT64 KernelResumeStackPointer;
    UINT64 CompletionStatusAddress;
    UINT64 CompletionResultValue;
} LOS_USER_IMAGE_RING3_CONTEXT;

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT64 Status;
    UINT64 ResultValue;
} LOS_USER_IMAGE_COMPLETION_RECORD;

#endif
