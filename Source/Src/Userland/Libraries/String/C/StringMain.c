/*
 * File Name: StringMain.c
 * File Version: 0.4.25
 * Author: OpenAI
 * Creation Timestamp: 2026-04-08T19:45:00Z
 * Last Update Timestamp: 2026-04-09T12:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements the external string library used by the shell.
 */

#include "StringMain.h"

static void LosStringLibraryZeroMemory(void *Buffer, UINTN Length)
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

static char LosStringLibraryUppercaseCharacter(char Character)
{
    if (Character >= 'a' && Character <= 'z')
    {
        return (char)(Character - ('a' - 'A'));
    }
    return Character;
}

UINT32 LosStringLibraryTransform(const LOS_STRING_LIBRARY_REQUEST *Request,
                                 LOS_STRING_LIBRARY_RESPONSE *Response)
{
    UINTN Index;
    BOOLEAN Changed = 0;

    if (Request == 0 || Response == 0)
    {
        return LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    }

    LosStringLibraryZeroMemory(Response, sizeof(*Response));
    Response->Version = LOS_STRING_LIBRARY_VERSION;
    Response->Status = LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER;
    Response->Signature = LOS_STRING_LIBRARY_RESPONSE_SIGNATURE;
    Response->Result = LOS_STRING_LIBRARY_RESULT_NONE;

    if (Request->Version != LOS_STRING_LIBRARY_VERSION ||
        Request->Signature != LOS_STRING_LIBRARY_REQUEST_SIGNATURE)
    {
        return Response->Status;
    }
    if (Request->Operation != LOS_STRING_LIBRARY_OPERATION_UPPERCASE)
    {
        Response->Status = LOS_STRING_LIBRARY_STATUS_UNSUPPORTED;
        return Response->Status;
    }

    for (Index = 0U; Index + 1U < sizeof(Response->Output) && Request->Input[Index] != 0; ++Index)
    {
        Response->Output[Index] = LosStringLibraryUppercaseCharacter(Request->Input[Index]);
        if (Response->Output[Index] != Request->Input[Index])
        {
            Changed = 1;
        }
    }

    Response->Status = LOS_STRING_LIBRARY_STATUS_SUCCESS;
    Response->Result = Changed ? LOS_STRING_LIBRARY_RESULT_CHANGED : LOS_STRING_LIBRARY_RESULT_UNCHANGED;
    if (Changed)
    {
        Response->Flags |= LOS_STRING_LIBRARY_FLAG_TEXT_CHANGED;
    }
    return Response->Status;
}

UINT64 LosStringLibraryBootstrapInvoke(const LOS_USER_IMAGE_CALL *Call)
{
    const LOS_STRING_LIBRARY_REQUEST *Request;
    LOS_STRING_LIBRARY_RESPONSE *Response;
    UINT32 Status;

    if (Call == 0 ||
        Call->Version != LOS_USER_IMAGE_CALL_VERSION ||
        Call->Signature != LOS_USER_IMAGE_CALL_SIGNATURE ||
        Call->CallKind != LOS_USER_IMAGE_CALL_KIND_LIBRARY)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }
    if (Call->RequestAddress == 0ULL || Call->ResponseAddress == 0ULL ||
        Call->RequestSize < sizeof(LOS_STRING_LIBRARY_REQUEST) ||
        Call->ResponseSize < sizeof(LOS_STRING_LIBRARY_RESPONSE))
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }

    Request = (const LOS_STRING_LIBRARY_REQUEST *)(UINTN)Call->RequestAddress;
    Response = (LOS_STRING_LIBRARY_RESPONSE *)(UINTN)Call->ResponseAddress;
    Status = LosStringLibraryTransform(Request, Response);
    if (Status == LOS_STRING_LIBRARY_STATUS_SUCCESS)
    {
        return LOS_USER_IMAGE_CALL_STATUS_SUCCESS;
    }
    if (Status == LOS_STRING_LIBRARY_STATUS_INVALID_PARAMETER)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }
    if (Status == LOS_STRING_LIBRARY_STATUS_TRUNCATED)
    {
        return LOS_USER_IMAGE_CALL_STATUS_TRUNCATED;
    }
    return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
}

#if !defined(LOS_EMBED_USER_IMAGE_BOOTSTRAP)
__attribute__((section(".text.start")))
UINT64 _start(const LOS_USER_IMAGE_CALL *Call)
{
    return LosStringLibraryBootstrapInvoke(Call);
}
#endif
