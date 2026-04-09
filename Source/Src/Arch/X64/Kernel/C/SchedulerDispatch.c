/*
 * File Name: SchedulerDispatch.c
 * File Version: 0.3.12
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T10:02:19Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements kernel functionality for Liberation OS.
 */



#include "SchedulerInternal.h"
#include "InterruptsInternal.h"

#if defined(__GNUC__)

#include "SchedulerDispatchSections/SchedulerDispatchSection01.c"
#include "SchedulerDispatchSections/SchedulerDispatchSection02.c"
