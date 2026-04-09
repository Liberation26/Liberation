/*
 * File Name: LoginMain.h
 * File Version: 0.4.25
 * Author: OpenAI
 * Creation Timestamp: 2026-04-08T18:45:00Z
 * Last Update Timestamp: 2026-04-09T12:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Declares the external login command for Liberation OS.
 */

#ifndef LOS_LOGIN_COMMAND_MAIN_H
#define LOS_LOGIN_COMMAND_MAIN_H

#include "Efi.h"
#include "CapabilitiesServiceAbi.h"
#include "LoginCommandAbi.h"
#include "UserImageAbi.h"

UINT32 LosLoginCommandAuthenticate(const LOS_LOGIN_COMMAND_REQUEST *Request,
                                   LOS_LOGIN_COMMAND_RESPONSE *Response);
UINT64 LosLoginCommandBootstrapInvoke(const LOS_USER_IMAGE_CALL *Call);
UINT64 _start(const LOS_USER_IMAGE_CALL *Call);

#endif
