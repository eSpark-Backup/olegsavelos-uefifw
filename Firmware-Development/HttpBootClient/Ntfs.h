#pragma once

#include <Uefi.h>

EFI_STATUS
EFIAPI
NTFSSequence (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
);