/*
 * File Name: UserExecutionRuntime.c
 * File Version: 0.4.35
 * Author: OpenAI
 * Creation Timestamp: 2026-04-09T16:40:00Z
 * Last Update Timestamp: 2026-04-09T18:15:00Z
 * Operating System Name: Liberation OS
 * Purpose: Owns mapped user address spaces, mappings, activation state, and completion copy-back.
 */

#include "MemoryManagerMainInternal.h"
#include "UserImageAbi.h"

#define LOS_MEMORY_MANAGER_MAX_USER_SPACES 8U
#define LOS_MEMORY_MANAGER_USER_IMAGE_SLOT_SIZE   (256U * 1024U)
#define LOS_MEMORY_MANAGER_USER_STACK_SLOT_SIZE   (16U * 1024U)
#define LOS_MEMORY_MANAGER_USER_CALL_SLOT_SIZE    (4U * 1024U)
#define LOS_MEMORY_MANAGER_USER_PAGING_SLOT_SIZE  (4U * 4096U)
#define LOS_MEMORY_MANAGER_USER_CODE_SELECTOR     0x1BULL
#define LOS_MEMORY_MANAGER_USER_DATA_SELECTOR     0x23ULL
#define LOS_MEMORY_MANAGER_USER_STACK_TOP         0x0000007000000000ULL
#define LOS_MEMORY_MANAGER_USER_CALL_BASE         0x0000007100000000ULL
#define LOS_MEMORY_MANAGER_USER_PAGE_SIZE         0x1000ULL

typedef struct
{
    UINT8 InUse;
    UINT8 Active;
    UINT16 Reserved0;
    UINT64 AddressSpaceId;
    UINT64 SyntheticRootTablePhysicalAddress;
    UINT64 UserImageBaseVirtualAddress;
    UINT64 UserImageSize;
    UINT64 UserEntryVirtualAddress;
    UINT64 UserStackBaseVirtualAddress;
    UINT64 UserStackSize;
    UINT64 UserStackPointer;
    UINT64 CallBlockVirtualAddress;
    UINT64 CallBlockKernelAddress;
    UINT64 CompletionStatusKernelAddress;
    UINT64 CompletionResultKernelAddress;
    UINT32 MappingCount;
    UINT32 Reserved1;
} LOS_MEMORY_MANAGER_USER_SPACE_SLOT;

static LOS_MEMORY_MANAGER_USER_SPACE_SLOT LosMemoryManagerUserSpaceSlots[LOS_MEMORY_MANAGER_MAX_USER_SPACES];
static UINT8 LosMemoryManagerUserImageSlots[LOS_MEMORY_MANAGER_MAX_USER_SPACES][LOS_MEMORY_MANAGER_USER_IMAGE_SLOT_SIZE];
static UINT8 LosMemoryManagerUserStackSlots[LOS_MEMORY_MANAGER_MAX_USER_SPACES][LOS_MEMORY_MANAGER_USER_STACK_SLOT_SIZE];
static UINT8 LosMemoryManagerUserCallSlots[LOS_MEMORY_MANAGER_MAX_USER_SPACES][LOS_MEMORY_MANAGER_USER_CALL_SLOT_SIZE];
static UINT8 LosMemoryManagerUserPagingSlots[LOS_MEMORY_MANAGER_MAX_USER_SPACES][LOS_MEMORY_MANAGER_USER_PAGING_SLOT_SIZE];
static LOS_USER_IMAGE_MAPPING_RECORD LosMemoryManagerUserMappingSlots[LOS_MEMORY_MANAGER_MAX_USER_SPACES][LOS_USER_IMAGE_MAX_MAPPING_RECORDS];
static UINT64 LosMemoryManagerNextUserSpaceId = 1ULL;
static UINT64 LosMemoryManagerCurrentUserAddressSpaceId = 0ULL;

static void LosMemoryManagerUserExecZero(void *Buffer, UINTN Length)
{
    UINTN Index;
    if (Buffer == 0)
    {
        return;
    }
    for (Index = 0U; Index < Length; ++Index)
    {
        ((UINT8 *)Buffer)[Index] = 0U;
    }
}

