/*
 * File Name: MemoryManagerLifecycle.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-07T12:35:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#include "MemoryManagerMainInternal.h"

static UINT64 ReadCurrentCs(void)
{
    UINT64 Value;

    __asm__ __volatile__("mov %%cs, %0" : "=r"(Value));
    return Value;
}

static UINT64 ReadCurrentRsp(void)
{
    UINT64 Value;

    __asm__ __volatile__("mov %%rsp, %0" : "=r"(Value));
    return Value;
}

static UINT64 ReadCurrentRflags(void)
{
    UINT64 Value;

    __asm__ __volatile__("pushfq\n\tpop %0" : "=r"(Value) : : "memory");
    return Value;
}

static void WriteCpl3ProofSignal(void)
{
    UINT64 CurrentCs;
    UINT64 CurrentRsp;
    UINT64 CurrentRflags;

    CurrentCs = ReadCurrentCs();
    CurrentRsp = ReadCurrentRsp();
    CurrentRflags = ReadCurrentRflags();

    LosMemoryManagerServiceSerialWriteLine("[MemManager] CPL3 proof: first user-mode service entry reached after iretq.");
    LosMemoryManagerServiceSerialWriteNamedHex("Current CS", CurrentCs);
    LosMemoryManagerServiceSerialWriteNamedUnsigned("Current CPL", CurrentCs & 0x3ULL);
    LosMemoryManagerServiceSerialWriteNamedHex("Current RSP", CurrentRsp);
    LosMemoryManagerServiceSerialWriteNamedHex("Current RFLAGS", CurrentRflags);
}

static LOS_MEMORY_MANAGER_TASK_OBJECT *TryTranslateTaskObjectForDiagnostics(
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock,
    UINT64 LaunchBlockAddress)
{
    UINT64 DirectMapOffset;

    if (LaunchBlock == 0)
    {
        return 0;
    }

    if (LaunchBlock->ServiceTaskObjectPhysicalAddress == 0ULL)
    {
        return 0;
    }

    if (LaunchBlock->LaunchBlockPhysicalAddress == 0ULL || LaunchBlockAddress < LaunchBlock->LaunchBlockPhysicalAddress)
    {
        return 0;
    }

    DirectMapOffset = LaunchBlockAddress - LaunchBlock->LaunchBlockPhysicalAddress;
    if (DirectMapOffset == 0ULL)
    {
        return 0;
    }

    return (LOS_MEMORY_MANAGER_TASK_OBJECT *)LosMemoryManagerTranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceTaskObjectPhysicalAddress);
}

BOOLEAN LosMemoryManagerServiceAttach(const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock)
{
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;
    LOS_MEMORY_MANAGER_TASK_OBJECT *DiagnosticTaskObject;
    UINT64 LaunchBlockAddress;
    UINT64 DirectMapOffset;
    UINT64 Detail;

    LaunchBlockAddress = (UINT64)(UINTN)LaunchBlock;
    Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE;
    if (!LosMemoryManagerValidateLaunchBlockDetailed(LaunchBlock, &Detail))
    {
        DiagnosticTaskObject = TryTranslateTaskObjectForDiagnostics(LaunchBlock, LaunchBlockAddress);
        LosMemoryManagerRecordAttachDiagnostic(DiagnosticTaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_LAUNCH_BLOCK, Detail);
        return 0;
    }

    DirectMapOffset = LosMemoryManagerResolveDirectMapOffset(LaunchBlock, LaunchBlockAddress);
    if (DirectMapOffset == 0ULL)
    {
        DiagnosticTaskObject = TryTranslateTaskObjectForDiagnostics(LaunchBlock, LaunchBlockAddress);
        LosMemoryManagerRecordAttachDiagnostic(DiagnosticTaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_DIRECT_MAP_OFFSET, LOS_MEMORY_MANAGER_ATTACH_DETAIL_LAUNCH_BLOCK_PHYSICAL);
        return 0;
    }

    State = LosMemoryManagerServiceState();
    LosMemoryManagerZeroMemory(State, sizeof(*State));
    State->LaunchBlock = (LOS_MEMORY_MANAGER_LAUNCH_BLOCK *)(UINTN)LaunchBlockAddress;
    State->DirectMapOffset = DirectMapOffset;
    State->ReceiveEndpoint = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)LosMemoryManagerTranslateBootstrapAddress(DirectMapOffset, LaunchBlock->KernelToServiceEndpointObjectPhysicalAddress);
    State->ReplyEndpoint = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)LosMemoryManagerTranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceToKernelEndpointObjectPhysicalAddress);
    State->EventEndpoint = (LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *)LosMemoryManagerTranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceEventsEndpointObjectPhysicalAddress);
    State->RequestMailbox = (LOS_MEMORY_MANAGER_REQUEST_MAILBOX *)LosMemoryManagerTranslateBootstrapAddress(DirectMapOffset, LaunchBlock->RequestMailboxPhysicalAddress);
    State->ResponseMailbox = (LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *)LosMemoryManagerTranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ResponseMailboxPhysicalAddress);
    State->EventMailbox = (LOS_MEMORY_MANAGER_EVENT_MAILBOX *)LosMemoryManagerTranslateBootstrapAddress(DirectMapOffset, LaunchBlock->EventMailboxPhysicalAddress);
    State->AddressSpaceObject = (LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *)LosMemoryManagerTranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress);
    State->TaskObject = (LOS_MEMORY_MANAGER_TASK_OBJECT *)LosMemoryManagerTranslateBootstrapAddress(DirectMapOffset, LaunchBlock->ServiceTaskObjectPhysicalAddress);
    if (State->TaskObject == 0)
    {
        return 0;
    }

    LosMemoryManagerRecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_NONE, LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE);

    if (!LosMemoryManagerValidateEndpointObjectDetailed(State->ReceiveEndpoint, LaunchBlock->Endpoints.KernelToService, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_RECEIVE, LaunchBlock->RequestMailboxPhysicalAddress, &Detail))
    {
        LosMemoryManagerRecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_RECEIVE_ENDPOINT, Detail);
        return 0;
    }

    if (!LosMemoryManagerValidateEndpointObjectDetailed(State->ReplyEndpoint, LaunchBlock->Endpoints.ServiceToKernel, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_REPLY, LaunchBlock->ResponseMailboxPhysicalAddress, &Detail))
    {
        LosMemoryManagerRecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_REPLY_ENDPOINT, Detail);
        return 0;
    }

    if (!LosMemoryManagerValidateEndpointObjectDetailed(State->EventEndpoint, LaunchBlock->Endpoints.ServiceEvents, LOS_MEMORY_MANAGER_ENDPOINT_ROLE_EVENT, LaunchBlock->EventMailboxPhysicalAddress, &Detail))
    {
        LosMemoryManagerRecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_EVENT_ENDPOINT, Detail);
        return 0;
    }

    if (!LosMemoryManagerValidateAddressSpaceObjectDetailed(State->AddressSpaceObject, LaunchBlock->ServiceImagePhysicalAddress, LaunchBlock->ServicePageMapLevel4PhysicalAddress, &Detail))
    {
        LosMemoryManagerRecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_ADDRESS_SPACE_OBJECT, Detail);
        return 0;
    }

    if (!LosMemoryManagerValidateTaskObjectDetailed(State->TaskObject, LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress, LaunchBlock->ServiceEntryVirtualAddress, LaunchBlock->ServiceStackTopPhysicalAddress, LaunchBlock->ServiceStackTopVirtualAddress, &Detail))
    {
        LosMemoryManagerRecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_TASK_OBJECT, Detail);
        return 0;
    }
    State->ActiveRootTablePhysicalAddress = LaunchBlock->ServicePageMapLevel4PhysicalAddress;
    State->KernelRootTablePhysicalAddress = State->AddressSpaceObject->KernelRootTablePhysicalAddress;
    if (!LosMemoryManagerServiceBuildMemoryView(State, &Detail))
    {
        LosMemoryManagerRecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_MEMORY_VIEW, Detail);
        return 0;
    }
    if (!LosMemoryManagerHeapInitialize(State, &Detail))
    {
        LosMemoryManagerRecordAttachDiagnostic(State->TaskObject, LOS_MEMORY_MANAGER_ATTACH_STAGE_HEAP, Detail);
        return 0;
    }
    State->ReceiveEndpoint->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->ReplyEndpoint->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->EventEndpoint->State = LOS_MEMORY_MANAGER_ENDPOINT_STATE_ONLINE;
    State->AddressSpaceObject->State = LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_ACTIVE;
    State->TaskObject->State = LOS_MEMORY_MANAGER_TASK_STATE_ONLINE;
    if (State->AddressSpaceObject->AddressSpaceId == 0ULL)
    {
        State->AddressSpaceObject->AddressSpaceId = 1ULL;
    }
    State->NextAddressSpaceId = State->AddressSpaceObject->AddressSpaceId + 1ULL;
    LosMemoryManagerServiceSerialWriteText("[MemManager] Bootstrap address space created id=");
    LosMemoryManagerServiceSerialWriteUnsigned(State->AddressSpaceObject->AddressSpaceId);
    LosMemoryManagerServiceSerialWriteText(" object=");
    LosMemoryManagerServiceSerialWriteHex64(LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress);
    LosMemoryManagerServiceSerialWriteText(" root=");
    LosMemoryManagerServiceSerialWriteHex64(State->AddressSpaceObject->RootTablePhysicalAddress);
    LosMemoryManagerServiceSerialWriteText("\n");
    State->NegotiatedOperations = 0ULL;
    State->AttachComplete = 0U;
    State->BootstrapResultCode = LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT_INVALID_REQUEST;
    State->Online = 1U;
    return 1;
}

void LosMemoryManagerRunServiceStep(void)
{
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;

    State = LosMemoryManagerServiceState();
    LosMemoryManagerServiceHeartbeat += 1ULL;
    State->Heartbeat = LosMemoryManagerServiceHeartbeat;
    if (State->TaskObject != 0)
    {
        if (State->LastRequestId == 0ULL)
        {
            State->TaskObject->LastRequestId = 0x1008ULL;
        }
        else
        {
            State->TaskObject->LastRequestId = State->LastRequestId;
        }
        State->TaskObject->Heartbeat = State->Heartbeat;
    }

    LosMemoryManagerServicePoll();
}

void LosMemoryManagerServiceBootstrapEntry(UINT64 LaunchBlockAddress)
{
    UINT64 RegisterLaunchBlockAddressRdi;
    UINT64 RegisterLaunchBlockAddressRsi;
    UINT64 RegisterLaunchBlockAddressRcx;
    UINT64 RegisterLaunchBlockAddressRdx;
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    LOS_MEMORY_MANAGER_SERVICE_STATE *State;

    __asm__ __volatile__("" : "=D"(RegisterLaunchBlockAddressRdi), "=S"(RegisterLaunchBlockAddressRsi), "=c"(RegisterLaunchBlockAddressRcx), "=d"(RegisterLaunchBlockAddressRdx));
    LosMemoryManagerRecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1001ULL, LaunchBlockAddress);
    if (LaunchBlockAddress == 0ULL)
    {
        if (RegisterLaunchBlockAddressRdi != 0ULL)
        {
            LaunchBlockAddress = RegisterLaunchBlockAddressRdi;
        }
        else if (RegisterLaunchBlockAddressRsi != 0ULL)
        {
            LaunchBlockAddress = RegisterLaunchBlockAddressRsi;
        }
        else if (RegisterLaunchBlockAddressRcx != 0ULL)
        {
            LaunchBlockAddress = RegisterLaunchBlockAddressRcx;
        }
        else if (RegisterLaunchBlockAddressRdx != 0ULL)
        {
            LaunchBlockAddress = RegisterLaunchBlockAddressRdx;
        }
    }

    LosMemoryManagerRecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1002ULL, RegisterLaunchBlockAddressRdi);
    LaunchBlock = (const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *)(UINTN)LaunchBlockAddress;
    LosMemoryManagerRecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1003ULL, RegisterLaunchBlockAddressRsi);
    State = LosMemoryManagerServiceState();
    LosMemoryManagerRecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1004ULL, RegisterLaunchBlockAddressRcx);

    LosMemoryManagerServiceSerialInit();
    if (State->Online == 0U)
    {
        LosMemoryManagerServiceSerialWriteLine("[MemManager] Memory-manager bootstrap entry reached.");
        WriteCpl3ProofSignal();
        LosMemoryManagerServiceSerialWriteNamedHex("Launch block", LaunchBlockAddress);
        LosMemoryManagerRecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1005ULL, RegisterLaunchBlockAddressRdx);
        if (!LosMemoryManagerServiceAttach(LaunchBlock))
        {
            LosMemoryManagerServiceSerialWriteLine("[MemManager] Memory-manager attach failed. Halting in service context.");
            LosMemoryManagerRecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x10FFULL, 0xFFFFFFFFFFFFFFFFULL);
            for (;; )
            {
                __asm__ __volatile__("pause" : : : "memory");
            }
        }
        LosMemoryManagerRecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1006ULL, LaunchBlock->ServiceEntryVirtualAddress);
        LosMemoryManagerServiceSerialWriteNamedHex("Service entry", LaunchBlock->ServiceEntryVirtualAddress);
        LosMemoryManagerServiceSerialWriteNamedHex("Service stack top virtual", LaunchBlock->ServiceStackTopVirtualAddress);
        LosMemoryManagerServiceSerialWriteNamedHex("Service root", LaunchBlock->ServicePageMapLevel4PhysicalAddress);
        LosMemoryManagerServiceSerialWriteNamedHex("Normalized region table", LaunchBlock->MemoryRegionTablePhysicalAddress);
        LosMemoryManagerServiceSerialWriteNamedUnsigned("Normalized region count", State->MemoryView.NormalizedRegionCount);
        LosMemoryManagerServiceSerialWriteNamedUnsigned("Memory-view descriptors", (UINT64)State->MemoryView.InternalDescriptorCount);
        LosMemoryManagerServiceSerialWriteNamedUnsigned("Page-frame DB entries", (UINT64)State->MemoryView.PageFrameDatabaseEntryCount);
        LosMemoryManagerServiceSerialWriteNamedHex("Total usable bytes", State->MemoryView.TotalUsableBytes);
        LosMemoryManagerServiceSerialWriteNamedHex("Bootstrap reserved bytes", State->MemoryView.TotalBootstrapReservedBytes);
        LosMemoryManagerServiceSerialWriteNamedHex("Firmware reserved bytes", State->MemoryView.TotalFirmwareReservedBytes);
        LosMemoryManagerServiceSerialWriteNamedHex("Runtime bytes", State->MemoryView.TotalRuntimeBytes);
        LosMemoryManagerServiceSerialWriteNamedHex("MMIO bytes", State->MemoryView.TotalMmioBytes);
        LosMemoryManagerServiceSerialWriteNamedHex("ACPI/NVS bytes", State->MemoryView.TotalAcpiBytes);
        LosMemoryManagerServiceSerialWriteNamedHex("Unusable bytes", State->MemoryView.TotalUnusableBytes);
        LosMemoryManagerServiceSerialWriteNamedHex("Total pages", State->MemoryView.TotalPages);
        LosMemoryManagerServiceSerialWriteNamedHex("Reserved pages", State->MemoryView.ReservedPages);
        LosMemoryManagerServiceSerialWriteNamedHex("Runtime pages", State->MemoryView.RuntimePages);
        LosMemoryManagerServiceSerialWriteNamedHex("MMIO pages", State->MemoryView.MmioPages);
        LosMemoryManagerServiceSerialWriteNamedHex("Free pages", State->MemoryView.FreePages);
        LosMemoryManagerServiceSerialWriteNamedUnsigned("Heap metadata pages", State->Heap.TotalReservedMetadataPages);
        LosMemoryManagerServiceSerialWriteNamedUnsigned("Heap reserved pages", State->Heap.TotalReservedHeapPages);
        LosMemoryManagerServiceSerialWriteNamedUnsigned("Heap slab descriptor capacity", (UINT64)State->Heap.SlabPageCapacity);
        LosMemoryManagerServiceSerialWriteNamedUnsigned("Heap large descriptor capacity", (UINT64)State->Heap.LargeAllocationCapacity);
        LosMemoryManagerServiceSerialWriteLine("[MemManager] Frame allocator ready.");
        LosMemoryManagerServiceSerialWriteLine("[MemManager] Heap subsystem ready.");
        LosMemoryManagerServiceSerialWriteLine("[MemManager] Internal heap ready.");
        LosMemoryManagerServiceSerialWriteLine("[MemManager] Memory-manager attach complete.");
        LosMemoryManagerPostEvent(LOS_MEMORY_MANAGER_EVENT_SERVICE_ONLINE, 0U, LaunchBlock->ServiceEntryVirtualAddress, LaunchBlock->ServiceStackTopPhysicalAddress);
    }

    LosMemoryManagerRecordEntryBreadcrumbFromLaunchBlock(LaunchBlockAddress, 0x1007ULL, LaunchBlock->ServiceStackTopVirtualAddress);
    LosMemoryManagerRunServiceStep();
}

void LosMemoryManagerServiceEntry(void)
{
    for (;; )
    {
        LosMemoryManagerRunServiceStep();
        __asm__ __volatile__("pause" : : : "memory");
    }
}
