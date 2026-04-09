/*
 * File Name: KernelScreenSection03.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from KernelScreen.c.
 */

static void PutText(const char *Text)
{
    PutTextColored(Text, GetTextColor());
}

static void DrawTextAt(UINT32 Column, UINT32 Row, const char *Text, UINT32 Color)
{
    UINTN Index;
    UINT32 DrawColumn;
    char Character;

    if (LosKernelScreenState.Ready == 0U || Text == 0)
    {
        return;
    }
    if (Row >= LosKernelScreenState.MaxRows || Column >= LosKernelScreenState.MaxColumns)
    {
        return;
    }

    DrawColumn = Column;
    for (Index = 0U; Text[Index] != '\0' && DrawColumn < LosKernelScreenState.MaxColumns; ++Index)
    {
        Character = Text[Index];
        if (Character == '\n' || Character == '\r')
        {
            break;
        }
        if (Character == '\t')
        {
            Character = ' ';
        }

        DrawCharacterAt(DrawColumn, Row, Character, Color);
        DrawColumn += 1U;
    }
}

static void DrawUnsignedAt(UINT32 Column, UINT32 Row, UINT64 Value, UINT32 Digits, UINT32 Color)
{
    char Buffer[21];
    UINT32 Index;

    if (Digits >= sizeof(Buffer))
    {
        Digits = (UINT32)(sizeof(Buffer) - 1U);
    }

    for (Index = 0U; Index < Digits; ++Index)
    {
        Buffer[Digits - 1U - Index] = (char)('0' + (Value % 10ULL));
        Value /= 10ULL;
    }

    Buffer[Digits] = '\0';
    DrawTextAt(Column, Row, Buffer, Color);
}

static UINT32 FormatUnsignedDecimal(UINT64 Value, char *Buffer, UINT32 BufferSize)
{
    char ReverseBuffer[21];
    UINT32 Length;
    UINT32 Index;

    if (Buffer == 0 || BufferSize == 0U)
    {
        return 0U;
    }

    if (Value == 0ULL)
    {
        Buffer[0] = '0';
        if (BufferSize > 1U)
        {
            Buffer[1] = '\0';
        }
        return 1U;
    }

    Length = 0U;
    while (Value != 0ULL && Length < (UINT32)(sizeof(ReverseBuffer)))
    {
        ReverseBuffer[Length] = (char)('0' + (Value % 10ULL));
        Value /= 10ULL;
        Length += 1U;
    }

    if (Length >= BufferSize)
    {
        Length = BufferSize - 1U;
    }

    for (Index = 0U; Index < Length; ++Index)
    {
        Buffer[Index] = ReverseBuffer[Length - 1U - Index];
    }

    Buffer[Length] = '\0';
    return Length;
}

static void DrawTimerOverlayFrame(void)
{
    UINT32 Row;

    if (LosKernelScreenState.Ready == 0U ||
        LosKernelScreenState.MaxRows <= LOS_KERNEL_SCREEN_TIMER_ROW ||
        LosKernelScreenState.MaxColumns < 40U)
    {
        return;
    }

    Row = LOS_KERNEL_SCREEN_TIMER_ROW;
    ClearRow(Row);
    DrawTextAt(0U, Row, "TIMER IRQ ", GetTextColor());
    DrawTextAt(10U, Row, "WAIT", GetFailPrefixColor());
    DrawTextAt(16U, Row, "HZ ", GetTextColor());
    DrawUnsignedAt(19U, Row, 0ULL, 3U, GetTextColor());
    DrawTextAt(24U, Row, "TICKS ", GetTextColor());
    DrawUnsignedAt(30U, Row, 0ULL, 10U, GetTextColor());
}

