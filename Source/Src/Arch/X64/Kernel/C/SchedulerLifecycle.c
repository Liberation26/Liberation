/*
 * File Name: SchedulerLifecycle.c
 * File Version: 0.3.12
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:38:05Z
 * Last Update Timestamp: 2026-04-09T13:20:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */

#include "SchedulerInternal.h"
#include "MemoryManagerBootstrap.h"
#include "VirtualMemoryInternal.h"
#include "SchedulerUserImage.h"

static LOS_KERNEL_SCHEDULER_STATE LosKernelSchedulerGlobalState;
static UINT8 LosKernelSchedulerBootstrapStacks[LOS_KERNEL_SCHEDULER_MAX_TASKS][LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES] __attribute__((aligned(4096)));
static UINT8 LosKernelSchedulerBootstrapStackUsed[LOS_KERNEL_SCHEDULER_MAX_TASKS];
static UINT8 LosKernelSchedulerDirectClaimStackUsed[LOS_KERNEL_SCHEDULER_MAX_TASKS];
static void *LosKernelSchedulerDirectClaimStackPoolBase;

LOS_KERNEL_SCHEDULER_STATE *LosKernelSchedulerState(void)
{
    return &LosKernelSchedulerGlobalState;
}

static void ZeroBytes(void *Buffer, UINTN ByteCount)
{
    UINT8 *Bytes;
    UINTN Index;

    if (Buffer == 0)
    {
        return;
    }

    Bytes = (UINT8 *)Buffer;
    for (Index = 0U; Index < ByteCount; ++Index)
    {
        Bytes[Index] = 0U;
    }
}

static UINT64 EnterSchedulerCriticalSection(void)
{
    UINT64 Flags;

    __asm__ __volatile__("pushfq; popq %0" : "=r"(Flags) : : "memory");
    __asm__ __volatile__("cli" : : : "memory");
    return Flags;
}

static void LeaveSchedulerCriticalSection(UINT64 Flags)
{
    if ((Flags & 0x200ULL) != 0ULL)
    {
        __asm__ __volatile__("sti" : : : "memory");
    }
}


static BOOLEAN IsMemoryManagerSchedulerTransportReady(void)
{
    const LOS_MEMORY_MANAGER_BOOTSTRAP_INFO *Info;

    Info = LosGetMemoryManagerBootstrapInfo();
    if (Info == 0)
    {
        return 0;
    }

    if ((Info->Flags & LOS_MEMORY_MANAGER_BOOTSTRAP_FLAG_ATTACH_COMPLETE) == 0ULL)
    {
        return 0;
    }

    return 1;
}

static BOOLEAN SchedulerMayUseMemoryManagerBackedThreadStacks(void)
{
    /*
     * The hosted bootstrap path can still lose AllocateFrames replies after
     * transient process-root activation. Keep scheduler-owned thread stacks on
     * the bootstrap fallback pool until the memory manager is running through
     * the normal scheduled-service path rather than the hosted bootstrap pump.
     */
    return 0;
}

static BOOLEAN ReserveDirectClaimKernelThreadStackPool(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_X64_CLAIM_FRAMES_REQUEST ClaimRequest;
    LOS_X64_CLAIM_FRAMES_RESULT ClaimResult;
    UINT64 TotalPages;
    BOOLEAN ClaimedThroughMemoryManager;

    State = LosKernelSchedulerState();
    if (State == 0)
    {
        return 0;
    }

    if (State->DirectClaimStackPoolReady != 0U)
    {
        return 1;
    }

    TotalPages = LOS_KERNEL_SCHEDULER_MAX_TASKS * LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES;
    ZeroBytes(&ClaimRequest, sizeof(ClaimRequest));
    ZeroBytes(&ClaimResult, sizeof(ClaimResult));
    ClaimRequest.PageCount = TotalPages;
    ClaimRequest.AlignmentBytes = 4096ULL;
    ClaimRequest.Flags = LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS;
    ClaimRequest.Owner = LOS_X64_MEMORY_REGION_OWNER_CLAIMED;
    ClaimedThroughMemoryManager = 0;

    if (IsMemoryManagerSchedulerTransportReady() != 0U)
    {
        LosMemoryManagerSendClaimFrames(&ClaimRequest, &ClaimResult);
        if (ClaimResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS &&
            ClaimResult.BaseAddress != 0ULL &&
            ClaimResult.PageCount == TotalPages)
        {
            ClaimedThroughMemoryManager = 1;
        }
        else
        {
            LosKernelTraceFail("Kernel scheduler could not reserve stack-pool frames through the memory manager; falling back to direct claims.");
            LosKernelTraceUnsigned("Kernel scheduler stack-pool claim status: ", ClaimResult.Status);
            LosKernelTraceUnsigned("Kernel scheduler stack-pool pages returned: ", ClaimResult.PageCount);
            ZeroBytes(&ClaimResult, sizeof(ClaimResult));
        }
    }

    if (ClaimedThroughMemoryManager == 0)
    {
        LosX64ClaimFrames(&ClaimRequest, &ClaimResult);
    }

    if (ClaimResult.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS ||
        ClaimResult.BaseAddress == 0ULL ||
        ClaimResult.PageCount != TotalPages)
    {
        LosKernelTraceFail("Kernel scheduler could not reserve direct-claim stack pool frames.");
        LosKernelTraceUnsigned("Kernel scheduler direct-claim stack-pool status: ", ClaimResult.Status);
        LosKernelTraceUnsigned("Kernel scheduler direct-claim stack-pool pages returned: ", ClaimResult.PageCount);
        return 0;
    }

    LosKernelSchedulerDirectClaimStackPoolBase = LosX64GetDirectMapVirtualAddress(ClaimResult.BaseAddress, TotalPages * 4096ULL);
    if (LosKernelSchedulerDirectClaimStackPoolBase == 0)
    {
        LosKernelTraceFail("Kernel scheduler could not direct-map the direct-claim stack pool.");
        LosKernelTraceHex64("Kernel scheduler direct-claim stack-pool base: ", ClaimResult.BaseAddress);
        return 0;
    }

    State->DirectClaimStackPoolPhysicalAddress = ClaimResult.BaseAddress;
    State->DirectClaimStackPoolBytes = TotalPages * 4096ULL;
    State->DirectClaimStackPoolReady = 1U;
    State->DirectClaimStackSlotsInUse = 0U;
    ZeroBytes(&LosKernelSchedulerDirectClaimStackUsed[0], sizeof(LosKernelSchedulerDirectClaimStackUsed));
    if (ClaimedThroughMemoryManager != 0U)
    {
        LosKernelTraceOk("Kernel scheduler stack pool reserved through the memory manager.");
    }
    else
    {
        LosKernelTraceOk("Kernel scheduler direct-claim stack pool ready.");
    }
    LosKernelTraceHex64("Kernel scheduler direct-claim stack-pool base: ", ClaimResult.BaseAddress);
    LosKernelTraceUnsigned("Kernel scheduler direct-claim stack-pool pages: ", TotalPages);
    return 1;
}

static void WriteStackReturnAddress(UINT64 StackAddress, UINT64 ReturnAddress)
{
    UINT64 *Pointer;

    Pointer = (UINT64 *)(UINTN)StackAddress;
    *Pointer = ReturnAddress;
}

static UINT64 ReadStackReturnAddress(UINT64 StackAddress)
{
    const UINT64 *Pointer;

    Pointer = (const UINT64 *)(UINTN)StackAddress;
    return *Pointer;
}

static UINT64 GetUserTransitionFrameStackPointer(const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    UINT64 MinimumFrameAddress;

    if (Task == 0 ||
        Task->StackBaseVirtualAddress == 0ULL ||
        Task->StackTopVirtualAddress == 0ULL ||
        Task->StackTopVirtualAddress <= Task->StackBaseVirtualAddress)
    {
        return 0ULL;
    }

    MinimumFrameAddress = Task->StackBaseVirtualAddress + sizeof(LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME);
    if (Task->StackTopVirtualAddress < MinimumFrameAddress + LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME_STACK_OFFSET_BYTES)
    {
        return 0ULL;
    }

    return (Task->StackTopVirtualAddress - LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME_STACK_OFFSET_BYTES) & ~0xFULL;
}

static UINT64 GetUserTransitionChainStackPointer(const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    UINT64 MinimumChainAddress;

    if (Task == 0 ||
        Task->StackBaseVirtualAddress == 0ULL ||
        Task->StackTopVirtualAddress == 0ULL ||
        Task->StackTopVirtualAddress <= Task->StackBaseVirtualAddress)
    {
        return 0ULL;
    }

    MinimumChainAddress = Task->StackBaseVirtualAddress + 16ULL;
    if (Task->StackTopVirtualAddress < MinimumChainAddress + LOS_KERNEL_SCHEDULER_USER_TRANSITION_CHAIN_STACK_OFFSET_BYTES)
    {
        return 0ULL;
    }

    return (Task->StackTopVirtualAddress - LOS_KERNEL_SCHEDULER_USER_TRANSITION_CHAIN_STACK_OFFSET_BYTES) & ~0xFULL;
}

static UINT64 RotateLeft64(UINT64 Value, UINT32 Shift)
{
    if (Shift == 0U)
    {
        return Value;
    }

    return (Value << Shift) | (Value >> (64U - Shift));
}

static UINT64 ComputeUserTransitionContractSignature(
    const LOS_KERNEL_SCHEDULER_PROCESS *Process,
    const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    const LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *Frame;
    UINT64 Signature;

    if (Process == 0 || Task == 0)
    {
        return 0ULL;
    }

    if (Process->UserEntryVirtualAddress == 0ULL ||
        Process->UserStackTopVirtualAddress == 0ULL ||
        Process->UserCodeSegmentSelector == 0ULL ||
        Process->UserStackSegmentSelector == 0ULL ||
        Process->UserRflags == 0ULL ||
        Process->UserTransitionFrameStackPointer == 0ULL ||
        Process->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Process->UserTransitionBridgeVirtualAddress == 0ULL ||
        Process->UserTransitionChainStackPointer == 0ULL ||
        Task->UserInstructionPointer == 0ULL ||
        Task->UserStackPointer == 0ULL ||
        Task->UserCodeSegmentSelector == 0ULL ||
        Task->UserStackSegmentSelector == 0ULL ||
        Task->UserRflags == 0ULL ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Task->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Task->UserTransitionBridgeVirtualAddress == 0ULL ||
        Task->UserTransitionChainStackPointer == 0ULL)
    {
        return 0ULL;
    }

    if (Process->UserEntryVirtualAddress != Task->UserInstructionPointer ||
        Process->UserStackTopVirtualAddress != Task->UserStackPointer ||
        Process->UserCodeSegmentSelector != Task->UserCodeSegmentSelector ||
        Process->UserStackSegmentSelector != Task->UserStackSegmentSelector ||
        Process->UserRflags != Task->UserRflags ||
        Process->UserTransitionFrameStackPointer != Task->UserTransitionFrameStackPointer ||
        Process->UserTransitionKernelEntryVirtualAddress != Task->UserTransitionKernelEntryVirtualAddress ||
        Process->UserTransitionBridgeVirtualAddress != Task->UserTransitionBridgeVirtualAddress ||
        Process->UserTransitionChainStackPointer != Task->UserTransitionChainStackPointer)
    {
        return 0ULL;
    }

    Frame = (const LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *)(UINTN)Task->UserTransitionFrameStackPointer;
    if (Frame == 0 ||
        Frame->Rip != Task->UserInstructionPointer ||
        Frame->Cs != Task->UserCodeSegmentSelector ||
        Frame->Rflags != Task->UserRflags ||
        Frame->Rsp != Task->UserStackPointer ||
        Frame->Ss != Task->UserStackSegmentSelector)
    {
        return 0ULL;
    }

    if (ReadStackReturnAddress(Task->UserTransitionChainStackPointer) != Task->UserTransitionBridgeVirtualAddress ||
        ReadStackReturnAddress(Task->UserTransitionChainStackPointer + 8ULL) != Task->UserTransitionKernelEntryVirtualAddress)
    {
        return 0ULL;
    }

    Signature = 0x555452414E534954ULL;
    Signature ^= Process->ProcessId;
    Signature = RotateLeft64(Signature, 7U) ^ Process->UserEntryVirtualAddress;
    Signature = RotateLeft64(Signature, 11U) ^ Process->UserStackTopVirtualAddress;
    Signature = RotateLeft64(Signature, 13U) ^ Process->UserCodeSegmentSelector;
    Signature = RotateLeft64(Signature, 17U) ^ Process->UserStackSegmentSelector;
    Signature = RotateLeft64(Signature, 19U) ^ Process->UserRflags;
    Signature = RotateLeft64(Signature, 23U) ^ Process->UserTransitionFrameStackPointer;
    Signature = RotateLeft64(Signature, 29U) ^ Process->UserTransitionKernelEntryVirtualAddress;
    Signature = RotateLeft64(Signature, 31U) ^ Process->UserTransitionBridgeVirtualAddress;
    Signature = RotateLeft64(Signature, 37U) ^ Process->UserTransitionChainStackPointer;
    Signature = RotateLeft64(Signature, 41U) ^ Frame->Rip;
    Signature = RotateLeft64(Signature, 43U) ^ Frame->Rsp;
    Signature = RotateLeft64(Signature, 47U) ^ Task->TaskId;
    if (Signature == 0ULL)
    {
        Signature = 0x434F4E5452414354ULL;
    }

    return Signature;
}


