#include "MemoryManagerAddressSpaceInternal.h"

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

static UINT64 *TranslatePageTable(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 PhysicalAddress)
{
    return (UINT64 *)LosMemoryManagerTranslatePhysical(State, PhysicalAddress, 0x1000ULL);
}

void *LosMemoryManagerTranslatePhysical(LOS_MEMORY_MANAGER_SERVICE_STATE *State, UINT64 PhysicalAddress, UINT64 Length)
{
    UINT64 EndAddress;

    if (State == 0 || State->DirectMapOffset == 0ULL)
    {
        return 0;
    }
    if (Length == 0ULL)
    {
        return 0;
    }

    EndAddress = PhysicalAddress + Length;
    if (EndAddress < PhysicalAddress)
    {
        return 0;
    }

    return (void *)(UINTN)(State->DirectMapOffset + PhysicalAddress);
}

BOOLEAN LosMemoryManagerResolveAddressSpaceObject(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 AddressSpaceObjectPhysicalAddress,
    BOOLEAN AllowBootstrapObject,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT **AddressSpaceObject)
{
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *Resolved;

    if (AddressSpaceObject != 0)
    {
        *AddressSpaceObject = 0;
    }
    if (State == 0 || AddressSpaceObject == 0 || AddressSpaceObjectPhysicalAddress == 0ULL)
    {
        return 0;
    }

    Resolved = (LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *)LosMemoryManagerTranslatePhysical(
        State,
        AddressSpaceObjectPhysicalAddress,
        sizeof(LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT));
    if (Resolved == 0)
    {
        return 0;
    }
    if (Resolved->Signature != LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE ||
        Resolved->Version != LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION ||
        Resolved->State < LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_READY ||
        Resolved->RootTablePhysicalAddress == 0ULL ||
        Resolved->KernelRootTablePhysicalAddress == 0ULL)
    {
        return 0;
    }
    if (Resolved->KernelRootTablePhysicalAddress != State->KernelRootTablePhysicalAddress)
    {
        LosMemoryManagerHardFail("address-space-root-mismatch", AddressSpaceObjectPhysicalAddress, Resolved->RootTablePhysicalAddress, Resolved->KernelRootTablePhysicalAddress);
    }
    if ((Resolved->Flags & LOS_MEMORY_MANAGER_ENDPOINT_FLAG_BOOTSTRAP_OBJECT) != 0ULL &&
        Resolved->RootTablePhysicalAddress != State->ActiveRootTablePhysicalAddress)
    {
        LosMemoryManagerHardFail("address-space-root-mismatch", AddressSpaceObjectPhysicalAddress, Resolved->RootTablePhysicalAddress, State->ActiveRootTablePhysicalAddress);
    }
    if (TranslatePageTable(State, Resolved->RootTablePhysicalAddress) == 0)
    {
        LosMemoryManagerHardFail("address-space-root-mismatch", AddressSpaceObjectPhysicalAddress, Resolved->RootTablePhysicalAddress, 0ULL);
    }
    if (!AllowBootstrapObject && (Resolved->Flags & LOS_MEMORY_MANAGER_ENDPOINT_FLAG_BOOTSTRAP_OBJECT) != 0ULL)
    {
        return 0;
    }

    *AddressSpaceObject = Resolved;
    return 1;
}

BOOLEAN LosMemoryManagerCreateAddressSpaceObject(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 *AddressSpaceObjectPhysicalAddress,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT **AddressSpaceObject)
{
    UINT64 ObjectPhysicalAddress;
    UINT64 RootPhysicalAddress;
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *NewAddressSpaceObject;
    UINT64 *NewRoot;
    UINT64 *KernelRoot;
    UINTN EntryIndex;

    if (AddressSpaceObjectPhysicalAddress != 0)
    {
        *AddressSpaceObjectPhysicalAddress = 0ULL;
    }
    if (AddressSpaceObject != 0)
    {
        *AddressSpaceObject = 0;
    }
    if (State == 0 || AddressSpaceObjectPhysicalAddress == 0 || AddressSpaceObject == 0)
    {
        return 0;
    }

    ObjectPhysicalAddress = 0ULL;
    RootPhysicalAddress = 0ULL;
    if (!LosMemoryManagerHeapAllocate(
            State,
            sizeof(LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT),
            sizeof(UINT64),
            LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_ADDRESS_SPACE_OBJECT,
            (void **)&NewAddressSpaceObject,
            &ObjectPhysicalAddress))
    {
        return 0;
    }

    if (!LosMemoryManagerHeapAllocatePages(
            State,
            1ULL,
            0x1000ULL,
            LOS_MEMORY_MANAGER_PAGE_FRAME_USAGE_PAGE_TABLE,
            (void **)&NewRoot,
            &RootPhysicalAddress))
    {
        ZeroBytes(NewAddressSpaceObject, sizeof(*NewAddressSpaceObject));
        (void)LosMemoryManagerHeapFree(State, ObjectPhysicalAddress, 0);
        return 0;
    }

    if (NewAddressSpaceObject == 0)
    {
        (void)LosMemoryManagerHeapFree(State, RootPhysicalAddress, 0);
        return 0;
    }

    KernelRoot = TranslatePageTable(State, State->KernelRootTablePhysicalAddress);
    if (NewRoot == 0 || KernelRoot == 0)
    {
        ZeroBytes(NewAddressSpaceObject, sizeof(*NewAddressSpaceObject));
        (void)LosMemoryManagerHeapFree(State, RootPhysicalAddress, 0);
        (void)LosMemoryManagerHeapFree(State, ObjectPhysicalAddress, 0);
        return 0;
    }

    for (EntryIndex = 0U; EntryIndex < 512U; ++EntryIndex)
    {
        NewRoot[EntryIndex] = 0ULL;
    }
    for (EntryIndex = 256U; EntryIndex < 512U; ++EntryIndex)
    {
        NewRoot[EntryIndex] = KernelRoot[EntryIndex];
    }

    ZeroBytes(NewAddressSpaceObject, sizeof(*NewAddressSpaceObject));
    NewAddressSpaceObject->Signature = LOS_MEMORY_MANAGER_BOOTSTRAP_SIGNATURE;
    NewAddressSpaceObject->Version = LOS_MEMORY_MANAGER_BOOTSTRAP_VERSION;
    NewAddressSpaceObject->State = LOS_MEMORY_MANAGER_ADDRESS_SPACE_STATE_READY;
    NewAddressSpaceObject->Flags = LOS_MEMORY_MANAGER_ENDPOINT_FLAG_SERVICE_VISIBLE;
    NewAddressSpaceObject->RootTablePhysicalAddress = RootPhysicalAddress;
    NewAddressSpaceObject->KernelRootTablePhysicalAddress = State->KernelRootTablePhysicalAddress;
    NewAddressSpaceObject->DirectMapBase = (State->AddressSpaceObject != 0) ? State->AddressSpaceObject->DirectMapBase : State->DirectMapOffset;
    NewAddressSpaceObject->DirectMapSize = (State->AddressSpaceObject != 0) ? State->AddressSpaceObject->DirectMapSize : 0ULL;
    NewAddressSpaceObject->AddressSpaceId = State->NextAddressSpaceId++;
    NewAddressSpaceObject->ReservedVirtualRegionCount = 0U;

    *AddressSpaceObjectPhysicalAddress = ObjectPhysicalAddress;
    *AddressSpaceObject = NewAddressSpaceObject;
    return 1;
}

