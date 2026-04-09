/*
 * File Name: EfiSection01.h
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from Efi.h.
 */

#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64)
#define EFIAPI __attribute__((ms_abi))
#else
#define EFIAPI
#endif

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef UINT64 UINTN;
typedef UINT64 EFI_STATUS;
typedef UINT64 EFI_LBA;
typedef void *EFI_HANDLE;
typedef UINT16 CHAR16;
typedef UINT8 BOOLEAN;
typedef void VOID;

typedef struct
{
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8 Data4[8];
} EFI_GUID;

#define EFI_SUCCESS ((EFI_STATUS)0)
#define EFI_LOAD_ERROR ((EFI_STATUS)0x8000000000000001ULL)
#define EFI_INVALID_PARAMETER ((EFI_STATUS)0x8000000000000002ULL)
#define EFI_UNSUPPORTED ((EFI_STATUS)0x8000000000000003ULL)
#define EFI_BAD_BUFFER_SIZE ((EFI_STATUS)0x8000000000000004ULL)
#define EFI_BUFFER_TOO_SMALL ((EFI_STATUS)0x8000000000000005ULL)
#define EFI_NOT_READY ((EFI_STATUS)0x8000000000000006ULL)
#define EFI_DEVICE_ERROR ((EFI_STATUS)0x8000000000000007ULL)
#define EFI_WRITE_PROTECTED ((EFI_STATUS)0x8000000000000008ULL)
#define EFI_OUT_OF_RESOURCES ((EFI_STATUS)0x8000000000000009ULL)
#define EFI_VOLUME_CORRUPTED ((EFI_STATUS)0x800000000000000AULL)
#define EFI_VOLUME_FULL ((EFI_STATUS)0x800000000000000BULL)
#define EFI_NO_MEDIA ((EFI_STATUS)0x800000000000000CULL)
#define EFI_MEDIA_CHANGED ((EFI_STATUS)0x800000000000000DULL)
#define EFI_NOT_FOUND ((EFI_STATUS)0x8000000000000014ULL)
#define EFI_ABORTED ((EFI_STATUS)0x8000000000000015ULL)
#define EFI_ERROR(Status) (((Status) & 0x8000000000000000ULL) != 0)

#define EFI_BLACK 0x00U
#define EFI_BLUE 0x01U
#define EFI_GREEN 0x02U
#define EFI_CYAN 0x03U
#define EFI_RED 0x04U
#define EFI_MAGENTA 0x05U
#define EFI_BROWN 0x06U
#define EFI_LIGHTGRAY 0x07U
#define EFI_BRIGHT 0x08U
#define EFI_LIGHTBLUE (EFI_BLUE | EFI_BRIGHT)
#define EFI_LIGHTGREEN (EFI_GREEN | EFI_BRIGHT)
#define EFI_LIGHTCYAN (EFI_CYAN | EFI_BRIGHT)
#define EFI_LIGHTRED (EFI_RED | EFI_BRIGHT)
#define EFI_LIGHTMAGENTA (EFI_MAGENTA | EFI_BRIGHT)
#define EFI_YELLOW (EFI_BROWN | EFI_BRIGHT)
#define EFI_WHITE (EFI_LIGHTGRAY | EFI_BRIGHT)
#define EFI_TEXT_ATTR(Foreground, Background) ((UINTN)((Foreground) | ((Background) << 4U)))

typedef struct
{
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 Crc32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct EFI_RUNTIME_SERVICES EFI_RUNTIME_SERVICES;
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef struct EFI_LOADED_IMAGE_PROTOCOL EFI_LOADED_IMAGE_PROTOCOL;
typedef struct EFI_BLOCK_IO_MEDIA EFI_BLOCK_IO_MEDIA;
typedef struct EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL;
typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;
typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;
typedef struct EFI_GRAPHICS_OUTPUT_MODE_INFORMATION EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct
{
    UINT32 Type;
    UINT32 Pad;
    UINT64 PhysicalStart;
    UINT64 VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef struct
{
    UINT32 RedMask;
    UINT32 GreenMask;
    UINT32 BlueMask;
    UINT32 ReservedMask;
} EFI_PIXEL_BITMASK;

typedef enum
{
    PixelRedGreenBlueReserved8BitPerColor = 0,
    PixelBlueGreenRedReserved8BitPerColor = 1,
    PixelBitMask = 2,
    PixelBltOnly = 3,
    PixelFormatMax = 4
} EFI_GRAPHICS_PIXEL_FORMAT;

struct EFI_GRAPHICS_OUTPUT_MODE_INFORMATION
{
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK PixelInformation;
    UINT32 PixelsPerScanLine;
};

struct EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE
{
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    UINT64 FrameBufferBase;
    UINTN FrameBufferSize;
};

struct EFI_GRAPHICS_OUTPUT_PROTOCOL
{
    VOID *QueryMode;
    VOID *SetMode;
    VOID *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

typedef enum
{
    EfiResetCold = 0,
    EfiResetWarm = 1,
    EfiResetShutdown = 2,
    EfiResetPlatformSpecific = 3
} EFI_RESET_TYPE;

typedef struct
{
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL
{
    EFI_STATUS (EFIAPI *Reset)(
        EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
        BOOLEAN ExtendedVerification);

    EFI_STATUS (EFIAPI *ReadKeyStroke)(
        EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
        EFI_INPUT_KEY *Key);

    VOID *WaitForKey;
};

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
{
    EFI_STATUS (EFIAPI *Reset)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        BOOLEAN ExtendedVerification);

    EFI_STATUS (EFIAPI *OutputString)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        CHAR16 *String);

    EFI_STATUS (EFIAPI *TestString)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        CHAR16 *String);

    EFI_STATUS (EFIAPI *QueryMode)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        UINTN ModeNumber,
        UINTN *Columns,
        UINTN *Rows);

    EFI_STATUS (EFIAPI *SetMode)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        UINTN ModeNumber);

    EFI_STATUS (EFIAPI *SetAttribute)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        UINTN Attribute);

    EFI_STATUS (EFIAPI *ClearScreen)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);

    EFI_STATUS (EFIAPI *SetCursorPosition)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        UINTN Column,
        UINTN Row);

    EFI_STATUS (EFIAPI *EnableCursor)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        BOOLEAN Visible);

    VOID *Mode;
};

typedef enum
{
    AllocateAnyPages = 0,
    AllocateMaxAddress = 1,
    AllocateAddress = 2
} EFI_ALLOCATE_TYPE;

typedef enum
{
    EfiReservedMemoryType = 0,
    EfiLoaderCode = 1,
    EfiLoaderData = 2
} EFI_MEMORY_TYPE;