static UINT64 ComputeUserTransitionSealValue(
    const LOS_KERNEL_SCHEDULER_PROCESS *Process,
    const LOS_KERNEL_SCHEDULER_TASK *Task)
{
    UINT64 SealValue;

    if (Process == 0 || Task == 0)
    {
        return 0ULL;
    }

    if (Process->UserTransitionContractSignature == 0ULL ||
        Task->UserTransitionContractSignature == 0ULL ||
        Process->UserTransitionContractSignature != Task->UserTransitionContractSignature ||
        Process->UserTransitionFrameStackPointer == 0ULL ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Process->UserTransitionChainStackPointer == 0ULL ||
        Task->UserTransitionChainStackPointer == 0ULL)
    {
        return 0ULL;
    }

    if (ReadStackReturnAddress(Task->UserTransitionChainStackPointer) != Task->UserTransitionBridgeVirtualAddress ||
        ReadStackReturnAddress(Task->UserTransitionChainStackPointer + 8ULL) != Task->UserTransitionKernelEntryVirtualAddress)
    {
        return 0ULL;
    }

    SealValue = 0x5345414C55534552ULL;
    SealValue ^= Process->UserTransitionContractSignature;
    SealValue = RotateLeft64(SealValue, 5U) ^ Process->UserTransitionFrameStackPointer;
    SealValue = RotateLeft64(SealValue, 11U) ^ Process->UserTransitionChainStackPointer;
    SealValue = RotateLeft64(SealValue, 17U) ^ Process->UserTransitionKernelEntryVirtualAddress;
    SealValue = RotateLeft64(SealValue, 23U) ^ Process->UserTransitionBridgeVirtualAddress;
    SealValue = RotateLeft64(SealValue, 29U) ^ Task->TaskId;
    if (SealValue == 0ULL)
    {
        SealValue = 0x5345414C4F4B0001ULL;
    }

    return SealValue;
}


static void WriteUserTransitionFrame(
    UINT64 FrameStackPointer,
    UINT64 InstructionPointer,
    UINT64 CodeSelector,
    UINT64 Rflags,
    UINT64 StackPointer,
    UINT64 StackSelector)
{
    LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *Frame;

    if (FrameStackPointer == 0ULL)
    {
        return;
    }

    Frame = (LOS_KERNEL_SCHEDULER_USER_TRANSITION_FRAME *)(UINTN)FrameStackPointer;
    ZeroBytes(Frame, sizeof(*Frame));
    Frame->Rip = InstructionPointer;
    Frame->Cs = CodeSelector;
    Frame->Rflags = Rflags;
    Frame->Rsp = StackPointer;
    Frame->Ss = StackSelector;
}

static LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *ResolveSchedulerAddressSpaceObject(UINT64 AddressSpaceObjectPhysicalAddress)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;

    if (AddressSpaceObjectPhysicalAddress == 0ULL)
    {
        return 0;
    }

    AddressSpaceObject = (LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *)LosX64GetDirectMapVirtualAddress(
        AddressSpaceObjectPhysicalAddress,
        sizeof(*AddressSpaceObject));
    if (AddressSpaceObject == 0)
    {
        return 0;
    }
    if (AddressSpaceObject->State < LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_READY ||
        AddressSpaceObject->RootTablePhysicalAddress == 0ULL ||
        (AddressSpaceObject->RootTablePhysicalAddress & 0xFFFULL) != 0ULL)
    {
        return 0;
    }

    return AddressSpaceObject;
}

static UINT64 *TranslateSchedulerPageTable(UINT64 PhysicalAddress)
{
    return (UINT64 *)LosX64GetDirectMapVirtualAddress(PhysicalAddress, 0x1000ULL);
}

static __attribute__((unused)) BOOLEAN SchedulerQueryLeafPageEntry(UINT64 RootTablePhysicalAddress, UINT64 VirtualAddress, UINT64 *Entry)
{
    UINT64 *PageMapLevel4;
    UINT64 CurrentEntry;
    UINT64 *PageDirectoryPointerTable;
    UINT64 *PageDirectory;
    UINT64 *PageTable;

    if (Entry != 0)
    {
        *Entry = 0ULL;
    }
    if (RootTablePhysicalAddress == 0ULL || (VirtualAddress & 0xFFFULL) != 0ULL)
    {
        return 0U;
    }

    PageMapLevel4 = TranslateSchedulerPageTable(RootTablePhysicalAddress);
    if (PageMapLevel4 == 0)
    {
        return 0U;
    }

    CurrentEntry = PageMapLevel4[(UINTN)((VirtualAddress >> 39U) & 0x1FFULL)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL || (CurrentEntry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 0U;
    }

    PageDirectoryPointerTable = TranslateSchedulerPageTable(CurrentEntry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageDirectoryPointerTable == 0)
    {
        return 0U;
    }

    CurrentEntry = PageDirectoryPointerTable[(UINTN)((VirtualAddress >> 30U) & 0x1FFULL)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL || (CurrentEntry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 0U;
    }

    PageDirectory = TranslateSchedulerPageTable(CurrentEntry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageDirectory == 0)
    {
        return 0U;
    }

    CurrentEntry = PageDirectory[(UINTN)((VirtualAddress >> 21U) & 0x1FFULL)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL || (CurrentEntry & LOS_X64_PAGE_LARGE) != 0ULL)
    {
        return 0U;
    }

    PageTable = TranslateSchedulerPageTable(CurrentEntry & LOS_X64_PAGE_TABLE_ADDRESS_MASK);
    if (PageTable == 0)
    {
        return 0U;
    }

    CurrentEntry = PageTable[(UINTN)((VirtualAddress >> 12U) & 0x1FFULL)];
    if ((CurrentEntry & LOS_X64_PAGE_PRESENT) == 0ULL)
    {
        return 0U;
    }

    if (Entry != 0)
    {
        *Entry = CurrentEntry;
    }
    return 1U;
}

static __attribute__((unused)) BOOLEAN QueryAddressSpaceMappingFromRecordedMetadata(
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 VirtualAddress,
    UINT64 *PhysicalAddress,
    UINT64 *PageFlags)
{
    UINT64 PageVirtualAddress;

    if (PhysicalAddress != 0)
    {
        *PhysicalAddress = 0ULL;
    }
    if (PageFlags != 0)
    {
        *PageFlags = 0ULL;
    }
    if (AddressSpaceObject == 0)
    {
        return 0U;
    }

    PageVirtualAddress = VirtualAddress & ~0xFFFULL;
    if (AddressSpaceObject->ServiceImageVirtualBase != 0ULL &&
        AddressSpaceObject->ServiceImageSize != 0ULL &&
        PageVirtualAddress >= AddressSpaceObject->ServiceImageVirtualBase &&
        PageVirtualAddress < AddressSpaceObject->ServiceImageVirtualBase +
            (((AddressSpaceObject->ServiceImageSize + 0xFFFULL) / 0x1000ULL) * 0x1000ULL) &&
        AddressSpaceObject->ServiceImagePhysicalAddress != 0ULL)
    {
        if (PhysicalAddress != 0)
        {
            *PhysicalAddress = AddressSpaceObject->ServiceImagePhysicalAddress +
                (PageVirtualAddress - AddressSpaceObject->ServiceImageVirtualBase);
        }
        if (PageFlags != 0)
        {
            *PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_USER |
                ((AddressSpaceObject->EntryVirtualAddress != 0ULL &&
                  PageVirtualAddress == (AddressSpaceObject->EntryVirtualAddress & ~0xFFFULL)) ?
                    0ULL : LOS_X64_PAGE_NX);
        }
        return 1U;
    }
    if (AddressSpaceObject->StackBaseVirtualAddress != 0ULL &&
        AddressSpaceObject->StackTopVirtualAddress > AddressSpaceObject->StackBaseVirtualAddress &&
        PageVirtualAddress >= AddressSpaceObject->StackBaseVirtualAddress &&
        PageVirtualAddress < AddressSpaceObject->StackTopVirtualAddress &&
        AddressSpaceObject->StackPhysicalAddress != 0ULL)
    {
        if (PhysicalAddress != 0)
        {
            *PhysicalAddress = AddressSpaceObject->StackPhysicalAddress +
                (PageVirtualAddress - AddressSpaceObject->StackBaseVirtualAddress);
        }
        if (PageFlags != 0)
        {
            *PageFlags = LOS_X64_PAGE_PRESENT | LOS_X64_PAGE_WRITABLE |
                LOS_X64_PAGE_USER | LOS_X64_PAGE_NX;
        }
        return 1U;
    }

    return 0U;
}


static BOOLEAN ApplyFirstUserTaskImageMetadata(
    const LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *AttachResult,
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 *ResolvedEntryVirtualAddress)
{
    UINT64 CandidateEntry;
    UINT64 CandidateBase;
    UINT64 CandidatePages;

    if (ResolvedEntryVirtualAddress == 0)
    {
        return 0U;
    }

    CandidateEntry = 0ULL;
    CandidateBase = 0ULL;
    CandidatePages = 0ULL;
    if (AttachResult != 0)
    {
        CandidateEntry = AttachResult->EntryVirtualAddress;
        CandidateBase = AttachResult->ImageVirtualBase;
        CandidatePages = AttachResult->ImagePageCount;
    }
    if ((CandidateEntry == 0ULL || CandidateBase == 0ULL || CandidatePages == 0ULL) && AddressSpaceObject != 0)
    {
        if (CandidateEntry == 0ULL)
        {
            CandidateEntry = AddressSpaceObject->EntryVirtualAddress;
            if (CandidateEntry == 0ULL)
            {
                CandidateEntry = AddressSpaceObject->ServiceImageVirtualBase;
            }
        }
        if (CandidateBase == 0ULL)
        {
            CandidateBase = AddressSpaceObject->ServiceImageVirtualBase;
        }
        if (CandidatePages == 0ULL && AddressSpaceObject->ServiceImageSize != 0ULL)
        {
            CandidatePages = (AddressSpaceObject->ServiceImageSize + 0xFFFULL) / 0x1000ULL;
        }
    }

    if (CandidateEntry == 0ULL || CandidateBase == 0ULL || CandidatePages == 0ULL)
    {
        return 0U;
    }
    if (CandidateEntry < CandidateBase || CandidateEntry >= CandidateBase + (CandidatePages * 0x1000ULL))
    {
        return 0U;
    }

    *ResolvedEntryVirtualAddress = CandidateEntry;
    return 1U;
}

static BOOLEAN ApplyFirstUserTaskStackMetadata(
    const LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *StackResult,
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 *ResolvedStackTopVirtualAddress)
{
    UINT64 CandidateBase;
    UINT64 CandidateTop;
    UINT64 CandidatePages;

    if (ResolvedStackTopVirtualAddress == 0)
    {
        return 0U;
    }

    CandidateBase = 0ULL;
    CandidateTop = 0ULL;
    CandidatePages = 0ULL;
    if (StackResult != 0)
    {
        CandidateBase = StackResult->StackBaseVirtualAddress;
        CandidateTop = StackResult->StackTopVirtualAddress;
        CandidatePages = StackResult->StackPageCount;
    }
    if ((CandidateBase == 0ULL || CandidateTop == 0ULL || CandidatePages == 0ULL) && AddressSpaceObject != 0)
    {
        if (CandidateBase == 0ULL)
        {
            CandidateBase = AddressSpaceObject->StackBaseVirtualAddress;
        }
        if (CandidateTop == 0ULL)
        {
            CandidateTop = AddressSpaceObject->StackTopVirtualAddress;
        }
        if (CandidatePages == 0ULL)
        {
            CandidatePages = AddressSpaceObject->StackPageCount;
        }
    }

    if (CandidateBase == 0ULL || CandidateTop == 0ULL || CandidatePages == 0ULL)
    {
        return 0U;
    }
    if (CandidateTop != CandidateBase + (CandidatePages * 0x1000ULL))
    {
        return 0U;
    }

    *ResolvedStackTopVirtualAddress = CandidateTop;
    return 1U;
}

static BOOLEAN QueryAddressSpaceMapping(UINT64 AddressSpaceObjectPhysicalAddress, UINT64 VirtualAddress, UINT64 *PhysicalAddress, UINT64 *PageFlags)
{
    LOS_MEMORY_MANAGER_QUERY_MAPPING_REQUEST Request;
    LOS_MEMORY_MANAGER_QUERY_MAPPING_RESULT Result;

    ZeroBytes(&Request, sizeof(Request));
    ZeroBytes(&Result, sizeof(Result));
    Request.AddressSpaceObjectPhysicalAddress = AddressSpaceObjectPhysicalAddress;
    Request.VirtualAddress = VirtualAddress;
    LosMemoryManagerSendQueryMapping(&Request, &Result);
    if (Result.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS || Result.PhysicalAddress == 0ULL)
    {
        if (PhysicalAddress != 0)
        {
            *PhysicalAddress = 0ULL;
        }
        if (PageFlags != 0)
        {
            *PageFlags = 0ULL;
        }
        return 0U;
    }

    if (PhysicalAddress != 0)
    {
        *PhysicalAddress = Result.PhysicalAddress;
    }
    if (PageFlags != 0)
    {
        *PageFlags = Result.PageFlags;
    }
    return 1U;
}

static BOOLEAN EnsureFirstUserTaskMappings(LOS_KERNEL_SCHEDULER_PROCESS *Process, LOS_KERNEL_SCHEDULER_TASK *Task)
{
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_REQUEST AttachRequest;
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT AttachResult;
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_REQUEST StackRequest;
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT StackResult;
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
    UINT64 ImagePhysicalAddress;
    UINT64 StackBaseVirtualAddress;
    UINT64 MappedPhysicalAddress;
    UINT64 MappedPageFlags;
    UINT64 CriticalSectionFlags;
    UINT64 ResolvedEntryVirtualAddress;
    UINT64 ResolvedStackTopVirtualAddress;
    BOOLEAN ImageMapped;
    BOOLEAN StackMapped;

    if (Process == 0 || Task == 0 ||
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) == 0U ||
        Process->AddressSpaceObjectPhysicalAddress == 0ULL ||
        Process->RootTablePhysicalAddress == 0ULL)
    {
        return 0U;
    }

    AddressSpaceObject = ResolveSchedulerAddressSpaceObject(Process->AddressSpaceObjectPhysicalAddress);
    ResolvedEntryVirtualAddress = Process->UserEntryVirtualAddress != 0ULL ?
        Process->UserEntryVirtualAddress : LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_ENTRY_VIRTUAL_ADDRESS;
    ResolvedStackTopVirtualAddress = Process->UserStackTopVirtualAddress != 0ULL ?
        Process->UserStackTopVirtualAddress : LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_TOP_VIRTUAL_ADDRESS;
    if (AddressSpaceObject != 0)
    {
        if (AddressSpaceObject->EntryVirtualAddress != 0ULL)
        {
            ResolvedEntryVirtualAddress = AddressSpaceObject->EntryVirtualAddress;
        }
        if (AddressSpaceObject->StackTopVirtualAddress != 0ULL)
        {
            ResolvedStackTopVirtualAddress = AddressSpaceObject->StackTopVirtualAddress;
        }
    }

    ImageMapped = QueryAddressSpaceMapping(
        Process->AddressSpaceObjectPhysicalAddress,
        ResolvedEntryVirtualAddress,
        &MappedPhysicalAddress,
        &MappedPageFlags);
    StackMapped = QueryAddressSpaceMapping(
        Process->AddressSpaceObjectPhysicalAddress,
        ResolvedStackTopVirtualAddress - 0x1000ULL,
        &MappedPhysicalAddress,
        &MappedPageFlags);

    if (!ImageMapped)
    {
        ImagePhysicalAddress = 0ULL;
        if (!LosX64TryTranslateKernelVirtualToPhysical((UINT64)(UINTN)LosKernelSchedulerUserImage, &ImagePhysicalAddress) ||
            ImagePhysicalAddress == 0ULL)
        {
            return 0U;
        }

        ZeroBytes(&AttachRequest, sizeof(AttachRequest));
        ZeroBytes(&AttachResult, sizeof(AttachResult));
        AttachRequest.AddressSpaceObjectPhysicalAddress = Process->AddressSpaceObjectPhysicalAddress;
        AttachRequest.StagedImagePhysicalAddress = ImagePhysicalAddress;
        AttachRequest.StagedImageSize = LosKernelSchedulerUserImageSize;
        LosMemoryManagerSendAttachStagedImage(&AttachRequest, &AttachResult);
        if (AttachResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS ||
            AttachResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT)
        {
            AddressSpaceObject = ResolveSchedulerAddressSpaceObject(Process->AddressSpaceObjectPhysicalAddress);
            if (ApplyFirstUserTaskImageMetadata(&AttachResult, AddressSpaceObject, &ResolvedEntryVirtualAddress))
            {
                ImageMapped = 1U;
            }
            if (!ImageMapped && ResolvedEntryVirtualAddress != 0ULL)
            {
                ImageMapped = QueryAddressSpaceMapping(
                    Process->AddressSpaceObjectPhysicalAddress,
                    ResolvedEntryVirtualAddress,
                    &MappedPhysicalAddress,
                    &MappedPageFlags);
            }
        }

        if (!ImageMapped)
        {
            LosKernelTraceFail("Kernel scheduler could not attach the first user-task image into its owned address space.");
            LosKernelTraceUnsigned("Kernel scheduler user-image attach status: ", AttachResult.Status);
            return 0U;
        }
    }

    if (!StackMapped)
    {
        StackBaseVirtualAddress = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_TOP_VIRTUAL_ADDRESS -
            (LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_PAGES * 0x1000ULL);
        ZeroBytes(&StackRequest, sizeof(StackRequest));
        ZeroBytes(&StackResult, sizeof(StackResult));
        StackRequest.AddressSpaceObjectPhysicalAddress = Process->AddressSpaceObjectPhysicalAddress;
        StackRequest.DesiredStackBaseVirtualAddress = StackBaseVirtualAddress;
        StackRequest.PageCount = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_PAGES;
        LosMemoryManagerSendAllocateAddressSpaceStack(&StackRequest, &StackResult);
        if (StackResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS ||
            StackResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_CONFLICT)
        {
            AddressSpaceObject = ResolveSchedulerAddressSpaceObject(Process->AddressSpaceObjectPhysicalAddress);
            if (ApplyFirstUserTaskStackMetadata(&StackResult, AddressSpaceObject, &ResolvedStackTopVirtualAddress))
            {
                StackMapped = 1U;
            }
            if (!StackMapped && ResolvedStackTopVirtualAddress != 0ULL)
            {
                StackMapped = QueryAddressSpaceMapping(
                    Process->AddressSpaceObjectPhysicalAddress,
                    ResolvedStackTopVirtualAddress - 0x1000ULL,
                    &MappedPhysicalAddress,
                    &MappedPageFlags);
            }
        }

        if (!StackMapped)
        {
            LosKernelTraceFail("Kernel scheduler could not allocate the first user-task stack inside its owned address space.");
            LosKernelTraceUnsigned("Kernel scheduler user-stack allocate status: ", StackResult.Status);
            return 0U;
        }
    }

    if (!QueryAddressSpaceMapping(
            Process->AddressSpaceObjectPhysicalAddress,
            ResolvedEntryVirtualAddress,
            &MappedPhysicalAddress,
            &MappedPageFlags) ||
        !QueryAddressSpaceMapping(
            Process->AddressSpaceObjectPhysicalAddress,
            ResolvedStackTopVirtualAddress - 0x1000ULL,
            &MappedPhysicalAddress,
            &MappedPageFlags))
    {
        return 0U;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    Process->UserEntryVirtualAddress = ResolvedEntryVirtualAddress;
    Process->UserStackTopVirtualAddress = ResolvedStackTopVirtualAddress;
    Task->UserInstructionPointer = ResolvedEntryVirtualAddress;
    Task->UserStackPointer = ResolvedStackTopVirtualAddress;
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
    return 1U;
}

static LOS_KERNEL_SCHEDULER_PROCESS *FindProcessByIdMutable(UINT64 ProcessId)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    if (ProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID)
    {
        return 0;
    }

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY)
        {
            continue;
        }
        if (Process->ProcessId == ProcessId)
        {
            return Process;
        }
    }

    return 0;
}

