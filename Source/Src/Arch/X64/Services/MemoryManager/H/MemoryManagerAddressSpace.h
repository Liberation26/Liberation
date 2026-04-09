/*
 * File Name: MemoryManagerAddressSpace.h
 * File Version: 0.3.11
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-07T07:24:34Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Implements a Liberation OS service component.
 */

#ifndef LOS_MEMORY_MANAGER_ADDRESS_SPACE_H
#define LOS_MEMORY_MANAGER_ADDRESS_SPACE_H

#include "MemoryManagerMain.h"

void LosMemoryManagerServiceCreateAddressSpace(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_REQUEST *Request,
    LOS_MEMORY_MANAGER_CREATE_ADDRESS_SPACE_RESULT *Result);
void LosMemoryManagerServiceMapPages(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_MAP_PAGES_REQUEST *Request,
    LOS_MEMORY_MANAGER_MAP_PAGES_RESULT *Result);
void LosMemoryManagerServiceUnmapPages(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_UNMAP_PAGES_REQUEST *Request,
    LOS_MEMORY_MANAGER_UNMAP_PAGES_RESULT *Result);
void LosMemoryManagerServiceProtectPages(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_PROTECT_PAGES_REQUEST *Request,
    LOS_MEMORY_MANAGER_PROTECT_PAGES_RESULT *Result);
void LosMemoryManagerServiceQueryMapping(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_QUERY_MAPPING_REQUEST *Request,
    LOS_MEMORY_MANAGER_QUERY_MAPPING_RESULT *Result);
void LosMemoryManagerServiceDestroyAddressSpace(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_REQUEST *Request,
    LOS_MEMORY_MANAGER_DESTROY_ADDRESS_SPACE_RESULT *Result);
void LosMemoryManagerServiceAttachStagedImage(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_REQUEST *Request,
    LOS_MEMORY_MANAGER_ATTACH_STAGED_IMAGE_RESULT *Result);
void LosMemoryManagerServiceAllocateAddressSpaceStack(
    LOS_MEMORY_MANAGER_SERVICE_STATE *State,
    const LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_REQUEST *Request,
    LOS_MEMORY_MANAGER_ALLOCATE_ADDRESS_SPACE_STACK_RESULT *Result);

#endif