static void LosMemoryManagerUserExecCopy(void *Destination, const void *Source, UINTN Length)
{
    UINTN Index;
    if (Destination == 0 || Source == 0)
    {
        return;
    }
    for (Index = 0U; Index < Length; ++Index)
    {
        ((UINT8 *)Destination)[Index] = ((const UINT8 *)Source)[Index];
    }
}

static UINT64 LosMemoryManagerUserExecAlignDown(UINT64 Value, UINT64 Alignment)
{
    if (Alignment == 0ULL)
    {
        return Value;
    }
    return Value & ~(Alignment - 1ULL);
}

static UINT64 LosMemoryManagerUserExecAlignUp(UINT64 Value, UINT64 Alignment)
{
    if (Alignment == 0ULL)
    {
        return Value;
    }
    return (Value + Alignment - 1ULL) & ~(Alignment - 1ULL);
}

static UINT64 LosMemoryManagerUserExecResolveEntry(UINT64 RawEntry,
                                                   UINT64 ImageBase,
                                                   UINT64 ImageSize)
{
    UINT64 ImageEnd;

    if (ImageBase == 0ULL || ImageSize == 0ULL)
    {
        return 0ULL;
    }

    ImageEnd = ImageBase + ImageSize;
    if (ImageEnd <= ImageBase)
    {
        return 0ULL;
    }

    if (RawEntry >= ImageBase && RawEntry < ImageEnd)
    {
        return RawEntry;
    }

    if (RawEntry < ImageSize)
    {
        RawEntry += ImageBase;
        if (RawEntry >= ImageBase && RawEntry < ImageEnd)
        {
            return RawEntry;
        }
    }

    return 0ULL;
}

static UINT64 LosMemoryManagerUserExecFindFreeSlot(void)
{
    UINT64 Index;
    for (Index = 0ULL; Index < LOS_MEMORY_MANAGER_MAX_USER_SPACES; ++Index)
    {
        if (LosMemoryManagerUserSpaceSlots[Index].InUse == 0U)
        {
            return Index;
        }
    }
    return LOS_MEMORY_MANAGER_MAX_USER_SPACES;
}

static UINT64 LosMemoryManagerUserExecFindSlotByAddressSpaceId(UINT64 AddressSpaceId)
{
    UINT64 Index;
    for (Index = 0ULL; Index < LOS_MEMORY_MANAGER_MAX_USER_SPACES; ++Index)
    {
        if (LosMemoryManagerUserSpaceSlots[Index].InUse != 0U &&
            LosMemoryManagerUserSpaceSlots[Index].AddressSpaceId == AddressSpaceId)
        {
            return Index;
        }
    }
    return LOS_MEMORY_MANAGER_MAX_USER_SPACES;
}

static void LosMemoryManagerUserExecAddMapping(UINT64 SlotIndex,
                                               UINT32 Kind,
                                               UINT32 Flags,
                                               UINT64 VirtualAddress,
                                               UINT64 PhysicalAddress,
                                               UINT64 Size)
{
    UINT32 MappingIndex;
    if (SlotIndex >= LOS_MEMORY_MANAGER_MAX_USER_SPACES)
    {
        return;
    }
    MappingIndex = LosMemoryManagerUserSpaceSlots[SlotIndex].MappingCount;
    if (MappingIndex >= LOS_USER_IMAGE_MAX_MAPPING_RECORDS)
    {
        return;
    }

    LosMemoryManagerUserMappingSlots[SlotIndex][MappingIndex].VirtualAddress = VirtualAddress;
    LosMemoryManagerUserMappingSlots[SlotIndex][MappingIndex].PhysicalAddress = PhysicalAddress;
    LosMemoryManagerUserMappingSlots[SlotIndex][MappingIndex].Size = Size;
    LosMemoryManagerUserMappingSlots[SlotIndex][MappingIndex].Flags = Flags;
    LosMemoryManagerUserMappingSlots[SlotIndex][MappingIndex].Kind = Kind;
    LosMemoryManagerUserSpaceSlots[SlotIndex].MappingCount = MappingIndex + 1U;
}

