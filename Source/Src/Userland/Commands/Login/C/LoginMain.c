/*
 * File Name: LoginMain.c
 * File Version: 0.4.25
 * Author: OpenAI
 * Creation Timestamp: 2026-04-08T18:45:00Z
 * Last Update Timestamp: 2026-04-09T12:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements the external login command for Liberation OS.
 */

#include "LoginMain.h"

__attribute__((weak)) UINT32 LosCapabilitiesServiceSubmitAccessRequest(const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                                                       LOS_CAPABILITIES_ACCESS_RESULT *Result);
__attribute__((weak)) UINT32 LosCapabilitiesServiceCheckAccess(const LOS_CAPABILITIES_ACCESS_REQUEST *Request,
                                                               LOS_CAPABILITIES_ACCESS_RESULT *Result);

static void LosLoginZeroMemory(void *Buffer, UINTN Length)
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

static void LosLoginCopyText(char *Destination, UINTN DestinationLength, const char *Source)
{
    UINTN Index;
    if (Destination == 0 || DestinationLength == 0U)
    {
        return;
    }
    for (Index = 0U; Index < DestinationLength; ++Index)
    {
        Destination[Index] = 0;
    }
    if (Source == 0)
    {
        return;
    }
    for (Index = 0U; Index + 1U < DestinationLength && Source[Index] != 0; ++Index)
    {
        Destination[Index] = Source[Index];
    }
}

