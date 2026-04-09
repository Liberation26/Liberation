/*
 * File Name: MemoryManagerServiceAbiSection03.h
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MemoryManagerServiceAbi.h.
 */

typedef struct
{
    UINT32 Operation;
    UINT32 Status;
    UINT64 RequestId;
    union
    {
        LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT BootstrapAttach;
        LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_RESULT AllocateFrames;
        LOS_X64_FREE_FRAMES_RESULT FreeFrames;
        LOS_MEMORY_MANAGER_MAP_PAGES_RESULT MapPages;
        LOS_MEMORY_MANAGER_UNMAP_PAGES_RESULT UnmapPages;
        LOS_MEMORY_MANAGER_PROTECT_PAGES_RESULT ProtectPages;
        LOS_MEMORY_MANAGER_QUERY_MAPPING_RESULT QueryMapping;
        LOS_X64_QUERY_MEMORY_REGIONS_RESULT QueryMemoryRegions;
        LOS_X64_RESERVE_FRAMES_RESULT ReserveFrames;
        LOS_X64_CLAIM_FRAMES_RESULT ClaimFrames;
        LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_RESULT CreateAddressSpace;
        LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_RESULT DestroyAddressSpace;
        LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT AttachStagedImage;
        LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT AllocateAddressSpaceStack;
    } Payload;
} LOS_MEMORY_MANAGER_RESPONSE_MESSAGE;

typedef struct
{
    UINT32 EventType;
    UINT32 Status;
    UINT64 Sequence;
    UINT64 Value0;
    UINT64 Value1;
} LOS_MEMORY_MANAGER_EVENT_MESSAGE;

typedef struct
{
    UINT32 SlotState;
    UINT32 Reserved;
    UINT64 Sequence;
    LOS_MEMORY_MANAGER_REQUEST_MESSAGE Message;
} LOS_MEMORY_MANAGER_REQUEST_SLOT;

typedef struct
{
    UINT32 SlotState;
    UINT32 Reserved;
    UINT64 Sequence;
    LOS_MEMORY_MANAGER_RESPONSE_MESSAGE Message;
} LOS_MEMORY_MANAGER_RESPONSE_SLOT;

typedef struct
{
    UINT32 SlotState;
    UINT32 Reserved;
    UINT64 Sequence;
    LOS_MEMORY_MANAGER_EVENT_MESSAGE Message;
} LOS_MEMORY_MANAGER_EVENT_SLOT;

typedef struct
{
    UINT64 Signature;
    UINT32 Version;
    UINT32 Reserved;
    UINT64 EndpointId;
    UINT64 SlotCount;
    UINT64 ProduceIndex;
    UINT64 ConsumeIndex;
} LOS_MEMORY_MANAGER_MAILBOX_HEADER;

typedef struct
{
    LOS_MEMORY_MANAGER_MAILBOX_HEADER Header;
    LOS_MEMORY_MANAGER_REQUEST_SLOT Slots[LOS_MEMORY_MANAGER_MAILBOX_SLOT_COUNT];
} LOS_MEMORY_MANAGER_REQUEST_MAILBOX;

typedef struct
{
    LOS_MEMORY_MANAGER_MAILBOX_HEADER Header;
    LOS_MEMORY_MANAGER_RESPONSE_SLOT Slots[LOS_MEMORY_MANAGER_MAILBOX_SLOT_COUNT];
} LOS_MEMORY_MANAGER_RESPONSE_MAILBOX;

typedef struct
{
    LOS_MEMORY_MANAGER_MAILBOX_HEADER Header;
    LOS_MEMORY_MANAGER_EVENT_SLOT Slots[LOS_MEMORY_MANAGER_MAILBOX_SLOT_COUNT];
} LOS_MEMORY_MANAGER_EVENT_MAILBOX;

typedef struct
{
    UINT64 Signature;
    UINT32 Version;
    UINT32 State;
    UINT64 Flags;
    LOS_MEMORY_MANAGER_ENDPOINT_SET Endpoints;
    UINT64 KernelToServiceEndpointObjectPhysicalAddress;
    UINT64 ServiceToKernelEndpointObjectPhysicalAddress;
    UINT64 ServiceEventsEndpointObjectPhysicalAddress;
    UINT64 ServiceAddressSpaceObjectPhysicalAddress;
    UINT64 ServiceTaskObjectPhysicalAddress;
    UINT64 ServicePageMapLevel4PhysicalAddress;
    UINT64 MemoryRegionTablePhysicalAddress;
    UINT64 MemoryRegionCount;
    UINT64 MemoryRegionEntrySize;
    UINT64 SupportedOperations;
    UINT64 ServiceImagePhysicalAddress;
    UINT64 ServiceImageSize;
    UINT64 ServiceEntryVirtualAddress;
    UINT64 ServiceStackPhysicalAddress;
    UINT64 ServiceStackPageCount;
    UINT64 RequestMailboxPhysicalAddress;
    UINT64 RequestMailboxSize;
    UINT64 ResponseMailboxPhysicalAddress;
    UINT64 ResponseMailboxSize;
    UINT64 EventMailboxPhysicalAddress;
    UINT64 EventMailboxSize;
    UINT64 LaunchBlockPhysicalAddress;
    UINT64 LaunchBlockSize;
    CHAR16 ServicePath[LOS_MEMORY_MANAGER_SERVICE_PATH_CHARACTERS];
} LOS_MEMORY_MANAGER_BOOTSTRAP_INFO;

typedef struct
{
    UINT64 Signature;
    UINT32 Version;
    UINT32 State;
    UINT64 Flags;
    LOS_MEMORY_MANAGER_ENDPOINT_SET Endpoints;
    UINT64 KernelToServiceEndpointObjectPhysicalAddress;
    UINT64 ServiceToKernelEndpointObjectPhysicalAddress;
    UINT64 ServiceEventsEndpointObjectPhysicalAddress;
    UINT64 ServiceAddressSpaceObjectPhysicalAddress;
    UINT64 ServiceTaskObjectPhysicalAddress;
    UINT64 ServicePageMapLevel4PhysicalAddress;
    UINT64 MemoryRegionTablePhysicalAddress;
    UINT64 MemoryRegionCount;
    UINT64 MemoryRegionEntrySize;
    UINT64 RequestMailboxPhysicalAddress;
    UINT64 RequestMailboxSize;
    UINT64 ResponseMailboxPhysicalAddress;
    UINT64 ResponseMailboxSize;
    UINT64 EventMailboxPhysicalAddress;
    UINT64 EventMailboxSize;
    UINT64 LaunchBlockPhysicalAddress;
    UINT64 ServiceStackPhysicalAddress;
    UINT64 ServiceStackPageCount;
    UINT64 ServiceStackTopPhysicalAddress;
    UINT64 ServiceStackTopVirtualAddress;
    UINT64 ServiceImagePhysicalAddress;
    UINT64 ServiceImageSize;
    UINT64 ServiceEntryVirtualAddress;
    CHAR16 ServicePath[LOS_MEMORY_MANAGER_SERVICE_PATH_CHARACTERS];
} LOS_MEMORY_MANAGER_LAUNCH_BLOCK;