UINT64 LosMemoryManagerActivateUserAddressSpace(const LOS_USER_IMAGE_ADDRESS_SPACE_DESCRIPTOR *Descriptor)
{
    UINT64 SlotIndex;
    if (Descriptor == 0 || Descriptor->Version != LOS_USER_IMAGE_CALL_VERSION || Descriptor->AddressSpaceId == 0ULL)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }

    SlotIndex = LosMemoryManagerUserExecFindSlotByAddressSpaceId(Descriptor->AddressSpaceId);
    if (SlotIndex >= LOS_MEMORY_MANAGER_MAX_USER_SPACES)
    {
        return LOS_USER_IMAGE_CALL_STATUS_NOT_FOUND;
    }

    LosMemoryManagerCurrentUserAddressSpaceId = Descriptor->AddressSpaceId;
    LosMemoryManagerUserSpaceSlots[SlotIndex].Active = 1U;
    return LOS_USER_IMAGE_CALL_STATUS_SUCCESS;
}

UINT64 LosMemoryManagerCompleteUserAddressSpaceCall(const LOS_USER_IMAGE_ISOLATED_SPACE *IsolatedSpace,
                                                    UINT64 *CompletionStatus,
                                                    UINT64 *CompletionResult)
{
    UINT64 SlotIndex;
    const LOS_USER_IMAGE_CALL *MappedCall;

    if (CompletionStatus != 0)
    {
        *CompletionStatus = LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }
    if (CompletionResult != 0)
    {
        *CompletionResult = 0ULL;
    }
    if (IsolatedSpace == 0 || IsolatedSpace->Version != LOS_USER_IMAGE_CALL_VERSION)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }

    SlotIndex = LosMemoryManagerUserExecFindSlotByAddressSpaceId(IsolatedSpace->AddressSpace.AddressSpaceId);
    if (SlotIndex >= LOS_MEMORY_MANAGER_MAX_USER_SPACES)
    {
        return LOS_USER_IMAGE_CALL_STATUS_NOT_FOUND;
    }

    MappedCall = (const LOS_USER_IMAGE_CALL *)(UINTN)LosMemoryManagerUserSpaceSlots[SlotIndex].CallBlockKernelAddress;
    if (CompletionStatus != 0)
    {
        *CompletionStatus = LOS_USER_IMAGE_CALL_STATUS_SUCCESS;
    }
    if (CompletionResult != 0)
    {
        if (MappedCall->ResultAddress != 0ULL)
        {
            *CompletionResult = *(const UINT64 *)(UINTN)MappedCall->ResultAddress;
        }
        else
        {
            *CompletionResult = 0ULL;
        }
    }

    LosMemoryManagerUserSpaceSlots[SlotIndex].Active = 0U;
    if (LosMemoryManagerCurrentUserAddressSpaceId == LosMemoryManagerUserSpaceSlots[SlotIndex].AddressSpaceId)
    {
        LosMemoryManagerCurrentUserAddressSpaceId = 0ULL;
    }
    return LOS_USER_IMAGE_CALL_STATUS_SUCCESS;
}

