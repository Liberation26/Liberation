/*
 * File Name: ShellRuntimeSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from ShellRuntime.c.
 */

static UINT64 LosShellRuntimeAlignDown64(UINT64 Value, UINT64 Alignment)
{
    if (Alignment == 0ULL)
    {
        return Value;
    }
    return Value & ~(Alignment - 1ULL);
}

static UINT64 LosShellRuntimeAlignUp64(UINT64 Value, UINT64 Alignment)
{
    if (Alignment == 0ULL)
    {
        return Value;
    }
    return (Value + Alignment - 1ULL) & ~(Alignment - 1ULL);
}

static UINT64 LosShellRuntimeMaterializeLoadedImage(const void *Image,
                                                    UINTN ImageSize,
                                                    UINT64 *LoadedBaseAddress,
                                                    UINT64 *LoadedSize,
                                                    UINT64 *LoadedEntryAddress)
{
    const LOS_USER_IMAGE_ELF64_HEADER *Header;
    const LOS_USER_IMAGE_ELF64_PROGRAM_HEADER *ProgramHeader;
    UINT16 ProgramIndex;
    UINT64 LowestVirtualAddress = ~0ULL;
    UINT64 HighestVirtualAddress = 0ULL;
    UINT64 ImageBase;
    UINT64 ImageSpan;

    if (Image == 0 || ImageSize < sizeof(LOS_USER_IMAGE_ELF64_HEADER) ||
        LoadedBaseAddress == 0 || LoadedSize == 0 || LoadedEntryAddress == 0)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }

    Header = (const LOS_USER_IMAGE_ELF64_HEADER *)Image;
    if (Header->ProgramHeaderOffset >= (UINT64)ImageSize ||
        Header->ProgramHeaderEntrySize < sizeof(LOS_USER_IMAGE_ELF64_PROGRAM_HEADER))
    {
        return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
    }

    for (ProgramIndex = 0U; ProgramIndex < Header->ProgramHeaderCount; ++ProgramIndex)
    {
        UINT64 ProgramOffset = Header->ProgramHeaderOffset +
                               ((UINT64)ProgramIndex * (UINT64)Header->ProgramHeaderEntrySize);

        if (ProgramOffset + sizeof(LOS_USER_IMAGE_ELF64_PROGRAM_HEADER) > (UINT64)ImageSize)
        {
            return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
        }

        ProgramHeader = (const LOS_USER_IMAGE_ELF64_PROGRAM_HEADER *)((const UINT8 *)Image + ProgramOffset);
        if (ProgramHeader->Type != LOS_ELF_PROGRAM_TYPE_LOAD)
        {
            continue;
        }
        if (ProgramHeader->MemorySize < ProgramHeader->FileSize)
        {
            return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
        }
        if (ProgramHeader->Offset + ProgramHeader->FileSize > (UINT64)ImageSize)
        {
            return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
        }

        if (ProgramHeader->VirtualAddress < LowestVirtualAddress)
        {
            LowestVirtualAddress = ProgramHeader->VirtualAddress;
        }
        if (ProgramHeader->VirtualAddress + ProgramHeader->MemorySize > HighestVirtualAddress)
        {
            HighestVirtualAddress = ProgramHeader->VirtualAddress + ProgramHeader->MemorySize;
        }
    }

    if (LowestVirtualAddress == ~0ULL || HighestVirtualAddress <= LowestVirtualAddress)
    {
        return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
    }

    ImageBase = LosShellRuntimeAlignDown64(LowestVirtualAddress, 0x1000ULL);
    ImageSpan = LosShellRuntimeAlignUp64(HighestVirtualAddress - ImageBase, 0x1000ULL);
    if (ImageSpan > (UINT64)sizeof(LosShellRuntimeMappedImageBuffer))
    {
        return LOS_USER_IMAGE_CALL_STATUS_TRUNCATED;
    }

    LosShellRuntimeZeroMemory(LosShellRuntimeMappedImageBuffer, sizeof(LosShellRuntimeMappedImageBuffer));

    for (ProgramIndex = 0U; ProgramIndex < Header->ProgramHeaderCount; ++ProgramIndex)
    {
        UINT64 ProgramOffset = Header->ProgramHeaderOffset +
                               ((UINT64)ProgramIndex * (UINT64)Header->ProgramHeaderEntrySize);
        UINT64 DestinationOffset;
        UINT64 ByteIndex;

        ProgramHeader = (const LOS_USER_IMAGE_ELF64_PROGRAM_HEADER *)((const UINT8 *)Image + ProgramOffset);
        if (ProgramHeader->Type != LOS_ELF_PROGRAM_TYPE_LOAD)
        {
            continue;
        }

        DestinationOffset = ProgramHeader->VirtualAddress - ImageBase;
        if (DestinationOffset + ProgramHeader->MemorySize > (UINT64)sizeof(LosShellRuntimeMappedImageBuffer))
        {
            return LOS_USER_IMAGE_CALL_STATUS_TRUNCATED;
        }

        for (ByteIndex = 0ULL; ByteIndex < ProgramHeader->FileSize; ++ByteIndex)
        {
            LosShellRuntimeMappedImageBuffer[DestinationOffset + ByteIndex] =
                ((const UINT8 *)Image)[ProgramHeader->Offset + ByteIndex];
        }

        for (ByteIndex = ProgramHeader->FileSize; ByteIndex < ProgramHeader->MemorySize; ++ByteIndex)
        {
            LosShellRuntimeMappedImageBuffer[DestinationOffset + ByteIndex] = 0U;
        }
    }

    if (Header->Entry < ImageBase || Header->Entry >= ImageBase + ImageSpan)
    {
        return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
    }

    *LoadedBaseAddress = (UINT64)(UINTN)LosShellRuntimeMappedImageBuffer;
    *LoadedSize = ImageSpan;
    *LoadedEntryAddress = ((UINT64)(UINTN)LosShellRuntimeMappedImageBuffer) + (Header->Entry - ImageBase);
    return LOS_USER_IMAGE_CALL_STATUS_SUCCESS;
}

