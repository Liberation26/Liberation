/*
 * File Name: ShellRuntime.c
 * File Version: 0.4.35
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-08T15:00:00Z
 * Last Update Timestamp: 2026-04-09T18:15:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#include "ShellMain.h"
#include "LoginMain.h"
#include "StringMain.h"

extern const UINT8 LosInstalledLoginImageStart[];
extern const UINT8 LosInstalledLoginImageEnd[];
extern const UINT8 LosInstalledStringImageStart[];
extern const UINT8 LosInstalledStringImageEnd[];

typedef struct
{
    const char *Path;
    const UINT8 *Start;
    const UINT8 *End;
} LOS_SHELL_RUNTIME_INSTALLED_USER_IMAGE;

static const LOS_SHELL_RUNTIME_INSTALLED_USER_IMAGE LosShellRuntimeInstalledUserImages[] =
{
    { "\\LIBERATION\\COMMANDS\\LOGIN.ELF", LosInstalledLoginImageStart, LosInstalledLoginImageEnd },
    { "\\LIBERATION\\LIBRARIES\\STRING.ELF", LosInstalledStringImageStart, LosInstalledStringImageEnd }
};


static LOS_SHELL_SERVICE_STATE LosShellGlobalState;

static BOOLEAN LosShellRuntimeTextEqual(const char *Left, const char *Right)
{
    UINTN Index;
    if (Left == 0 || Right == 0)
    {
        return 0;
    }
    for (Index = 0U;; ++Index)
    {
        if (Left[Index] != Right[Index])
        {
            return 0;
        }
        if (Left[Index] == 0)
        {
            return 1;
        }
    }
}

static UINTN LosShellRuntimeTextLength(const char *Text)
{
    UINTN Length = 0U;
    if (Text == 0)
    {
        return 0U;
    }
    while (Text[Length] != 0)
    {
        ++Length;
    }
    return Length;
}

static const char *LosShellRuntimeSkipSpaces(const char *Text)
{
    if (Text == 0)
    {
        return 0;
    }
    while (*Text == ' ' || *Text == '	')
    {
        ++Text;
    }
    return Text;
}

static void LosShellRuntimeNextToken(const char *Text, char *Token, UINTN TokenLength, const char **Remainder)
{
    UINTN Index = 0U;
    Text = LosShellRuntimeSkipSpaces(Text);
    if (Token != 0 && TokenLength != 0U)
    {
        Token[0] = 0;
    }
    if (Text == 0)
    {
        if (Remainder != 0)
        {
            *Remainder = 0;
        }
        return;
    }
    while (Text[Index] != 0 && Text[Index] != ' ' && Text[Index] != '	')
    {
        if (Token != 0 && Index + 1U < TokenLength)
        {
            Token[Index] = Text[Index];
        }
        ++Index;
    }
    if (Token != 0 && TokenLength != 0U)
    {
        if (Index < TokenLength)
        {
            Token[Index] = 0;
        }
        else
        {
            Token[TokenLength - 1U] = 0;
        }
    }
    if (Remainder != 0)
    {
        *Remainder = LosShellRuntimeSkipSpaces(Text + Index);
    }
}

static BOOLEAN LosShellRuntimeTextEndsWith(const char *Text, const char *Suffix)
{
    UINTN TextLength;
    UINTN SuffixLength;
    if (Text == 0 || Suffix == 0)
    {
        return 0;
    }
    TextLength = LosShellRuntimeTextLength(Text);
    SuffixLength = LosShellRuntimeTextLength(Suffix);
    if (SuffixLength > TextLength)
    {
        return 0;
    }
    return LosShellRuntimeTextEqual(Text + (TextLength - SuffixLength), Suffix);
}


static const LOS_SHELL_RUNTIME_INSTALLED_USER_IMAGE *LosShellRuntimeFindInstalledUserImage(const char *Path)
{
    UINTN Index;
    if (Path == 0)
    {
        return 0;
    }
    for (Index = 0U; Index < (sizeof(LosShellRuntimeInstalledUserImages) / sizeof(LosShellRuntimeInstalledUserImages[0])); ++Index)
    {
        if (LosShellRuntimeTextEqual(Path, LosShellRuntimeInstalledUserImages[Index].Path))
        {
            return &LosShellRuntimeInstalledUserImages[Index];
        }
    }
    return 0;
}

__attribute__((weak)) UINT32 LosCapabilitiesServiceSubmitAccessRequest(const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                                                LOS_CAPABILITIES_ACCESS_RESULT *Result);
__attribute__((weak)) UINT32 LosCapabilitiesServiceCheckAccess(const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                                      LOS_CAPABILITIES_ACCESS_RESULT *Result);
__attribute__((weak)) void LosUserWriteText(const char *Text) { (void)Text; }
__attribute__((weak)) UINT64 LosUserReadImageFile(const char *Path, void *Buffer, UINTN BufferLength, UINTN *BytesRead)
{
    const LOS_SHELL_RUNTIME_INSTALLED_USER_IMAGE *InstalledImage;
    UINTN RequiredLength;
    UINTN Index;

    if (BytesRead != 0)
    {
        *BytesRead = 0U;
    }
    if (Path == 0 || Buffer == 0)
    {
        return LOS_USER_IMAGE_FILE_STATUS_INVALID_PARAMETER;
    }

    InstalledImage = LosShellRuntimeFindInstalledUserImage(Path);
    if (InstalledImage == 0)
    {
        return LOS_USER_IMAGE_FILE_STATUS_NOT_FOUND;
    }

    RequiredLength = (UINTN)(InstalledImage->End - InstalledImage->Start);
    if (BufferLength < RequiredLength)
    {
        return LOS_USER_IMAGE_FILE_STATUS_BUFFER_TOO_SMALL;
    }

    for (Index = 0U; Index < RequiredLength; ++Index)
    {
        ((UINT8 *)Buffer)[Index] = InstalledImage->Start[Index];
    }

    if (BytesRead != 0)
    {
        *BytesRead = RequiredLength;
    }
    return LOS_USER_IMAGE_FILE_STATUS_SUCCESS;
}
__attribute__((weak)) UINT64 LosUserExecuteLoadedImage(const LOS_USER_IMAGE_EXECUTION_CONTEXT *Context)
{
    UINT64 LoadedBaseAddress = 0ULL;
    UINT64 LoadedSize = 0ULL;
    UINT64 LoadedEntryAddress = 0ULL;
    UINT64 Status;

    if (Context == 0 ||
        Context->Version != LOS_USER_IMAGE_CALL_VERSION ||
        Context->ImageAddress == 0ULL ||
        Context->ImageSize == 0ULL ||
        Context->CallAddress == 0ULL)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }

    Status = LosUserExecuteIsolatedImage(Context,
                                         &LosShellRuntimeLastIsolatedSpace,
                                         &LosShellRuntimeLastRing3Context);
    if (Status == LOS_USER_IMAGE_CALL_STATUS_SUCCESS)
    {
        LosShellRuntimeWriteDebugUnsigned("[shell] isolated address space id=", LosShellRuntimeLastIsolatedSpace.AddressSpaceId, "
");
        LosShellRuntimeWriteDebugUnsigned("[shell] isolated root table=", LosShellRuntimeLastIsolatedSpace.RootTablePhysicalAddress, "
");
        LosShellRuntimeWriteDebugUnsigned("[shell] ring3 entry=", LosShellRuntimeLastRing3Context.UserInstructionPointer, "
");
        return LosShellEnterUserMode(&LosShellRuntimeLastRing3Context);
    }

    Status = LosShellRuntimeMaterializeLoadedImage((const void *)(UINTN)Context->ImageAddress,
                                                   (UINTN)Context->ImageSize,
                                                   &LoadedBaseAddress,
                                                   &LoadedSize,
                                                   &LoadedEntryAddress);
    if (Status != LOS_USER_IMAGE_CALL_STATUS_SUCCESS)
    {
        return Status;
    }

    LosShellRuntimeWriteDebugUnsigned("[shell] mapped image base=", LoadedBaseAddress, "
");
    LosShellRuntimeWriteDebugUnsigned("[shell] mapped image size=", LoadedSize, "
");
    LosShellRuntimeWriteDebugUnsigned("[shell] executing ELF entry=", LoadedEntryAddress, "
");
    Status = LosShellRuntimeInvokeLoadedEntry(LoadedEntryAddress,
                                              (const LOS_USER_IMAGE_CALL *)(UINTN)Context->CallAddress,
                                              Context->StackAddress,
                                              Context->StackSize);
    LosShellRuntimeWriteDebugUnsigned("[shell] returned from ELF status=", Status, "
");
    return Status;
}

__attribute__((weak)) UINT64 LosUserExecuteIsolatedImage(const LOS_USER_IMAGE_EXECUTION_CONTEXT *Context,
                                                         LOS_USER_IMAGE_ISOLATED_SPACE *IsolatedSpace,
                                                         LOS_USER_IMAGE_RING3_CONTEXT *Ring3Context)
{
    (void)Context;
    if (IsolatedSpace != 0)
    {
        LosShellRuntimeZeroMemory(IsolatedSpace, sizeof(*IsolatedSpace));
    }
    if (Ring3Context != 0)
    {
        LosShellRuntimeZeroMemory(Ring3Context, sizeof(*Ring3Context));
    }
    return LOS_USER_IMAGE_CALL_STATUS_ISOLATION_UNAVAILABLE;
}
__attribute__((weak)) void LosUserWriteUnsigned(UINT64 Value) { (void)Value; }

static UINT8 LosShellRuntimeImageBuffer[LOS_USER_IMAGE_MAX_STAGED_IMAGE_SIZE];
static UINT8 LosShellRuntimeMappedImageBuffer[LOS_USER_IMAGE_MAX_STAGED_IMAGE_SIZE];
static UINT8 LosShellRuntimeUserStack[16U * 1024U];
static LOS_USER_IMAGE_ISOLATED_SPACE LosShellRuntimeLastIsolatedSpace;
static LOS_USER_IMAGE_RING3_CONTEXT LosShellRuntimeLastRing3Context;
static volatile UINT64 LosShellRuntimeUserModeCompletionStatus = LOS_USER_IMAGE_CALL_STATUS_UNSUPPORTED;
static volatile UINT64 LosShellRuntimeUserModeCompletionResult = 0ULL;
static LOS_USER_IMAGE_COMPLETION_RECORD LosShellRuntimeCompletionRecord;
static UINT64 LosShellRuntimeCallResultShadow = 0ULL;

static void LosShellRuntimeWriteDebugText(const char *Text)
{
    if (Text != 0)
    {
        LosUserWriteText(Text);
    }
}

static void LosShellRuntimeWriteDebugUnsigned(const char *Prefix, UINT64 Value, const char *Suffix)
{
    if (Prefix != 0)
    {
        LosUserWriteText(Prefix);
    }
    LosUserWriteUnsigned(Value);
    if (Suffix != 0)
    {
        LosUserWriteText(Suffix);
    }
}

static void LosShellRuntimeZeroMemory(void *Buffer, UINTN Length)
{
    UINTN Index;
    if (Buffer == 0)
    {
        return;
    }
    for (Index = 0U; Index < Length; ++Index)
    {
        ((UINT8 *)Buffer)[Index] = 0U;
    }
}


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

    LosShellRuntimeWriteDebugUnsigned("[shell] ring3 cr3=", Context->PageMapLevel4PhysicalAddress, "
");
    LosShellRuntimeWriteDebugUnsigned("[shell] ring3 rip=", Context->UserInstructionPointer, "
");
    LosShellRuntimeWriteDebugUnsigned("[shell] ring3 rsp=", Context->UserStackPointer, "
");
    LosShellRuntimeWriteDebugUnsigned("[shell] ring3 arg=", Context->UserCallArgument, "
");

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

static UINT64 LosShellRuntimeDispatchBootstrapImage(const LOS_USER_IMAGE_CALL *Call)
{
    (void)Call;
    return LOS_USER_IMAGE_CALL_STATUS_NOT_FOUND;
}

static UINT64 LosShellRuntimeTryDiskBackedImage(const LOS_USER_IMAGE_CALL *Call)
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


UINT64 LosShellRuntimeInvokeUserImageCall(LOS_USER_IMAGE_CALL *Call)
{
    UINT64 Status;

    if (Call == 0)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }

    LosShellRuntimeLastCall = *Call;
    LosShellRuntimeStageCompletion(LOS_USER_IMAGE_CALL_STATUS_TRANSITION_FAILED, 0ULL);
    Status = LosUserLoadAndCallImage(Call);
    if (Status != LOS_USER_IMAGE_CALL_STATUS_SUCCESS)
    {
        LosShellRuntimeStageCompletion(Status, 0ULL);
    }
    LosShellRuntimeCommitCompletionToShell(Call);
    return LosShellRuntimeCompletionRecord.Status;
}

__attribute__((weak)) UINT64 LosUserLoadAndCallImage(const LOS_USER_IMAGE_CALL *Call)
{
    if (Call == 0)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }
    return LosShellRuntimeTryDiskBackedImage(Call);
}

static void LosShellRuntimeInitializeImageCall(LOS_USER_IMAGE_CALL *Call,
                                               UINT32 CallKind,
                                               const char *Path,
                                               const void *Request,
                                               UINTN RequestSize,
                                               void *Response,
                                               UINTN ResponseSize,
                                               void *Result,
                                               UINTN ResultSize)
{
    if (Call == 0)
    {
        return;
    }
    LosShellRuntimeZeroMemory(Call, sizeof(*Call));
    Call->Version = LOS_USER_IMAGE_CALL_VERSION;
    Call->CallKind = CallKind;
    Call->Signature = LOS_USER_IMAGE_CALL_SIGNATURE;
    Call->RequestAddress = (UINT64)(UINTN)Request;
    Call->RequestSize = (UINT64)RequestSize;
    Call->ResponseAddress = (UINT64)(UINTN)Response;
    Call->ResponseSize = (UINT64)ResponseSize;
    Call->ResultAddress = (UINT64)(UINTN)Result;
    Call->ResultSize = (UINT64)ResultSize;
    LosShellServiceCopyText(Call->Path, sizeof(Call->Path), Path);
}

__attribute__((weak)) UINT64 LosUserInvokeStringLibrary(const char *LibraryPath,
                                                        const LOS_STRING_LIBRARY_REQUEST *Request,
                                                        LOS_STRING_LIBRARY_RESPONSE *Response)
{
    LOS_USER_IMAGE_CALL Call;
    UINT64 Status;

    if (LibraryPath == 0 || Request == 0 || Response == 0)
    {
        return LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    }

    LosShellRuntimeInitializeImageCall(&Call,
                                       LOS_USER_IMAGE_CALL_KIND_LIBRARY,
                                       LibraryPath,
                                       Request,
                                       sizeof(*Request),
                                       Response,
                                       sizeof(*Response),
                                       0,
                                       0U);
    Status = LosShellRuntimeInvokeUserImageCall(&Call);
    switch (Status)
    {
    case LOS_USER_IMAGE_CALL_STATUS_SUCCESS:
        return LOS_STRING_LIBRARY_STATUS_SUCCESS;
    case LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER:
        return LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    case LOS_USER_IMAGE_CALL_STATUS_TRUNCATED:
        return LOS_STRING_LIBRARY_STATUS_TRUNCATED;
    default:
        return LOS_STRING_LIBRARY_STATUS_UNSUPPORTED;
    }
}

__attribute__((weak)) UINT64 LosUserLaunchExternalCommand(const char *Path,
                                                          const char *Arguments,
                                                          UINT64 *CommandResult,
                                                          char *Output,
                                                          UINTN OutputLength)
{
    LOS_USER_IMAGE_CALL Call;
    UINT64 Status;

    if (CommandResult != 0)
    {
        *CommandResult = LOS_LOGIN_COMMAND_RESULT_NONE;
    }
    if (Output != 0 && OutputLength != 0U)
    {
        Output[0] = 0;
    }
    if (Path == 0)
    {
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    }

    LosShellRuntimeInitializeImageCall(&Call,
                                       LOS_USER_IMAGE_CALL_KIND_COMMAND,
                                       Path,
                                       Arguments,
                                       Arguments != 0 ? (LosShellRuntimeTextLength(Arguments) + 1U) : 0U,
                                       Output,
                                       OutputLength,
                                       CommandResult,
                                       CommandResult != 0 ? sizeof(*CommandResult) : 0U);
    Status = LosShellRuntimeInvokeUserImageCall(&Call);
    switch (Status)
    {
    case LOS_USER_IMAGE_CALL_STATUS_SUCCESS:
        return LOS_SHELL_SERVICE_STATUS_SUCCESS;
    case LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER:
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    case LOS_USER_IMAGE_CALL_STATUS_ACCESS_DENIED:
        return LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED;
    case LOS_USER_IMAGE_CALL_STATUS_TRUNCATED:
        return LOS_SHELL_SERVICE_STATUS_TRUNCATED;
    case LOS_USER_IMAGE_CALL_STATUS_NOT_FOUND:
        return LOS_SHELL_SERVICE_STATUS_NOT_FOUND;
    case LOS_USER_IMAGE_CALL_STATUS_UNSUPPORTED:
        return LOS_SHELL_SERVICE_STATUS_UNSUPPORTED;
    default:
        return LOS_SHELL_SERVICE_STATUS_LAUNCH_FAILED;
    }
}

LOS_SHELL_SERVICE_STATE *LosShellServiceState(void) { return &LosShellGlobalState; }
void LosShellServiceWriteText(const char *Text) { if (Text != 0) { LosUserWriteText(Text); } }
void LosShellServiceWriteUnsigned(UINT64 Value) { LosUserWriteUnsigned(Value); }
void LosShellServiceWriteLine(const char *Text) { LosShellServiceWriteText(Text); LosShellServiceWriteText("\n"); }
void LosShellServiceYield(void) { __asm__ __volatile__("pause"); }

void LosShellServiceCopyText(char *Destination, UINTN DestinationLength, const char *Source)
{
    UINTN Index;
    if (Destination == 0 || DestinationLength == 0U) { return; }
    for (Index = 0U; Index < DestinationLength; ++Index) { Destination[Index] = 0; }
    if (Source == 0) { return; }
    for (Index = 0U; Index + 1U < DestinationLength && Source[Index] != 0; ++Index) { Destination[Index] = Source[Index]; }
}

void LosShellServiceAppendText(char *Destination, UINTN DestinationLength, const char *Source)
{
    UINTN Offset = 0U;
    UINTN Index;
    if (Destination == 0 || DestinationLength == 0U || Source == 0) { return; }
    while (Offset + 1U < DestinationLength && Destination[Offset] != 0) { ++Offset; }
    for (Index = 0U; Offset + 1U < DestinationLength && Source[Index] != 0; ++Index, ++Offset) { Destination[Offset] = Source[Index]; }
    Destination[Offset < DestinationLength ? Offset : (DestinationLength - 1U)] = 0;
}


static char LosShellServiceUppercaseCharacter(char Character)
{
    if (Character >= 'a' && Character <= 'z')
    {
        return (char)(Character - ('a' - 'A'));
    }
    return Character;
}

static UINT64 LosShellServiceUppercaseBootstrapAdapter(const LOS_STRING_LIBRARY_REQUEST *Request,
                                                       LOS_STRING_LIBRARY_RESPONSE *Response)
{
    UINTN Index;
    BOOLEAN Changed = 0;

    if (Request == 0 || Response == 0)
    {
        return LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    }

    for (Index = 0U; Index < sizeof(*Response); ++Index)
    {
        ((UINT8 *)Response)[Index] = 0U;
    }
    Response->Version = LOS_STRING_LIBRARY_VERSION;
    Response->Status = LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    Response->Signature = LOS_STRING_LIBRARY_RESPONSE_SIGNATURE;

    if (Request->Version != LOS_STRING_LIBRARY_VERSION ||
        Request->Signature != LOS_STRING_LIBRARY_REQUEST_SIGNATURE ||
        Request->Operation != LOS_STRING_LIBRARY_OPERATION_UPPERCASE)
    {
        return Response->Status;
    }

    for (Index = 0U; Index + 1U < sizeof(Response->Output) && Request->Input[Index] != 0; ++Index)
    {
        Response->Output[Index] = LosShellServiceUppercaseCharacter(Request->Input[Index]);
        if (Response->Output[Index] != Request->Input[Index])
        {
            Changed = 1;
        }
    }

    Response->Status = LOS_STRING_LIBRARY_STATUS_SUCCESS;
    Response->Result = Changed ? LOS_STRING_LIBRARY_RESULT_CHANGED : LOS_STRING_LIBRARY_RESULT_UNCHANGED;
    Response->Flags = LOS_STRING_LIBRARY_FLAG_BOOTSTRAP_ADAPTER;
    if (Changed)
    {
        Response->Flags |= LOS_STRING_LIBRARY_FLAG_TEXT_CHANGED;
    }
    return Response->Status;
}

UINT64 LosShellServiceUppercaseCommandExternal(const char *Input, char *Output, UINTN OutputLength)
{
    LOS_STRING_LIBRARY_REQUEST Request;
    LOS_STRING_LIBRARY_RESPONSE Response;
    UINTN Index;
    UINT64 Status;

    if (Input == 0 || Output == 0 || OutputLength == 0U)
    {
        return LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    }

    for (Index = 0U; Index < sizeof(Request); ++Index)
    {
        ((UINT8 *)&Request)[Index] = 0U;
    }
    for (Index = 0U; Index < sizeof(Response); ++Index)
    {
        ((UINT8 *)&Response)[Index] = 0U;
    }

    Request.Version = LOS_STRING_LIBRARY_VERSION;
    Request.Operation = LOS_STRING_LIBRARY_OPERATION_UPPERCASE;
    Request.Signature = LOS_STRING_LIBRARY_REQUEST_SIGNATURE;
    LosShellServiceCopyText(Request.Input, sizeof(Request.Input), Input);

    if (LosUserInvokeStringLibrary != 0)
    {
        Status = LosUserInvokeStringLibrary("\\LIBERATION\\LIBRARIES\\STRING.ELF", &Request, &Response);
        if (Status == LOS_STRING_LIBRARY_STATUS_SUCCESS &&
            Response.Signature == LOS_STRING_LIBRARY_RESPONSE_SIGNATURE &&
            Response.Status == LOS_STRING_LIBRARY_STATUS_SUCCESS)
        {
            LosShellServiceCopyText(Output, OutputLength, Response.Output);
            return Response.Status;
        }
    }

    Status = LosShellServiceUppercaseBootstrapAdapter(&Request, &Response);
    if (Status == LOS_STRING_LIBRARY_STATUS_SUCCESS)
    {
        LosShellServiceCopyText(Output, OutputLength, Response.Output);
    }
    return Status;
}

void LosShellServiceClearResponse(LOS_SHELL_SERVICE_RESPONSE *Response)
{
    UINTN Index;
    if (Response == 0) { return; }
    for (Index = 0U; Index < sizeof(*Response); ++Index) { ((UINT8 *)Response)[Index] = 0U; }
    Response->Version = LOS_SHELL_SERVICE_VERSION;
    Response->Signature = LOS_SHELL_SERVICE_RESPONSE_SIGNATURE;
}

void LosShellServicePreparePromptText(char *Buffer, UINTN BufferLength)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    if (Buffer == 0 || BufferLength == 0U) { return; }
    if (State != 0 && State->LoginRequired != 0ULL && State->Authenticated == 0ULL)
    {
        LosShellServiceCopyText(Buffer, BufferLength, "login>");
        return;
    }
    LosShellServiceCopyText(Buffer, BufferLength, "shell>");
}

void LosShellServiceInitialize(void)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    UINTN Index;
    if (State == 0) { return; }
    for (Index = 0U; Index < sizeof(*State); ++Index) { ((UINT8 *)State)[Index] = 0U; }
    State->LoginRequired = 1ULL;
    LosShellServiceCopyText(State->ServiceName, sizeof(State->ServiceName), "shell");
    LosShellServiceCopyText(State->WorkingDirectory, sizeof(State->WorkingDirectory), "\");
    LosShellServiceCopyText(State->LastNormalizedCommand, sizeof(State->LastNormalizedCommand), "");
}

const LOS_SHELL_SERVICE_DISCOVERY_RECORD *LosShellServiceGetDiscoveryRecord(void)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    if (State == 0 || State->Online == 0U || State->TransportReady == 0ULL) { return 0; }
    return &State->DiscoveryRecord;
}

const LOS_SHELL_SERVICE_TRANSPORT_EXPORT *LosShellServiceExportTransport(void)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    if (State == 0 || State->Online == 0U || State->TransportReady == 0ULL) { return 0; }
    return &State->TransportExport;
}

UINT64 LosShellServiceBindTransport(LOS_SHELL_SERVICE_TRANSPORT_BINDING *Binding)
{
    const LOS_SHELL_SERVICE_TRANSPORT_EXPORT *Exported;
    if (Binding == 0) { return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; }
    Exported = LosShellServiceExportTransport();
    if (Exported == 0 || Exported->Signature != LOS_SHELL_SERVICE_TRANSPORT_EXPORT_SIGNATURE) { return LOS_SHELL_SERVICE_STATUS_NOT_ATTACHED; }
    Binding->Version = LOS_SHELL_SERVICE_VERSION;
    Binding->Flags = Exported->Flags;
    Binding->TransportState = Exported->TransportState;
    Binding->Signature = LOS_SHELL_SERVICE_TRANSPORT_BINDING_SIGNATURE;
    Binding->Generation = Exported->Generation;
    Binding->ConnectionNonce = Exported->ConnectionNonce;
    Binding->NextRequestSequence = 1ULL;
    Binding->RequestEndpointId = Exported->RequestEndpointId;
    Binding->ResponseEndpointId = Exported->ResponseEndpointId;
    Binding->EventEndpointId = Exported->EventEndpointId;
    Binding->RequestMailbox = (LOS_SHELL_SERVICE_MAILBOX *)(UINTN)Exported->RequestMailboxAddress;
    Binding->ResponseMailbox = (LOS_SHELL_SERVICE_MAILBOX *)(UINTN)Exported->ResponseMailboxAddress;
    return LOS_SHELL_SERVICE_STATUS_SUCCESS;
}

UINT64 LosShellServiceSignalEvent(UINT64 EventCode, UINT64 EventValue) { (void)EventCode; (void)EventValue; return LOS_SHELL_SERVICE_STATUS_SUCCESS; }

UINT64 LosShellServiceBindFromRequest(const LOS_SHELL_SERVICE_BIND_REQUEST *Request, LOS_SHELL_SERVICE_TRANSPORT_BINDING *Binding)
{
    const LOS_SHELL_SERVICE_DISCOVERY_RECORD *Discovery;
    if (Request == 0 || Binding == 0) { return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; }
    if (Request->Version != LOS_SHELL_SERVICE_VERSION || Request->Signature != LOS_SHELL_SERVICE_BIND_REQUEST_SIGNATURE) { return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; }
    Discovery = LosShellServiceGetDiscoveryRecord();
    if (Discovery == 0 || Discovery->Signature != LOS_SHELL_SERVICE_DISCOVERY_SIGNATURE) { return LOS_SHELL_SERVICE_STATUS_NOT_ATTACHED; }
    if (LosShellRuntimeTextEqual(Request->ServiceName, Discovery->ServiceName) == 0) { return LOS_SHELL_SERVICE_STATUS_NOT_FOUND; }
    if (Request->ExpectedVersion != LOS_SHELL_SERVICE_VERSION) { return LOS_SHELL_SERVICE_STATUS_UNSUPPORTED; }
    if ((Discovery->Flags & Request->RequiredFlags) != Request->RequiredFlags) { return LOS_SHELL_SERVICE_STATUS_UNSUPPORTED; }
    if (Request->RequestedGeneration != 0ULL && Request->RequestedGeneration != Discovery->Generation) { return LOS_SHELL_SERVICE_STATUS_RETRY; }
    return LosShellServiceBindTransport(Binding);
}

const LOS_SHELL_SERVICE_REGISTRY_ENTRY *LosShellServiceGetRegistryEntry(void)
{
    LOS_SHELL_SERVICE_STATE *State = LosShellServiceState();
    if (State == 0 || State->Online == 0U || State->RegistryReady == 0ULL) { return 0; }
    return &State->RegistryEntry;
}

UINT64 LosShellServiceResolveFromRequest(const LOS_SHELL_SERVICE_RESOLVE_REQUEST *Request, LOS_SHELL_SERVICE_RESOLVE_RESPONSE *Response)
{
    const LOS_SHELL_SERVICE_REGISTRY_ENTRY *Entry;
    if (Request == 0 || Response == 0) { return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; }
    LosShellServiceCopyText((char *)Response, sizeof(*Response), "");
    Response->Version = LOS_SHELL_SERVICE_VERSION;
    Response->Signature = LOS_SHELL_SERVICE_RESOLVE_RESPONSE_SIGNATURE;
    if (Request->Version != LOS_SHELL_SERVICE_VERSION || Request->Signature != LOS_SHELL_SERVICE_RESOLVE_REQUEST_SIGNATURE) {
        Response->Status = LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER; return Response->Status; }
    Entry = LosShellServiceGetRegistryEntry();
    if (Entry == 0 || Entry->Signature != LOS_SHELL_SERVICE_REGISTRY_ENTRY_SIGNATURE) { Response->Status = LOS_SHELL_SERVICE_STATUS_NOT_ATTACHED; return Response->Status; }
    if (LosShellRuntimeTextEqual(Request->ServiceName, Entry->ServiceName) == 0) { Response->Status = LOS_SHELL_SERVICE_STATUS_NOT_FOUND; return Response->Status; }
    if (Request->ExpectedVersion != Entry->ExpectedVersion) { Response->Status = LOS_SHELL_SERVICE_STATUS_UNSUPPORTED; return Response->Status; }
    if ((Entry->Flags & Request->RequiredFlags) != Request->RequiredFlags) { Response->Status = LOS_SHELL_SERVICE_STATUS_UNSUPPORTED; return Response->Status; }
    Response->Status = LOS_SHELL_SERVICE_STATUS_SUCCESS;
    Response->Flags = Entry->Flags;
    Response->RegistryState = Entry->RegistryState;
    Response->ServiceId = Entry->ServiceId;
    Response->Generation = Entry->Generation;
    Response->ConnectionNonce = LosShellServiceState()->Channel.ConnectionNonce;
    LosShellServiceCopyText(Response->ServiceName, sizeof(Response->ServiceName), Entry->ServiceName);
    return Response->Status;
}

static UINT64 LosShellRuntimeCheckLoginCapability(const char *UserName,
                                                   LOS_CAPABILITIES_ACCESS_RESULT *Result)
{
    LOS_CAPABILITIES_ACCESS_REQUEST Request;
    UINTN Index;

    if (Result == 0 || UserName == 0 || UserName[0] == 0)
    {
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    }

    for (Index = 0U; Index < sizeof(Request); ++Index)
    {
        ((UINT8 *)&Request)[Index] = 0U;
    }
    for (Index = 0U; Index < sizeof(*Result); ++Index)
    {
        ((UINT8 *)Result)[Index] = 0U;
    }

    Request.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Request.PrincipalType = LOS_CAPABILITIES_PRINCIPAL_TYPE_USER;
    Request.AccessRight = LOS_CAPABILITIES_ACCESS_RIGHT_QUERY;
    Request.PrincipalId = 0ULL;
    LosShellServiceCopyText(Request.PrincipalName, sizeof(Request.PrincipalName), UserName);
    LosShellServiceCopyText(Request.Namespace, sizeof(Request.Namespace), "session");
    LosShellServiceCopyText(Request.Name, sizeof(Request.Name), "login");

    if (LosCapabilitiesServiceSubmitAccessRequest != 0)
    {
        return (UINT64)LosCapabilitiesServiceSubmitAccessRequest(&Request, Result);
    }
    if (LosCapabilitiesServiceCheckAccess != 0)
    {
        return (UINT64)LosCapabilitiesServiceCheckAccess(&Request, Result);
    }

    Result->Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Result->Status = LOS_CAPABILITIES_SERVICE_STATUS_UNSUPPORTED;
    Result->Granted = 0U;
    Result->MatchingGrantId = 0ULL;
    return LOS_CAPABILITIES_SERVICE_STATUS_UNSUPPORTED;
}

static UINT64 LosShellRuntimeAuthenticateLoginBootstrap(const char *Arguments,
                                                        UINT64 *CommandResult,
                                                        char *Output,
                                                        UINTN OutputLength)
{
    char UserName[32];
    char Password[32];
    const char *Remainder = 0;
    LOS_CAPABILITIES_ACCESS_RESULT AccessResult;
    UINT64 CapabilityStatus;

    if (CommandResult != 0)
    {
        *CommandResult = LOS_LOGIN_COMMAND_RESULT_NONE;
    }
    LosShellRuntimeNextToken(Arguments, UserName, sizeof(UserName), &Remainder);
    LosShellRuntimeNextToken(Remainder, Password, sizeof(Password), &Remainder);
    if (UserName[0] == 0 || Password[0] == 0)
    {
        if (CommandResult != 0)
        {
            *CommandResult = LOS_LOGIN_COMMAND_RESULT_BAD_REQUEST;
        }
        LosShellServiceCopyText(Output, OutputLength, "usage: login <user> <password>");
        return LOS_SHELL_SERVICE_STATUS_INVALID_PARAMETER;
    }

    if (LosShellRuntimeTextEqual(Password, "liberation") == 0)
    {
        if (CommandResult != 0)
        {
            *CommandResult = LOS_LOGIN_COMMAND_RESULT_DENIED;
        }
        LosShellServiceCopyText(Output, OutputLength, "login denied");
        return LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED;
    }

    CapabilityStatus = LosShellRuntimeCheckLoginCapability(UserName, &AccessResult);
    if (CapabilityStatus != LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS ||
        AccessResult.Status != LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS ||
        AccessResult.Granted == 0U)
    {
        if (CommandResult != 0)
        {
            *CommandResult = LOS_LOGIN_COMMAND_RESULT_DENIED;
        }
        if (CapabilityStatus == LOS_CAPABILITIES_SERVICE_STATUS_UNSUPPORTED)
        {
            LosShellServiceCopyText(Output, OutputLength, "login denied: capability service unavailable");
            return LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED;
        }
        LosShellServiceCopyText(Output, OutputLength, "login denied by capability policy");
        return LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED;
    }

    if (CommandResult != 0)
    {
        *CommandResult = LOS_LOGIN_COMMAND_RESULT_AUTHENTICATED;
    }
    LosShellServiceCopyText(Output, OutputLength, "login authenticated by capability service");
    return LOS_SHELL_SERVICE_STATUS_SUCCESS;
}

UINT64 LosShellServiceLaunchExternal(const char *CommandPath, const char *Arguments, UINT64 *CommandResult, char *Output, UINTN OutputLength)
{
    UINT64 Status;
    if (CommandResult != 0)
    {
        *CommandResult = LOS_LOGIN_COMMAND_RESULT_NONE;
    }
    Status = LosUserLaunchExternalCommand(CommandPath, Arguments, CommandResult, Output, OutputLength);
    if (Status == LOS_SHELL_SERVICE_STATUS_SUCCESS || Status == LOS_SHELL_SERVICE_STATUS_ACCESS_DENIED)
    {
        return Status;
    }
    if (Status == LOS_SHELL_SERVICE_STATUS_UNSUPPORTED && LosShellRuntimeTextEndsWith(CommandPath, "\\LIBERATION\\COMMANDS\\LOGIN.ELF"))
    {
        return LosShellRuntimeAuthenticateLoginBootstrap(Arguments, CommandResult, Output, OutputLength);
    }
    if (Status == LOS_SHELL_SERVICE_STATUS_UNSUPPORTED)
    {
        LosShellServiceCopyText(Output, OutputLength, "external launch ABI not wired yet");
        return LOS_SHELL_SERVICE_STATUS_LAUNCH_FAILED;
    }
    LosShellServiceCopyText(Output, OutputLength, "external command launch failed");
    return LOS_SHELL_SERVICE_STATUS_LAUNCH_FAILED;
}
