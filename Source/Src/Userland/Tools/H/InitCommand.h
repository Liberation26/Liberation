/*
 * File Name: InitCommand.h
 * File Version: 0.4.7
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T08:42:21Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS userland component.
 */

#ifndef LOS_USERLAND_INIT_COMMAND_H
#define LOS_USERLAND_INIT_COMMAND_H

#include "InitCommandAbi.h"
#include "ShellServiceAbi.h"

void LosInitCommandMain(const LOS_INIT_COMMAND_CONTEXT *Context);

#endif
