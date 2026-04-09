/*
 * File Name: KernelScreenSection01.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from KernelScreen.c.
 */

typedef struct
{
    UINT32 Magic;
    UINT32 Version;
    UINT32 HeaderSize;
    UINT32 Flags;
    UINT32 Length;
    UINT32 CharSize;
    UINT32 Height;
    UINT32 Width;
} LOS_PSF2_HEADER;

typedef struct
{
    UINT32 *FrameBuffer;
    UINT64 FrameBufferVirtualAddress;
    UINT64 FrameBufferSize;
    UINT32 Width;
    UINT32 Height;
    UINT32 PixelsPerScanLine;
    UINT32 PixelFormat;
    UINT32 CursorColumn;
    UINT32 CursorRow;
    UINT32 MaxColumns;
    UINT32 MaxRows;
    UINT32 CellWidth;
    UINT32 CellHeight;
    UINT32 GlyphWidth;
    UINT32 GlyphHeight;
    UINT32 FontGlyphCount;
    UINT32 FontBytesPerGlyph;
    UINT32 FontRowStride;
    const UINT8 *FontGlyphs;
    UINT32 FontScale;
    UINT64 LastDisplayedTimerTick;
    volatile UINT32 UpdateBusy;
    BOOLEAN FontLoaded;
    BOOLEAN TimerLive;
    BOOLEAN Ready;
} LOS_KERNEL_SCREEN_STATE;

static LOS_KERNEL_SCREEN_STATE LosKernelScreenState;

#define LOS_KERNEL_SCREEN_DEFAULT_FONT_SCALE 2U
#define LOS_KERNEL_SCREEN_LINE_SPACING 1U
#define LOS_KERNEL_SCREEN_BASE_INDENT_COLUMNS 1U
#define LOS_KERNEL_SCREEN_WRAP_INDENT_COLUMNS 8U
#define LOS_KERNEL_SCREEN_RIGHT_MARGIN_COLUMNS 1U
#define LOS_KERNEL_SCREEN_TIMER_ROW 1U
#define LOS_KERNEL_SCREEN_FIRST_LOG_ROW 3U

static void ApplyLineIndent(UINT32 IndentColumns);
static UINT32 GetWritableColumnLimit(void);
static UINT32 GetLastWritableLogRow(void);
static void ClearRow(UINT32 Row);
static void FillCellBackground(UINT32 Column, UINT32 Row, UINT32 Color);
static void DrawStaticScreenDecorations(void);
static void DrawTimerOverlayFrame(void);
static void ResetLogCursor(void);
static void ScrollLogRegionUp(void);
static void CopyPixelsForward(UINT32 *Destination, const UINT32 *Source, UINTN PixelCount);
static void FillPixels(UINT32 *Destination, UINT32 Color, UINTN PixelCount);
static BOOLEAN TryBeginScreenUpdate(void);
static void EndScreenUpdate(void);

static UINT64 GetRequiredFrameBufferBytes(UINT32 PixelsPerScanLine, UINT32 Height)
{
    return ((UINT64)PixelsPerScanLine * (UINT64)Height * 4ULL);
}

static UINT64 GetUsableFrameBufferBytes(UINT64 ReportedBytes, UINT32 PixelsPerScanLine, UINT32 Height)
{
    UINT64 RequiredBytes;

    RequiredBytes = GetRequiredFrameBufferBytes(PixelsPerScanLine, Height);
    if (ReportedBytes == 0ULL)
    {
        return RequiredBytes;
    }

    return ReportedBytes < RequiredBytes ? ReportedBytes : RequiredBytes;
}

static void TraceScreenInitValue(const char *Prefix, UINT64 Value)
{
    LosKernelSerialWriteText(Prefix);
    LosKernelSerialWriteHex64(Value);
    LosKernelSerialWriteText("\n");
}

static char ToUpperAscii(char Character)
{
    if (Character >= 'a' && Character <= 'z')
    {
        return (char)(Character - ('a' - 'A'));
    }
    return Character;
}

static void CopyPixelsForward(UINT32 *Destination, const UINT32 *Source, UINTN PixelCount)
{
    UINTN PairCount;
    UINTN Index;

    if (Destination == 0 || Source == 0 || PixelCount == 0U)
    {
        return;
    }

    PairCount = PixelCount / 2U;
    for (Index = 0U; Index < PairCount; ++Index)
    {
        ((UINT64 *)Destination)[Index] = ((const UINT64 *)Source)[Index];
    }

    if ((PixelCount & 1U) != 0U)
    {
        Destination[PixelCount - 1U] = Source[PixelCount - 1U];
    }
}

static void FillPixels(UINT32 *Destination, UINT32 Color, UINTN PixelCount)
{
    UINT64 PackedColor;
    UINTN PairCount;
    UINTN Index;

    if (Destination == 0 || PixelCount == 0U)
    {
        return;
    }

    PackedColor = ((UINT64)Color << 32) | (UINT64)Color;
    PairCount = PixelCount / 2U;
    for (Index = 0U; Index < PairCount; ++Index)
    {
        ((UINT64 *)Destination)[Index] = PackedColor;
    }

    if ((PixelCount & 1U) != 0U)
    {
        Destination[PixelCount - 1U] = Color;
    }
}