static BOOLEAN ReleaseLowerHalfPageTableBranch(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    UINT64 TablePhysicalAddress,
    UINT32 Level,
    UINT64 *ReleasedPageCount)
{
    UINT64 *Table;
    UINTN EntryIndex;

    if (State == 0 || TablePhysicalAddress == 0ULL || Level == 0U)
    {
        return 0;
    }

    Table = TranslatePageTable(State, TablePhysicalAddress);
    if (Table == 0)
    {
        return 0;
    }

    for (EntryIndex = 0U; EntryIndex < 512U; ++EntryIndex)
    {
        UINT64 Entry;
        UINT64 ChildPhysicalAddress;

        Entry = Table[EntryIndex];
        if ((Entry & LOS_X64_PAGE_PRESENT) == 0ULL)
        {
            continue;
        }

        ChildPhysicalAddress = Entry & LOS_X64_PAGE_TABLE_ADDRESS_MASK;
        if (Level > 1U && (Entry & LOS_X64_PAGE_LARGE) == 0ULL)
        {
            if (!ReleaseLowerHalfPageTableBranch(State, ChildPhysicalAddress, Level - 1U, ReleasedPageCount))
            {
                return 0;
            }
            {
                UINT64 ReleasedFromHeap;

                ReleasedFromHeap = 0ULL;
                if (!LosMemoryManagerHeapFree(State, ChildPhysicalAddress, &ReleasedFromHeap))
                {
                    return 0;
                }
                if (ReleasedPageCount != 0)
                {
                    *ReleasedPageCount += ReleasedFromHeap;
                }
            }
        }

        Table[EntryIndex] = 0ULL;
    }

    return 1;
}

BOOLEAN LosMemoryManagerDestroyAddressSpaceMappings(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    LOS_MEMORY_MANAGER_ADDRESS_SPACE_OBJECT *AddressSpaceObject,
    UINT64 *ReleasedPageCount)
{
    UINT64 *RootTable;
    UINTN EntryIndex;

    if (ReleasedPageCount != 0)
    {
        *ReleasedPageCount = 0ULL;
    }
    if (State == 0 || AddressSpaceObject == 0 || AddressSpaceObject->RootTablePhysicalAddress == 0ULL)
    {
        return 0;
    }

    RootTable = TranslatePageTable(State, AddressSpaceObject->RootTablePhysicalAddress);
    if (RootTable == 0)
    {
        return 0;
    }

    for (EntryIndex = 0U; EntryIndex < 256U; ++EntryIndex)
    {
        UINT64 Entry;
        UINT64 ChildPhysicalAddress;

        Entry = RootTable[EntryIndex];
        if ((Entry & LOS_X64_PAGE_PRESENT) == 0ULL)
        {
            continue;
        }

        ChildPhysicalAddress = Entry & LOS_X64_PAGE_TABLE_ADDRESS_MASK;
        if ((Entry & LOS_X64_PAGE_LARGE) == 0ULL)
        {
            if (!ReleaseLowerHalfPageTableBranch(State, ChildPhysicalAddress, 3U, ReleasedPageCount))
            {
                return 0;
            }
            {
                UINT64 ReleasedFromHeap;

                ReleasedFromHeap = 0ULL;
                if (!LosMemoryManagerHeapFree(State, ChildPhysicalAddress, &ReleasedFromHeap))
                {
                    return 0;
                }
                if (ReleasedPageCount != 0)
                {
                    *ReleasedPageCount += ReleasedFromHeap;
                }
            }
        }

        RootTable[EntryIndex] = 0ULL;
    }

    {
        UINT64 ReleasedFromHeap;

        ReleasedFromHeap = 0ULL;
        if (!LosMemoryManagerHeapFree(State, AddressSpaceObject->RootTablePhysicalAddress, &ReleasedFromHeap))
        {
            return 0;
        }
        if (ReleasedPageCount != 0)
        {
            *ReleasedPageCount += ReleasedFromHeap;
        }
    }

    return 1;
}