static const LOS_KERNEL_SCHEDULER_PROCESS *FindProcessById(UINT64 ProcessId)
{
    return (const LOS_KERNEL_SCHEDULER_PROCESS *)FindProcessByIdMutable(ProcessId);
}

static LOS_KERNEL_SCHEDULER_TASK *FindTaskByIdMutable(UINT64 TaskId)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    if (TaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        LOS_KERNEL_SCHEDULER_TASK *Task;

        Task = &State->Tasks[Index];
        if (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_UNUSED)
        {
            continue;
        }
        if (Task->TaskId == TaskId)
        {
            return Task;
        }
    }

    return 0;
}

BOOLEAN LosKernelSchedulerHasActiveTransientProcess(void)
{
    const LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        const LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_TRANSIENT) == 0U)
        {
            continue;
        }
        if (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY ||
            Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_INITIALIZING ||
            (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED && Process->CleanupPending != 0U))
        {
            return 1;
        }
    }

    return 0;
}


static void AbandonCreatedProcess(LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT64 CriticalSectionFlags;

    if (Process == 0)
    {
        return;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED &&
        Process->ThreadCount == 0U)
    {
        if ((Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY ||
             Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED) &&
            State->ProcessCount > 0U)
        {
            State->ProcessCount -= 1U;
        }
        ZeroBytes(Process, sizeof(*Process));
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
}

static UINT64 GetKernelRootTablePhysicalAddress(void);
static BOOLEAN BindOwnedProcessAddressSpace(LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_REQUEST Request;
    LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_RESULT Result;
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT64 CriticalSectionFlags;
    UINT64 ProcessId;

    if (Process == 0)
    {
        return 0;
    }

    State = LosKernelSchedulerState();
    CriticalSectionFlags = EnterSchedulerCriticalSection();
    if (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED ||
        Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED ||
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) != 0U ||
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) != 0U ||
        Process->AddressSpaceObjectPhysicalAddress != 0ULL)
    {
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        return 1;
    }
    if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS) != 0U)
    {
        State->AddressSpaceBindDeferredCount += 1ULL;
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        return 0;
    }

    Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS;
    ProcessId = Process->ProcessId;
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (IsMemoryManagerSchedulerTransportReady() == 0U)
    {
        CriticalSectionFlags = EnterSchedulerCriticalSection();
        Process->Flags &= ~LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS;
        State->AddressSpaceBindDeferredCount += 1ULL;
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        (void)ProcessId;
        return 0;
    }

    ZeroBytes(&Request, sizeof(Request));
    ZeroBytes(&Result, sizeof(Result));
    LosMemoryManagerSendCreateAddressSpace(&Request, &Result);
    if (Result.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS ||
        Result.AddressSpaceObjectPhysicalAddress == 0ULL ||
        Result.RootTablePhysicalAddress == 0ULL ||
        Result.AddressSpaceId == 0ULL)
    {
        CriticalSectionFlags = EnterSchedulerCriticalSection();
        Process->Flags &= ~LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS;
        State->AddressSpaceBindDeferredCount += 1ULL;
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        LosKernelTraceFail("Kernel scheduler could not bind a distinct process address space.");
        LosKernelTraceUnsigned("Kernel scheduler process address-space-create status: ", Result.Status);
        LosKernelTraceUnsigned("Kernel scheduler process id awaiting root binding: ", ProcessId);
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    if (Process->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED)
    {
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        return 0;
    }
    Process->AddressSpaceId = Result.AddressSpaceId;
    Process->AddressSpaceObjectPhysicalAddress = Result.AddressSpaceObjectPhysicalAddress;
    Process->RootTablePhysicalAddress = Result.RootTablePhysicalAddress;
    Process->Flags &= ~(LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT | LOS_KERNEL_SCHEDULER_PROCESS_FLAG_BIND_IN_PROGRESS);
    Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE;
    State->AddressSpaceBindCount += 1ULL;
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
    return 1;
}

