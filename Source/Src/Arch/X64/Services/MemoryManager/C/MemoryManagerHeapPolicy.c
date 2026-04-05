#include "MemoryManagerHeapInternal.h"

static const UINT32 LosMemoryManagerHeapDefaultSlabSizes[LOS_MEMORY_MANAGER_HEAP_MAX_SLAB_CLASSES] =
{
    64U,
    128U,
    256U,
    512U,
    1024U,
    2048U
};

UINT64 LosMemoryManagerHeapAlignUpValue(UINT64 Value, UINT64 Alignment)
{
    if (Alignment == 0ULL)
    {
        return Value;
    }

    return (Value + (Alignment - 1ULL)) & ~(Alignment - 1ULL);
}

BOOLEAN LosMemoryManagerHeapIsPowerOfTwo(UINT64 Value)
{
    return Value != 0ULL && (Value & (Value - 1ULL)) == 0ULL;
}

BOOLEAN LosMemoryManagerHeapSelectSlabClass(UINT64 ByteCount, UINT64 AlignmentBytes, UINTN *ClassIndex)
{
    UINTN Index;
    UINT64 EffectiveAlignment;
    UINT64 RequiredBytes;

    if (ClassIndex != 0)
    {
        *ClassIndex = 0U;
    }
    if (ClassIndex == 0 || ByteCount == 0ULL)
    {
        return 0;
    }

    EffectiveAlignment = AlignmentBytes == 0ULL ? 8ULL : AlignmentBytes;
    if (!LosMemoryManagerHeapIsPowerOfTwo(EffectiveAlignment))
    {
        return 0;
    }

    RequiredBytes = ByteCount > EffectiveAlignment ? ByteCount : EffectiveAlignment;
    for (Index = 0U; Index < LOS_MEMORY_MANAGER_HEAP_MAX_SLAB_CLASSES; ++Index)
    {
        if ((UINT64)LosMemoryManagerHeapDefaultSlabSizes[Index] >= RequiredBytes)
        {
            *ClassIndex = Index;
            return 1;
        }
    }

    return 0;
}
