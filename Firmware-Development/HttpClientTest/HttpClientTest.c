#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/Http.h>
#include <Protocol/ServiceBinding.h>
#include <Protocol/Ip4Config2.h>
#include <Protocol/Ip4.h>

#include <Protocol/Dhcp4.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

EFI_STATUS EFIAPI DhcpCallback(
    IN EFI_DHCP4_PROTOCOL* This,
    IN VOID* Context,
    IN EFI_DHCP4_STATE CurrentState,
    IN EFI_DHCP4_EVENT DhcpEvent,
    IN EFI_DHCP4_PACKET* Packet,
    OUT EFI_DHCP4_PACKET** NewPacket
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
sendHttpRequest(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable)
{
    EFI_STATUS Status;

    EFI_DHCP4_PROTOCOL* Dhcp4;
    EFI_DHCP4_CONFIG_DATA DhcpConfig;
    EFI_DHCP4_MODE_DATA Dhcp4ModeData;
    UINT32 DiscoverTimeout = 6;
    UINT32 RequestTimeout = 6;

    EFI_HTTP_PROTOCOL* Http;
    EFI_SERVICE_BINDING_PROTOCOL* HttpServiceBinding;
    EFI_HANDLE HttpHandle = NULL;
    EFI_HTTP_CONFIG_DATA HttpConfigData;
    EFI_HTTP_REQUEST_DATA RequestData;
    EFI_HTTP_RESPONSE_DATA ResponseData;
    EFI_HTTP_TOKEN RequestToken, ResponseToken;
    EFI_HTTP_MESSAGE RequestMessage, ResponseMessage;
    EFI_HTTP_HEADER Headers[1];
    CHAR16* RequestUrl = L"http://93.184.215.14";
    // EFI_IP4_CONFIG2_PROTOCOL *Ip4Config;
    Print(L"Starting HttpBootClient example\r\n");

    Status = gBS->LocateProtocol(&gEfiDhcp4ProtocolGuid, NULL, (VOID**)&Dhcp4);
    if (EFI_ERROR(Status)) {
        Print(L"Error locating DHCP protocol: %r\n", Status);
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

    EFI_IPv4_ADDRESS* AssignedIp = &Dhcp4ModeData.ClientAddress;
    Print(L"Assigned IP address: %d.%d.%d.%d\n",
        AssignedIp->Addr[0],
        AssignedIp->Addr[1],
        AssignedIp->Addr[2],
        AssignedIp->Addr[3]);

    // Locate the HTTP service binding protocol
    Status = gBS->LocateProtocol(&gEfiHttpServiceBindingProtocolGuid, NULL, (VOID**)&HttpServiceBinding);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to locate HTTP service binding protocol: %r\n", Status);
        return Status;
    }
    Print(L"HTTP service binding protocol found: %r\n", Status);

    // Create HTTP instance
    Status = HttpServiceBinding->CreateChild(HttpServiceBinding, &HttpHandle);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to create HTTP child: %r\n", Status);
        return Status;
    }
    Print(L"HTTP child created: %r\n", Status);

    // Locate the HTTP protocol on the created child
    Status = gBS->HandleProtocol(HttpHandle, &gEfiHttpProtocolGuid, (VOID**)&Http);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to locate HTTP protocol: %r\n", Status);
        return Status;
    }
    Print(L"HTTP protocol located: %r\n", Status);

    // Configure HTTP protocol
    ZeroMem(&HttpConfigData, sizeof(HttpConfigData));
    HttpConfigData.HttpVersion = HttpVersion11;
    HttpConfigData.TimeOutMillisec = 1000;  // No timeout
    HttpConfigData.LocalAddressIsIPv6 = FALSE;

    // Allocate memory for the structure
    HttpConfigData.AccessPoint.IPv4Node = AllocatePool(sizeof(EFI_HTTPv4_ACCESS_POINT));
    if (!HttpConfigData.AccessPoint.IPv4Node) {
        Print(L"Failed to allocate memory for the structure: %r\n", Status);
        return Status; // or handle the error appropriately
    }

    CopyMem(&HttpConfigData.AccessPoint.IPv4Node->LocalAddress, AssignedIp, sizeof(EFI_IPv4_ADDRESS));
    CopyMem(&HttpConfigData.AccessPoint.IPv4Node->LocalSubnet, &Dhcp4ModeData.SubnetMask, sizeof(EFI_IPv4_ADDRESS));
    HttpConfigData.AccessPoint.IPv4Node->LocalPort = 80;

    Status = Http->Configure(Http, &HttpConfigData);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to configure HTTP protocol: %r\n", Status);
        return Status;
    }

    ZeroMem(&RequestData, sizeof(RequestData));
    RequestData.Method = HttpMethodGet;
    RequestData.Url = RequestUrl;

    ZeroMem(Headers, sizeof(Headers));
    Headers[0].FieldName = "Host";
    Headers[0].FieldValue = "example.com";

    ZeroMem(&RequestMessage, sizeof(RequestMessage));
    RequestMessage.Data.Request = &RequestData;
    RequestMessage.Headers = Headers;
    RequestMessage.HeaderCount = 1;
    RequestMessage.BodyLength = 0;
    RequestMessage.Body = NULL;


    ZeroMem(&RequestToken, sizeof(RequestToken));
    RequestToken.Event = NULL;  // Synchronous request
    RequestToken.Message = &RequestMessage;
    RequestToken.Status = EFI_SUCCESS;

    // Step 4: Send HTTP Request
    Status = Http->Request(Http, &RequestToken);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to send HTTP request: %r\n", Status);
        Http->Configure(Http, NULL);
        HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpHandle);
        return Status;
    }
    Print(L"HTTP Request Sent\n");
    Print(L"HTTP Request Status: %r\n", RequestToken.Status);

    // Step 5: Receive HTTP Response
    ZeroMem(&ResponseMessage, sizeof(ResponseMessage));
    ResponseMessage.Data.Response = &ResponseData;
    ZeroMem(&ResponseToken, sizeof(ResponseToken));
    ResponseToken.Event = NULL;
    ResponseToken.Message = &ResponseMessage;
    ResponseToken.Status = EFI_SUCCESS;

    Status = Http->Response(Http, &ResponseToken);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to receive HTTP response: %r\n", Status);
        Http->Configure(Http, NULL);
        HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpHandle);
        return Status;
    }

    // Print the response body
    // Print(L"HTTP Response: %a\n, ResponseData);
    Print(L"HTTP Status: %r\n", ResponseToken.Status);
    Print(L"HTTP Response 3: %s\n", RequestToken.Message->Body);

    // Clean up
    FreePool(RequestUrl);  // Free the memory for the URL
    Http->Configure(Http, NULL);  // Close the connection
    HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpHandle);  // Destroy HTTP child

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    sendHttpRequest(ImageHandle, SystemTable);
    return waitForButtonPress(ImageHandle, SystemTable);
}


// Output 
//Failed to Configure HTTP Protocol : Invalid Parameter