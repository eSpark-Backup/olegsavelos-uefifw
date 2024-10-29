#include "Button.h"
#include <Library/UefiLib.h>

EFI_STATUS
EFIAPI
waitForButtonPress (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
)
{   
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  UINTN EventIndex;

  Print(L"\nPress any key to continue...\n");

  // Wait for a key press
  Status = SystemTable->ConIn->Reset(SystemTable->ConIn, FALSE);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key);
  while (EFI_ERROR(Status)) {
    // Wait for key press
    SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, &EventIndex);
    Status = SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key);
  }

  return EFI_SUCCESS;
}