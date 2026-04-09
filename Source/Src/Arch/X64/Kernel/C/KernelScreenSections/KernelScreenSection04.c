/*
 * File Name: KernelScreenSection04.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from KernelScreen.c.
 */

void LosKernelInitializeScreen(const LOS_BOOT_CONTEXT *BootContext)
{
    LOS_X64_MAP_PAGES_REQUEST MapRequest;
    LOS_X64_MAP_PAGES_RESULT MapResult;
    UINT64 PhysicalBase;
    UINT64 PhysicalOffset;
    UINT64 MappingBytes;
    UINT64 PageCount;
    UINT64 UsableFrameBufferBytes;
    void *DirectMapPointer;

    LosKernelScreenState.FrameBuffer = 0;
    LosKernelScreenState.FrameBufferVirtualAddress = 0ULL;
    LosKernelScreenState.FrameBufferSize = 0ULL;
    LosKernelScreenState.Width = 0U;
    LosKernelScreenState.Height = 0U;
    LosKernelScreenState.PixelsPerScanLine = 0U;
    LosKernelScreenState.PixelFormat = PixelRedGreenBlueReserved8BitPerColor;
    LosKernelScreenState.CursorColumn = 0U;
    LosKernelScreenState.CursorRow = 0U;
    ApplyLineIndent(LOS_KERNEL_SCREEN_BASE_INDENT_COLUMNS);
    LosKernelScreenState.MaxColumns = 0U;
    LosKernelScreenState.MaxRows = 0U;
    InitializeDefaultFont();
    LosKernelScreenState.Ready = 0U;

    if (BootContext == 0 ||
        BootContext->FrameBufferPhysicalAddress == 0ULL ||
        BootContext->FrameBufferWidth == 0U ||
        BootContext->FrameBufferHeight == 0U ||
        BootContext->FrameBufferPixelsPerScanLine == 0U)
    {
        LosKernelSerialWriteText("[KernelScreen] Framebuffer information missing.\n");
        return;
    }

    UsableFrameBufferBytes = GetUsableFrameBufferBytes(
        BootContext->FrameBufferSize,
        BootContext->FrameBufferPixelsPerScanLine,
        BootContext->FrameBufferHeight);
    if (UsableFrameBufferBytes == 0ULL)
    {
        LosKernelSerialWriteText("[KernelScreen] Computed framebuffer size was zero.\n");
        return;
    }

    TraceScreenInitValue("[KernelScreen] Framebuffer physical base: ", BootContext->FrameBufferPhysicalAddress);
    TraceScreenInitValue("[KernelScreen] Framebuffer bytes: ", UsableFrameBufferBytes);
    TraceScreenInitValue("[KernelScreen] Framebuffer width: ", (UINT64)BootContext->FrameBufferWidth);
    TraceScreenInitValue("[KernelScreen] Framebuffer height: ", (UINT64)BootContext->FrameBufferHeight);
    TraceScreenInitValue("[KernelScreen] Framebuffer pixels per scan line: ", (UINT64)BootContext->FrameBufferPixelsPerScanLine);
    TraceScreenInitValue("[KernelScreen] Framebuffer pixel format: ", (UINT64)BootContext->FrameBufferPixelFormat);

    DirectMapPointer = LosX64GetDirectMapVirtualAddress(BootContext->FrameBufferPhysicalAddress, UsableFrameBufferBytes);
    if (DirectMapPointer != 0)
    {
        LosKernelScreenState.FrameBufferVirtualAddress = (UINT64)(UINTN)DirectMapPointer;
        LosKernelScreenState.FrameBuffer = (UINT32 *)DirectMapPointer;
        LosKernelSerialWriteText("[KernelScreen] Using existing higher-half direct-map framebuffer mapping.\n");
    }
    else
    {
        PhysicalBase = BootContext->FrameBufferPhysicalAddress & ~0xFFFULL;
        PhysicalOffset = BootContext->FrameBufferPhysicalAddress - PhysicalBase;
        MappingBytes = PhysicalOffset + UsableFrameBufferBytes;
        PageCount = (MappingBytes + 4095ULL) / 4096ULL;

        MapRequest.PageMapLevel4PhysicalAddress = 0ULL;
        MapRequest.VirtualAddress = LOS_KERNEL_SCREEN_VIRTUAL_BASE;
        MapRequest.PhysicalAddress = PhysicalBase;
        MapRequest.PageCount = PageCount;
        MapRequest.PageFlags = LOS_KERNEL_PAGE_WRITABLE | LOS_KERNEL_PAGE_WRITE_THROUGH | LOS_KERNEL_PAGE_CACHE_DISABLE;
        MapRequest.Flags = LOS_X64_MAP_PAGES_FLAG_ALLOW_REMAP | LOS_X64_MAP_PAGES_FLAG_ALLOW_UNDISCOVERED_PHYSICAL;
        MapRequest.Reserved = 0U;
        LosX64MapPages(&MapRequest, &MapResult);
        if (MapResult.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS)
        {
            LosKernelSerialWriteText("[KernelScreen] Explicit framebuffer remap failed with status ");
            LosKernelSerialWriteUnsigned(MapResult.Status);
            LosKernelSerialWriteText(".\n");
            return;
        }

        LosKernelScreenState.FrameBufferVirtualAddress = LOS_KERNEL_SCREEN_VIRTUAL_BASE + PhysicalOffset;
        LosKernelScreenState.FrameBuffer = (UINT32 *)(UINTN)LosKernelScreenState.FrameBufferVirtualAddress;
        LosKernelSerialWriteText("[KernelScreen] Explicit higher-half framebuffer remap installed.\n");
    }

    LosKernelScreenState.FrameBufferSize = UsableFrameBufferBytes;
    LosKernelScreenState.Width = BootContext->FrameBufferWidth;
    LosKernelScreenState.Height = BootContext->FrameBufferHeight;
    LosKernelScreenState.PixelsPerScanLine = BootContext->FrameBufferPixelsPerScanLine;
    LosKernelScreenState.PixelFormat = BootContext->FrameBufferPixelFormat;
    TryInitializePsfFont(BootContext);
    if (LosKernelScreenState.FontScale == 0U)
    {
        LosKernelScreenState.FontScale = 1U;
    }

    LosKernelScreenState.MaxColumns = BootContext->FrameBufferWidth / (LosKernelScreenState.CellWidth * LosKernelScreenState.FontScale);
    LosKernelScreenState.MaxRows = BootContext->FrameBufferHeight / (LosKernelScreenState.CellHeight * LosKernelScreenState.FontScale);
    LosKernelScreenState.Ready = (LosKernelScreenState.MaxColumns != 0U && LosKernelScreenState.MaxRows != 0U) ? 1U : 0U;
    if (LosKernelScreenState.Ready != 0U)
    {
        ApplyLineIndent(LOS_KERNEL_SCREEN_BASE_INDENT_COLUMNS);
    }
    if (LosKernelScreenState.Ready == 0U)
    {
        LosKernelSerialWriteText("[KernelScreen] Screen geometry was too small for the text grid.\n");
        return;
    }

    TraceScreenInitValue("[KernelScreen] Text cell width: ", (UINT64)LosKernelScreenState.CellWidth);
    TraceScreenInitValue("[KernelScreen] Text cell height: ", (UINT64)LosKernelScreenState.CellHeight);
    TraceScreenInitValue("[KernelScreen] Font scale: ", (UINT64)LosKernelScreenState.FontScale);
    TraceScreenInitValue("[KernelScreen] Max columns: ", (UINT64)LosKernelScreenState.MaxColumns);
    TraceScreenInitValue("[KernelScreen] Max rows: ", (UINT64)LosKernelScreenState.MaxRows);

    ClearScreen();
    DrawInitializationProbe();
    DrawStaticScreenDecorations();
    ResetLogCursor();
    LosKernelSerialWriteText("[KernelScreen] Screen path initialized.\n");
}

static void WriteStatusLine(const char *Prefix, UINT32 PrefixColor, const char *Text)
{
    if (LosKernelScreenState.Ready == 0U || TryBeginScreenUpdate() == 0U)
    {
        return;
    }

    PutTextColored(Prefix, PrefixColor);
    PutCharacter(' ');
    PutText(Text);
    PutCharacter('\n');
    EndScreenUpdate();
}

void LosKernelStatusScreenWriteOk(const char *Text)
{
    WriteStatusLine("[OK]", GetOkPrefixColor(), Text);
}

void LosKernelStatusScreenWriteFail(const char *Text)
{
    WriteStatusLine("[FAIL]", GetFailPrefixColor(), Text);
}
