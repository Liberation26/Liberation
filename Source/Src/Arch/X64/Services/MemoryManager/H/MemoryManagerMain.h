#ifndef LOS_MEMORY_MANAGER_SERVICE_MAIN_H
#define LOS_MEMORY_MANAGER_SERVICE_MAIN_H

#include "Efi.h"
#include "MemoryManagerServiceAbi.h"

typedef struct
{
    const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *ReceiveEndpoint;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *ReplyEndpoint;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *EventEndpoint;
    LOS_MEMORY_MANAGER_REQUEST_MAILBOX *RequestMailbox;
    LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *ResponseMailbox;
    LOS_MEMORY_MANAGER_EVENT_MAILBOX *EventMailbox;
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject;
    LOS_MEMORY_MANAGER_TASK_OBJECT *TaskObject;
    UINT64 Heartbeat;
    UINT64 LastRequestId;
    UINT64 ActiveRootTablePhysicalAddress;
    UINT64 KernelRootTablePhysicalAddress;
    UINT32 Online;
} LOS_MEMORY_MANAGER_SERVICE_STATE;

void LosMemoryManagerServiceEntry(void);
void LosMemoryManagerServiceBootstrapEntry(UINT64 LaunchBlockAddress);
BOOLEAN LosMemoryManagerServiceAttach(const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock);
void LosMemoryManagerServicePoll(void);
LOS_MEMORY_MANAGER_SERVICE_STATE *LosMemoryManagerServiceState(void);

#endif