static void DrawStaticScreenDecorations(void)
{
    UINT32 LastColumn;
    UINT32 LastRow;
    UINT32 StatusColor;
    UINT32 TextColor;
    char WidthBuffer[21];
    char HeightBuffer[21];
    char ColumnBuffer[21];
    char RowBuffer[21];

    if (LosKernelScreenState.Ready == 0U ||
        LosKernelScreenState.MaxColumns == 0U ||
        LosKernelScreenState.MaxRows == 0U)
    {
        return;
    }

    LastColumn = LosKernelScreenState.MaxColumns - 1U;
    LastRow = LosKernelScreenState.MaxRows - 1U;
    StatusColor = GetOkPrefixColor();
    TextColor = GetTextColor();

    ClearRow(0U);
    ClearRow(LastRow);

    DrawCharacterAt(0U, 0U, 'Q', StatusColor);
    DrawCharacterAt(LastColumn, 0U, 'Q', StatusColor);
    DrawTextAt(2U, 0U, "LIBERATION KERNEL SCREEN ONLINE", TextColor);

    DrawCharacterAt(0U, LastRow, 'Q', StatusColor);
    DrawCharacterAt(LastColumn, LastRow, 'Q', StatusColor);

    FormatUnsignedDecimal((UINT64)LosKernelScreenState.Width, WidthBuffer, (UINT32)sizeof(WidthBuffer));
    FormatUnsignedDecimal((UINT64)LosKernelScreenState.Height, HeightBuffer, (UINT32)sizeof(HeightBuffer));
    FormatUnsignedDecimal((UINT64)LosKernelScreenState.MaxColumns, ColumnBuffer, (UINT32)sizeof(ColumnBuffer));
    FormatUnsignedDecimal((UINT64)LosKernelScreenState.MaxRows, RowBuffer, (UINT32)sizeof(RowBuffer));

    DrawTextAt(2U, LastRow, "SCREEN ", TextColor);
    DrawTextAt(9U, LastRow, WidthBuffer, TextColor);
    DrawTextAt(9U + GetWordLength(WidthBuffer), LastRow, "X", TextColor);
    DrawTextAt(10U + GetWordLength(WidthBuffer), LastRow, HeightBuffer, TextColor);
    DrawTextAt(12U + GetWordLength(WidthBuffer) + GetWordLength(HeightBuffer), LastRow, " GRID ", TextColor);
    DrawTextAt(18U + GetWordLength(WidthBuffer) + GetWordLength(HeightBuffer), LastRow, ColumnBuffer, TextColor);
    DrawTextAt(18U + GetWordLength(WidthBuffer) + GetWordLength(HeightBuffer) + GetWordLength(ColumnBuffer), LastRow, "X", TextColor);
    DrawTextAt(19U + GetWordLength(WidthBuffer) + GetWordLength(HeightBuffer) + GetWordLength(ColumnBuffer), LastRow, RowBuffer, TextColor);
    DrawTimerOverlayFrame();
}

static void DrawInitializationProbe(void)
{
    UINT32 X;
    UINT32 Y;
    UINT32 LimitX;
    UINT32 LimitY;

    if (LosKernelScreenState.Ready == 0U)
    {
        return;
    }

    LimitX = LosKernelScreenState.Width < 16U ? LosKernelScreenState.Width : 16U;
    LimitY = LosKernelScreenState.Height < 16U ? LosKernelScreenState.Height : 16U;

    for (Y = 0U; Y < LimitY; ++Y)
    {
        for (X = 0U; X < LimitX; ++X)
        {
            PutPixel(X, Y, ComposePixelColor(0x00U, 0x80U, 0xFFU));
        }
    }
}

static void InitializeDefaultFont(void)
{
    LosKernelScreenState.CellWidth = LOS_KERNEL_SCREEN_DEFAULT_CELL_WIDTH;
    LosKernelScreenState.CellHeight = LOS_KERNEL_SCREEN_DEFAULT_CELL_HEIGHT + LOS_KERNEL_SCREEN_LINE_SPACING;
    LosKernelScreenState.GlyphWidth = LOS_KERNEL_SCREEN_DEFAULT_GLYPH_WIDTH;
    LosKernelScreenState.GlyphHeight = LOS_KERNEL_SCREEN_DEFAULT_GLYPH_HEIGHT;
    LosKernelScreenState.FontGlyphCount = 0U;
    LosKernelScreenState.FontBytesPerGlyph = 0U;
    LosKernelScreenState.FontRowStride = 0U;
    LosKernelScreenState.FontGlyphs = 0;
    LosKernelScreenState.FontScale = LOS_KERNEL_SCREEN_DEFAULT_FONT_SCALE;
    LosKernelScreenState.LastDisplayedTimerTick = 0ULL;
    LosKernelScreenState.UpdateBusy = 0U;
    LosKernelScreenState.FontLoaded = 0U;
    LosKernelScreenState.TimerLive = 0U;
}

