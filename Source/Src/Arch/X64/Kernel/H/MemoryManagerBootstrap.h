#ifndef LOS_MEMORY_MANAGER_BOOTSTRAP_H
#define LOS_MEMORY_MANAGER_BOOTSTRAP_H

#include "KernelMain.h"
#include "VirtualMemory.h"

#include "MemoryManagerServiceAbi.h"

void LosInitializeMemoryManagerBootstrap(const LOS_BOOT_CONTEXT *BootContext);
void LosLaunchMemoryManagerBootstrap(void);
void LosDescribeMemoryManagerBootstrap(void);
BOOLEAN LosIsMemoryManagerBootstrapReady(void);
const LOS_MEMORY_MANAGER_BOOTSTRAP_INFO *LosGetMemoryManagerBootstrapInfo(void);
const LOS_MEMORY_MANAGER_LAUNCH_BLOCK *LosGetMemoryManagerLaunchBlock(void);
void LosMemoryManagerSendQueryMemoryRegions(LOS_X64_MEMORY_REGION *Buffer, UINTN BufferRegionCapacity, LOS_X64_QUERY_MEMORY_REGIONS_RESULT *Result);
void LosMemoryManagerSendReserveFrames(const LOS_X64_RESERVE_FRAMES_REQUEST *Request, LOS_X64_RESERVE_FRAMES_RESULT *Result);
void LosMemoryManagerSendClaimFrames(const LOS_X64_CLAIM_FRAMES_REQUEST *Request, LOS_X64_CLAIM_FRAMES_RESULT *Result);
void LosMemoryManagerSendMapPages(const LOS_X64_MAP_PAGES_REQUEST *Request, LOS_X64_MAP_PAGES_RESULT *Result);
void LosMemoryManagerSendUnmapPages(const LOS_X64_UNMAP_PAGES_REQUEST *Request, LOS_X64_UNMAP_PAGES_RESULT *Result);

#endif