static UINT64 LosShellRuntimeInvokeLoadedEntry(UINT64 EntryAddress,
                                               const LOS_USER_IMAGE_CALL *Call,
                                               UINT64 StackAddress,
                                               UINT64 StackSize)
{
    typedef UINT64 (*LOS_SHELL_RUNTIME_ENTRY_POINT)(const LOS_USER_IMAGE_CALL *Call);
    LOS_SHELL_RUNTIME_ENTRY_POINT Entry;
    UINT64 Result = LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
    UINT64 StackTop;

    if (EntryAddress == 0ULL || Call == 0 || StackAddress == 0ULL || StackSize < 256ULL)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }

    StackTop = LosShellRuntimeAlignDown64(StackAddress + StackSize, 16ULL);
    Entry = (LOS_SHELL_RUNTIME_ENTRY_POINT)(UINTN)EntryAddress;

#if defined(__x86_64__)
    __asm__ __volatile__(
        "mov %%rsp, %%r11\n"
        "mov %2, %%rsp\n"
        "call *%3\n"
        "mov %%r11, %%rsp\n"
        : "=a"(Result)
        : "D"(Call), "r"(StackTop), "r"(Entry)
        : "r11", "memory");
#else
    Result = Entry(Call);
#endif

    return Result;
}


UINT64 LosShellEnterUserMode(const LOS_USER_IMAGE_RING3_CONTEXT *Context)
{
    if (Context == 0 || Context->Version != LOS_USER_IMAGE_CALL_VERSION)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }
    if (Context->UserInstructionPointer == 0ULL ||
        Context->UserStackPointer == 0ULL ||
        Context->UserCallArgument == 0ULL ||
        Context->PageMapLevel4PhysicalAddress == 0ULL ||
        Context->UserCodeSelector == 0ULL ||
        Context->UserDataSelector == 0ULL)
    {
        return LOS_USER_IMAGE_CALL_STATUS_TRANSITION_FAILED;
    }
    if ((Context->UserStackPointer & 0xFULL) != 0ULL)
    {
        return LOS_USER_IMAGE_CALL_STATUS_TRANSITION_FAILED;
    }

    LosShellRuntimeWriteDebugUnsigned("[shell] ring3 cr3=", Context->PageMapLevel4PhysicalAddress, "\n");
    LosShellRuntimeWriteDebugUnsigned("[shell] ring3 rip=", Context->UserInstructionPointer, "\n");
    LosShellRuntimeWriteDebugUnsigned("[shell] ring3 rsp=", Context->UserStackPointer, "\n");
    LosShellRuntimeWriteDebugUnsigned("[shell] ring3 arg=", Context->UserCallArgument, "\n");

