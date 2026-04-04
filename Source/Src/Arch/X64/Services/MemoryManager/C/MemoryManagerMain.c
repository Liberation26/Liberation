#include "MemoryManagerMain.h"

volatile UINT64 LosMemoryManagerServiceHeartbeat = 0ULL;
const char LosMemoryManagerServiceBanner[] = "Liberation Memory Manager Service";

void LosMemoryManagerServiceEntry(void)
{
    for (;;)
    {
        LosMemoryManagerServiceHeartbeat += 1ULL;
        __asm__ __volatile__("pause" : : : "memory");
    }
}