static UINT32 LosLoginCheckCapability(const char *UserName, LOS_CAPABILITIES_ACCESS_RESULT *Result)
{
    LOS_CAPABILITIES_ACCESS_REQUEST Request;

    if (UserName == 0 || UserName[0] == 0 || Result == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    LosLoginZeroMemory(&Request, sizeof(Request));
    LosLoginZeroMemory(Result, sizeof(*Result));

    Request.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    Request.PrincipalType = LOS_CAPABILITIES_PRINCIPAL_TYPE_USER;
    Request.AccessRight = LOS_CAPABILITIES_ACCESS_RIGHT_QUERY;
    Request.PrincipalId = 0ULL;
    LosLoginCopyText(Request.PrincipalName, sizeof(Request.PrincipalName), UserName);
    LosLoginCopyText(Request.Namespace, sizeof(Request.Namespace), "session");
    LosLoginCopyText(Request.Name, sizeof(Request.Name), "login");

    if (LosCapabilitiesServiceSubmitAccessRequest != 0)
    {
        return LosCapabilitiesServiceSubmitAccessRequest(&Request, Result);
    }
    if (LosCapabilitiesServiceCheckAccess != 0)
    {
        return LosCapabilitiesServiceCheckAccess(&Request, Result);
    }
    return LOS_CAPABILITIES_SERVICE_STATUS_UNSUPPORTED;
}

UINT32 LosLoginCommandAuthenticate(const LOS_LOGIN_COMMAND_REQUEST *Request,
                                   LOS_LOGIN_COMMAND_RESPONSE *Response)
{
    UINT32 CapabilityStatus;
    LOS_CAPABILITIES_ACCESS_RESULT AccessResult;

    if (Request == 0 || Response == 0)
    {
        return LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    }

    LosLoginZeroMemory(&AccessResult, sizeof(AccessResult));
    LosLoginZeroMemory(Response, sizeof(*Response));
    Response->Version = LOS_LOGIN_COMMAND_VERSION;
    Response->Signature = LOS_LOGIN_COMMAND_RESPONSE_SIGNATURE;
    Response->Result = LOS_LOGIN_COMMAND_RESULT_NONE;
    Response->Status = LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER;
    LosLoginCopyText(Response->Message, sizeof(Response->Message), "usage: login <user> <password>");

    if (Request->Version != LOS_LOGIN_COMMAND_VERSION || Request->Signature != LOS_LOGIN_COMMAND_REQUEST_SIGNATURE)
    {
        return Response->Status;
    }
    if (Request->UserName[0] == 0 || Request->Password[0] == 0)
    {
        return Response->Status;
    }
    if (Request->Password[0] != 'l' || Request->Password[1] != 'i' || Request->Password[2] != 'b' ||
        Request->Password[3] != 'e' || Request->Password[4] != 'r' || Request->Password[5] != 'a' ||
        Request->Password[6] != 't' || Request->Password[7] != 'i' || Request->Password[8] != 'o' ||
        Request->Password[9] != 'n' || Request->Password[10] != 0)
    {
        Response->Status = LOS_CAPABILITIES_SERVICE_STATUS_ACCESS_DENIED;
        Response->Result = LOS_LOGIN_COMMAND_RESULT_DENIED;
        LosLoginCopyText(Response->Message, sizeof(Response->Message), "login denied");
        return Response->Status;
    }

    CapabilityStatus = LosLoginCheckCapability(Request->UserName, &AccessResult);
    if (CapabilityStatus != LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS ||
        AccessResult.Status != LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS ||
        AccessResult.Granted == 0U)
    {
        Response->Status = LOS_CAPABILITIES_SERVICE_STATUS_ACCESS_DENIED;
        Response->Result = LOS_LOGIN_COMMAND_RESULT_DENIED;
        if (CapabilityStatus == LOS_CAPABILITIES_SERVICE_STATUS_UNSUPPORTED)
        {
            LosLoginCopyText(Response->Message, sizeof(Response->Message), "login denied: capability service unavailable");
        }
        else
        {
            LosLoginCopyText(Response->Message, sizeof(Response->Message), "login denied by capability policy");
        }
        return Response->Status;
    }

    Response->Status = LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS;
    Response->Result = LOS_LOGIN_COMMAND_RESULT_AUTHENTICATED;
    LosLoginCopyText(Response->Message, sizeof(Response->Message), "login authenticated by capability service");
    return Response->Status;
}

UINT64 LosLoginCommandBootstrapInvoke(const LOS_USER_IMAGE_CALL *Call)
{
    const char *Arguments;
    LOS_LOGIN_COMMAND_REQUEST Request;
    LOS_LOGIN_COMMAND_RESPONSE Response;
    char *Output;
    UINT64 *CommandResult;
    UINTN Index;
    UINT32 Status;

    if (Call == 0 ||
        Call->Version != LOS_USER_IMAGE_CALL_VERSION ||
        Call->Signature != LOS_USER_IMAGE_CALL_SIGNATURE ||
        Call->CallKind != LOS_USER_IMAGE_CALL_KIND_COMMAND)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }
    if (Call->ResponseAddress == 0ULL || Call->ResponseSize == 0ULL)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }

    Arguments = (const char *)(UINTN)Call->RequestAddress;
    Output = (char *)(UINTN)Call->ResponseAddress;
    CommandResult = (UINT64 *)(UINTN)Call->ResultAddress;

    LosLoginZeroMemory(&Request, sizeof(Request));
    LosLoginZeroMemory(&Response, sizeof(Response));
    Request.Version = LOS_LOGIN_COMMAND_VERSION;
    Request.Signature = LOS_LOGIN_COMMAND_REQUEST_SIGNATURE;

    if (Arguments != 0)
    {
        while (*Arguments == ' ' || *Arguments == '\t')
        {
            ++Arguments;
        }
        for (Index = 0U; Index + 1U < sizeof(Request.UserName) && Arguments[Index] != 0 && Arguments[Index] != ' ' && Arguments[Index] != '\t'; ++Index)
        {
            Request.UserName[Index] = Arguments[Index];
        }
        Arguments += Index;
        while (*Arguments == ' ' || *Arguments == '\t')
        {
            ++Arguments;
        }
        for (Index = 0U; Index + 1U < sizeof(Request.Password) && Arguments[Index] != 0 && Arguments[Index] != ' ' && Arguments[Index] != '\t'; ++Index)
        {
            Request.Password[Index] = Arguments[Index];
        }
    }

    Status = LosLoginCommandAuthenticate(&Request, &Response);
    if (Output != 0 && Call->ResponseSize != 0ULL)
    {
        LosLoginCopyText(Output, (UINTN)Call->ResponseSize, Response.Message);
    }
    if (CommandResult != 0 && Call->ResultSize >= sizeof(*CommandResult))
    {
        *CommandResult = (UINT64)Response.Result;
    }

    if (Status == LOS_CAPABILITIES_SERVICE_STATUS_SUCCESS)
    {
        return LOS_USER_IMAGE_CALL_STATUS_SUCCESS;
    }
    if (Status == LOS_CAPABILITIES_SERVICE_STATUS_INVALID_PARAMETER)
    {
        return LOS_USER_IMAGE_CALL_STATUS_INVALID_PARAMETER;
    }
    if (Status == LOS_CAPABILITIES_SERVICE_STATUS_ACCESS_DENIED)
    {
        return LOS_USER_IMAGE_CALL_STATUS_ACCESS_DENIED;
    }
    return LOS_USER_IMAGE_CALL_STATUS_LAUNCH_FAILED;
}

#if !defined(LOS_EMBED_USER_IMAGE_BOOTSTRAP)
__attribute__((section(".text.start")))
UINT64 _start(const LOS_USER_IMAGE_CALL *Call)
{
    return LosLoginCommandBootstrapInvoke(Call);
}
#endif
