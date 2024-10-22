#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/NetLib.h>
#include <Protocol/Dhcp4.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>

EFI_STATUS EFIAPI DhcpCallback(
    IN EFI_DHCP4_PROTOCOL *This,
    IN VOID *Context,
    IN EFI_DHCP4_STATE CurrentState,
    IN EFI_DHCP4_EVENT DhcpEvent,
    IN EFI_DHCP4_PACKET *Packet,
    OUT EFI_DHCP4_PACKET **NewPacket
)
{
    // Print the current DHCP event
    switch (DhcpEvent) {
        case Dhcp4SendDiscover:
            Print(L"DHCP Event: Send Discover\n");
            break;
        case Dhcp4RcvdOffer:
            Print(L"DHCP Event: Received an offer from DHCP server\n");
            break;
        case Dhcp4SelectOffer:
            Print(L"DHCP Event: Selecting a DHCP offer\n");
            break;
        case Dhcp4SendRequest:
            Print(L"DHCP Event: Sending a DHCP request\n");
            break;
        case Dhcp4RcvdAck:
            Print(L"DHCP Event: Received Ack\n");
            break;
        case Dhcp4RcvdNak:
            Print(L"DHCP Event: Received Nack\n");
            break;
        case Dhcp4BoundCompleted:
            Print(L"DHCP Event: Lease successfully bound\n");
            break;
        default:
            Print(L"DHCP Event: Unknown event\n");
            break;
    }

    // You can implement any additional logic here if necessary
    return EFI_SUCCESS;
}


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

EFI_STATUS
EFIAPI
DhcpClientEntryPoint (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
)
{
    EFI_STATUS Status;
    EFI_DHCP4_PROTOCOL *Dhcp4;
    EFI_DHCP4_CONFIG_DATA DhcpConfig;
    EFI_DHCP4_MODE_DATA Dhcp4ModeData;
    UINT32 DiscoverTimeout = 6;
    UINT32 RequestTimeout = 6;

    Status = gBS->LocateProtocol(&gEfiDhcp4ProtocolGuid, NULL, (VOID **)&Dhcp4);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to locate DHCP4 protocol: %r\n", Status);
        return Status;
    }

    ZeroMem(&DhcpConfig, sizeof(DhcpConfig));
    DhcpConfig.DiscoverTryCount = 4;
    DhcpConfig.RequestTryCount = 4;
    DhcpConfig.DiscoverTimeout = &DiscoverTimeout;
    DhcpConfig.RequestTimeout = &RequestTimeout;
    DhcpConfig.Dhcp4Callback = DhcpCallback;

    Status = Dhcp4->Configure(Dhcp4, &DhcpConfig);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to configure DHCP: %r\n", Status);
        return Status;
    }

    Status = Dhcp4->Start(Dhcp4, NULL);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to start DHCP: %r\n", Status);
        return Status;
    }

    Status = Dhcp4->GetModeData(Dhcp4, &Dhcp4ModeData);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get DHCP mode data: %r\n", Status);
        return Status;
    }

    EFI_IPv4_ADDRESS *AssignedIp = &Dhcp4ModeData.ClientAddress;
    Print(L"Assigned IP address: %d.%d.%d.%d\n",
        AssignedIp->Addr[0],
        AssignedIp->Addr[1],
        AssignedIp->Addr[2],
        AssignedIp->Addr[3]);

    return EFI_SUCCESS;
}

// Entry point for the application
EFI_STATUS
EFIAPI
UefiMain (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
)
{
    DhcpClientEntryPoint(ImageHandle, SystemTable);
    return waitForButtonPress(ImageHandle, SystemTable);
}

