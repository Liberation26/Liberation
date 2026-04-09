/*
 * File Name: MonitorCapabilitiesSection01Section02.c
 * File Version: 0.0.1
 * Author: OpenAI
 * Email: dave66samaa@gmail.com
 * Creation Timestamp: 2026-04-09T19:40:00Z
 * Last Update Timestamp: 2026-04-09T19:40:00Z
 * Operating System Name: Liberation OS
 * Purpose: Contains a split section extracted from MonitorCapabilitiesSection01.c.
 */

EFI_STATUS LosMonitorLoadCapabilitiesFromEsp(EFI_HANDLE DeviceHandle,
                                             EFI_SYSTEM_TABLE *SystemTable,
                                             LOS_BOOT_CONTEXT *BootContext)
{
    EFI_FILE_PROTOCOL *Root;
    EFI_STATUS Status;
    UINT64 FilePhysicalAddress;
    UINT64 FileSize;
    const char *Buffer;
    const char *Cursor;
    const char *End;
    LOS_CAPABILITY_GRANT_BLOCK *CurrentBlock;
    UINT32 BlockIndex;

    if (SystemTable == 0 || SystemTable->BootServices == 0 || BootContext == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    BootContext->Capabilities.Version = LOS_CAPABILITIES_SERVICE_VERSION;
    BootContext->Capabilities.Flags = LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP;
    BootContext->Capabilities.BlockCount = 0U;
    BootContext->Capabilities.Capacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_BLOCKS;
    BootContext->Capabilities.AssignmentCount = 0U;
    BootContext->Capabilities.AssignmentCapacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_ASSIGNMENTS;
    BootContext->Capabilities.EventCount = 0U;
    BootContext->Capabilities.EventCapacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_EVENTS;

    Root = 0;
    if (DeviceHandle == 0)
    {
        return EFI_NOT_FOUND;
    }

    Status = LosMonitorOpenRootForHandle(SystemTable, DeviceHandle, &Root);
    if (EFI_ERROR(Status) || Root == 0)
    {
        return Status;
    }

    Status = LosMonitorReadBinaryFileFromRoot(SystemTable, Root, LosMonitorCapabilitiesPath, &FilePhysicalAddress, &FileSize);
    Root->Close(Root);
    if (EFI_ERROR(Status) || FilePhysicalAddress == 0ULL || FileSize == 0ULL)
    {
        return Status;
    }

    Buffer = (const char *)(UINTN)FilePhysicalAddress;
    Cursor = Buffer;
    End = Buffer + (UINTN)FileSize;
    CurrentBlock = 0;
    BlockIndex = 0U;
    Status = EFI_SUCCESS;

    while (Cursor < End)
    {
        const char *LineStart;
        const char *LineEnd;
        char Token[32];

        while (Cursor < End && (*Cursor == '\r' || *Cursor == '\n'))
        {
            Cursor += 1;
        }
        if (Cursor >= End)
        {
            break;
        }

        LineStart = Cursor;
        while (Cursor < End && *Cursor != '\n')
        {
            Cursor += 1;
        }
        LineEnd = Cursor;
        if (Cursor < End && *Cursor == '\n')
        {
            Cursor += 1;
        }

        Cursor = LineStart;
        if (!LosMonitorReadToken(&Cursor, LineEnd, &Token[0], sizeof(Token)))
        {
            continue;
        }

        if (LosMonitorTextEqual(&Token[0], "block"))
        {
            char PrincipalTypeText[32];
            char ProfileName[LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH];

            if (BlockIndex >= LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_BLOCKS)
            {
                Status = EFI_BUFFER_TOO_SMALL;
                break;
            }
            if (!LosMonitorReadToken(&Cursor, LineEnd, &PrincipalTypeText[0], sizeof(PrincipalTypeText)) ||
                !LosMonitorReadToken(&Cursor, LineEnd, &ProfileName[0], sizeof(ProfileName)))
            {
                Status = EFI_LOAD_ERROR;
                break;
            }

            CurrentBlock = &BootContext->Capabilities.Blocks[BlockIndex];
            CurrentBlock->Version = LOS_CAPABILITIES_SERVICE_VERSION;
            CurrentBlock->Flags = LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP;
            CurrentBlock->PrincipalType = LosMonitorPrincipalTypeFromToken(&PrincipalTypeText[0]);
            CurrentBlock->Reserved = 0U;
            CurrentBlock->PrincipalId = LosMonitorHashText(&ProfileName[0]);
            CurrentBlock->ProfileId = LosMonitorHashText(&ProfileName[0]);
            CurrentBlock->GrantCount = 0U;
            CurrentBlock->Capacity = LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_GRANTS_PER_BLOCK;
            LosMonitorCopyAscii(CurrentBlock->PrincipalName,
                                LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH,
                                &ProfileName[0]);
            LosMonitorCopyAscii(CurrentBlock->ProfileName,
                                LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH,
                                &ProfileName[0]);
            if (CurrentBlock->PrincipalType == 0U)
            {
                Status = EFI_LOAD_ERROR;
                break;
            }

            BootContext->Capabilities.BlockCount = BlockIndex + 1U;
            BlockIndex += 1U;
            continue;
        }

        if (LosMonitorTextEqual(&Token[0], "grant"))
        {
            char Namespace[LOS_CAPABILITIES_SERVICE_NAMESPACE_LENGTH];
            char Name[LOS_CAPABILITIES_SERVICE_NAME_LENGTH];
            char ClassText[16];
            char FlagsText[16];
            UINT32 CapabilityClass;
            UINT32 Flags;
            LOS_CAPABILITY_GRANT_ENTRY *Grant;
            UINT32 GrantIndex;

            if (CurrentBlock == 0)
            {
                Status = EFI_LOAD_ERROR;
                break;
            }
            if (CurrentBlock->GrantCount >= LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_GRANTS_PER_BLOCK)
            {
                Status = EFI_BUFFER_TOO_SMALL;
                break;
            }
            if (!LosMonitorReadToken(&Cursor, LineEnd, &Namespace[0], sizeof(Namespace)) ||
                !LosMonitorReadToken(&Cursor, LineEnd, &Name[0], sizeof(Name)) ||
                !LosMonitorReadToken(&Cursor, LineEnd, &ClassText[0], sizeof(ClassText)) ||
                !LosMonitorReadToken(&Cursor, LineEnd, &FlagsText[0], sizeof(FlagsText)) ||
                !LosMonitorParseUnsignedToken(&ClassText[0], &CapabilityClass) ||
                !LosMonitorParseUnsignedToken(&FlagsText[0], &Flags))
            {
                Status = EFI_LOAD_ERROR;
                break;
            }

            GrantIndex = CurrentBlock->GrantCount;
            Grant = &CurrentBlock->Grants[GrantIndex];
            Grant->Version = LOS_CAPABILITIES_SERVICE_VERSION;
            Grant->CapabilityClass = CapabilityClass;
            Grant->Flags = Flags | LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP;
            Grant->GrantMode = LOS_CAPABILITIES_GRANT_MODE_BOOTSTRAP;
            Grant->State = LOS_CAPABILITIES_GRANT_STATE_ACTIVE;
            Grant->AuthoriserType = LOS_CAPABILITIES_AUTHORISER_BOOT_POLICY;
            Grant->Reserved0 = 0U;
            Grant->Reserved1 = 0U;
            Grant->CapabilityId = LosMonitorComposeCapabilityId(CapabilityClass, BlockIndex - 1U, GrantIndex);
            Grant->GrantId = LosMonitorComposeGrantId(BlockIndex - 1U, GrantIndex);
            Grant->ParentGrantId = 0ULL;
            Grant->GrantedAtUtc = 0ULL;
            Grant->EffectiveFromUtc = 0ULL;
            Grant->EffectiveUntilUtc = 0ULL;
            Grant->RevokedAtUtc = 0ULL;
            Grant->SuspendedAtUtc = 0ULL;
            LosMonitorCopyAscii(Grant->Namespace, LOS_CAPABILITIES_SERVICE_NAMESPACE_LENGTH, &Namespace[0]);
            LosMonitorCopyAscii(Grant->Name, LOS_CAPABILITIES_SERVICE_NAME_LENGTH, &Name[0]);
            LosMonitorCopyAscii(Grant->AuthoriserName,
                                LOS_CAPABILITIES_AUTHORISER_NAME_LENGTH,
                                "bootstrap_policy");
            CurrentBlock->GrantCount += 1U;
            if (!LosMonitorAppendGrantImportEvent(BootContext, CurrentBlock, Grant))
            {
                Status = EFI_BUFFER_TOO_SMALL;
                break;
            }
            continue;
        }

        if (LosMonitorTextEqual(&Token[0], "assign"))
        {
            char PrincipalTypeText[32];
            char PrincipalName[LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH];
            char ProfileName[LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH];
            LOS_CAPABILITY_PROFILE_ASSIGNMENT *Assignment;
            UINT32 PrincipalType;
            UINT32 AssignmentIndex;

            if (BootContext->Capabilities.AssignmentCount >= LOS_CAPABILITIES_BOOTSTRAP_CONTEXT_MAX_ASSIGNMENTS)
            {
                Status = EFI_BUFFER_TOO_SMALL;
                break;
            }
            if (!LosMonitorReadToken(&Cursor, LineEnd, &PrincipalTypeText[0], sizeof(PrincipalTypeText)) ||
                !LosMonitorReadToken(&Cursor, LineEnd, &PrincipalName[0], sizeof(PrincipalName)) ||
                !LosMonitorReadToken(&Cursor, LineEnd, &ProfileName[0], sizeof(ProfileName)))
            {
                Status = EFI_LOAD_ERROR;
                break;
            }

            PrincipalType = LosMonitorPrincipalTypeFromToken(&PrincipalTypeText[0]);
            if (PrincipalType == 0U)
            {
                Status = EFI_LOAD_ERROR;
                break;
            }

            AssignmentIndex = BootContext->Capabilities.AssignmentCount;
            Assignment = &BootContext->Capabilities.Assignments[AssignmentIndex];
            Assignment->Version = LOS_CAPABILITIES_SERVICE_VERSION;
            Assignment->Flags = LOS_CAPABILITIES_SERVICE_FLAG_BOOTSTRAP;
            Assignment->PrincipalType = PrincipalType;
            Assignment->AuthoriserType = LOS_CAPABILITIES_AUTHORISER_BOOT_POLICY;
            Assignment->PrincipalId = LosMonitorHashText(&PrincipalName[0]);
            Assignment->ProfileId = LosMonitorHashText(&ProfileName[0]);
            Assignment->AssignedAtUtc = 0ULL;
            LosMonitorCopyAscii(Assignment->PrincipalName, LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH, &PrincipalName[0]);
            LosMonitorCopyAscii(Assignment->ProfileName, LOS_CAPABILITIES_PRINCIPAL_NAME_LENGTH, &ProfileName[0]);
            LosMonitorCopyAscii(Assignment->AuthoriserName, LOS_CAPABILITIES_AUTHORISER_NAME_LENGTH, "bootstrap_policy");
            BootContext->Capabilities.AssignmentCount += 1U;
            continue;
        }

        if (LosMonitorTextEqual(&Token[0], "endblock"))
        {
            CurrentBlock = 0;
            continue;
        }

        Status = EFI_LOAD_ERROR;
        break;
    }

    if (!EFI_ERROR(Status) && BootContext->Capabilities.BlockCount != 0U)
    {
        BootContext->Flags |= LOS_BOOT_CONTEXT_FLAG_CAPABILITIES_VALID;
    }

    if (SystemTable->BootServices != 0 && FilePhysicalAddress != 0ULL)
    {
        UINTN PageCount;

        PageCount = (UINTN)((FileSize + LOS_PAGE_SIZE - 1ULL) / LOS_PAGE_SIZE);
        SystemTable->BootServices->FreePages(FilePhysicalAddress, PageCount);
    }

    return Status;
}
