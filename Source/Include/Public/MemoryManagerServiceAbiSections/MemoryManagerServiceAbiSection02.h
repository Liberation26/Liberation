/*
 * File Name: MemoryManagerServiceAbiSection02.h
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
    UINT32 Status;
    UINT32 Reserved;
    UINT64 AddressSpaceObjectPhysicalAddress;
    UINT64 PagesProcessed;
    UINT64 LastVirtualAddress;
    UINT64 AppliedPageFlags;
} LOS_MEMORY_MANAGER_PROTECT_PAGES_RESULT;

typedef struct
{
    UINT64 AddressSpaceObjectPhysicalAddress;
    UINT64 VirtualAddress;
} LOS_MEMORY_MANAGER_QUERY_MAPPING_REQUEST;

typedef struct
{
    UINT32 Status;
    UINT32 Reserved;
    UINT64 AddressSpaceObjectPhysicalAddress;
    UINT64 VirtualAddress;
    UINT64 PhysicalAddress;
    UINT64 PageFlags;
    UINT64 PageCount;
} LOS_MEMORY_MANAGER_QUERY_MAPPING_RESULT;

typedef struct
{
    UINT64 Flags;
    UINT64 Reserved0;
} LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_REQUEST;

typedef struct
{
    UINT32 Status;
    UINT32 Reserved;
    UINT64 AddressSpaceObjectPhysicalAddress;
    UINT64 RootTablePhysicalAddress;
    UINT64 AddressSpaceId;
} LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_RESULT;

typedef struct
{
    UINT64 AddressSpaceObjectPhysicalAddress;
    UINT64 Flags;
} LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_REQUEST;

typedef struct
{
    UINT32 Status;
    UINT32 Reserved;
    UINT64 AddressSpaceObjectPhysicalAddress;
    UINT64 ReleasedPageCount;
    UINT64 ReleasedVirtualRegionCount;
} LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_RESULT;

typedef struct
{
    UINT64 AddressSpaceObjectPhysicalAddress;
    UINT64 StagedImagePhysicalAddress;
    UINT64 StagedImageSize;
    UINT64 Flags;
} LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_REQUEST;

typedef struct
{
    UINT32 Status;
    UINT32 Reserved;
    UINT64 AddressSpaceObjectPhysicalAddress;
    UINT64 ImagePhysicalAddress;
    UINT64 ImageSize;
    UINT64 ImageVirtualBase;
    UINT64 EntryVirtualAddress;
    UINT64 ImagePageCount;
} LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT;

typedef struct
{
    UINT64 AddressSpaceObjectPhysicalAddress;
    UINT64 DesiredStackBaseVirtualAddress;
    UINT64 PageCount;
    UINT32 Flags;
    UINT32 Reserved;
} LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_REQUEST;

typedef struct
{
    UINT32 Status;
    UINT32 Reserved;
    UINT64 AddressSpaceObjectPhysicalAddress;
    UINT64 StackPhysicalAddress;
    UINT64 StackPageCount;
    UINT64 StackBaseVirtualAddress;
    UINT64 StackTopVirtualAddress;
} LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT;

typedef struct
{
    UINT64 Signature;
    UINT32 Version;
    UINT32 State;
    UINT64 Flags;
    UINT64 RootTablePhysicalAddress;
    UINT64 KernelRootTablePhysicalAddress;
    UINT64 DirectMapBase;
    UINT64 DirectMapSize;
    UINT64 ServiceImagePhysicalAddress;
    UINT64 ServiceImageSize;
    UINT64 ServiceImageVirtualBase;
    UINT64 EntryVirtualAddress;
    UINT64 StackPhysicalAddress;
    UINT64 StackPageCount;
    UINT64 StackBaseVirtualAddress;
    UINT64 StackTopVirtualAddress;
    UINT64 AddressSpaceId;
    UINT32 ReservedVirtualRegionCount;
    UINT32 Reserved0;
    LOS_MEMORY_MANAGER_RESERVED_VIRTUAL_REGION ReservedVirtualRegions[LOS_MEMORY_MANAGER_MAX_RESERVED_VIRTUAL_REGIONS];
} LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT;

typedef struct
{
    UINT64 Signature;
    UINT32 Version;
    UINT32 State;
    UINT64 Flags;
    UINT64 TaskId;
    UINT64 AddressSpaceObjectPhysicalAddress;
    UINT64 EntryVirtualAddress;
    UINT64 StackTopPhysicalAddress;
    UINT64 StackTopVirtualAddress;
    UINT64 LastRequestId;
    UINT64 Heartbeat;
} LOS_MEMORY_MANAGER_TASK_OBJECT;

typedef struct
{
    UINT32 Version;
    UINT32 Reserved;
    UINT64 LaunchBlockPhysicalAddress;
    UINT64 OfferedOperations;
    UINT64 BootstrapFlags;
    UINT64 KernelToServiceEndpointId;
    UINT64 ServiceToKernelEndpointId;
    UINT64 ServiceEventsEndpointId;
    UINT64 RequestMailboxPhysicalAddress;
    UINT64 ResponseMailboxPhysicalAddress;
    UINT64 EventMailboxPhysicalAddress;
    UINT64 MemoryRegionTablePhysicalAddress;
    UINT64 MemoryRegionCount;
    UINT64 MemoryRegionEntrySize;
    UINT64 ServicePageMapLevel4PhysicalAddress;
    UINT64 ServiceImagePhysicalAddress;
    UINT64 ServiceImageSize;
    UINT64 ServiceEntryVirtualAddress;
    UINT64 ServiceStackTopVirtualAddress;
} LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_REQUEST;

typedef struct
{
    UINT32 BootstrapResult;
    UINT32 BootstrapState;
    UINT64 NegotiatedOperations;
    UINT64 BootstrapFlags;
    UINT64 ActiveRootTablePhysicalAddress;
    UINT64 KernelRootTablePhysicalAddress;
    UINT64 ServiceHeartbeat;
    UINT64 LastProcessedRequestId;
    UINT64 TotalUsableBytes;
    UINT64 TotalBootstrapReservedBytes;
    UINT64 TotalFirmwareReservedBytes;
    UINT64 TotalRuntimeBytes;
    UINT64 TotalMmioBytes;
    UINT64 TotalAcpiBytes;
    UINT64 TotalUnusableBytes;
    UINT64 TotalPages;
    UINT64 FreePages;
    UINT64 ReservedPages;
    UINT64 RuntimePages;
    UINT64 MmioPages;
    UINT64 InternalDescriptorCount;
    UINT64 PageFrameDatabaseEntryCount;
    UINT64 HeapMetadataPages;
    UINT64 HeapReservedPages;
    UINT64 HeapSlabPageCapacity;
    UINT64 HeapLargeAllocationCapacity;
} LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_RESULT;

typedef struct
{
    UINT32 Operation;
    UINT32 Reserved0;
    UINT64 RequestId;
    UINT32 CallerPrincipalType;
    UINT32 Reserved1;
    UINT64 CallerPrincipalId;
    char CallerPrincipalName[LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH];
    union
    {
        LOS_MEMORY_MANAGER_BOOTSTRAP_ATTACH_REQUEST BootstrapAttach;
        struct
        {
            LOS_X64_MEMORY_REGION *Buffer;
            UINTN BufferRegionCapacity;
        } QueryMemoryRegions;
        LOS_MEMORY_MANAGER_ALLOCATE_FRAMES_REQUEST AllocateFrames;
        LOS_X64_FREE_FRAMES_REQUEST FreeFrames;
        LOS_MEMORY_MANAGER_MAP_PAGES_REQUEST MapPages;
        LOS_MEMORY_MANAGER_UNMAP_PAGES_REQUEST UnmapPages;
        LOS_MEMORY_MANAGER_PROTECT_PAGES_REQUEST ProtectPages;
        LOS_MEMORY_MANAGER_QUERY_MAPPING_REQUEST QueryMapping;
        LOS_X64_RESERVE_FRAMES_REQUEST ReserveFrames;
        LOS_X64_CLAIM_FRAMES_REQUEST ClaimFrames;
        LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_REQUEST CreateAddressSpace;
        LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_REQUEST DestroyAddressSpace;
        LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_REQUEST AttachStagedImage;
        LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_REQUEST AllocateAddressSpaceStack;
    } Payload;
} LOS_MEMORY_MANAGER_REQUEST_MESSAGE;
