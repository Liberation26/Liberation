/*
 * File Name: KernelScreenSection02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from KernelScreen.c.
 */

static void ResetLogCursor(void)
{
    LosKernelScreenState.CursorColumn = 0U;
    LosKernelScreenState.CursorRow = LOS_KERNEL_SCREEN_FIRST_LOG_ROW;
    ApplyLineIndent(LOS_KERNEL_SCREEN_BASE_INDENT_COLUMNS);
}

static UINT32 GetLastWritableLogRow(void)
{
    if (LosKernelScreenState.MaxRows == 0U)
    {
        return 0U;
    }

    if (LosKernelScreenState.MaxRows <= (LOS_KERNEL_SCREEN_FIRST_LOG_ROW + 1U))
    {
        return LOS_KERNEL_SCREEN_FIRST_LOG_ROW;
    }

    return LosKernelScreenState.MaxRows - 2U;
}

static void ScrollLogRegionUp(void)
{
    UINT32 StartY;
    UINT32 EndY;
    UINT32 RowPixelHeight;
    UINTN VisiblePixelCount;
    UINTN ClearPixelCount;
    UINT32 *Destination;
    const UINT32 *Source;

    if (LosKernelScreenState.Ready == 0U ||
        LosKernelScreenState.FrameBuffer == 0 ||
        LosKernelScreenState.PixelsPerScanLine == 0U)
    {
        return;
    }

    RowPixelHeight = LosKernelScreenState.CellHeight * LosKernelScreenState.FontScale;
    if (RowPixelHeight == 0U)
    {
        return;
    }

    StartY = LOS_KERNEL_SCREEN_FIRST_LOG_ROW * RowPixelHeight;
    EndY = (GetLastWritableLogRow() + 1U) * RowPixelHeight;
    if (EndY <= StartY || (EndY - StartY) <= RowPixelHeight)
    {
        ClearRow(GetLastWritableLogRow());
        return;
    }

    VisiblePixelCount = (UINTN)(EndY - StartY - RowPixelHeight) * (UINTN)LosKernelScreenState.PixelsPerScanLine;
    ClearPixelCount = (UINTN)RowPixelHeight * (UINTN)LosKernelScreenState.PixelsPerScanLine;
    Destination = LosKernelScreenState.FrameBuffer + ((UINTN)StartY * (UINTN)LosKernelScreenState.PixelsPerScanLine);
    Source = Destination + ClearPixelCount;

    CopyPixelsForward(Destination, Source, VisiblePixelCount);
    FillPixels(Destination + VisiblePixelCount, 0x00000000U, ClearPixelCount);
    DrawStaticScreenDecorations();
}

static void AdvanceLine(void)
{
    UINT32 LastWritableLogRow;

    LosKernelScreenState.CursorColumn = 0U;
    LosKernelScreenState.CursorRow += 1U;

    LastWritableLogRow = GetLastWritableLogRow();
    if (LosKernelScreenState.CursorRow > LastWritableLogRow)
    {
        ScrollLogRegionUp();
        LosKernelScreenState.CursorRow = LastWritableLogRow;
    }

    ApplyLineIndent(LOS_KERNEL_SCREEN_BASE_INDENT_COLUMNS);
}

static void PutScaledPixelBlock(UINT32 BaseX, UINT32 BaseY, UINT32 Scale, UINT32 Color)
{
    UINT32 ScaleX;
    UINT32 ScaleY;

    if (Scale == 0U)
    {
        Scale = 1U;
    }

    for (ScaleY = 0U; ScaleY < Scale; ++ScaleY)
    {
        for (ScaleX = 0U; ScaleX < Scale; ++ScaleX)
        {
            PutPixel(BaseX + ScaleX, BaseY + ScaleY, Color);
        }
    }
}

static void RenderFallbackGlyph(char Character, UINT32 BaseX, UINT32 BaseY, UINT32 Color)
{
    UINT32 GlyphRow;
    UINT32 GlyphColumn;
    UINT32 Scale;

    Scale = LosKernelScreenState.FontScale == 0U ? 1U : LosKernelScreenState.FontScale;

    for (GlyphRow = 0U; GlyphRow < LOS_KERNEL_SCREEN_DEFAULT_GLYPH_HEIGHT; ++GlyphRow)
    {
        UINT8 RowBits;
        RowBits = GetGlyphRow(Character, GlyphRow);
        for (GlyphColumn = 0U; GlyphColumn < LOS_KERNEL_SCREEN_DEFAULT_GLYPH_WIDTH; ++GlyphColumn)
        {
            if ((RowBits & (UINT8)(1U << (4U - GlyphColumn))) != 0U)
            {
                PutScaledPixelBlock(BaseX + ((GlyphColumn + 1U) * Scale), BaseY + (GlyphRow * Scale), Scale, Color);
            }
        }
    }
}

static void RenderPsfGlyph(UINT8 Character, UINT32 BaseX, UINT32 BaseY, UINT32 Color)
{
    const UINT8 *Glyph;
    UINT32 Row;
    UINT32 Column;
    UINT32 Scale;

    if (LosKernelScreenState.FontLoaded == 0U || LosKernelScreenState.FontGlyphs == 0)
    {
        return;
    }

    if ((UINT32)Character >= LosKernelScreenState.FontGlyphCount)
    {
        Character = (UINT8)'?';
    }

    Scale = LosKernelScreenState.FontScale == 0U ? 1U : LosKernelScreenState.FontScale;

    Glyph = LosKernelScreenState.FontGlyphs + ((UINT32)Character * LosKernelScreenState.FontBytesPerGlyph);
    for (Row = 0U; Row < LosKernelScreenState.GlyphHeight; ++Row)
    {
        const UINT8 *RowData;
        RowData = Glyph + (Row * LosKernelScreenState.FontRowStride);
        for (Column = 0U; Column < LosKernelScreenState.GlyphWidth; ++Column)
        {
            UINT8 ByteValue;
            UINT8 BitMask;
            ByteValue = RowData[Column / 8U];
            BitMask = (UINT8)(0x80U >> (Column & 7U));
            if ((ByteValue & BitMask) != 0U)
            {
                PutScaledPixelBlock(BaseX + (Column * Scale), BaseY + (Row * Scale), Scale, Color);
            }
        }
    }
}