static BOOLEAN TryBeginScreenUpdate(void)
{
    if (LosKernelScreenState.Ready == 0U || LosKernelScreenState.UpdateBusy != 0U)
    {
        return 0U;
    }

    LosKernelScreenState.UpdateBusy = 1U;
    __asm__ __volatile__("" : : : "memory");
    return 1U;
}

static void EndScreenUpdate(void)
{
    __asm__ __volatile__("" : : : "memory");
    LosKernelScreenState.UpdateBusy = 0U;
}

static UINT8 GetGlyphRow(char Character, UINT32 Row)
{
    Character = ToUpperAscii(Character);
    switch (Character)
    {
        case 'A': { static const UINT8 R[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; return Row < 7U ? R[Row] : 0U; }
        case 'B': { static const UINT8 R[7] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}; return Row < 7U ? R[Row] : 0U; }
        case 'C': { static const UINT8 R[7] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; return Row < 7U ? R[Row] : 0U; }
        case 'D': { static const UINT8 R[7] = {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}; return Row < 7U ? R[Row] : 0U; }
        case 'E': { static const UINT8 R[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; return Row < 7U ? R[Row] : 0U; }
        case 'F': { static const UINT8 R[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; return Row < 7U ? R[Row] : 0U; }
        case 'G': { static const UINT8 R[7] = {0x0F,0x10,0x10,0x17,0x11,0x11,0x0F}; return Row < 7U ? R[Row] : 0U; }
        case 'H': { static const UINT8 R[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}; return Row < 7U ? R[Row] : 0U; }
        case 'I': { static const UINT8 R[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}; return Row < 7U ? R[Row] : 0U; }
        case 'J': { static const UINT8 R[7] = {0x1F,0x02,0x02,0x02,0x02,0x12,0x0C}; return Row < 7U ? R[Row] : 0U; }
        case 'K': { static const UINT8 R[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11}; return Row < 7U ? R[Row] : 0U; }
        case 'L': { static const UINT8 R[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; return Row < 7U ? R[Row] : 0U; }
        case 'M': { static const UINT8 R[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; return Row < 7U ? R[Row] : 0U; }
        case 'N': { static const UINT8 R[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11}; return Row < 7U ? R[Row] : 0U; }
        case 'O': { static const UINT8 R[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; return Row < 7U ? R[Row] : 0U; }
        case 'P': { static const UINT8 R[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; return Row < 7U ? R[Row] : 0U; }
        case 'Q': { static const UINT8 R[7] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}; return Row < 7U ? R[Row] : 0U; }
        case 'R': { static const UINT8 R[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; return Row < 7U ? R[Row] : 0U; }
        case 'S': { static const UINT8 R[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}; return Row < 7U ? R[Row] : 0U; }
        case 'T': { static const UINT8 R[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; return Row < 7U ? R[Row] : 0U; }
        case 'U': { static const UINT8 R[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; return Row < 7U ? R[Row] : 0U; }
        case 'V': { static const UINT8 R[7] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}; return Row < 7U ? R[Row] : 0U; }
        case 'W': { static const UINT8 R[7] = {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}; return Row < 7U ? R[Row] : 0U; }
        case 'X': { static const UINT8 R[7] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}; return Row < 7U ? R[Row] : 0U; }
        case 'Y': { static const UINT8 R[7] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}; return Row < 7U ? R[Row] : 0U; }
        case 'Z': { static const UINT8 R[7] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}; return Row < 7U ? R[Row] : 0U; }
        case '0': { static const UINT8 R[7] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}; return Row < 7U ? R[Row] : 0U; }
        case '1': { static const UINT8 R[7] = {0x04,0x0C,0x14,0x04,0x04,0x04,0x1F}; return Row < 7U ? R[Row] : 0U; }
        case '2': { static const UINT8 R[7] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}; return Row < 7U ? R[Row] : 0U; }
        case '3': { static const UINT8 R[7] = {0x1E,0x01,0x01,0x06,0x01,0x01,0x1E}; return Row < 7U ? R[Row] : 0U; }
        case '4': { static const UINT8 R[7] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}; return Row < 7U ? R[Row] : 0U; }
        case '5': { static const UINT8 R[7] = {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E}; return Row < 7U ? R[Row] : 0U; }
        case '6': { static const UINT8 R[7] = {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E}; return Row < 7U ? R[Row] : 0U; }
        case '7': { static const UINT8 R[7] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}; return Row < 7U ? R[Row] : 0U; }
        case '8': { static const UINT8 R[7] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}; return Row < 7U ? R[Row] : 0U; }
        case '9': { static const UINT8 R[7] = {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}; return Row < 7U ? R[Row] : 0U; }
        case '[': { static const UINT8 R[7] = {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}; return Row < 7U ? R[Row] : 0U; }
        case ']': { static const UINT8 R[7] = {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}; return Row < 7U ? R[Row] : 0U; }
        case '-': { static const UINT8 R[7] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}; return Row < 7U ? R[Row] : 0U; }
        case '.': { static const UINT8 R[7] = {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}; return Row < 7U ? R[Row] : 0U; }
        case ',': { static const UINT8 R[7] = {0x00,0x00,0x00,0x00,0x0C,0x0C,0x08}; return Row < 7U ? R[Row] : 0U; }
        case ':': { static const UINT8 R[7] = {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}; return Row < 7U ? R[Row] : 0U; }
        case '/': { static const UINT8 R[7] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10}; return Row < 7U ? R[Row] : 0U; }
        case '(': { static const UINT8 R[7] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02}; return Row < 7U ? R[Row] : 0U; }
        case ')': { static const UINT8 R[7] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08}; return Row < 7U ? R[Row] : 0U; }
        case ' ': return 0U;
        default:  { static const UINT8 R[7] = {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}; return Row < 7U ? R[Row] : 0U; }
    }
}