static BOOLEAN DestroyOwnedProcessAddressSpace(LOS_KERNEL_SCHEDULER_PROCESS *Process)
{
    LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_REQUEST Request;
    LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_RESULT Result;

    if (Process == 0)
    {
        return 0;
    }
    if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) == 0U ||
        Process->AddressSpaceObjectPhysicalAddress == 0ULL)
    {
        return 1;
    }
    if (IsMemoryManagerSchedulerTransportReady() == 0U)
    {
        LosKernelTraceFail("Kernel scheduler could not destroy a process address space because the memory manager bootstrap is not ready.");
        LosKernelTraceUnsigned("Kernel scheduler process awaiting address-space destroy: ", Process->ProcessId);
        return 0;
    }

    ZeroBytes(&Request, sizeof(Request));
    ZeroBytes(&Result, sizeof(Result));
    Request.AddressSpaceObjectPhysicalAddress = Process->AddressSpaceObjectPhysicalAddress;
    LosMemoryManagerSendDestroyAddressSpace(&Request, &Result);
    if (Result.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS)
    {
        LosKernelTraceFail("Kernel scheduler could not destroy a terminated process address space.");
        LosKernelTraceUnsigned("Kernel scheduler process address-space-destroy status: ", Result.Status);
        LosKernelTraceUnsigned("Kernel scheduler process awaiting destroy completion: ", Process->ProcessId);
        LosKernelTraceHex64("Kernel scheduler address-space object awaiting destroy: ", Process->AddressSpaceObjectPhysicalAddress);
        return 0;
    }

    Process->AddressSpaceObjectPhysicalAddress = 0ULL;
    Process->AddressSpaceId = 0ULL;
    Process->RootTablePhysicalAddress = GetKernelRootTablePhysicalAddress();
    Process->Flags &= ~LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE;
    Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT;
    return 1;
}

static UINT64 ResolveProcessRootTablePhysicalAddress(UINT64 RequestedRootTablePhysicalAddress, UINT32 *ResolvedFlags)
{
    const LOS_KERNEL_SCHEDULER_PROCESS *CurrentProcess;
    UINT64 ResolvedRoot;

    ResolvedRoot = RequestedRootTablePhysicalAddress;
    if (ResolvedRoot != LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS)
    {
        return ResolvedRoot;
    }

    CurrentProcess = LosKernelSchedulerGetCurrentProcess();
    if (CurrentProcess != 0 &&
        CurrentProcess->RootTablePhysicalAddress != LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS)
    {
        ResolvedRoot = CurrentProcess->RootTablePhysicalAddress;
    }
    else
    {
        ResolvedRoot = GetKernelRootTablePhysicalAddress();
    }

    if (ResolvedFlags != 0)
    {
        *ResolvedFlags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT;
    }

    return ResolvedRoot;
}

static UINT64 GetCreatingOwnerTaskId(void)
{
    const LOS_KERNEL_SCHEDULER_TASK *CurrentTask;

    CurrentTask = LosKernelSchedulerGetCurrentTask();
    if (CurrentTask == 0)
    {
        return 0ULL;
    }

    return CurrentTask->TaskId;
}

static UINT64 GetCreatingOwnerProcessId(void)
{
    const LOS_KERNEL_SCHEDULER_TASK *CurrentTask;

    CurrentTask = LosKernelSchedulerGetCurrentTask();
    if (CurrentTask == 0)
    {
        return 0ULL;
    }

    return CurrentTask->ProcessId;
}

static UINT64 GetKernelRootTablePhysicalAddress(void)
{
    return LosX64GetCurrentPageMapLevel4PhysicalAddress();
}

static void InitializeTaskStackContext(LOS_KERNEL_SCHEDULER_TASK *Task, void *StackBase, UINT64 StackPhysicalAddress)
{
    UINT64 InitialStackPointer;

    ZeroBytes(StackBase, (UINTN)LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES);
    Task->StackPhysicalAddress = StackPhysicalAddress;
    Task->StackBaseVirtualAddress = (UINT64)(UINTN)StackBase;
    Task->StackTopVirtualAddress = Task->StackBaseVirtualAddress + LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES;
    Task->StackSizeBytes = LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES;
    Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_NONE;

    InitialStackPointer = (Task->StackTopVirtualAddress - 16ULL) & ~0xFULL;
    WriteStackReturnAddress(InitialStackPointer, (UINT64)(UINTN)LosKernelSchedulerThreadTrampoline);
    Task->ExecutionContext.StackPointer = InitialStackPointer;
    Task->ExecutionContext.Rbx = 0ULL;
    Task->ExecutionContext.Rbp = 0ULL;
    Task->ExecutionContext.R12 = 0ULL;
    Task->ExecutionContext.R13 = 0ULL;
    Task->ExecutionContext.R14 = 0ULL;
    Task->ExecutionContext.R15 = 0ULL;
    Task->ExecutionContext.Rflags = 0x202ULL;
}

static BOOLEAN AllocateBootstrapKernelThreadStack(LOS_KERNEL_SCHEDULER_TASK *Task)
{
    UINT32 Index;
    UINT64 PhysicalAddress;
    void *StackBase;

    if (Task == 0)
    {
        return 0;
    }

    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        if (LosKernelSchedulerBootstrapStackUsed[Index] == 0U)
        {
            LosKernelSchedulerBootstrapStackUsed[Index] = 1U;
            StackBase = &LosKernelSchedulerBootstrapStacks[Index][0];
            PhysicalAddress = 0ULL;
            LosX64TryTranslateKernelVirtualToPhysical((UINT64)(UINTN)StackBase, &PhysicalAddress);
            Task->BootstrapStackSlot = Index;
            InitializeTaskStackContext(Task, StackBase, PhysicalAddress);
            Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_BOOTSTRAP;
            LosKernelTraceOk("Kernel scheduler using bootstrap stack fallback.");
            return 1;
        }
    }

    return 0;
}

static BOOLEAN AllocateDirectClaimKernelThreadStack(LOS_KERNEL_SCHEDULER_TASK *Task)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;
    UINT8 *StackBase;
    UINT64 StackPhysicalAddress;

    if (Task == 0)
    {
        return 0;
    }

    State = LosKernelSchedulerState();
    if (State == 0 || State->DirectClaimStackPoolReady == 0U ||
        LosKernelSchedulerDirectClaimStackPoolBase == 0)
    {
        return 0;
    }

    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        if (LosKernelSchedulerDirectClaimStackUsed[Index] != 0U)
        {
            continue;
        }

        LosKernelSchedulerDirectClaimStackUsed[Index] = 1U;
        if (State->DirectClaimStackSlotsInUse < LOS_KERNEL_SCHEDULER_MAX_TASKS)
        {
            State->DirectClaimStackSlotsInUse += 1U;
        }
        StackBase = &((UINT8 *)LosKernelSchedulerDirectClaimStackPoolBase)[Index * LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES];
        StackPhysicalAddress = State->DirectClaimStackPoolPhysicalAddress +
            ((UINT64)Index * LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES);
        Task->BootstrapStackSlot = Index;
        InitializeTaskStackContext(Task, StackBase, StackPhysicalAddress);
        Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_DIRECT_CLAIM;
        LosKernelTraceOk("Kernel scheduler using direct-claim stack pool.");
        return 1;
    }

    return 0;
}

static BOOLEAN AllocateKernelThreadStack(LOS_KERNEL_SCHEDULER_TASK *Task)
{
    LOS_X64_CLAIM_FRAMES_REQUEST ClaimRequest;
    LOS_X64_CLAIM_FRAMES_RESULT ClaimResult;
    void *StackBase;

    if (Task == 0)
    {
        return 0;
    }

    Task->BootstrapStackSlot = LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT;
    Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_NONE;

    if (LosKernelSchedulerIsOnline() != 0U &&
        IsMemoryManagerSchedulerTransportReady() != 0U &&
        SchedulerMayUseMemoryManagerBackedThreadStacks() != 0U)
    {
        ZeroBytes(&ClaimRequest, sizeof(ClaimRequest));
        ZeroBytes(&ClaimResult, sizeof(ClaimResult));
        ClaimRequest.PageCount = LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES;
        ClaimRequest.AlignmentBytes = 4096ULL;
        ClaimRequest.Flags = LOS_X64_CLAIM_FRAMES_FLAG_CONTIGUOUS;
        ClaimRequest.Owner = LOS_X64_MEMORY_REGION_OWNER_CLAIMED;
        LosMemoryManagerSendClaimFrames(&ClaimRequest, &ClaimResult);
        if (ClaimResult.Status == LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS &&
            ClaimResult.BaseAddress != 0ULL &&
            ClaimResult.PageCount == LOS_KERNEL_SCHEDULER_THREAD_STACK_PAGES)
        {
            StackBase = LosX64GetDirectMapVirtualAddress(ClaimResult.BaseAddress, LOS_KERNEL_SCHEDULER_THREAD_STACK_BYTES);
            if (StackBase != 0)
            {
                InitializeTaskStackContext(Task, StackBase, ClaimResult.BaseAddress);
                Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_MEMORY_MANAGER;
                return 1;
            }

            {
                LOS_X64_FREE_FRAMES_REQUEST FreeRequest;
                LOS_X64_FREE_FRAMES_RESULT FreeResult;

                ZeroBytes(&FreeRequest, sizeof(FreeRequest));
                ZeroBytes(&FreeResult, sizeof(FreeResult));
                FreeRequest.PhysicalAddress = ClaimResult.BaseAddress;
                FreeRequest.PageCount = ClaimResult.PageCount;
                LosMemoryManagerSendFreeFrames(&FreeRequest, &FreeResult);
            }

            LosKernelTraceFail("Kernel scheduler could not direct-map memory-manager-backed thread stack.");
        }
        else
        {
            LosKernelTraceFail("Kernel scheduler could not allocate memory-manager-backed thread stack frames.");
            LosKernelTraceUnsigned("Kernel scheduler stack-claim status: ", ClaimResult.Status);
            LosKernelTraceUnsigned("Kernel scheduler stack-claim pages returned: ", ClaimResult.PageCount);
        }
    }

    if (AllocateDirectClaimKernelThreadStack(Task) != 0U)
    {
        return 1;
    }

    if (LosKernelSchedulerIsOnline() != 0U &&
        IsMemoryManagerSchedulerTransportReady() != 0U)
    {
        LosKernelTrace("Kernel scheduler keeping hosted transient thread stack on bootstrap fallback until AllocateFrames replies are stable.");
    }

    return AllocateBootstrapKernelThreadStack(Task);
}

static void ReleaseTaskStackResources(LOS_KERNEL_SCHEDULER_TASK *Task)
{
    if (Task == 0)
    {
        return;
    }

    if (Task->StackAllocationSource == LOS_KERNEL_SCHEDULER_STACK_SOURCE_BOOTSTRAP)
    {
        if (Task->BootstrapStackSlot != LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT &&
            Task->BootstrapStackSlot < LOS_KERNEL_SCHEDULER_MAX_TASKS)
        {
            LosKernelSchedulerBootstrapStackUsed[Task->BootstrapStackSlot] = 0U;
            Task->BootstrapStackSlot = LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT;
        }
        else
        {
            LosKernelTraceFail("Kernel scheduler lost track of a bootstrap fallback stack slot.");
        }
    }
    else if (Task->StackAllocationSource == LOS_KERNEL_SCHEDULER_STACK_SOURCE_DIRECT_CLAIM)
    {
        LOS_KERNEL_SCHEDULER_STATE *State;

        State = LosKernelSchedulerState();
        if (Task->BootstrapStackSlot != LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT &&
            Task->BootstrapStackSlot < LOS_KERNEL_SCHEDULER_MAX_TASKS)
        {
            LosKernelSchedulerDirectClaimStackUsed[Task->BootstrapStackSlot] = 0U;
            if (State != 0 && State->DirectClaimStackSlotsInUse > 0U)
            {
                State->DirectClaimStackSlotsInUse -= 1U;
            }
            Task->BootstrapStackSlot = LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT;
        }
        else
        {
            LosKernelTraceFail("Kernel scheduler lost track of a direct-claim stack-pool slot.");
            LosKernelTraceHex64("Kernel scheduler direct-claim stack base: ", Task->StackPhysicalAddress);
        }
    }
    else if (Task->StackPhysicalAddress != 0ULL && Task->StackSizeBytes != 0ULL)
    {
        if (Task->StackAllocationSource == LOS_KERNEL_SCHEDULER_STACK_SOURCE_MEMORY_MANAGER)
        {
            LOS_X64_FREE_FRAMES_REQUEST FreeRequest;
            LOS_X64_FREE_FRAMES_RESULT FreeResult;

            ZeroBytes(&FreeRequest, sizeof(FreeRequest));
            ZeroBytes(&FreeResult, sizeof(FreeResult));
            FreeRequest.PhysicalAddress = Task->StackPhysicalAddress;
            FreeRequest.PageCount = Task->StackSizeBytes / 4096ULL;

            if (IsMemoryManagerSchedulerTransportReady() != 0U)
            {
                LosMemoryManagerSendFreeFrames(&FreeRequest, &FreeResult);
                if (FreeResult.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS)
                {
                    LosKernelTraceFail("Kernel scheduler could not free terminated thread stack frames.");
                    LosKernelTraceUnsigned("Kernel scheduler stack-free status: ", FreeResult.Status);
                    LosKernelTraceHex64("Kernel scheduler stack-free base: ", FreeRequest.PhysicalAddress);
                }
            }
            else
            {
                LosKernelTraceFail("Kernel scheduler could not free terminated thread stack because the memory manager bootstrap is not ready.");
            }
        }
    }

    Task->StackPhysicalAddress = 0ULL;
    Task->StackBaseVirtualAddress = 0ULL;
    Task->StackTopVirtualAddress = 0ULL;
    Task->StackSizeBytes = 0ULL;
    Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_NONE;
    ZeroBytes(&Task->ExecutionContext, sizeof(Task->ExecutionContext));
}


