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
    EFI_GUID Dhcp4ProtocolGuid = EFI_DHCP4_PROTOCOL_GUID;
    // EFI_DHCP4_CONFIG_DATA DhcpConfigData;
    // EFI_IPv4_ADDRESS Ipv4Address;
    // UINTN DataSize;
    // UINT32 Timeout;
    
    // Locate the DHCP4 protocol
    Status = gBS->LocateProtocol(&gEfiDhcp4ProtocolGuid, NULL, (VOID **)&Dhcp4);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to locate DHCP4 protocol: %r\n", Status);
        return Status;
    }

    // // Initialize DHCP configuration
    // ZeroMem(&DhcpConfigData, sizeof(DhcpConfigData));
    
    // // Start the DHCP process
    // Status = Dhcp4->Start(Dhcp4);
    // if (EFI_ERROR(Status)) {
    //     Print(L"Failed to start DHCP: %r\n", Status);
    //     return Status;
    // }

    // // Renew and rebind if necessary (set a timeout as needed)
    // Timeout = 5; // 5 seconds timeout for simplicity
    // Status = Dhcp4->RenewRebind(Dhcp4, &Timeout);
    // if (EFI_ERROR(Status)) {
    //     Print(L"Failed to renew/rebind DHCP: %r\n", Status);
    //     return Status;
    // }

    // // Retrieve assigned IP address from the DHCP configuration
    // DataSize = sizeof(Ipv4Address);
    // Status = Dhcp4->GetModeData(Dhcp4, NULL, &DataSize, &Ipv4Address, NULL, NULL);
    // if (EFI_ERROR(Status)) {
    //     Print(L"Failed to get IP address: %r\n", Status);
    //     return Status;
    // }

    // // Print the assigned IP address
    // Print(L"Assigned IP address: %d.%d.%d.%d\n",
    //     Ipv4Address.Addr[0],
    //     Ipv4Address.Addr[1],
    //     Ipv4Address.Addr[2],
    //     Ipv4Address.Addr[3]);

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


// #include <Protocol/Ip4Config2.h>

// EFI_STATUS
// EFIAPI
// UefiMain(
//     IN EFI_HANDLE        ImageHandle,
//     IN EFI_SYSTEM_TABLE  *SystemTable
//     )
// {
//     EFI_STATUS                   Status;
//     EFI_IP4_CONFIG2_PROTOCOL      *Ip4Config2;
//     EFI_IP4_CONFIG2_INTERFACE_INFO *InterfaceInfo;
//     UINTN                         InfoSize;
//     CHAR16                        IpAddrStr[48];

//     // Locate the EFI_IP4_CONFIG2_PROTOCOL
//     Status = gBS->LocateProtocol(&gEfiIp4Config2ProtocolGuid, NULL, (VOID **)&Ip4Config2);
//     if (EFI_ERROR(Status)) {
//         Print(L"Unable to locate IP4 Config protocol: %r\n", Status);
//         return Status;
//     }

//     // First, get the size of the buffer needed for interface info
//     InfoSize = 0;
//     Status = Ip4Config2->GetData(Ip4Config2, Ip4Config2DataTypeInterfaceInfo, &InfoSize, NULL);
//     if (Status != EFI_BUFFER_TOO_SMALL) {
//         Print(L"Error getting interface info size: %r\n", Status);
//         return Status;
//     }

//     // Allocate memory for interface info
//     InterfaceInfo = AllocateZeroPool(InfoSize);
//     if (InterfaceInfo == NULL) {
//         Print(L"Failed to allocate memory\n");
//         return EFI_OUT_OF_RESOURCES;
//     }

//     // Get the actual interface info
//     Status = Ip4Config2->GetData(Ip4Config2, Ip4Config2DataTypeInterfaceInfo, &InfoSize, InterfaceInfo);
//     if (EFI_ERROR(Status)) {
//         Print(L"Error getting interface info: %r\n", Status);
//         FreePool(InterfaceInfo);
//         return Status;
//     }

//     // Check if an IP is assigned
//     if (InterfaceInfo->StationAddress.Addr[0] == 0) {
//         Print(L"No IP address assigned.\n");
//     } else {
//         // Convert the IP address from binary format to string format
//         UnicodeSPrint(IpAddrStr, sizeof(IpAddrStr), L"%d.%d.%d.%d",
//                       InterfaceInfo->StationAddress.Addr[0],
//                       InterfaceInfo->StationAddress.Addr[1],
//                       InterfaceInfo->StationAddress.Addr[2],
//                       InterfaceInfo->StationAddress.Addr[3]);

//         // Print the IP address
//         Print(L"Assigned IP Address: %s\n", IpAddrStr);
//     }

//     // Free the allocated memory
//     FreePool(InterfaceInfo);

//     return EFI_SUCCESS;
// }