#if defined(__x86_64__)
    return LosShellEnterUserModeAsm(Context);
#else
    return LOS_USER_IMAGE_CALL_STATUS_TRANSITION_FAILED;
#endif
}
static BOOLEAN LosShellRuntimeValidateElfImage(const void *Image, UINTN ImageSize, UINT64 *EntryAddress)
{
    const LOS_USER_IMAGE_ELF64_HEADER *Header;

    if (Image == 0 || ImageSize < sizeof(LOS_USER_IMAGE_ELF64_HEADER))
    {
        return 0;
    }

    Header = (const LOS_USER_IMAGE_ELF64_HEADER *)Image;
    if (Header->Ident[0] != LOS_ELF_MAGIC_0 ||
        Header->Ident[1] != LOS_ELF_MAGIC_1 ||
        Header->Ident[2] != LOS_ELF_MAGIC_2 ||
        Header->Ident[3] != LOS_ELF_MAGIC_3 ||
        Header->Ident[4] != LOS_ELF_CLASS_64 ||
        Header->Ident[5] != LOS_ELF_DATA_LITTLE_ENDIAN ||
        Header->Machine != LOS_ELF_MACHINE_X86_64 ||
        Header->Type != LOS_ELF_TYPE_EXEC ||
        Header->HeaderSize < sizeof(LOS_USER_IMAGE_ELF64_HEADER) ||
        Header->Entry == 0ULL)
    {
        return 0;
    }

    if (EntryAddress != 0)
    {
        *EntryAddress = Header->Entry;
    }
    return 1;
}

static UINT64 __attribute__((unused)) LosShellRuntimeDispatchBootstrapImage(const LOS_USER_IMAGE_CALL *Call)
{
    (void)Call;
    return LOS_USER_IMAGE_CALL_STATUS_NOT_FOUND;
}

