#ifndef LOS_MEMORY_MANAGER_BOOTSTRAP_INTERNAL_H
#define LOS_MEMORY_MANAGER_BOOTSTRAP_INTERNAL_H

#include "MemoryManagerBootstrap.h"

extern const UINT8 LosMemoryManagerServiceImageStart[];
extern const UINT8 LosMemoryManagerServiceImageEnd[];


typedef struct
{
    LOS_MEMORY_MANAGER_BOOTSTRAP_INFO Info;
    LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LaunchBlock;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *KernelToServiceEndpointObject;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *ServiceToKernelEndpointObject;
    LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *ServiceEventsEndpointObject;
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *ServiceAddressSpaceObject;
    LOS_MEMORY_MANAGER_TASK_OBJECT *ServiceTaskObject;
    UINT64 NextRequestId;
    UINT64 MessagesSent;
    UINT64 MessagesCompleted;
    UINT64 LastOperation;
    UINT64 LastStatus;
    UINT64 RequestMailboxVirtualAddress;
    UINT64 ResponseMailboxVirtualAddress;
    UINT64 EventMailboxVirtualAddress;
    UINT64 ServiceImageVirtualAddress;
} LOS_MEMORY_MANAGER_BOOTSTRAP_STATE;

LOS_MEMORY_MANAGER_BOOTSTRAP_STATE *LosMemoryManagerBootstrapState(void);
void LosMemoryManagerBootstrapReset(const LOS_BOOT_CONTEXT *BootContext);
void LosMemoryManagerBootstrapUpdateState(UINT32 State);
void LosMemoryManagerBootstrapSetFlag(UINT64 Flag);
UINT64 LosMemoryManagerBootstrapAllocateRequestId(void);
void LosMemoryManagerBootstrapRecordRequest(UINT32 Operation);
void LosMemoryManagerBootstrapRecordCompletion(UINT32 Operation, UINT32 Status);
BOOLEAN LosMemoryManagerBootstrapOperationSupported(UINT32 Operation);
void LosMemoryManagerBootstrapDispatch(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response);
void LosMemoryManagerBootstrapRunProbe(void);
void LosMemoryManagerBootstrapDescribeState(void);
BOOLEAN LosMemoryManagerBootstrapValidateServiceImage(void);
BOOLEAN LosMemoryManagerBootstrapStageTransport(void);
LOS_MEMORY_MANAGER_REQUEST_MAILBOX *LosMemoryManagerBootstrapGetRequestMailbox(void);
LOS_MEMORY_MANAGER_RESPONSE_MAILBOX *LosMemoryManagerBootstrapGetResponseMailbox(void);
LOS_MEMORY_MANAGER_EVENT_MAILBOX *LosMemoryManagerBootstrapGetEventMailbox(void);
void LosMemoryManagerBootstrapInitializeMailboxes(void);
BOOLEAN LosMemoryManagerBootstrapEnqueueRequest(const LOS_MEMORY_MANAGER_REQUEST_MESSAGE *Request);
BOOLEAN LosMemoryManagerBootstrapDequeueResponse(UINT64 RequestId, LOS_MEMORY_MANAGER_RESPONSE_MESSAGE *Response);
void LosMemoryManagerBootstrapPublishLaunchReady(void);
LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *LosMemoryManagerBootstrapGetKernelToServiceEndpointObject(void);
LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *LosMemoryManagerBootstrapGetServiceToKernelEndpointObject(void);
LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *LosMemoryManagerBootstrapGetServiceEventsEndpointObject(void);
BOOLEAN LosMemoryManagerBootstrapEndpointsReady(void);
void LosMemoryManagerBootstrapSetEndpointState(LOS_MEMORY_MANAGER_ENDPOINT_OBJECT *Endpoint, UINT32 State);
BOOLEAN LosMemoryManagerBootstrapHostedServiceStep(void);
BOOLEAN LosMemoryManagerBootstrapEnsureServiceEntryReady(void);
void LosMemoryManagerBootstrapTransferToServiceTask(UINT64 ServiceRootPhysicalAddress, UINT64 EntryVirtualAddress, UINT64 StackTopVirtualAddress, UINT64 LaunchBlockAddress);
BOOLEAN LosMemoryManagerBootstrapInvokeServiceEntry(void);
void LosMemoryManagerBootstrapReportFailureAndHalt(const char *Reason);
void LosMemoryManagerBootstrapReportFailureValueAndHalt(const char *Reason, const char *Prefix, UINT64 Value);

#endif