void LosKernelSchedulerBindPendingProcessAddressSpaces(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;

    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY)
        {
            continue;
        }
        if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) != 0U)
        {
            continue;
        }
        if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) != 0U)
        {
            continue;
        }
        if ((Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_INHERITS_ROOT) == 0U)
        {
            continue;
        }
        if (Process->AddressSpaceId != 0ULL || Process->AddressSpaceObjectPhysicalAddress != 0ULL)
        {
            continue;
        }

        (void)BindOwnedProcessAddressSpace(Process);
    }
}

BOOLEAN LosKernelSchedulerCreateProcess(
    const char *Name,
    UINT32 Flags,
    UINT64 AddressSpaceId,
    UINT64 RootTablePhysicalAddress,
    UINT64 *ProcessId)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *CreatedProcess;
    UINT32 Index;
    UINT64 LocalProcessId;
    UINT64 CriticalSectionFlags;
    BOOLEAN NeedsOwnedAddressSpace;

    if (ProcessId != 0)
    {
        *ProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    }

    NeedsOwnedAddressSpace = ((Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) == 0U &&
                              RootTablePhysicalAddress == LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS &&
                              AddressSpaceId == 0ULL) ? 1U : 0U;
    if (NeedsOwnedAddressSpace != 0U &&
        (Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_REQUIRE_OWN_ADDRESS_SPACE) != 0U &&
        LosIsMemoryManagerBootstrapReady() == 0U)
    {
        State = LosKernelSchedulerState();
        CriticalSectionFlags = EnterSchedulerCriticalSection();
        State->AddressSpaceBindDeferredCount += 1ULL;
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        LosKernelTraceFail("Kernel scheduler rejected process creation because the memory manager bootstrap is not ready for a distinct process root.");
        return 0;
    }

    CreatedProcess = 0;
    LocalProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();

    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED)
        {
            continue;
        }

        ZeroBytes(Process, sizeof(*Process));
        Process->Signature = LOS_KERNEL_SCHEDULER_SIGNATURE;
        Process->Version = LOS_KERNEL_SCHEDULER_VERSION;
        Process->State = NeedsOwnedAddressSpace != 0U
            ? LOS_KERNEL_SCHEDULER_PROCESS_STATE_INITIALIZING
            : LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY;
        Process->ProcessId = State->NextProcessId;
        Process->OwnerProcessId = GetCreatingOwnerProcessId();
        Process->Generation = State->CreatedProcessCount + 1ULL;
        Process->Name = Name;
        Process->Flags = Flags;
        if (NeedsOwnedAddressSpace != 0U)
        {
            Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_CREATE_IN_PROGRESS;
        }
        Process->RootTablePhysicalAddress = ResolveProcessRootTablePhysicalAddress(RootTablePhysicalAddress, &Process->Flags);
        Process->ThreadCount = 0U;
        Process->AddressSpaceId = AddressSpaceId;
        Process->AddressSpaceObjectPhysicalAddress = 0ULL;
        Process->CreatedTick = State->TickCount;
        Process->TerminatedTick = 0ULL;
        Process->DispatchCount = 0ULL;
        Process->TotalTicks = 0ULL;
        Process->LastRunTick = 0ULL;
        Process->UserEntryVirtualAddress = 0ULL;
        Process->UserStackTopVirtualAddress = 0ULL;
        Process->UserCodeSegmentSelector = 0ULL;
        Process->UserStackSegmentSelector = 0ULL;
        Process->UserRflags = 0ULL;
        Process->UserTransitionFrameStackPointer = 0ULL;
        Process->UserTransitionKernelEntryVirtualAddress = 0ULL;
        Process->UserTransitionBridgeVirtualAddress = 0ULL;
        Process->UserTransitionChainStackPointer = 0ULL;
        Process->ExitStatus = 0ULL;
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_NONE;
        Process->CleanupPending = 0U;
        Process->Reserved0 = 0U;
        Process->Reserved1 = 0U;

        State->NextProcessId += 1ULL;
        State->CreatedProcessCount += 1ULL;
        if (NeedsOwnedAddressSpace == 0U)
        {
            State->ProcessCount += 1U;
        }
        LocalProcessId = Process->ProcessId;
        CreatedProcess = Process;
        break;
    }

    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (CreatedProcess != 0)
    {
        if (NeedsOwnedAddressSpace != 0U)
        {
            if (!BindOwnedProcessAddressSpace(CreatedProcess))
            {
                if ((Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_REQUIRE_OWN_ADDRESS_SPACE) != 0U)
                {
                    LosKernelTraceFail("Kernel scheduler rejected process creation because a distinct address space was required but not available.");
                    LosKernelTraceUnsigned("Kernel scheduler rejected process id: ", LocalProcessId);
                }
                AbandonCreatedProcess(CreatedProcess);
                return 0;
            }

            CriticalSectionFlags = EnterSchedulerCriticalSection();
            if (CreatedProcess->State == LOS_KERNEL_SCHEDULER_PROCESS_STATE_INITIALIZING)
            {
                CreatedProcess->State = LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY;
                CreatedProcess->Flags &= ~LOS_KERNEL_SCHEDULER_PROCESS_FLAG_CREATE_IN_PROGRESS;
                State = LosKernelSchedulerState();
                State->ProcessCount += 1U;
            }
            LeaveSchedulerCriticalSection(CriticalSectionFlags);
        }

        if (ProcessId != 0)
        {
            *ProcessId = LocalProcessId;
        }
        LosKernelSchedulerTraceProcess("Registered scheduler process", CreatedProcess);
        return 1;
    }

    return 0;
}

BOOLEAN LosKernelSchedulerMarkProcessTerminated(
    UINT64 ProcessId,
    UINT64 ExitStatus)
{
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    UINT64 CriticalSectionFlags;
    BOOLEAN Success;

    Success = 0;
    if (ProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    Process = FindProcessByIdMutable(ProcessId);
    if (Process != 0 &&
        Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_UNUSED &&
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) == 0U)
    {
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED)
        {
            LosKernelSchedulerState()->TerminatedProcessCount += 1ULL;
        }
        Process->State = LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED;
        Process->TerminatedTick = LosKernelSchedulerState()->TickCount;
        Process->ExitStatus = ExitStatus;
        Process->CleanupPending = 1U;
        Success = 1;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    return Success;
}

BOOLEAN LosKernelSchedulerCreateTask(
    const char *Name,
    UINT64 ProcessId,
    UINT32 Flags,
    UINT32 Priority,
    UINT32 QuantumTicks,
    UINT64 WakePeriodTicks,
    LOS_KERNEL_SCHEDULER_THREAD_ROUTINE ThreadRoutine,
    void *Context,
    UINT64 *TaskId)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    UINT32 Index;
    LOS_KERNEL_SCHEDULER_TASK *CreatedTask;
    UINT64 CriticalSectionFlags;
    UINT64 LocalTaskId;

    if (TaskId != 0)
    {
        *TaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    }
    if (ThreadRoutine == 0)
    {
        return 0;
    }

    CreatedTask = 0;
    LocalTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(ProcessId);
    if (Process == 0 || Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_READY)
    {
        LeaveSchedulerCriticalSection(CriticalSectionFlags);
        return 0;
    }

    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        if (State->Tasks[Index].State == LOS_KERNEL_SCHEDULER_TASK_STATE_UNUSED)
        {
            LOS_KERNEL_SCHEDULER_TASK *Task;

            Task = &State->Tasks[Index];
            ZeroBytes(Task, sizeof(*Task));
            Task->Signature = LOS_KERNEL_SCHEDULER_SIGNATURE;
            Task->Version = LOS_KERNEL_SCHEDULER_VERSION;
            Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
            Task->TaskId = State->NextTaskId;
            Task->OwnerTaskId = GetCreatingOwnerTaskId();
            Task->ProcessId = ProcessId;
            Task->Generation = State->CreatedTaskCount + 1ULL;
            Task->Name = Name;
            Task->Flags = Flags;
            Task->Priority = Priority;
            Task->QuantumTicks = QuantumTicks == 0U ? LOS_KERNEL_SCHEDULER_DEFAULT_QUANTUM_TICKS : QuantumTicks;
            Task->RemainingQuantumTicks = Task->QuantumTicks;
            Task->WakePeriodTicks = WakePeriodTicks;
            Task->NextWakeTick = 0ULL;
            Task->DispatchCount = 0ULL;
            Task->TotalTicks = 0ULL;
            Task->LastRunTick = 0ULL;
            Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_NONE;
            Task->LastWakeTick = 0ULL;
            Task->ReadySinceTick = State->TickCount;
            Task->UserInstructionPointer = 0ULL;
            Task->UserStackPointer = 0ULL;
            Task->UserCodeSegmentSelector = 0ULL;
            Task->UserStackSegmentSelector = 0ULL;
            Task->UserRflags = 0ULL;
            Task->UserTransitionFrameStackPointer = 0ULL;
            Task->UserTransitionKernelEntryVirtualAddress = 0ULL;
            Task->UserTransitionBridgeVirtualAddress = 0ULL;
            Task->UserTransitionChainStackPointer = 0ULL;
            Task->PreemptionCount = 0ULL;
            Task->ExitStatus = 0ULL;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_NONE;
            Task->CleanupPending = 0U;
            Task->BootstrapStackSlot = LOS_KERNEL_SCHEDULER_INVALID_STACK_SLOT;
            Task->WakeDispatchPending = 0U;
            Task->ResumeBoostTicks = 0U;
            Task->StackAllocationSource = LOS_KERNEL_SCHEDULER_STACK_SOURCE_NONE;
            Task->ThreadRoutine = ThreadRoutine;
            Task->Context = Context;

            if (!AllocateKernelThreadStack(Task))
            {
                ZeroBytes(Task, sizeof(*Task));
                break;
            }

            Process->ThreadCount += 1U;
            State->TaskCount += 1U;
            State->NextTaskId += 1ULL;
            State->CreatedTaskCount += 1ULL;
            LocalTaskId = Task->TaskId;
            CreatedTask = Task;
            break;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (CreatedTask != 0)
    {
        if (TaskId != 0)
        {
            *TaskId = LocalTaskId;
        }
        LosKernelSchedulerTraceTask("Registered scheduler task", CreatedTask);
        return 1;
    }

    return 0;
}

BOOLEAN LosKernelSchedulerPrepareUserTransitionScaffold(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 ProcessId;
    UINT64 TaskId;
    UINT64 CriticalSectionFlags;

    State = LosKernelSchedulerState();
    if (State == 0)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldReady != 0U)
    {
        return 1;
    }

    if (LosIsMemoryManagerBootstrapReady() == 0U)
    {
        return 0;
    }

    ProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    TaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    if (!LosKernelSchedulerCreateProcess(
            "InitCommandProcess",
            LOS_KERNEL_SCHEDULER_PROCESS_FLAG_USER_TRANSITION |
            LOS_KERNEL_SCHEDULER_PROCESS_FLAG_REQUIRE_OWN_ADDRESS_SPACE,
            0ULL,
            LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS,
            &ProcessId))
    {
        return 0;
    }

    if (!LosKernelSchedulerCreateTask(
            "InitCommandTask",
            ProcessId,
            LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE |
            LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_SCAFFOLD,
            LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_PRIORITY,
            1U,
            0ULL,
            LosKernelSchedulerUserTransitionTrapThread,
            0,
            &TaskId))
    {
        (void)LosKernelSchedulerMarkProcessTerminated(ProcessId, 1ULL);
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(ProcessId);
    Task = FindTaskByIdMutable(TaskId);
    if (Process != 0)
    {
        Process->Flags |= LOS_KERNEL_SCHEDULER_PROCESS_FLAG_USER_TRANSITION;
        Process->UserEntryVirtualAddress = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_ENTRY_VIRTUAL_ADDRESS;
        Process->UserStackTopVirtualAddress = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_TOP_VIRTUAL_ADDRESS;
        Process->UserCodeSegmentSelector = 0ULL;
        Process->UserStackSegmentSelector = 0ULL;
        Process->UserRflags = 0ULL;
        Process->UserTransitionFrameStackPointer = 0ULL;
        Process->UserTransitionKernelEntryVirtualAddress = 0ULL;
        Process->UserTransitionBridgeVirtualAddress = 0ULL;
        Process->UserTransitionChainStackPointer = 0ULL;
        Process->UserTransitionContractSignature = 0ULL;
        Process->UserTransitionSealValue = 0ULL;
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_PREPARED;
    }
    if (Task != 0)
    {
        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED;
        Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION;
        Task->ReadySinceTick = 0ULL;
        Task->NextWakeTick = 0ULL;
        Task->RemainingQuantumTicks = Task->QuantumTicks;
        Task->Flags |= LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_MODE | LOS_KERNEL_SCHEDULER_TASK_FLAG_USER_SCAFFOLD;
        Task->UserInstructionPointer = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_ENTRY_VIRTUAL_ADDRESS;
        Task->UserStackPointer = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_TOP_VIRTUAL_ADDRESS;
        Task->UserCodeSegmentSelector = 0ULL;
        Task->UserStackSegmentSelector = 0ULL;
        Task->UserRflags = 0ULL;
        Task->UserTransitionFrameStackPointer = 0ULL;
        Task->UserTransitionKernelEntryVirtualAddress = 0ULL;
        Task->UserTransitionBridgeVirtualAddress = 0ULL;
        Task->UserTransitionChainStackPointer = 0ULL;
        Task->UserTransitionContractSignature = 0ULL;
        Task->UserTransitionSealValue = 0ULL;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_PREPARED;
    }
    State->UserTransitionScaffoldReady = 1U;
    State->UserTransitionPreparedCount += 1ULL;
    State->UserTransitionScaffoldProcessId = ProcessId;
    State->UserTransitionScaffoldTaskId = TaskId;
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    LosKernelTraceOk("Scheduler init command prepared.");
    LosKernelTraceHex64("Scheduler init command entry: ", LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_ENTRY_VIRTUAL_ADDRESS);
    LosKernelTraceHex64("Scheduler init command user-stack top: ", LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_STACK_TOP_VIRTUAL_ADDRESS);
    LosKernelSchedulerTraceProcess("Prepared scheduler init command process", Process);
    LosKernelSchedulerTraceTask("Prepared scheduler init command task", Task);
    return 1;
}

