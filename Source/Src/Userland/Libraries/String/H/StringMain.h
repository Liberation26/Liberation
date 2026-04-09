/*
 * File Name: StringMain.h
 * File Version: 0.4.25
 * Author: OpenAI
 * Creation Timestamp: 2026-04-08T19:45:00Z
 * Last Update Timestamp: 2026-04-09T12:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Declares the external string-library entry points for Liberation OS.
 */

#ifndef LOS_USERLAND_STRING_LIBRARY_MAIN_H
#define LOS_USERLAND_STRING_LIBRARY_MAIN_H

#include "Efi.h"
#include "StringLibraryAbi.h"
#include "UserImageAbi.h"

UINT32 LosStringLibraryTransform(const LOS_STRING_LIBRARY_REQUEST *Request,
                                 LOS_STRING_LIBRARY_RESPONSE *Response);
UINT64 LosStringLibraryBootstrapInvoke(const LOS_USER_IMAGE_CALL *Call);
UINT64 _start(const LOS_USER_IMAGE_CALL *Call);

#endif