static void PutCharacterColored(char Character, UINT32 Color)
{
    UINT32 BaseX;
    UINT32 BaseY;

    if (LosKernelScreenState.Ready == 0U)
    {
        return;
    }

    if (Character == '\n')
    {
        AdvanceLine();
        return;
    }

    if (Character == '\r')
    {
        return;
    }

    if (LosKernelScreenState.CursorColumn >= GetWritableColumnLimit())
    {
        AdvanceLine();
    }

    BaseX = LosKernelScreenState.CursorColumn * LosKernelScreenState.CellWidth * LosKernelScreenState.FontScale;
    BaseY = LosKernelScreenState.CursorRow * LosKernelScreenState.CellHeight * LosKernelScreenState.FontScale;

    if (LosKernelScreenState.FontLoaded != 0U)
    {
        RenderPsfGlyph((UINT8)Character, BaseX, BaseY, Color);
    }
    else
    {
        RenderFallbackGlyph(Character, BaseX, BaseY, Color);
    }

    LosKernelScreenState.CursorColumn += 1U;
}

static void PutCharacter(char Character)
{
    PutCharacterColored(Character, GetTextColor());
}

static void FillCellBackground(UINT32 Column, UINT32 Row, UINT32 Color)
{
    UINT32 StartX;
    UINT32 StartY;
    UINT32 X;
    UINT32 Y;

    if (LosKernelScreenState.Ready == 0U)
    {
        return;
    }
    if (Column >= LosKernelScreenState.MaxColumns || Row >= LosKernelScreenState.MaxRows)
    {
        return;
    }

    StartX = Column * LosKernelScreenState.CellWidth * LosKernelScreenState.FontScale;
    StartY = Row * LosKernelScreenState.CellHeight * LosKernelScreenState.FontScale;
    for (Y = 0U; Y < (LosKernelScreenState.CellHeight * LosKernelScreenState.FontScale); ++Y)
    {
        for (X = 0U; X < (LosKernelScreenState.CellWidth * LosKernelScreenState.FontScale); ++X)
        {
            PutPixel(StartX + X, StartY + Y, Color);
        }
    }
}

static void DrawCharacterAt(UINT32 Column, UINT32 Row, char Character, UINT32 Color)
{
    UINT32 BaseX;
    UINT32 BaseY;

    if (LosKernelScreenState.Ready == 0U)
    {
        return;
    }
    if (Column >= LosKernelScreenState.MaxColumns || Row >= LosKernelScreenState.MaxRows)
    {
        return;
    }

    FillCellBackground(Column, Row, 0x00000000U);
    BaseX = Column * LosKernelScreenState.CellWidth * LosKernelScreenState.FontScale;
    BaseY = Row * LosKernelScreenState.CellHeight * LosKernelScreenState.FontScale;

    if (LosKernelScreenState.FontLoaded != 0U)
    {
        RenderPsfGlyph((UINT8)Character, BaseX, BaseY, Color);
    }
    else
    {
        RenderFallbackGlyph(Character, BaseX, BaseY, Color);
    }
}

static UINT32 GetWordLength(const char *Text)
{
    UINT32 Length;

    Length = 0U;
    while (Text[Length] != '\0' && Text[Length] != ' ' && Text[Length] != '\n' && Text[Length] != '\r' && Text[Length] != '\t')
    {
        Length += 1U;
    }

    return Length;
}

static void PutTextColored(const char *Text, UINT32 Color)
{
    UINTN Index;

    if (Text == 0)
    {
        return;
    }

    for (Index = 0U; Text[Index] != '\0'; )
    {
        UINT32 WordLength;
        UINT32 RemainingColumns;

        if (Text[Index] == '\n' || Text[Index] == '\r')
        {
            PutCharacterColored(Text[Index], Color);
            ApplyLineIndent(LOS_KERNEL_SCREEN_BASE_INDENT_COLUMNS);
            Index += 1U;
            continue;
        }

        if (Text[Index] == '\t')
        {
            PutCharacterColored(' ', Color);
            PutCharacterColored(' ', Color);
            PutCharacterColored(' ', Color);
            PutCharacterColored(' ', Color);
            Index += 1U;
            continue;
        }

        if (Text[Index] == ' ')
        {
            if (LosKernelScreenState.CursorColumn != 0U)
            {
                PutCharacterColored(' ', Color);
            }
            Index += 1U;
            continue;
        }

        WordLength = GetWordLength(Text + Index);
        RemainingColumns = GetWritableColumnLimit() > LosKernelScreenState.CursorColumn
            ? (GetWritableColumnLimit() - LosKernelScreenState.CursorColumn)
            : 0U;

        if (WordLength != 0U && WordLength <= GetWritableColumnLimit() && WordLength > RemainingColumns && LosKernelScreenState.CursorColumn != 0U)
        {
            AdvanceLine();
            ApplyLineIndent(LOS_KERNEL_SCREEN_WRAP_INDENT_COLUMNS);
        }

        while (Text[Index] != '\0' && Text[Index] != ' ' && Text[Index] != '\n' && Text[Index] != '\r' && Text[Index] != '\t')
        {
            PutCharacterColored(Text[Index], Color);
            Index += 1U;
        }
    }
}
