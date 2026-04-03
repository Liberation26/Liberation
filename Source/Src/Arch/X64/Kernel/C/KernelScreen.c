#include "KernelMain.h"
#include "VirtualMemory.h"

#define LOS_KERNEL_SCREEN_VIRTUAL_BASE 0xFFFF900000000000ULL
#define LOS_KERNEL_SCREEN_DEFAULT_CELL_WIDTH 8U
#define LOS_KERNEL_SCREEN_DEFAULT_CELL_HEIGHT 8U
#define LOS_KERNEL_SCREEN_DEFAULT_GLYPH_WIDTH 5U
#define LOS_KERNEL_SCREEN_DEFAULT_GLYPH_HEIGHT 7U
#define LOS_KERNEL_PAGE_WRITABLE 0x002ULL
#define LOS_KERNEL_PAGE_WRITE_THROUGH 0x008ULL
#define LOS_KERNEL_PAGE_CACHE_DISABLE 0x010ULL
#define LOS_PSF2_MAGIC 0x864AB572U

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
    BOOLEAN FontLoaded;
    BOOLEAN Ready;
} LOS_KERNEL_SCREEN_STATE;

static LOS_KERNEL_SCREEN_STATE LosKernelScreenState;

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

static void AdvanceLine(void)
{
    LosKernelScreenState.CursorColumn = 0U;
    LosKernelScreenState.CursorRow += 1U;
    if (LosKernelScreenState.CursorRow >= LosKernelScreenState.MaxRows)
    {
        ClearScreen();
    }
}

static void RenderFallbackGlyph(char Character, UINT32 BaseX, UINT32 BaseY, UINT32 Color)
{
    UINT32 GlyphRow;
    UINT32 GlyphColumn;

    for (GlyphRow = 0U; GlyphRow < LOS_KERNEL_SCREEN_DEFAULT_GLYPH_HEIGHT; ++GlyphRow)
    {
        UINT8 RowBits;
        RowBits = GetGlyphRow(Character, GlyphRow);
        for (GlyphColumn = 0U; GlyphColumn < LOS_KERNEL_SCREEN_DEFAULT_GLYPH_WIDTH; ++GlyphColumn)
        {
            if ((RowBits & (UINT8)(1U << (4U - GlyphColumn))) != 0U)
            {
                PutPixel(BaseX + GlyphColumn + 1U, BaseY + GlyphRow, Color);
            }
        }
    }
}

static void RenderPsfGlyph(UINT8 Character, UINT32 BaseX, UINT32 BaseY, UINT32 Color)
{
    const UINT8 *Glyph;
    UINT32 Row;
    UINT32 Column;

    if (LosKernelScreenState.FontLoaded == 0U || LosKernelScreenState.FontGlyphs == 0)
    {
        return;
    }

    if ((UINT32)Character >= LosKernelScreenState.FontGlyphCount)
    {
        Character = (UINT8)'?';
    }

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
                PutPixel(BaseX + Column, BaseY + Row, Color);
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

    if (LosKernelScreenState.CursorColumn >= LosKernelScreenState.MaxColumns)
    {
        AdvanceLine();
    }

    BaseX = LosKernelScreenState.CursorColumn * LosKernelScreenState.CellWidth;
    BaseY = LosKernelScreenState.CursorRow * LosKernelScreenState.CellHeight;

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

    StartX = Column * LosKernelScreenState.CellWidth;
    StartY = Row * LosKernelScreenState.CellHeight;
    for (Y = 0U; Y < LosKernelScreenState.CellHeight; ++Y)
    {
        for (X = 0U; X < LosKernelScreenState.CellWidth; ++X)
        {
            PutPixel(StartX + X, StartY + Y, Color);
        }
    }
}

static void DrawCharacterAt(UINT32 Column, UINT32 Row, char Character, UINT32 Color)
{
    UINT32 SavedColumn;
    UINT32 SavedRow;

    if (LosKernelScreenState.Ready == 0U)
    {
        return;
    }

    SavedColumn = LosKernelScreenState.CursorColumn;
    SavedRow = LosKernelScreenState.CursorRow;
    LosKernelScreenState.CursorColumn = Column;
    LosKernelScreenState.CursorRow = Row;
    PutCharacterColored(Character, Color);
    LosKernelScreenState.CursorColumn = SavedColumn;
    LosKernelScreenState.CursorRow = SavedRow;
}

static void PutTextColored(const char *Text, UINT32 Color)
{
    UINTN Index;

    if (Text == 0)
    {
        return;
    }

    for (Index = 0U; Text[Index] != '\0'; ++Index)
    {
        PutCharacterColored(Text[Index], Color);
    }
}

static void PutText(const char *Text)
{
    PutTextColored(Text, GetTextColor());
}