static void TryInitializePsfFont(const LOS_BOOT_CONTEXT *BootContext)
{
    const LOS_PSF2_HEADER *Header;
    const void *MappedPointer;
    UINT32 RowStride;
    UINT64 RequiredBytes;

    InitializeDefaultFont();
    if (BootContext == 0 || BootContext->KernelFontPhysicalAddress == 0ULL || BootContext->KernelFontSize < sizeof(LOS_PSF2_HEADER))
    {
        LosKernelSerialWriteText("[KernelScreen] No boot font supplied. Using built-in font.\n");
        return;
    }

    MappedPointer = LosX64GetDirectMapVirtualAddress(BootContext->KernelFontPhysicalAddress, BootContext->KernelFontSize);
    if (MappedPointer == 0)
    {
        LosKernelSerialWriteText("[KernelScreen] Boot font direct-map lookup failed. Using built-in font.\n");
        return;
    }

    Header = (const LOS_PSF2_HEADER *)MappedPointer;
    if (Header->Magic != LOS_PSF2_MAGIC)
    {
        LosKernelSerialWriteText("[KernelScreen] Boot font is not PSF2. Using built-in font.\n");
        return;
    }
    if (Header->HeaderSize < sizeof(LOS_PSF2_HEADER) || Header->Length == 0U || Header->CharSize == 0U || Header->Width == 0U || Header->Height == 0U)
    {
        LosKernelSerialWriteText("[KernelScreen] Boot font header invalid. Using built-in font.\n");
        return;
    }

    RowStride = (Header->Width + 7U) / 8U;
    if (RowStride == 0U || Header->CharSize < (RowStride * Header->Height))
    {
        LosKernelSerialWriteText("[KernelScreen] Boot font glyph layout invalid. Using built-in font.\n");
        return;
    }

    RequiredBytes = (UINT64)Header->HeaderSize + ((UINT64)Header->Length * (UINT64)Header->CharSize);
    if (RequiredBytes > BootContext->KernelFontSize)
    {
        LosKernelSerialWriteText("[KernelScreen] Boot font truncated. Using built-in font.\n");
        return;
    }

    LosKernelScreenState.CellWidth = Header->Width;
    LosKernelScreenState.CellHeight = Header->Height + LOS_KERNEL_SCREEN_LINE_SPACING;
    LosKernelScreenState.GlyphWidth = Header->Width;
    LosKernelScreenState.GlyphHeight = Header->Height;
    LosKernelScreenState.FontGlyphCount = Header->Length;
    LosKernelScreenState.FontBytesPerGlyph = Header->CharSize;
    LosKernelScreenState.FontRowStride = RowStride;
    LosKernelScreenState.FontGlyphs = ((const UINT8 *)MappedPointer) + Header->HeaderSize;
    LosKernelScreenState.FontLoaded = 1U;
    LosKernelSerialWriteText("[KernelScreen] Boot PSF2 font loaded from monitor handoff.\n");
    TraceScreenInitValue("[KernelScreen] Boot font physical base: ", BootContext->KernelFontPhysicalAddress);
    TraceScreenInitValue("[KernelScreen] Boot font bytes: ", BootContext->KernelFontSize);
    TraceScreenInitValue("[KernelScreen] Boot font glyph width: ", (UINT64)Header->Width);
    TraceScreenInitValue("[KernelScreen] Boot font glyph height: ", (UINT64)Header->Height);
}

void LosKernelScreenUpdateSpinner(UINT64 TickCount)
{
    static const char LosSpinnerFrames[4] = {'|', '/', '-', '\\'};
    UINT32 SpinnerColumn;
    char SpinnerCharacter;

    if (LosKernelScreenState.Ready == 0U ||
        LosKernelScreenState.MaxColumns < 4U ||
        LosKernelScreenState.TimerLive != 0U ||
        (TickCount & 0x3ULL) != 0ULL ||
        TryBeginScreenUpdate() == 0U)
    {
        return;
    }

    SpinnerColumn = LosKernelScreenState.MaxColumns - 3U;
    SpinnerCharacter = LosSpinnerFrames[(UINTN)((TickCount >> 2U) & 0x3ULL)];
    DrawCharacterAt(SpinnerColumn, 0U, SpinnerCharacter, GetOkPrefixColor());
    EndScreenUpdate();
}

void LosKernelScreenUpdateTimer(UINT64 TickCount, UINT64 TargetHz, BOOLEAN InterruptsLive)
{
    UINT32 StatusColor;
    UINT32 Row;

    if (LosKernelScreenState.Ready == 0U || LosKernelScreenState.MaxColumns < 40U || LosKernelScreenState.MaxRows < 3U)
    {
        return;
    }

    if (InterruptsLive != 0U &&
        LosKernelScreenState.TimerLive != 0U &&
        TargetHz != 0ULL &&
        TickCount >= LosKernelScreenState.LastDisplayedTimerTick &&
        (TickCount - LosKernelScreenState.LastDisplayedTimerTick) < TargetHz)
    {
        return;
    }

    if (TryBeginScreenUpdate() == 0U)
    {
        return;
    }

    StatusColor = InterruptsLive != 0U ? GetOkPrefixColor() : GetFailPrefixColor();
    Row = LOS_KERNEL_SCREEN_TIMER_ROW;

    DrawTextAt(10U, Row, InterruptsLive != 0U ? "LIVE" : "WAIT", StatusColor);
    DrawUnsignedAt(19U, Row, TargetHz, 3U, GetTextColor());
    DrawUnsignedAt(30U, Row, TickCount, 10U, GetTextColor());

    LosKernelScreenState.TimerLive = InterruptsLive != 0U ? 1U : 0U;
    LosKernelScreenState.LastDisplayedTimerTick = TickCount;
    EndScreenUpdate();
}