static void ClearScreen(void)
{
    UINT64 PixelCount;
    UINT64 Index;
    UINT64 MaximumPixelsFromBytes;

    if (LosKernelScreenState.Ready == 0U || LosKernelScreenState.FrameBuffer == 0)
    {
        return;
    }

    PixelCount = (UINT64)LosKernelScreenState.PixelsPerScanLine * (UINT64)LosKernelScreenState.Height;
    MaximumPixelsFromBytes = LosKernelScreenState.FrameBufferSize / 4ULL;
    if (MaximumPixelsFromBytes != 0ULL && PixelCount > MaximumPixelsFromBytes)
    {
        PixelCount = MaximumPixelsFromBytes;
    }

    for (Index = 0ULL; Index < PixelCount; ++Index)
    {
        LosKernelScreenState.FrameBuffer[Index] = 0x00000000U;
    }

    LosKernelScreenState.CursorColumn = 0U;
    LosKernelScreenState.CursorRow = 0U;
    ApplyLineIndent(LOS_KERNEL_SCREEN_BASE_INDENT_COLUMNS);
}

static UINT32 ComposePixelColor(UINT8 Red, UINT8 Green, UINT8 Blue)
{
    if (LosKernelScreenState.PixelFormat == PixelBlueGreenRedReserved8BitPerColor)
    {
        return ((UINT32)Blue) | ((UINT32)Green << 8U) | ((UINT32)Red << 16U);
    }

    return ((UINT32)Red) | ((UINT32)Green << 8U) | ((UINT32)Blue << 16U);
}

static UINT32 GetTextColor(void)
{
    return ComposePixelColor(0xFFU, 0xFFU, 0xFFU);
}

static UINT32 GetOkPrefixColor(void)
{
    return ComposePixelColor(0x30U, 0xD1U, 0x58U);
}

static UINT32 GetFailPrefixColor(void)
{
    return ComposePixelColor(0xFFU, 0x55U, 0x55U);
}


static void ApplyLineIndent(UINT32 IndentColumns)
{
    if (LosKernelScreenState.Ready == 0U)
    {
        return;
    }

    if (IndentColumns >= LosKernelScreenState.MaxColumns)
    {
        IndentColumns = LosKernelScreenState.MaxColumns > 0U
            ? (LosKernelScreenState.MaxColumns - 1U)
            : 0U;
    }

    if (LosKernelScreenState.CursorColumn < IndentColumns)
    {
        LosKernelScreenState.CursorColumn = IndentColumns;
    }
}

static UINT32 GetWritableColumnLimit(void)
{
    if (LosKernelScreenState.MaxColumns <= LOS_KERNEL_SCREEN_RIGHT_MARGIN_COLUMNS)
    {
        return 0U;
    }

    return LosKernelScreenState.MaxColumns - LOS_KERNEL_SCREEN_RIGHT_MARGIN_COLUMNS;
}

static void PutPixel(UINT32 X, UINT32 Y, UINT32 Color)
{
    if (LosKernelScreenState.Ready == 0U || LosKernelScreenState.FrameBuffer == 0)
    {
        return;
    }
    if (X >= LosKernelScreenState.Width || Y >= LosKernelScreenState.Height)
    {
        return;
    }

    LosKernelScreenState.FrameBuffer[((UINT64)Y * (UINT64)LosKernelScreenState.PixelsPerScanLine) + (UINT64)X] = Color;
}

static void ClearRow(UINT32 Row)
{
    UINT32 Column;

    if (LosKernelScreenState.Ready == 0U || Row >= LosKernelScreenState.MaxRows)
    {
        return;
    }

    for (Column = 0U; Column < LosKernelScreenState.MaxColumns; ++Column)
    {
        FillCellBackground(Column, Row, 0x00000000U);
    }
}