static void DrawTextAt(UINT32 Column, UINT32 Row, const char *Text, UINT32 Color)
{
    UINT32 SavedColumn;
    UINT32 SavedRow;
    UINTN Index;

    if (LosKernelScreenState.Ready == 0U || Text == 0)
    {
        return;
    }

    SavedColumn = LosKernelScreenState.CursorColumn;
    SavedRow = LosKernelScreenState.CursorRow;
    LosKernelScreenState.CursorColumn = Column;
    LosKernelScreenState.CursorRow = Row;

    for (Index = 0U; Text[Index] != '\0'; ++Index)
    {
        PutCharacterColored(Text[Index], Color);
    }

    LosKernelScreenState.CursorColumn = SavedColumn;
    LosKernelScreenState.CursorRow = SavedRow;
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
    LosKernelScreenState.CellHeight = LOS_KERNEL_SCREEN_DEFAULT_CELL_HEIGHT;
    LosKernelScreenState.GlyphWidth = LOS_KERNEL_SCREEN_DEFAULT_GLYPH_WIDTH;
    LosKernelScreenState.GlyphHeight = LOS_KERNEL_SCREEN_DEFAULT_GLYPH_HEIGHT;
    LosKernelScreenState.FontGlyphCount = 0U;
    LosKernelScreenState.FontBytesPerGlyph = 0U;
    LosKernelScreenState.FontRowStride = 0U;
    LosKernelScreenState.FontGlyphs = 0;
    LosKernelScreenState.FontLoaded = 0U;
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
    LosKernelScreenState.CellHeight = Header->Height;
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

    if (LosKernelScreenState.Ready == 0U || LosKernelScreenState.MaxColumns == 0U)
    {
        return;
    }

    SpinnerColumn = LosKernelScreenState.MaxColumns - 1U;
    SpinnerCharacter = LosSpinnerFrames[(UINTN)(TickCount & 0x3ULL)];
    FillCellBackground(SpinnerColumn, 0U, 0x00000000U);
    DrawCharacterAt(SpinnerColumn, 0U, SpinnerCharacter, GetOkPrefixColor());
}

void LosKernelScreenUpdateTimer(UINT64 TickCount, UINT64 TargetHz, BOOLEAN InterruptsLive)
{
    UINT32 StatusColor;
    UINT32 Row;

    if (LosKernelScreenState.Ready == 0U || LosKernelScreenState.MaxColumns < 30U || LosKernelScreenState.MaxRows < 3U)
    {
        return;
    }

    StatusColor = InterruptsLive != 0U ? GetOkPrefixColor() : GetFailPrefixColor();
    Row = 1U;

    DrawTextAt(0U, Row, "TIMER IRQ ", GetTextColor());
    DrawTextAt(10U, Row, InterruptsLive != 0U ? "LIVE" : "WAIT", StatusColor);
    DrawTextAt(16U, Row, "HZ ", GetTextColor());
    DrawUnsignedAt(19U, Row, TargetHz, 3U, GetTextColor());
    DrawTextAt(24U, Row, "TICKS ", GetTextColor());
    DrawUnsignedAt(30U, Row, TickCount, 10U, GetTextColor());
}

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
    LosKernelScreenState.MaxColumns = BootContext->FrameBufferWidth / LosKernelScreenState.CellWidth;
    LosKernelScreenState.MaxRows = BootContext->FrameBufferHeight / LosKernelScreenState.CellHeight;
    LosKernelScreenState.Ready = (LosKernelScreenState.MaxColumns != 0U && LosKernelScreenState.MaxRows != 0U) ? 1U : 0U;
    if (LosKernelScreenState.Ready == 0U)
    {
        LosKernelSerialWriteText("[KernelScreen] Screen geometry was too small for the text grid.\n");
        return;
    }

    TraceScreenInitValue("[KernelScreen] Text cell width: ", (UINT64)LosKernelScreenState.CellWidth);
    TraceScreenInitValue("[KernelScreen] Text cell height: ", (UINT64)LosKernelScreenState.CellHeight);
    TraceScreenInitValue("[KernelScreen] Max columns: ", (UINT64)LosKernelScreenState.MaxColumns);
    TraceScreenInitValue("[KernelScreen] Max rows: ", (UINT64)LosKernelScreenState.MaxRows);

    ClearScreen();
    DrawInitializationProbe();
    PutText("LIBERATION KERNEL SCREEN ONLINE\n");
    LosKernelSerialWriteText("[KernelScreen] Screen path initialized.\n");
}

static void WriteStatusLine(const char *Prefix, UINT32 PrefixColor, const char *Text)
{
    if (LosKernelScreenState.Ready == 0U)
    {
        return;
    }

    PutTextColored(Prefix, PrefixColor);
    PutCharacter(' ');
    PutText(Text);
    PutCharacter('\n');
}

void LosKernelStatusScreenWriteOk(const char *Text)
{
    WriteStatusLine("[OK]", GetOkPrefixColor(), Text);
}

void LosKernelStatusScreenWriteFail(const char *Text)
{
    WriteStatusLine("[FAIL]", GetFailPrefixColor(), Text);
}