UINT64 LosUserExecuteIsolatedImage(const LOS_USER_IMAGE_EXECUTION_CONTEXT *Context,
                                   LOS_USER_IMAGE_ISOLATED_SPACE *IsolatedSpace,
                                   LOS_USER_IMAGE_RING3_CONTEXT *Ring3Context)
{
    const LOS_USER_IMAGE_ELF64_HEADER *Header;
    UINT64 SlotIndex;
    UINT64 LowestVirtualAddress = ~0ULL;
    UINT64 HighestVirtualAddress = 0ULL;
    UINT64 UserEntryVirtualAddress;
    UINT16 ProgramIndex;

    if (Context == 0 || IsolatedSpace == 0 || Ring3Context == 0)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }
    if (Context->Version != LOS_USER_IMAGE_CALL_VERSION ||
        Context->ImageAddress == 0ULL ||
        Context->ImageSize < sizeof(LOS_USER_IMAGE_ELF64_HEADER) ||
        Context->CallAddress == 0ULL)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }

    SlotIndex = LosMemoryManagerUserExecFindFreeSlot();
    if (SlotIndex >= LOS_MEMORY_MANAGER_MAX_USER_SPACES)
    {
        return LOS_USER_IMAGE_CALL_STATUS_TRUNCATED;
    }

    Header = (const LOS_USER_IMAGE_ELF64_HEADER *)(UINTN)Context->ImageAddress;
    for (ProgramIndex = 0U; ProgramIndex < Header->ProgramHeaderCount; ++ProgramIndex)
    {
        const LOS_USER_IMAGE_ELF64_PROGRAM_HEADER *ProgramHeader;
        UINT64 ProgramOffset = Header->ProgramHeaderOffset + ((UINT64)ProgramIndex * (UINT64)Header->ProgramHeaderEntrySize);

        if (ProgramOffset + sizeof(*ProgramHeader) > Context->ImageSize)
        {
            return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
        }

        ProgramHeader = (const LOS_USER_IMAGE_ELF64_PROGRAM_HEADER *)((const UINT8 *)(UINTN)Context->ImageAddress + ProgramOffset);
        if (ProgramHeader->Type != LOS_ELF_PROGRAM_TYPE_LOAD)
        {
            continue;
        }
        if (ProgramHeader->MemorySize < ProgramHeader->FileSize ||
            ProgramHeader->Offset + ProgramHeader->FileSize > Context->ImageSize)
        {
            return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
        }

        if (ProgramHeader->VirtualAddress < LowestVirtualAddress)
        {
            LowestVirtualAddress = ProgramHeader->VirtualAddress;
        }
        if (ProgramHeader->VirtualAddress + ProgramHeader->MemorySize > HighestVirtualAddress)
        {
            HighestVirtualAddress = ProgramHeader->VirtualAddress + ProgramHeader->MemorySize;
        }
    }

    if (LowestVirtualAddress == ~0ULL || HighestVirtualAddress <= LowestVirtualAddress)
    {
        return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
    }

    LowestVirtualAddress = LosMemoryManagerUserExecAlignDown(LowestVirtualAddress, LOS_MEMORY_MANAGER_USER_PAGE_SIZE);
    HighestVirtualAddress = LosMemoryManagerUserExecAlignUp(HighestVirtualAddress, LOS_MEMORY_MANAGER_USER_PAGE_SIZE);
    if (HighestVirtualAddress - LowestVirtualAddress > LOS_MEMORY_MANAGER_USER_IMAGE_SLOT_SIZE)
    {
        return LOS_USER_IMAGE_CALL_STATUS_TRUNCATED;
    }

    UserEntryVirtualAddress = LosMemoryManagerUserExecResolveEntry(Header->Entry,
                                                                   LowestVirtualAddress,
                                                                   HighestVirtualAddress - LowestVirtualAddress);
    if (UserEntryVirtualAddress == 0ULL)
    {
        return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
    }

    LosMemoryManagerUserExecZero(&LosMemoryManagerUserImageSlots[SlotIndex][0], LOS_MEMORY_MANAGER_USER_IMAGE_SLOT_SIZE);
    LosMemoryManagerUserExecZero(&LosMemoryManagerUserStackSlots[SlotIndex][0], LOS_MEMORY_MANAGER_USER_STACK_SLOT_SIZE);
    LosMemoryManagerUserExecZero(&LosMemoryManagerUserCallSlots[SlotIndex][0], LOS_MEMORY_MANAGER_USER_CALL_SLOT_SIZE);
    LosMemoryManagerUserExecZero(&LosMemoryManagerUserPagingSlots[SlotIndex][0], LOS_MEMORY_MANAGER_USER_PAGING_SLOT_SIZE);
    LosMemoryManagerUserExecZero(&LosMemoryManagerUserMappingSlots[SlotIndex][0], sizeof(LosMemoryManagerUserMappingSlots[SlotIndex]));
    LosMemoryManagerUserExecZero(&LosMemoryManagerUserSpaceSlots[SlotIndex], sizeof(LosMemoryManagerUserSpaceSlots[SlotIndex]));

    for (ProgramIndex = 0U; ProgramIndex < Header->ProgramHeaderCount; ++ProgramIndex)
    {
        const LOS_USER_IMAGE_ELF64_PROGRAM_HEADER *ProgramHeader;
        UINT64 ProgramOffset = Header->ProgramHeaderOffset + ((UINT64)ProgramIndex * (UINT64)Header->ProgramHeaderEntrySize);
        UINT64 DestinationOffset;
        UINT64 ByteIndex;
        UINT32 MappingFlags = LOS_USER_IMAGE_MAPPING_FLAG_PRESENT |
                              LOS_USER_IMAGE_MAPPING_FLAG_USER |
                              LOS_USER_IMAGE_MAPPING_FLAG_READ;

        ProgramHeader = (const LOS_USER_IMAGE_ELF64_PROGRAM_HEADER *)((const UINT8 *)(UINTN)Context->ImageAddress + ProgramOffset);
        if (ProgramHeader->Type != LOS_ELF_PROGRAM_TYPE_LOAD)
        {
            continue;
        }

        DestinationOffset = ProgramHeader->VirtualAddress - LowestVirtualAddress;
        if (DestinationOffset + ProgramHeader->MemorySize > LOS_MEMORY_MANAGER_USER_IMAGE_SLOT_SIZE)
        {
            return LOS_USER_IMAGE_CALL_STATUS_TRUNCATED;
        }

        for (ByteIndex = 0ULL; ByteIndex < ProgramHeader->FileSize; ++ByteIndex)
        {
            LosMemoryManagerUserImageSlots[SlotIndex][DestinationOffset + ByteIndex] =
                ((const UINT8 *)(UINTN)Context->ImageAddress)[ProgramHeader->Offset + ByteIndex];
        }
        for (ByteIndex = ProgramHeader->FileSize; ByteIndex < ProgramHeader->MemorySize; ++ByteIndex)
        {
            LosMemoryManagerUserImageSlots[SlotIndex][DestinationOffset + ByteIndex] = 0U;
        }

        if ((ProgramHeader->Flags & LOS_ELF_PROGRAM_FLAG_WRITE) != 0U)
        {
            MappingFlags |= LOS_USER_IMAGE_MAPPING_FLAG_WRITE;
        }
        if ((ProgramHeader->Flags & LOS_ELF_PROGRAM_FLAG_EXECUTE) != 0U)
        {
            MappingFlags |= LOS_USER_IMAGE_MAPPING_FLAG_EXECUTE;
        }

        LosMemoryManagerUserExecAddMapping(
            SlotIndex,
            LOS_USER_IMAGE_MAPPING_KIND_IMAGE,
            MappingFlags,
            LosMemoryManagerUserExecAlignDown(ProgramHeader->VirtualAddress, LOS_MEMORY_MANAGER_USER_PAGE_SIZE),
            (UINT64)(UINTN)&LosMemoryManagerUserImageSlots[SlotIndex][LosMemoryManagerUserExecAlignDown(DestinationOffset, LOS_MEMORY_MANAGER_USER_PAGE_SIZE)],
            LosMemoryManagerUserExecAlignUp(ProgramHeader->MemorySize, LOS_MEMORY_MANAGER_USER_PAGE_SIZE));
    }

    LosMemoryManagerUserExecCopy(&LosMemoryManagerUserCallSlots[SlotIndex][0],
                                 (const void *)(UINTN)Context->CallAddress,
                                 sizeof(LOS_USER_IMAGE_CALL));

    LosMemoryManagerUserSpaceSlots[SlotIndex].InUse = 1U;
    LosMemoryManagerUserSpaceSlots[SlotIndex].Active = 0U;
    LosMemoryManagerUserSpaceSlots[SlotIndex].AddressSpaceId = LosMemoryManagerNextUserSpaceId++;
    LosMemoryManagerUserSpaceSlots[SlotIndex].SyntheticRootTablePhysicalAddress =
        (UINT64)(UINTN)&LosMemoryManagerUserPagingSlots[SlotIndex][0];
    LosMemoryManagerUserSpaceSlots[SlotIndex].UserImageBaseVirtualAddress = LowestVirtualAddress;
    LosMemoryManagerUserSpaceSlots[SlotIndex].UserImageSize = HighestVirtualAddress - LowestVirtualAddress;
    LosMemoryManagerUserSpaceSlots[SlotIndex].UserEntryVirtualAddress = UserEntryVirtualAddress;
    LosMemoryManagerUserSpaceSlots[SlotIndex].UserStackBaseVirtualAddress = LOS_MEMORY_MANAGER_USER_STACK_TOP - LOS_MEMORY_MANAGER_USER_STACK_SLOT_SIZE;
    LosMemoryManagerUserSpaceSlots[SlotIndex].UserStackSize = LOS_MEMORY_MANAGER_USER_STACK_SLOT_SIZE;
    LosMemoryManagerUserSpaceSlots[SlotIndex].UserStackPointer = LOS_MEMORY_MANAGER_USER_STACK_TOP - 16ULL;
    LosMemoryManagerUserSpaceSlots[SlotIndex].CallBlockVirtualAddress = LOS_MEMORY_MANAGER_USER_CALL_BASE;
    LosMemoryManagerUserSpaceSlots[SlotIndex].CallBlockKernelAddress = (UINT64)(UINTN)&LosMemoryManagerUserCallSlots[SlotIndex][0];
    LosMemoryManagerUserSpaceSlots[SlotIndex].CompletionStatusKernelAddress = (UINT64)(UINTN)&LosMemoryManagerUserCallSlots[SlotIndex][0];
    LosMemoryManagerUserSpaceSlots[SlotIndex].CompletionResultKernelAddress = (UINT64)(UINTN)&LosMemoryManagerUserCallSlots[SlotIndex][0];
    LosMemoryManagerUserSpaceSlots[SlotIndex].MappingCount = 0U;

    LosMemoryManagerUserExecAddMapping(
        SlotIndex,
        LOS_USER_IMAGE_MAPPING_KIND_STACK,
        LOS_USER_IMAGE_MAPPING_FLAG_PRESENT | LOS_USER_IMAGE_MAPPING_FLAG_USER |
        LOS_USER_IMAGE_MAPPING_FLAG_READ | LOS_USER_IMAGE_MAPPING_FLAG_WRITE,
        LosMemoryManagerUserSpaceSlots[SlotIndex].UserStackBaseVirtualAddress,
        (UINT64)(UINTN)&LosMemoryManagerUserStackSlots[SlotIndex][0],
        LOS_MEMORY_MANAGER_USER_STACK_SLOT_SIZE);

    LosMemoryManagerUserExecAddMapping(
        SlotIndex,
        LOS_USER_IMAGE_MAPPING_KIND_CALL_BLOCK,
        LOS_USER_IMAGE_MAPPING_FLAG_PRESENT | LOS_USER_IMAGE_MAPPING_FLAG_USER |
        LOS_USER_IMAGE_MAPPING_FLAG_READ | LOS_USER_IMAGE_MAPPING_FLAG_WRITE,
        LosMemoryManagerUserSpaceSlots[SlotIndex].CallBlockVirtualAddress,
        LosMemoryManagerUserSpaceSlots[SlotIndex].CallBlockKernelAddress,
        LOS_MEMORY_MANAGER_USER_CALL_SLOT_SIZE);

    LosMemoryManagerUserExecAddMapping(
        SlotIndex,
        LOS_USER_IMAGE_MAPPING_KIND_PAGING,
        LOS_USER_IMAGE_MAPPING_FLAG_PRESENT | LOS_USER_IMAGE_MAPPING_FLAG_READ |
        LOS_USER_IMAGE_MAPPING_FLAG_WRITE,
        0ULL,
        LosMemoryManagerUserSpaceSlots[SlotIndex].SyntheticRootTablePhysicalAddress,
        LOS_MEMORY_MANAGER_USER_PAGING_SLOT_SIZE);

    IsolatedSpace->Version = LOS_USER_IMAGE_CALL_VERSION;
    IsolatedSpace->Flags = LOS_USER_IMAGE_RING3_FLAG_IF_ENABLED;
    IsolatedSpace->AddressSpaceId = LosMemoryManagerUserSpaceSlots[SlotIndex].AddressSpaceId;
    IsolatedSpace->RootTablePhysicalAddress = LosMemoryManagerUserSpaceSlots[SlotIndex].SyntheticRootTablePhysicalAddress;
    IsolatedSpace->UserImageBaseAddress = LosMemoryManagerUserSpaceSlots[SlotIndex].UserImageBaseVirtualAddress;
    IsolatedSpace->UserImageSize = LosMemoryManagerUserSpaceSlots[SlotIndex].UserImageSize;
    IsolatedSpace->UserEntryAddress = LosMemoryManagerUserSpaceSlots[SlotIndex].UserEntryVirtualAddress;
    IsolatedSpace->UserStackBaseAddress = LosMemoryManagerUserSpaceSlots[SlotIndex].UserStackBaseVirtualAddress;
    IsolatedSpace->UserStackSize = LosMemoryManagerUserSpaceSlots[SlotIndex].UserStackSize;
    IsolatedSpace->UserStackPointer = LosMemoryManagerUserSpaceSlots[SlotIndex].UserStackPointer;
    IsolatedSpace->CallBlockUserAddress = LosMemoryManagerUserSpaceSlots[SlotIndex].CallBlockVirtualAddress;
    IsolatedSpace->CallBlockKernelAddress = LosMemoryManagerUserSpaceSlots[SlotIndex].CallBlockKernelAddress;
    IsolatedSpace->MappingRecordAddress = (UINT64)(UINTN)&LosMemoryManagerUserMappingSlots[SlotIndex][0];
    IsolatedSpace->MappingCount = LosMemoryManagerUserSpaceSlots[SlotIndex].MappingCount;
    IsolatedSpace->Reserved0 = 0U;
    IsolatedSpace->AddressSpace.Version = LOS_USER_IMAGE_CALL_VERSION;
    IsolatedSpace->AddressSpace.Flags = LOS_USER_IMAGE_RING3_FLAG_IF_ENABLED;
    IsolatedSpace->AddressSpace.AddressSpaceId = LosMemoryManagerUserSpaceSlots[SlotIndex].AddressSpaceId;
    IsolatedSpace->AddressSpace.RootTablePhysicalAddress = LosMemoryManagerUserSpaceSlots[SlotIndex].SyntheticRootTablePhysicalAddress;
    IsolatedSpace->AddressSpace.MappingRecordAddress = (UINT64)(UINTN)&LosMemoryManagerUserMappingSlots[SlotIndex][0];
    IsolatedSpace->AddressSpace.MappingCount = LosMemoryManagerUserSpaceSlots[SlotIndex].MappingCount;
    IsolatedSpace->AddressSpace.Reserved0 = 0U;

    Ring3Context->Version = LOS_USER_IMAGE_CALL_VERSION;
    Ring3Context->Flags = LOS_USER_IMAGE_RING3_FLAG_IF_ENABLED;
    Ring3Context->UserInstructionPointer = IsolatedSpace->UserEntryAddress;
    Ring3Context->UserStackPointer = IsolatedSpace->UserStackPointer;
    Ring3Context->UserCallArgument = IsolatedSpace->CallBlockUserAddress;
    Ring3Context->PageMapLevel4PhysicalAddress = IsolatedSpace->RootTablePhysicalAddress;
    Ring3Context->UserCodeSelector = LOS_MEMORY_MANAGER_USER_CODE_SELECTOR;
    Ring3Context->UserDataSelector = LOS_MEMORY_MANAGER_USER_DATA_SELECTOR;
    Ring3Context->UserRflags = 0x202ULL;
    Ring3Context->KernelResumeInstructionPointer = 0ULL;
    Ring3Context->KernelResumeStackPointer = 0ULL;
    Ring3Context->CompletionStatusAddress = 0ULL;
    Ring3Context->CompletionResultValue = 0ULL;
    return LOS_USER_IMAGE_CALL_STATUS_SUCCESS;
}
