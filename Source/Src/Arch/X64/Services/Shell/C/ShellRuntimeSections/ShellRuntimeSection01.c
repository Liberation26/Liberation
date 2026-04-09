/*
 * File Name: ShellRuntimeSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from ShellRuntime.c.
 */

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


static UINT8 LosShellRuntimeImageBuffer[LOS_USER_IMAGE_MAX_STAGED_IMAGE_SIZE];
static UINT8 LosShellRuntimeMappedImageBuffer[LOS_USER_IMAGE_MAX_STAGED_IMAGE_SIZE];
static UINT8 LosShellRuntimeUserStack[16U * 1024U];
static LOS_USER_IMAGE_ISOLATED_SPACE LosShellRuntimeLastIsolatedSpace;
static LOS_USER_IMAGE_RING3_CONTEXT LosShellRuntimeLastRing3Context;
static LOS_USER_IMAGE_COMPLETION_RECORD LosShellRuntimeCompletionRecord;
static LOS_USER_IMAGE_CALL LosShellRuntimeLastCall;
static volatile UINT64 LosShellRuntimeUserModeCompletionStatus = LOS_USER_IMAGE_CALL_STATUS_UNSUPPORTED;
static volatile UINT64 LosShellRuntimeUserModeCompletionResult = 0ULL;
static UINT64 LosShellRuntimeCallResultShadow = 0ULL;

static void LosShellRuntimeWriteDebugText(const char *Text);
static void LosShellRuntimeWriteDebugUnsigned(const char *Prefix, UINT64 Value, const char *Suffix);
static void LosShellRuntimeZeroMemory(void *Buffer, UINTN Length);
static UINT64 LosShellRuntimeAlignDown64(UINT64 Value, UINT64 Alignment);
static UINT64 LosShellRuntimeAlignUp64(UINT64 Value, UINT64 Alignment);
static UINT64 LosShellRuntimeMaterializeLoadedImage(const void *Image,
                                                    UINTN ImageSize,
                                                    UINT64 *LoadedBaseAddress,
                                                    UINT64 *LoadedSize,
                                                    UINT64 *LoadedEntryAddress);
static UINT64 LosShellRuntimeInvokeLoadedEntry(UINT64 EntryAddress,
                                               const LOS_USER_IMAGE_CALL *Call,
                                               UINT64 StackAddress,
                                               UINT64 StackSize);
static BOOLEAN LosShellRuntimeValidateElfImage(const void *Image, UINTN ImageSize, UINT64 *EntryAddress);
static UINT64 __attribute__((unused)) LosShellRuntimeDispatchBootstrapImage(const LOS_USER_IMAGE_CALL *Call);
static UINT64 __attribute__((unused)) LosShellRuntimeTryDiskBackedImage(const LOS_USER_IMAGE_CALL *Call);
static void LosShellRuntimeStageCompletion(UINT64 Status, UINT64 ResultValue);
static void LosShellRuntimeCommitCompletionToShell(LOS_USER_IMAGE_CALL *Call);


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
        LosShellRuntimeWriteDebugUnsigned("[shell] isolated address space id=", LosShellRuntimeLastIsolatedSpace.AddressSpaceId, "\n");
        LosShellRuntimeWriteDebugUnsigned("[shell] isolated root table=", LosShellRuntimeLastIsolatedSpace.RootTablePhysicalAddress, "\n");
        LosShellRuntimeWriteDebugUnsigned("[shell] ring3 entry=", LosShellRuntimeLastRing3Context.UserInstructionPointer, "\n");
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

    LosShellRuntimeWriteDebugUnsigned("[shell] mapped image base=", LoadedBaseAddress, "\n");
    LosShellRuntimeWriteDebugUnsigned("[shell] mapped image size=", LoadedSize, "\n");
    LosShellRuntimeWriteDebugUnsigned("[shell] executing ELF entry=", LoadedEntryAddress, "\n");
    Status = LosShellRuntimeInvokeLoadedEntry(LoadedEntryAddress,
                                              (const LOS_USER_IMAGE_CALL *)(UINTN)Context->CallAddress,
                                              Context->StackAddress,
                                              Context->StackSize);
    LosShellRuntimeWriteDebugUnsigned("[shell] returned from ELF status=", Status, "\n");
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

