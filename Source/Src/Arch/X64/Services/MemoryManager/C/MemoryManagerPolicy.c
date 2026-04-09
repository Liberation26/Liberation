/*
 * File Name: MemoryManagerPolicy.c
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#include "MemoryManagerMainInternal.h"

BOOLEAN LosMemoryManagerValidateEndpointObjectDetailed(
    const LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *Endpoint,
    UINT64 EndpointId,
    UINT32 Role,
    UINT64 MailboxPhysicalAddress,
    UINT64 *Detail)
{
    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE;
    }

    if (Endpoint == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NULL;
        }
        return 0;
    }

    if (Endpoint->Signature != LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SIGNATURE;
        }
        return 0;
    }

    if (Endpoint->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_VERSION;
        }
        return 0;
    }

    if (Endpoint->EndpointId != EndpointId)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ENDPOINT_ID;
        }
        return 0;
    }

    if (Endpoint->Role != Role)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ROLE;
        }
        return 0;
    }

    if (Endpoint->MailboxPhysicalAddress != MailboxPhysicalAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_MAILBOX_PHYSICAL;
        }
        return 0;
    }

    if ((Endpoint->Flags & LOS_MEMORY_MANAGER_ENDPOINT_FLAG_MAILBOX_ATTACHED) == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_MAILBOX_FLAG;
        }
        return 0;
    }

    return 1;
}

BOOLEAN LosMemoryManagerValidateAddressSpaceObjectDetailed(
    const LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 ServiceImagePhysicalAddress,
    UINT64 ServiceRootTablePhysicalAddress,
    UINT64 *Detail)
{
    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE;
    }

    if (AddressSpaceObject == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NULL;
        }
        return 0;
    }

    if (AddressSpaceObject->Signature != LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SIGNATURE;
        }
        return 0;
    }

    if (AddressSpaceObject->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_VERSION;
        }
        return 0;
    }

    if (AddressSpaceObject->State < LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_READY)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_STATE;
        }
        return 0;
    }

    if (AddressSpaceObject->ServiceImagePhysicalAddress != ServiceImagePhysicalAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SERVICE_IMAGE_PHYSICAL;
        }
        return 0;
    }

    if (AddressSpaceObject->RootTablePhysicalAddress != ServiceRootTablePhysicalAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ROOT_TABLE_PHYSICAL;
        }
        return 0;
    }

    if (AddressSpaceObject->KernelRootTablePhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_KERNEL_ROOT_PHYSICAL;
        }
        return 0;
    }

    return 1;
}

BOOLEAN LosMemoryManagerValidateTaskObjectDetailed(
    const LOS_MEMORY_MANAGER_TASK_OBJECT *TaskObject,
    UINT64 AddressSpaceObjectPhysicalAddress,
    UINT64 EntryVirtualAddress,
    UINT64 StackTopPhysicalAddress,
    UINT64 StackTopVirtualAddress,
    UINT64 *Detail)
{
    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE;
    }

    if (TaskObject == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NULL;
        }
        return 0;
    }

    if (TaskObject->Signature != LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SIGNATURE;
        }
        return 0;
    }

    if (TaskObject->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_VERSION;
        }
        return 0;
    }

    if (TaskObject->AddressSpaceObjectPhysicalAddress != AddressSpaceObjectPhysicalAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ADDRESS_SPACE_OBJECT_PHYSICAL;
        }
        return 0;
    }

    if (TaskObject->EntryVirtualAddress != EntryVirtualAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ENTRY_VIRTUAL;
        }
        return 0;
    }

    if (TaskObject->StackTopPhysicalAddress != StackTopPhysicalAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_STACK_TOP_PHYSICAL;
        }
        return 0;
    }

    if (TaskObject->StackTopVirtualAddress != StackTopVirtualAddress)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_STACK_TOP_VIRTUAL;
        }
        return 0;
    }

    return 1;
}

BOOLEAN LosMemoryManagerValidateLaunchBlockDetailed(const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock, UINT64 *Detail)
{
    if (Detail != 0)
    {
        *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NONE;
    }

    if (LaunchBlock == 0)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_NULL;
        }
        return 0;
    }

    if (LaunchBlock->Signature != LOS_MEMORY_MANAGER_LAUNCH_BLOCK_SIGNATURE)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SIGNATURE;
        }
        return 0;
    }

    if (LaunchBlock->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_VERSION;
        }
        return 0;
    }

    if (LaunchBlock->RequestMailboxPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_REQUEST_MAILBOX_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->ResponseMailboxPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_RESPONSE_MAILBOX_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->EventMailboxPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_EVENT_MAILBOX_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->LaunchBlockPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_LAUNCH_BLOCK_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->KernelToServiceEndpointObjectPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_RECEIVE_ENDPOINT_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->ServiceToKernelEndpointObjectPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_REPLY_ENDPOINT_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->ServiceEventsEndpointObjectPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_EVENT_ENDPOINT_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->ServiceAddressSpaceObjectPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ADDRESS_SPACE_OBJECT_PHYSICAL_POINTER;
        }
        return 0;
    }

    if (LaunchBlock->ServiceTaskObjectPhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_TASK_OBJECT_PHYSICAL_POINTER;
        }
        return 0;
    }

    if (LaunchBlock->ServicePageMapLevel4PhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_SERVICE_ROOT_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->MemoryRegionTablePhysicalAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_MEMORY_REGION_TABLE_PHYSICAL;
        }
        return 0;
    }

    if (LaunchBlock->MemoryRegionCount == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_MEMORY_REGION_COUNT;
        }
        return 0;
    }

    if (LaunchBlock->MemoryRegionEntrySize != (UINT64)sizeof(LOS_X64_MEMORY_REGION))
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_MEMORY_REGION_ENTRY_SIZE;
        }
        return 0;
    }

    if (LaunchBlock->ServiceEntryVirtualAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_ENTRY_VIRTUAL;
        }
        return 0;
    }

    if (LaunchBlock->ServiceStackTopVirtualAddress == 0ULL)
    {
        if (Detail != 0)
        {
            *Detail = LOS_MEMORY_MANAGER_ATTACH_DETAIL_STACK_TOP_VIRTUAL;
        }
        return 0;
    }

    return 1;
}