static UINT64 __attribute__((unused)) LosShellRuntimeTryDiskBackedImage(const LOS_USER_IMAGE_CALL *Call)
{
    UINTN BytesRead = 0U;
    UINT64 EntryAddress = 0ULL;
    LOS_USER_IMAGE_EXECUTION_CONTEXT Context;
    UINT64 ReadStatus;
    UINT64 ExecuteStatus;

    if (Call == 0 ||
        Call->Version != LOS_USER_IMAGE_CALL_VERSION ||
        Call->Signature != LOS_USER_IMAGE_CALL_SIGNATURE ||
        Call->Path[0] == 0)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }

    LosShellRuntimeWriteDebugText("[shell] reading user image: ");
    LosShellRuntimeWriteDebugText(Call->Path);
    LosShellRuntimeWriteDebugText("\n");
    LosShellRuntimeZeroMemory(LosShellRuntimeImageBuffer, sizeof(LosShellRuntimeImageBuffer));
    ReadStatus = LosUserReadImageFile(Call->Path,
                                      LosShellRuntimeImageBuffer,
                                      sizeof(LosShellRuntimeImageBuffer),
                                      &BytesRead);
    LosShellRuntimeWriteDebugUnsigned("[shell] user image read status=", ReadStatus, "\n");
    LosShellRuntimeWriteDebugUnsigned("[shell] user image bytes=", (UINT64)BytesRead, "\n");
    if (ReadStatus == LOS_USER_IMAGE_FILE_STATUS_NOT_FOUND)
    {
        return LOS_USER_IMAGE_CALL_STATUS_NOT_FOUND;
    }
    if (ReadStatus == LOS_USER_IMAGE_FILE_STATUS_BUFFER_TOO_SMALL)
    {
        return LOS_USER_IMAGE_CALL_STATUS_TRUNCATED;
    }
    if (ReadStatus != LOS_USER_IMAGE_FILE_STATUS_SUCCESS)
    {
        return LOS_USER_IMAGE_CALL_STATUS_UNSUPPORTED;
    }
    if (!LosShellRuntimeValidateElfImage(LosShellRuntimeImageBuffer, BytesRead, &EntryAddress))
    {
        return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
    }

    LosShellRuntimeWriteDebugUnsigned("[shell] validated entry=", EntryAddress, "\n");
    LosShellRuntimeZeroMemory(&Context, sizeof(Context));
    Context.Version = LOS_USER_IMAGE_CALL_VERSION;
    Context.ExecutionMode = LOS_USER_IMAGE_EXECUTION_MODE_DISK;
    Context.CallAddress = (UINT64)(UINTN)Call;
    Context.ImageAddress = (UINT64)(UINTN)LosShellRuntimeImageBuffer;
    Context.ImageSize = (UINT64)BytesRead;
    Context.EntryAddress = EntryAddress;
    Context.StackAddress = (UINT64)(UINTN)LosShellRuntimeUserStack;
    Context.StackSize = (UINT64)sizeof(LosShellRuntimeUserStack);

    LosShellRuntimeZeroMemory(LosShellRuntimeUserStack, sizeof(LosShellRuntimeUserStack));
    LosShellRuntimeWriteDebugText("[shell] dispatching loaded user image\n");
    ExecuteStatus = LosUserExecuteLoadedImage(&Context);
    LosShellRuntimeWriteDebugUnsigned("[shell] loaded user image status=", ExecuteStatus, "\n");
    return ExecuteStatus;
}



static void LosShellRuntimeStageCompletion(UINT64 Status, UINT64 ResultValue)
{
    LosShellRuntimeCompletionRecord.Version = LOS_USER_IMAGE_CALL_VERSION;
    LosShellRuntimeCompletionRecord.Flags = 0U;
    LosShellRuntimeCompletionRecord.Status = Status;
    LosShellRuntimeCompletionRecord.ResultValue = ResultValue;
    LosShellRuntimeUserModeCompletionStatus = Status;
    LosShellRuntimeUserModeCompletionResult = ResultValue;
    LosShellRuntimeCallResultShadow = ResultValue;
}

void LosShellStageUserModeCompletion(UINT64 Status, UINT64 ResultValue)
{
    UINT64 CompletionStatus = Status;
    UINT64 CompletionResult = ResultValue;

    if (LosShellRuntimeLastIsolatedSpace.Version == LOS_USER_IMAGE_CALL_VERSION &&
        LosShellRuntimeLastIsolatedSpace.AddressSpaceId != 0ULL)
    {
        UINT64 FinalizeStatus = LosMemoryManagerCompleteUserAddressSpaceCall(&LosShellRuntimeLastIsolatedSpace,
                                                                             &CompletionStatus,
                                                                             &CompletionResult);
        if (FinalizeStatus != LOS_USER_IMAGE_CALL_STATUS_SUCCESS)
        {
            CompletionStatus = FinalizeStatus;
            CompletionResult = 0ULL;
        }

        LosShellRuntimeZeroMemory(&LosShellRuntimeLastIsolatedSpace, sizeof(LosShellRuntimeLastIsolatedSpace));
        LosShellRuntimeZeroMemory(&LosShellRuntimeLastRing3Context, sizeof(LosShellRuntimeLastRing3Context));
    }

    LosShellRuntimeStageCompletion(CompletionStatus, CompletionResult);
}
