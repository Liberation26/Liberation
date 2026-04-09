/*
 * File Name: LoginCommandAbi.h
 * File Version: 0.4.10
 * Author: OpenAI
 * Creation Timestamp: 2026-04-08T19:05:00Z
 * Last Update Timestamp: 2026-04-08T19:05:00Z
 * Operating System Name: Liberation OS
 * Purpose: Declares the external login command contract for Liberation OS.
 */

#ifndef LOS_PUBLIC_LOGIN_COMMAND_ABI_H
#define LOS_PUBLIC_LOGIN_COMMAND_ABI_H

#include "Efi.h"

#define LOS_LOGIN_COMMAND_VERSION 1U

#define LOS_LOGIN_COMMAND_REQUEST_SIGNATURE  0x4C4F47494E525131ULL
#define LOS_LOGIN_COMMAND_RESPONSE_SIGNATURE 0x4C4F47494E525331ULL

#define LOS_LOGIN_COMMAND_RESULT_NONE            0ULL
#define LOS_LOGIN_COMMAND_RESULT_AUTHENTICATED   1ULL
#define LOS_LOGIN_COMMAND_RESULT_DENIED          2ULL
#define LOS_LOGIN_COMMAND_RESULT_BAD_REQUEST     3ULL

typedef struct
{
    UINT32 Version;
    UINT32 Flags;
    UINT32 Reserved0;
    UINT32 Reserved1;
    UINT64 Signature;
    char UserName[32];
    char Password[32];
} LOS_LOGIN_COMMAND_REQUEST;

typedef struct
{
    UINT32 Version;
    UINT32 Status;
    UINT32 Reserved0;
    UINT32 Reserved1;
    UINT64 Signature;
    UINT64 Result;
    char Message[96];
} LOS_LOGIN_COMMAND_RESPONSE;

#endif
