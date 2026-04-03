#include "KernelMain.h"
#include "VirtualMemory.h"

#define LOS_KERNEL_SCREEN_VIRTUAL_BASE 0xFFFFFF003F000000ULL
#define LOS_KERNEL_SCREEN_CELL_WIDTH 8U
#define LOS_KERNEL_SCREEN_CELL_HEIGHT 8U
#define LOS_KERNEL_PAGE_WRITABLE 0x002ULL
#define LOS_KERNEL_PAGE_WRITE_THROUGH 0x008ULL
#define LOS_KERNEL_PAGE_CACHE_DISABLE 0x010ULL

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
    BOOLEAN Ready;
} LOS_KERNEL_SCREEN_STATE;

static LOS_KERNEL_SCREEN_STATE LosKernelScreenState;

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

    if (LosKernelScreenState.Ready == 0U || LosKernelScreenState.FrameBuffer == 0)
    {
        return;
    }

    PixelCount = (UINT64)LosKernelScreenState.PixelsPerScanLine * (UINT64)LosKernelScreenState.Height;
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

static void PutCharacterColored(char Character, UINT32 Color)
{
    UINT32 GlyphRow;
    UINT32 GlyphColumn;
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

    BaseX = LosKernelScreenState.CursorColumn * LOS_KERNEL_SCREEN_CELL_WIDTH;
    BaseY = LosKernelScreenState.CursorRow * LOS_KERNEL_SCREEN_CELL_HEIGHT;

    for (GlyphRow = 0U; GlyphRow < 7U; ++GlyphRow)
    {
        UINT8 RowBits;
        RowBits = GetGlyphRow(Character, GlyphRow);
        for (GlyphColumn = 0U; GlyphColumn < 5U; ++GlyphColumn)
        {
            if ((RowBits & (UINT8)(1U << (4U - GlyphColumn))) != 0U)
            {
                PutPixel(BaseX + GlyphColumn + 1U, BaseY + GlyphRow, Color);
            }
        }
    }

    LosKernelScreenState.CursorColumn += 1U;
}

static void PutCharacter(char Character)
{
    PutCharacterColored(Character, GetTextColor());
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

void LosKernelInitializeScreen(const LOS_BOOT_CONTEXT *BootContext)
{
    LOS_X64_MAP_PAGES_REQUEST MapRequest;
    LOS_X64_MAP_PAGES_RESULT MapResult;
    UINT64 PhysicalBase;
    UINT64 PhysicalOffset;
    UINT64 MappingBytes;
    UINT64 PageCount;

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
    LosKernelScreenState.Ready = 0U;

    if (BootContext == 0 ||
        BootContext->FrameBufferPhysicalAddress == 0ULL ||
        BootContext->FrameBufferSize == 0ULL ||
        BootContext->FrameBufferWidth == 0U ||
        BootContext->FrameBufferHeight == 0U ||
        BootContext->FrameBufferPixelsPerScanLine == 0U)
    {
        return;
    }

    PhysicalBase = BootContext->FrameBufferPhysicalAddress & ~0xFFFULL;
    PhysicalOffset = BootContext->FrameBufferPhysicalAddress - PhysicalBase;
    MappingBytes = PhysicalOffset + BootContext->FrameBufferSize;
    PageCount = (MappingBytes + 4095ULL) / 4096ULL;

    MapRequest.PageMapLevel4PhysicalAddress = 0ULL;
    MapRequest.VirtualAddress = LOS_KERNEL_SCREEN_VIRTUAL_BASE;
    MapRequest.PhysicalAddress = PhysicalBase;
    MapRequest.PageCount = PageCount;
    MapRequest.PageFlags = LOS_KERNEL_PAGE_WRITABLE | LOS_KERNEL_PAGE_WRITE_THROUGH | LOS_KERNEL_PAGE_CACHE_DISABLE;
    MapRequest.Flags = LOS_X64_MAP_PAGES_FLAG_ALLOW_REMAP;
    MapRequest.Reserved = 0U;
    LosX64MapPages(&MapRequest, &MapResult);
    if (MapResult.Status != LOS_X64_MEMORY_OPERATION_STATUS_SUCCESS)
    {
        return;
    }

    LosKernelScreenState.FrameBufferVirtualAddress = LOS_KERNEL_SCREEN_VIRTUAL_BASE + PhysicalOffset;
    LosKernelScreenState.FrameBuffer = (UINT32 *)(UINTN)LosKernelScreenState.FrameBufferVirtualAddress;
    LosKernelScreenState.FrameBufferSize = BootContext->FrameBufferSize;
    LosKernelScreenState.Width = BootContext->FrameBufferWidth;
    LosKernelScreenState.Height = BootContext->FrameBufferHeight;
    LosKernelScreenState.PixelsPerScanLine = BootContext->FrameBufferPixelsPerScanLine;
    LosKernelScreenState.PixelFormat = BootContext->FrameBufferPixelFormat;
    LosKernelScreenState.MaxColumns = BootContext->FrameBufferWidth / LOS_KERNEL_SCREEN_CELL_WIDTH;
    LosKernelScreenState.MaxRows = BootContext->FrameBufferHeight / LOS_KERNEL_SCREEN_CELL_HEIGHT;
    LosKernelScreenState.Ready = (LosKernelScreenState.MaxColumns != 0U && LosKernelScreenState.MaxRows != 0U) ? 1U : 0U;
    ClearScreen();
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