BOOLEAN LosKernelSchedulerValidateUserTransitionScaffold(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN Validated;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->AddressSpaceId == 0ULL ||
        Process->RootTablePhysicalAddress == LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS ||
        Process->UserEntryVirtualAddress == 0ULL ||
        Process->UserStackTopVirtualAddress == 0ULL ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Task->UserInstructionPointer == 0ULL ||
        Task->UserStackPointer == 0ULL)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED)
    {
        return 1;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Validated = 0U;
    if (Process != 0 && Task != 0 &&
        Process->AddressSpaceId != 0ULL &&
        Process->RootTablePhysicalAddress != LOS_KERNEL_SCHEDULER_INVALID_ROOT_TABLE_PHYSICAL_ADDRESS &&
        Process->UserEntryVirtualAddress != 0ULL &&
        Process->UserStackTopVirtualAddress != 0ULL &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->UserInstructionPointer != 0ULL &&
        Task->UserStackPointer != 0ULL)
    {
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED;
        State->UserTransitionValidatedCount += 1ULL;
        Validated = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Validated != 0U)
    {
        LosKernelTraceOk("Scheduler first user task validated.");
        LosKernelSchedulerTraceProcess("Validated scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Validated scheduler first user task task", Task);
        return 1;
    }

    return 0;
}

BOOLEAN LosKernelSchedulerArmUserTransitionScaffold(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN Armed;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionValidatedCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED)
    {
        return 1;
    }

    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Armed = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_VALIDATED &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED;
        State->UserTransitionArmedCount += 1ULL;
        Armed = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Armed != 0U)
    {
        LosKernelTraceOk("Scheduler first user task armed.");
        LosKernelSchedulerTraceProcess("Armed scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Armed scheduler first user task task", Task);
        return 1;
    }

    return 0;
}

BOOLEAN LosKernelSchedulerRequestUserTransitionScaffold(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN Requested;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionArmedCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED)
    {
        return 1;
    }

    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Requested = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ARMED &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED;
        State->UserTransitionLaunchRequestCount += 1ULL;
        Requested = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Requested != 0U)
    {
        LosKernelTraceOk("Scheduler first user task launch requested.");
        LosKernelSchedulerTraceProcess("Launch-requested scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Launch-requested scheduler first user task task", Task);
        return 1;
    }

    return 0;
}

void LosKernelSchedulerCleanupTerminatedTasks(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;
    UINT64 CriticalSectionFlags;

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_TASKS; ++Index)
    {
        LOS_KERNEL_SCHEDULER_TASK *Task;
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Task = &State->Tasks[Index];
        if (Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED || Task->CleanupPending == 0U)
        {
            continue;
        }

        LosKernelSchedulerTraceTask("Reclaimed scheduler task", Task);
        Process = FindProcessByIdMutable(Task->ProcessId);
        if (Process != 0 && Process->ThreadCount > 0U)
        {
            Process->ThreadCount -= 1U;
            if (Process->ThreadCount == 0U && (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL) == 0U)
            {
                if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED)
                {
                    State->TerminatedProcessCount += 1ULL;
                }
                Process->State = LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED;
                Process->TerminatedTick = State->TickCount;
                Process->ExitStatus = Task->ExitStatus;
                Process->CleanupPending = 1U;
            }
        }

        if (Task->TaskId == State->UserTransitionScaffoldTaskId)
        {
            State->UserTransitionScaffoldTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
        }
        ReleaseTaskStackResources(Task);
        if (State->TaskCount > 0U)
        {
            State->TaskCount -= 1U;
        }
        State->ReapedTaskCount += 1ULL;
        ZeroBytes(Task, sizeof(*Task));
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
}

void LosKernelSchedulerCleanupTerminatedProcesses(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT32 Index;
    UINT64 CriticalSectionFlags;

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    for (Index = 0U; Index < LOS_KERNEL_SCHEDULER_MAX_PROCESSES; ++Index)
    {
        LOS_KERNEL_SCHEDULER_PROCESS *Process;

        Process = &State->Processes[Index];
        if (Process->State != LOS_KERNEL_SCHEDULER_PROCESS_STATE_TERMINATED ||
            Process->CleanupPending == 0U ||
            Process->ThreadCount != 0U)
        {
            continue;
        }

        if (!DestroyOwnedProcessAddressSpace(Process))
        {
            continue;
        }

        if (Process->ProcessId == State->UserTransitionScaffoldProcessId)
        {
            State->UserTransitionScaffoldProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
        }
        LosKernelSchedulerTraceProcess("Reclaimed scheduler process", Process);
        if (State->ProcessCount > 0U)
        {
            State->ProcessCount -= 1U;
        }
        State->ReapedProcessCount += 1ULL;
        ZeroBytes(Process, sizeof(*Process));
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);
}

BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldEntryReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN EntryReady;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionLaunchRequestCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY)
    {
        return 1;
    }

    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        (Process->Flags & LOS_KERNEL_SCHEDULER_PROCESS_FLAG_OWNS_ADDRESS_SPACE) == 0U ||
        Process->AddressSpaceObjectPhysicalAddress == 0ULL ||
        Process->RootTablePhysicalAddress == 0ULL)
    {
        return 0;
    }

    if (EnsureFirstUserTaskMappings(Process, Task) == 0U)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0 ||
        Task->UserInstructionPointer == 0ULL ||
        Task->UserStackPointer == 0ULL ||
        Process->UserEntryVirtualAddress == 0ULL ||
        Process->UserStackTopVirtualAddress == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    EntryReady = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_REQUESTED &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->UserInstructionPointer != 0ULL &&
        Task->UserStackPointer != 0ULL &&
        Process->UserEntryVirtualAddress != 0ULL &&
        Process->UserStackTopVirtualAddress != 0ULL)
    {
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY;
        State->UserTransitionEntryReadyCount += 1ULL;
        EntryReady = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (EntryReady != 0U)
    {
        LosKernelTraceOk("Scheduler first user task entry-ready.");
        LosKernelSchedulerTraceProcess("Entry-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Entry-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}

BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldDescriptorReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN DescriptorReady;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionEntryReadyCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY)
    {
        return 1;
    }

    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Process->UserEntryVirtualAddress == 0ULL ||
        Process->UserStackTopVirtualAddress == 0ULL ||
        Task->UserInstructionPointer == 0ULL ||
        Task->UserStackPointer == 0ULL ||
        LOS_X64_USER_CODE_SELECTOR == 0U ||
        LOS_X64_USER_DATA_SELECTOR == 0U)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    DescriptorReady = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_ENTRY_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Process->UserEntryVirtualAddress != 0ULL &&
        Process->UserStackTopVirtualAddress != 0ULL &&
        Task->UserInstructionPointer != 0ULL &&
        Task->UserStackPointer != 0ULL)
    {
        Process->UserCodeSegmentSelector = (UINT64)LOS_X64_USER_CODE_SELECTOR;
        Process->UserStackSegmentSelector = (UINT64)LOS_X64_USER_DATA_SELECTOR;
        Process->UserRflags = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_RFLAGS;
        Task->UserCodeSegmentSelector = (UINT64)LOS_X64_USER_CODE_SELECTOR;
        Task->UserStackSegmentSelector = (UINT64)LOS_X64_USER_DATA_SELECTOR;
        Task->UserRflags = LOS_KERNEL_SCHEDULER_USER_SCAFFOLD_RFLAGS;
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY;
        State->UserTransitionDescriptorReadyCount += 1ULL;
        DescriptorReady = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (DescriptorReady != 0U)
    {
        LosKernelTraceOk("Scheduler first user task descriptor-ready.");
        LosKernelSchedulerTraceProcess("Descriptor-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Descriptor-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldFrameReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    UINT64 FrameStackPointer;
    BOOLEAN FrameReady;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionDescriptorReadyCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY)
    {
        return 1;
    }

    FrameStackPointer = GetUserTransitionFrameStackPointer(Task);
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Task->UserInstructionPointer == 0ULL ||
        Task->UserStackPointer == 0ULL ||
        Process->UserEntryVirtualAddress == 0ULL ||
        Process->UserStackTopVirtualAddress == 0ULL ||
        Process->UserCodeSegmentSelector == 0ULL ||
        Process->UserStackSegmentSelector == 0ULL ||
        Process->UserRflags == 0ULL ||
        Task->UserCodeSegmentSelector == 0ULL ||
        Task->UserStackSegmentSelector == 0ULL ||
        Task->UserRflags == 0ULL ||
        FrameStackPointer == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    FrameReady = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_DESCRIPTOR_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->UserInstructionPointer != 0ULL &&
        Task->UserStackPointer != 0ULL &&
        Process->UserEntryVirtualAddress != 0ULL &&
        Process->UserStackTopVirtualAddress != 0ULL &&
        Process->UserCodeSegmentSelector != 0ULL &&
        Process->UserStackSegmentSelector != 0ULL &&
        Process->UserRflags != 0ULL &&
        Task->UserCodeSegmentSelector != 0ULL &&
        Task->UserStackSegmentSelector != 0ULL &&
        Task->UserRflags != 0ULL)
    {
        FrameStackPointer = GetUserTransitionFrameStackPointer(Task);
        if (FrameStackPointer != 0ULL)
        {
            WriteUserTransitionFrame(
                FrameStackPointer,
                Task->UserInstructionPointer,
                Task->UserCodeSegmentSelector,
                Task->UserRflags,
                Task->UserStackPointer,
                Task->UserStackSegmentSelector);
            Process->UserTransitionFrameStackPointer = FrameStackPointer;
            Task->UserTransitionFrameStackPointer = FrameStackPointer;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY;
            State->UserTransitionFrameReadyCount += 1ULL;
            FrameReady = 1U;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (FrameReady != 0U)
    {
        LosKernelTraceOk("Scheduler first user task frame-ready.");
        LosKernelSchedulerTraceProcess("Frame-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Frame-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldTrampolineReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    UINT64 KernelEntryAddress;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionFrameReadyCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY)
    {
        return 1;
    }

    KernelEntryAddress = (UINT64)(UINTN)LosKernelSchedulerUserTransitionKernelEntry;
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Task->ExecutionContext.StackPointer == 0ULL ||
        Process->UserTransitionFrameStackPointer == 0ULL ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        KernelEntryAddress == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_FRAME_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->ExecutionContext.StackPointer != 0ULL &&
        Process->UserTransitionFrameStackPointer != 0ULL &&
        Task->UserTransitionFrameStackPointer != 0ULL &&
        KernelEntryAddress != 0ULL)
    {
        WriteStackReturnAddress(Task->ExecutionContext.StackPointer, KernelEntryAddress);
        if (ReadStackReturnAddress(Task->ExecutionContext.StackPointer) == KernelEntryAddress)
        {
            Process->UserTransitionKernelEntryVirtualAddress = KernelEntryAddress;
            Task->UserTransitionKernelEntryVirtualAddress = KernelEntryAddress;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY;
            State->UserTransitionTrampolineReadyCount += 1ULL;
            Ready = 1U;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task trampoline-ready.");
        LosKernelSchedulerTraceProcess("Trampoline-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Trampoline-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}

BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldBridgeReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 BridgeAddress;
    UINT64 CriticalSectionFlags;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionTrampolineReadyCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY)
    {
        return 1;
    }

    BridgeAddress = (UINT64)(UINTN)LosKernelSchedulerUserTransitionDispatchBridge;
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Task->ExecutionContext.StackPointer == 0ULL ||
        Process->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Task->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        BridgeAddress == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_TRAMPOLINE_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->ExecutionContext.StackPointer != 0ULL &&
        Process->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Task->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        BridgeAddress != 0ULL)
    {
        WriteStackReturnAddress(Task->ExecutionContext.StackPointer, BridgeAddress);
        if (ReadStackReturnAddress(Task->ExecutionContext.StackPointer) == BridgeAddress)
        {
            Process->UserTransitionBridgeVirtualAddress = BridgeAddress;
            Task->UserTransitionBridgeVirtualAddress = BridgeAddress;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY;
            State->UserTransitionBridgeReadyCount += 1ULL;
            Ready = 1U;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task bridge-ready.");
        LosKernelSchedulerTraceProcess("Bridge-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Bridge-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldChainReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 ChainStackPointer;
    UINT64 CriticalSectionFlags;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionBridgeReadyCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY)
    {
        return 1;
    }

    ChainStackPointer = GetUserTransitionChainStackPointer(Task);
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Process->UserTransitionFrameStackPointer == 0ULL ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Process->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Task->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Process->UserTransitionBridgeVirtualAddress == 0ULL ||
        Task->UserTransitionBridgeVirtualAddress == 0ULL ||
        ChainStackPointer == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_BRIDGE_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Process->UserTransitionFrameStackPointer != 0ULL &&
        Task->UserTransitionFrameStackPointer != 0ULL &&
        Process->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Task->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Process->UserTransitionBridgeVirtualAddress != 0ULL &&
        Task->UserTransitionBridgeVirtualAddress != 0ULL)
    {
        ChainStackPointer = GetUserTransitionChainStackPointer(Task);
        if (ChainStackPointer != 0ULL)
        {
            WriteStackReturnAddress(ChainStackPointer, Task->UserTransitionBridgeVirtualAddress);
            WriteStackReturnAddress(ChainStackPointer + 8ULL, Task->UserTransitionKernelEntryVirtualAddress);
            if (ReadStackReturnAddress(ChainStackPointer) == Task->UserTransitionBridgeVirtualAddress &&
                ReadStackReturnAddress(ChainStackPointer + 8ULL) == Task->UserTransitionKernelEntryVirtualAddress)
            {
                Task->ExecutionContext.StackPointer = ChainStackPointer;
                Process->UserTransitionChainStackPointer = ChainStackPointer;
                Task->UserTransitionChainStackPointer = ChainStackPointer;
                Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY;
                Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY;
                State->UserTransitionChainReadyCount += 1ULL;
                Ready = 1U;
            }
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task chain-ready.");
        LosKernelSchedulerTraceProcess("Chain-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Chain-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldContractReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    UINT64 ContractSignature;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionChainReadyCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY)
    {
        return 1;
    }

    ContractSignature = ComputeUserTransitionContractSignature(Process, Task);
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        ContractSignature == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CHAIN_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        ContractSignature = ComputeUserTransitionContractSignature(Process, Task);
        if (ContractSignature != 0ULL)
        {
            Process->UserTransitionContractSignature = ContractSignature;
            Task->UserTransitionContractSignature = ContractSignature;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY;
            State->UserTransitionContractReadyCount += 1ULL;
            Ready = 1U;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task contract-ready.");
        LosKernelSchedulerTraceProcess("Contract-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Contract-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldSealReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    UINT64 SealValue;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionContractReadyCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY)
    {
        return 1;
    }

    SealValue = ComputeUserTransitionSealValue(Process, Task);
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Process->UserTransitionFrameStackPointer == 0ULL ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Process->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Task->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Process->UserTransitionBridgeVirtualAddress == 0ULL ||
        Task->UserTransitionBridgeVirtualAddress == 0ULL ||
        Process->UserTransitionChainStackPointer == 0ULL ||
        Task->UserTransitionChainStackPointer == 0ULL ||
        Process->UserTransitionContractSignature == 0ULL ||
        Task->UserTransitionContractSignature == 0ULL ||
        SealValue == 0ULL)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_CONTRACT_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Process->UserTransitionFrameStackPointer != 0ULL &&
        Task->UserTransitionFrameStackPointer != 0ULL &&
        Process->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Task->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Process->UserTransitionBridgeVirtualAddress != 0ULL &&
        Task->UserTransitionBridgeVirtualAddress != 0ULL &&
        Process->UserTransitionChainStackPointer != 0ULL &&
        Task->UserTransitionChainStackPointer != 0ULL &&
        Process->UserTransitionContractSignature != 0ULL &&
        Task->UserTransitionContractSignature != 0ULL)
    {
        SealValue = ComputeUserTransitionSealValue(Process, Task);
        if (SealValue != 0ULL)
        {
            WriteStackReturnAddress(Task->UserTransitionChainStackPointer + 16ULL, SealValue);
            if (ReadStackReturnAddress(Task->UserTransitionChainStackPointer + 16ULL) == SealValue)
            {
                Process->UserTransitionSealValue = SealValue;
                Task->UserTransitionSealValue = SealValue;
                Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY;
                Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY;
                State->UserTransitionSealReadyCount += 1ULL;
                Ready = 1U;
            }
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task seal-ready.");
        LosKernelSchedulerTraceProcess("Seal-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Seal-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldHandoffReady(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    UINT64 HandoffStackPointer;
    BOOLEAN Ready;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionSealReadyCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY)
    {
        return 1;
    }

    HandoffStackPointer = Task->ExecutionContext.StackPointer;
    if (Process->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY ||
        Task->UserTransitionState != LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Process->UserTransitionChainStackPointer == 0ULL ||
        Task->UserTransitionChainStackPointer == 0ULL ||
        HandoffStackPointer == 0ULL ||
        HandoffStackPointer != Task->UserTransitionChainStackPointer ||
        ReadStackReturnAddress(Task->UserTransitionChainStackPointer + 16ULL) != Task->UserTransitionSealValue)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Ready = 0U;
    if (Process != 0 && Task != 0 &&
        Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_SEAL_READY &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Process->UserTransitionChainStackPointer != 0ULL &&
        Task->UserTransitionChainStackPointer != 0ULL)
    {
        HandoffStackPointer = Task->ExecutionContext.StackPointer;
        if (HandoffStackPointer != 0ULL &&
            HandoffStackPointer == Task->UserTransitionChainStackPointer &&
            ReadStackReturnAddress(Task->UserTransitionChainStackPointer + 16ULL) == Task->UserTransitionSealValue)
        {
            Process->UserTransitionHandoffStackPointer = HandoffStackPointer;
            Task->UserTransitionHandoffStackPointer = HandoffStackPointer;
            Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY;
            Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY;
            State->UserTransitionHandoffReadyCount += 1ULL;
            Ready = 1U;
        }
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Ready != 0U)
    {
        LosKernelTraceOk("Scheduler first user task handoff-ready.");
        LosKernelSchedulerTraceProcess("Handoff-ready scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Handoff-ready scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldComplete(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    /*
     * Compatibility shim for older lifecycle callers: do not mark the
     * first user task COMPLETE before it ever reaches the real CPL3 entry
     * path. Promote the handoff-ready scaffold to LIVE instead.
     */
    if (State->UserTransitionLiveCount == 0ULL &&
        State->UserTransitionHandoffReadyCount != 0ULL)
    {
        if (State->UserTransitionLiveGateClosed != 0U)
        {
            State->UserTransitionLiveGateClosed = 0U;
        }
        return LosKernelSchedulerMarkUserTransitionScaffoldLive();
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldLive(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN Live;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    if (State->UserTransitionHandoffReadyCount == 0ULL)
    {
        return 0;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0;
    }

    if (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE &&
        Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE)
    {
        return 1;
    }

    if (!((Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY ||
           Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE)) ||
        !((Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY ||
           Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE)) ||
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
        Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION ||
        Task->UserInstructionPointer == 0ULL ||
        Task->UserStackPointer == 0ULL ||
        Process->UserEntryVirtualAddress == 0ULL ||
        Process->UserStackTopVirtualAddress == 0ULL ||
        Process->UserCodeSegmentSelector == 0ULL ||
        Process->UserStackSegmentSelector == 0ULL ||
        Process->UserRflags == 0ULL ||
        Task->UserCodeSegmentSelector == 0ULL ||
        Task->UserStackSegmentSelector == 0ULL ||
        Task->UserRflags == 0ULL ||
        Process->UserTransitionFrameStackPointer == 0ULL ||
        Task->UserTransitionFrameStackPointer == 0ULL ||
        Process->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Task->UserTransitionKernelEntryVirtualAddress == 0ULL ||
        Process->UserTransitionBridgeVirtualAddress == 0ULL ||
        Task->UserTransitionBridgeVirtualAddress == 0ULL ||
        Process->UserTransitionChainStackPointer == 0ULL ||
        Task->UserTransitionChainStackPointer == 0ULL ||
        Process->UserTransitionContractSignature == 0ULL ||
        Task->UserTransitionContractSignature == 0ULL ||
        Process->UserTransitionSealValue == 0ULL ||
        Task->UserTransitionSealValue == 0ULL ||
        Process->UserTransitionHandoffStackPointer == 0ULL ||
        Task->UserTransitionHandoffStackPointer == 0ULL ||
        Process->UserTransitionHandoffStackPointer != Task->UserTransitionHandoffStackPointer ||
        Task->ExecutionContext.StackPointer != Task->UserTransitionHandoffStackPointer ||
        ReadStackReturnAddress(Task->UserTransitionChainStackPointer + 16ULL) != Task->UserTransitionSealValue)
    {
        return 0;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Live = 0U;
    if (Process != 0 && Task != 0 &&
        (Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY ||
         Process->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE) &&
        (Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_HANDOFF_READY ||
         Task->UserTransitionState == LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_COMPLETE) &&
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION &&
        Task->UserInstructionPointer != 0ULL &&
        Task->UserStackPointer != 0ULL &&
        Process->UserEntryVirtualAddress != 0ULL &&
        Process->UserStackTopVirtualAddress != 0ULL &&
        Process->UserCodeSegmentSelector != 0ULL &&
        Process->UserStackSegmentSelector != 0ULL &&
        Process->UserRflags != 0ULL &&
        Task->UserCodeSegmentSelector != 0ULL &&
        Task->UserStackSegmentSelector != 0ULL &&
        Task->UserRflags != 0ULL &&
        Process->UserTransitionFrameStackPointer != 0ULL &&
        Task->UserTransitionFrameStackPointer != 0ULL &&
        Process->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Task->UserTransitionKernelEntryVirtualAddress != 0ULL &&
        Process->UserTransitionBridgeVirtualAddress != 0ULL &&
        Task->UserTransitionBridgeVirtualAddress != 0ULL &&
        Process->UserTransitionChainStackPointer != 0ULL &&
        Task->UserTransitionChainStackPointer != 0ULL &&
        Process->UserTransitionContractSignature != 0ULL &&
        Task->UserTransitionContractSignature != 0ULL &&
        Process->UserTransitionSealValue != 0ULL &&
        Task->UserTransitionSealValue != 0ULL &&
        Process->UserTransitionHandoffStackPointer != 0ULL &&
        Task->UserTransitionHandoffStackPointer != 0ULL &&
        Process->UserTransitionHandoffStackPointer == Task->UserTransitionHandoffStackPointer &&
        Task->ExecutionContext.StackPointer == Task->UserTransitionHandoffStackPointer &&
        ReadStackReturnAddress(Task->UserTransitionChainStackPointer + 16ULL) == Task->UserTransitionSealValue)
    {
        Process->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE;
        Task->UserTransitionState = LOS_KERNEL_SCHEDULER_USER_TRANSITION_STATE_LIVE;
        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_READY;
        Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_NONE;
        Task->ReadySinceTick = State->TickCount;
        Task->LastWakeTick = State->TickCount;
        Task->WakeDispatchPending = 1U;
        Task->ResumeBoostTicks = 4U;
        Task->NextWakeTick = 0ULL;
        Task->RemainingQuantumTicks = Task->QuantumTicks;
        State->UserTransitionLiveCount += 1ULL;
        State->UserTransitionLiveGateClosed = 0U;
        State->UserTransitionCompleteCount = 0ULL;
        Live = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Live != 0U)
    {
        LosKernelTraceOk("Scheduler first user task marked live for real iretq dispatch.");
        LosKernelSchedulerTraceProcess("Live scheduler first user task process", Process);
        LosKernelSchedulerTraceTask("Live scheduler first user task task", Task);
        return 1;
    }

    return 0;
}


BOOLEAN LosKernelSchedulerMarkUserTransitionScaffoldLiveGateClosed(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0;
    }

    /*
     * Compatibility shim for older lifecycle callers: the scaffold should
     * be promoted to LIVE and dispatched through the real iretq bridge,
     * not parked behind the old non-live gate.
     */
    if (State->UserTransitionLiveCount == 0ULL &&
        State->UserTransitionHandoffReadyCount != 0ULL)
    {
        if (State->UserTransitionLiveGateClosed != 0U)
        {
            State->UserTransitionLiveGateClosed = 0U;
        }
        return LosKernelSchedulerMarkUserTransitionScaffoldLive();
    }

    return 0;
}


BOOLEAN LosKernelSchedulerIsUserTransitionScaffoldBlocked(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_TASK *Task;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0U;
    }

    if (State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0U;
    }

    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Task == 0)
    {
        return 0U;
    }

    return (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
            Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
        ? 1U
        : 0U;
}

BOOLEAN LosKernelSchedulerGuardUserTransitionScaffold(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    LOS_KERNEL_SCHEDULER_PROCESS *Process;
    LOS_KERNEL_SCHEDULER_TASK *Task;
    UINT64 CriticalSectionFlags;
    BOOLEAN Reblocked;

    State = LosKernelSchedulerState();
    if (State == 0 || State->UserTransitionScaffoldReady == 0U)
    {
        return 0U;
    }

    if (State->UserTransitionLiveCount != 0ULL)
    {
        return 0U;
    }

    if (State->UserTransitionScaffoldProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID ||
        State->UserTransitionScaffoldTaskId == LOS_KERNEL_SCHEDULER_INVALID_TASK_ID)
    {
        return 0U;
    }

    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    if (Process == 0 || Task == 0)
    {
        return 0U;
    }

    if (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING)
    {
        LosKernelTraceFail("Scheduler first user task reached RUNNING before the live handoff existed.");
        LosKernelSchedulerTraceProcess("Invalid running scheduler first user-task process", Process);
        LosKernelSchedulerTraceTask("Invalid running scheduler first user-task task", Task);
        LosKernelHaltForever();
    }

    if (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED &&
        Task->LastBlockReason == LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION)
    {
        return 1U;
    }

    if (Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED ||
        Task->State == LOS_KERNEL_SCHEDULER_TASK_STATE_UNUSED)
    {
        return 0U;
    }

    CriticalSectionFlags = EnterSchedulerCriticalSection();
    State = LosKernelSchedulerState();
    Process = FindProcessByIdMutable(State->UserTransitionScaffoldProcessId);
    Task = FindTaskByIdMutable(State->UserTransitionScaffoldTaskId);
    Reblocked = 0U;
    if (Process != 0 && Task != 0 &&
        State->UserTransitionLiveCount == 0ULL &&
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_RUNNING &&
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_TERMINATED &&
        Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_UNUSED &&
        (Task->State != LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED ||
         Task->LastBlockReason != LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION))
    {
        Task->State = LOS_KERNEL_SCHEDULER_TASK_STATE_BLOCKED;
        Task->LastBlockReason = LOS_KERNEL_SCHEDULER_BLOCK_REASON_USER_TRANSITION;
        Task->ReadySinceTick = 0ULL;
        Task->NextWakeTick = 0ULL;
        Task->WakeDispatchPending = 0U;
        Task->ResumeBoostTicks = 0U;
        Task->RemainingQuantumTicks = Task->QuantumTicks;
        State->UserTransitionScaffoldReblockCount += 1ULL;
        Reblocked = 1U;
    }
    LeaveSchedulerCriticalSection(CriticalSectionFlags);

    if (Reblocked != 0U)
    {
        LosKernelTraceOk("Scheduler first user-task guard re-blocked a non-live task before dispatch.");
        LosKernelSchedulerTraceProcess("Guarded scheduler first user-task process", Process);
        LosKernelSchedulerTraceTask("Guarded scheduler first user-task task", Task);
    }

    return 1U;
}


void LosKernelSchedulerInitialize(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;
    UINT64 ProcessId;
    UINT64 TaskId;

    State = LosKernelSchedulerState();
    ZeroBytes(State, sizeof(*State));
    ZeroBytes(&LosKernelSchedulerBootstrapStackUsed[0], sizeof(LosKernelSchedulerBootstrapStackUsed));
    ZeroBytes(&LosKernelSchedulerDirectClaimStackUsed[0], sizeof(LosKernelSchedulerDirectClaimStackUsed));
    LosKernelSchedulerDirectClaimStackPoolBase = 0;
    State->Signature = LOS_KERNEL_SCHEDULER_SIGNATURE;
    State->Version = LOS_KERNEL_SCHEDULER_VERSION;
    State->Online = 0U;
    State->TickCount = 0ULL;
    State->DispatchCount = 0ULL;
    State->CurrentTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    State->CurrentProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    State->NextTaskId = 1ULL;
    State->NextProcessId = 1ULL;
    State->KernelProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    State->TaskCount = 0U;
    State->ProcessCount = 0U;
    State->CurrentTaskIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
    State->LastSelectedIndex = LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX;
    State->ReschedulePending = 0U;
    State->InScheduler = 0U;
    State->Reserved0 = 0U;
    State->InterruptPreemptionCount = 0ULL;
    State->StarvationReliefDispatchCount = 0ULL;
    State->CreatedTaskCount = 0ULL;
    State->TerminatedTaskCount = 0ULL;
    State->ReapedTaskCount = 0ULL;
    State->CreatedProcessCount = 0ULL;
    State->TerminatedProcessCount = 0ULL;
    State->ReapedProcessCount = 0ULL;
    State->ActiveRootTablePhysicalAddress = GetKernelRootTablePhysicalAddress();
    State->AddressSpaceSwitchCount = 0ULL;
    State->AddressSpaceReuseCount = 0ULL;
    State->AddressSpaceBindCount = 0ULL;
    State->AddressSpaceBindDeferredCount = 0ULL;
    State->WakeupCount = 0ULL;
    State->WakePriorityDispatchCount = 0ULL;
    State->WakeResumeWindowDispatchCount = 0ULL;
    State->MaxReadyDelayTicks = 0ULL;
    State->MaxWakeDelayTicks = 0ULL;
    State->MaxRunSliceTicks = 0ULL;
    State->IdleTicks = 0ULL;
    State->BusyTicks = 0ULL;
    State->UserTransitionPreparedCount = 0ULL;
    State->UserTransitionValidatedCount = 0ULL;
    State->UserTransitionArmedCount = 0ULL;
    State->UserTransitionLaunchRequestCount = 0ULL;
    State->UserTransitionEntryReadyCount = 0ULL;
    State->UserTransitionDescriptorReadyCount = 0ULL;
    State->UserTransitionFrameReadyCount = 0ULL;
    State->UserTransitionTrampolineReadyCount = 0ULL;
    State->UserTransitionBridgeReadyCount = 0ULL;
    State->UserTransitionChainReadyCount = 0ULL;
    State->UserTransitionContractReadyCount = 0ULL;
    State->UserTransitionSealReadyCount = 0ULL;
    State->UserTransitionHandoffReadyCount = 0ULL;
    State->UserTransitionCompleteCount = 0ULL;
    State->UserTransitionLiveCount = 0ULL;
    State->UserTransitionDispatchSkipCount = 0ULL;
    State->UserTransitionScaffoldReblockCount = 0ULL;
    State->UserTransitionScaffoldProcessId = LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID;
    State->UserTransitionScaffoldTaskId = LOS_KERNEL_SCHEDULER_INVALID_TASK_ID;
    State->DirectClaimStackPoolPhysicalAddress = 0ULL;
    State->DirectClaimStackPoolBytes = 0ULL;
    State->DirectClaimStackPoolReady = 0U;
    State->DirectClaimStackSlotsInUse = 0U;
    State->UserTransitionScaffoldReady = 0U;
    State->UserTransitionLiveGateClosed = 0U;
    ZeroBytes(&State->SchedulerContext, sizeof(State->SchedulerContext));
    State->SchedulerContext.Rflags = 0x202ULL;

    (void)ReserveDirectClaimKernelThreadStackPool();

    if (!LosKernelSchedulerCreateProcess(
            "KernelProcess",
            LOS_KERNEL_SCHEDULER_PROCESS_FLAG_KERNEL,
            0ULL,
            GetKernelRootTablePhysicalAddress(),
            &ProcessId))
    {
        LosKernelTraceFail("Kernel scheduler could not create kernel process.");
        LosKernelHaltForever();
    }
    State->KernelProcessId = ProcessId;

    if (!LosKernelSchedulerCreateTask(
            "Idle",
            State->KernelProcessId,
            LOS_KERNEL_SCHEDULER_TASK_FLAG_IDLE,
            LOS_KERNEL_SCHEDULER_IDLE_PRIORITY,
            1U,
            0ULL,
            LosKernelSchedulerIdleThread,
            0,
            &TaskId))
    {
        LosKernelTraceFail("Kernel scheduler could not create idle task.");
        LosKernelHaltForever();
    }

    LosKernelTraceOk("Kernel scheduler initialized.");
}

void LosKernelSchedulerRegisterBootstrapTasks(void)
{
    UINT64 TaskId;
    UINT64 KernelProcessId;

    KernelProcessId = LosKernelSchedulerState()->KernelProcessId;
    if (!LosKernelSchedulerCreateTask(
            "Heartbeat",
            KernelProcessId,
            LOS_KERNEL_SCHEDULER_TASK_FLAG_PERIODIC,
            LOS_KERNEL_SCHEDULER_HEARTBEAT_PRIORITY,
            1U,
            LOS_KERNEL_SCHEDULER_HEARTBEAT_PERIOD_TICKS,
            LosKernelSchedulerHeartbeatThread,
            0,
            &TaskId))
    {
        LosKernelTraceFail("Kernel scheduler could not create heartbeat task.");
        LosKernelHaltForever();
    }

    if (!LosKernelSchedulerCreateTask(
            "LifecycleManager",
            KernelProcessId,
            LOS_KERNEL_SCHEDULER_TASK_FLAG_PERIODIC,
            LOS_KERNEL_SCHEDULER_LIFECYCLE_PRIORITY,
            1U,
            LOS_KERNEL_SCHEDULER_LIFECYCLE_PERIOD_TICKS,
            LosKernelSchedulerLifecycleThread,
            0,
            &TaskId))
    {
        LosKernelTraceFail("Kernel scheduler could not create lifecycle manager task.");
        LosKernelHaltForever();
    }

    if (!LosKernelSchedulerCreateTask(
            "BusyWorker",
            KernelProcessId,
            0U,
            LOS_KERNEL_SCHEDULER_BUSY_PRIORITY,
            1U,
            0ULL,
            LosKernelSchedulerBusyThread,
            0,
            &TaskId))
    {
        LosKernelTraceFail("Kernel scheduler could not create busy worker task.");
        LosKernelHaltForever();
    }

    LosKernelTraceOk("Kernel scheduler bootstrap tasks registered.");
}

BOOLEAN LosKernelSchedulerIsOnline(void)
{
    return LosKernelSchedulerState()->Online != 0U ? 1U : 0U;
}

UINT64 LosKernelSchedulerGetTickCount(void)
{
    return LosKernelSchedulerState()->TickCount;
}

const LOS_KERNEL_SCHEDULER_TASK *LosKernelSchedulerGetCurrentTask(void)
{
    LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerState();
    if (State->CurrentTaskIndex == LOS_KERNEL_SCHEDULER_INVALID_TASK_INDEX ||
        State->CurrentTaskIndex >= LOS_KERNEL_SCHEDULER_MAX_TASKS)
    {
        return 0;
    }

    return &State->Tasks[State->CurrentTaskIndex];
}

const LOS_KERNEL_SCHEDULER_PROCESS *LosKernelSchedulerGetCurrentProcess(void)
{
    const LOS_KERNEL_SCHEDULER_STATE *State;

    State = LosKernelSchedulerGetState();
    if (State == 0 || State->CurrentProcessId == LOS_KERNEL_SCHEDULER_INVALID_PROCESS_ID)
    {
        return 0;
    }

    return FindProcessById(State->CurrentProcessId);
}

const LOS_KERNEL_SCHEDULER_STATE *LosKernelSchedulerGetState(void)
{
    return LosKernelSchedulerState();
}
