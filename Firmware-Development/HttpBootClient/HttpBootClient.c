#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/Http.h>
#include <Protocol/ServiceBinding.h>
#include <Protocol/Ip4Config2.h>
#include <Protocol/Ip4.h>
#include <Protocol/Dhcp4.h>
#include "DHCPClient.h"
#include "Button.h"
#include "Ntfs.h"
// #include <Protocol/SimpleFileSystem.h>

#define BOOT_SERVER_IP_NAT "192.168.82.132"
#define BOOT_SERVER_IP_BRIDGE "192.168.1.4"
#define BOOT_SERVER_IP BOOT_SERVER_IP_NAT
#define BOOT_CONFIG_PATH "/boot-config"
#define BOOT_IMAGE_PATH "/boot/image.efi"
#define BOOT_FILE_PATH "/bootx64.efi"

#define BUFFER_SIZE 0x100000

BOOLEAN gRequestCallbackComplete = FALSE;
BOOLEAN gResponseCallbackComplete = FALSE;
VOID
    EFIAPI
    RequestCallback(
        IN EFI_EVENT Event,
        IN VOID *Context)
{
    gRequestCallbackComplete = TRUE;
}
VOID
    EFIAPI
    ResponseCallback(
        IN EFI_EVENT Event,
        IN VOID *Context)
{
    gResponseCallbackComplete = TRUE;
}

static UINTN
ascii_to_int (CONST CHAR8 *str)
{
    UINTN u;
    CHAR8 c;

    // skip preceeding white space
    while (*str && *str == ' ') {
        str += 1;
    }

    // convert digits
    u = 0;
    while ((c = *(str++))) {
        if (c >= '0' && c <= '9') {
            u = (u * 10) + c - '0';
        } else {
            break;
        }
    }

    return u;
}

static UINTN
convert_http_status_code (EFI_HTTP_STATUS_CODE status_code)
{
	if (status_code >= HTTP_STATUS_100_CONTINUE &&
	    status_code <  HTTP_STATUS_200_OK) {
		return (status_code - HTTP_STATUS_100_CONTINUE + 100);
	} else if (status_code >= HTTP_STATUS_400_BAD_REQUEST &&
		   status_code <  HTTP_STATUS_500_INTERNAL_SERVER_ERROR) {
		return (status_code - HTTP_STATUS_400_BAD_REQUEST + 400);
	} else if (status_code >= HTTP_STATUS_500_INTERNAL_SERVER_ERROR) {
		return (status_code - HTTP_STATUS_500_INTERNAL_SERVER_ERROR + 500);
	}

	return 0;
}

static VOID EFIAPI
httpnotify (EFI_EVENT Event, VOID *Context)
{
	*((BOOLEAN *) Context) = TRUE;
    Print(L"Received Event\n");
}

static EFI_STATUS
send_http_request (EFI_HTTP_PROTOCOL *http, CHAR8 *hostname, CHAR16 *Url)
{
	EFI_HTTP_TOKEN tx_token;
	EFI_HTTP_MESSAGE tx_message;
	EFI_HTTP_REQUEST_DATA request;
	EFI_HTTP_HEADER headers[3];
	BOOLEAN request_done;
	EFI_STATUS efi_status;
	EFI_STATUS event_status;

	/* Convert the ascii string to the UCS2 string */
	request.Method = HttpMethodGet;
	request.Url = Url;

	/* Prepare the HTTP headers */
	headers[0].FieldName = (CHAR8 *)"Host";
	headers[0].FieldValue = hostname;
	headers[1].FieldName = (CHAR8 *)"Accept";
	headers[1].FieldValue = (CHAR8 *)"*/*";
	headers[2].FieldName = (CHAR8 *)"User-Agent";
	headers[2].FieldValue = (CHAR8 *)"UefiHttpBoot/1.0";

	tx_message.Data.Request = &request;
	tx_message.HeaderCount = 3;
	tx_message.Headers = headers;
	tx_message.BodyLength = 0;
	tx_message.Body = NULL;

	tx_token.Status = EFI_NOT_READY;
	tx_token.Message = &tx_message;
	tx_token.Event = NULL;
	request_done = FALSE;
	efi_status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_NOTIFY,
				      httpnotify, &request_done,
				      &tx_token.Event);
	if (EFI_ERROR(efi_status)) {
		Print(L"Failed to Create Event for HTTP request: %r\n",
		       efi_status);
		goto no_event;
	}

	/* Send out the request */
	efi_status = http->Request(http, &tx_token);
	if (EFI_ERROR(efi_status)) {
		Print(L"HTTP request failed: %r\n", efi_status);
		goto error;
	}

	/* Wait for the response */
	while (!request_done)
		http->Poll(http);

	if (EFI_ERROR(tx_token.Status)) {
		Print(L"HTTP request: %r\n", tx_token.Status);
		efi_status = tx_token.Status;
	}

error:
	event_status = gBS->CloseEvent(tx_token.Event);
	if (EFI_ERROR(event_status)) {
		Print(L"Failed to close Event for HTTP request: %r\n",
		       event_status);
	}

no_event:
	if (Url)
		FreePool(Url);

	return efi_status;
}

static EFI_STATUS
receive_http_response(EFI_HTTP_PROTOCOL *http, CHAR8 **buffer, UINT64 *buf_size)
{
	EFI_HTTP_TOKEN rx_token;
	EFI_HTTP_MESSAGE rx_message;
	EFI_HTTP_RESPONSE_DATA response;
	EFI_HTTP_STATUS_CODE http_status;
	BOOLEAN response_done;
	UINTN i, downloaded;
	CHAR8 rx_buffer[9216];
	EFI_STATUS efi_status;
	EFI_STATUS event_status;

	/* Initialize the rx message and buffer */
	response.StatusCode = HTTP_STATUS_UNSUPPORTED_STATUS;
	rx_message.Data.Response = &response;
	rx_message.HeaderCount = 0;
	rx_message.Headers = 0;
	rx_message.BodyLength = sizeof(rx_buffer);
	rx_message.Body = rx_buffer;

	rx_token.Status = EFI_NOT_READY;
	rx_token.Message = &rx_message;
	rx_token.Event = NULL;
	response_done = FALSE;
	efi_status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_NOTIFY,
				      httpnotify, &response_done,
				      &rx_token.Event);
	if (EFI_ERROR(efi_status)) {
		Print(L"Failed to Create Event for HTTP response: %r\n",
		       efi_status);
		goto no_event;
	}

	/* Notify the firmware to receive the HTTP messages */
	efi_status = http->Response(http, &rx_token);
	if (EFI_ERROR(efi_status)) {
		Print(L"HTTP response failed: %r\n", efi_status);
		goto error;
	}

	/* Wait for the response */
	// while (!response_done)
	// 	http->Poll(http);

	if (EFI_ERROR(rx_token.Status)) {
		Print(L"HTTP response: %r\n", rx_token.Status);
		efi_status = rx_token.Status;
		goto error;
	}

	/* Check the HTTP status code */
	http_status = rx_token.Message->Data.Response->StatusCode;
	if (http_status != HTTP_STATUS_200_OK) {
		Print(L"HTTP Status Code: %d\n",
		       convert_http_status_code(http_status));
		efi_status = EFI_ABORTED;
		goto error;
	}

	/* Check the length of the file */
	for (i = 0; i < rx_message.HeaderCount; i++) {
		if (!AsciiStrCmp(rx_message.Headers[i].FieldName, (CHAR8 *)"Content-Length")) {
			*buf_size = ascii_to_int(rx_message.Headers[i].FieldValue);
		}
	}

	if (*buf_size == 0) {
		Print(L"Failed to get Content-Lenght\n");
		goto error;
	}

	*buffer = AllocatePool(*buf_size);
	if (!*buffer) {
		Print(L"Failed to allocate new rx buffer\n");
		goto error;
	}

	downloaded = rx_message.BodyLength;

	CopyMem(*buffer, rx_buffer, downloaded);

	/* Retreive the rest of the message */
	while (downloaded < *buf_size) {
		if (rx_message.Headers) {
			FreePool(rx_message.Headers);
		}
		rx_message.Headers = NULL;
		rx_message.HeaderCount = 0;
		rx_message.Data.Response = NULL;
		rx_message.BodyLength = sizeof(rx_buffer);
		rx_message.Body = rx_buffer;

		rx_token.Status = EFI_NOT_READY;
		response_done = FALSE;

		efi_status = http->Response(http, &rx_token);
		if (EFI_ERROR(efi_status)) {
			Print(L"HTTP response failed: %r\n", efi_status);
			goto error;
		}

		while (!response_done)
			http->Poll(http);

		if (EFI_ERROR(rx_token.Status)) {
			Print(L"HTTP response: %r\n", rx_token.Status);
			efi_status = rx_token.Status;
			goto error;
		}

		if (rx_message.BodyLength + downloaded > *buf_size) {
			efi_status = EFI_BAD_BUFFER_SIZE;
			goto error;
		}

		CopyMem(*buffer + downloaded, rx_buffer, rx_message.BodyLength);

		downloaded += rx_message.BodyLength;
	}

error:
	event_status = gBS->CloseEvent(rx_token.Event);
	if (EFI_ERROR(event_status)) {
		Print(L"Failed to close Event for HTTP response: %r\n",
		       event_status);
	}

no_event:
	if (EFI_ERROR(efi_status) && *buffer)
		FreePool(*buffer);

	return efi_status;
}


#if 1
EFI_STATUS
EFIAPI
startBoot(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_DHCP4_PROTOCOL *Dhcp4;
    EFI_DHCP4_CONFIG_DATA DhcpConfig;
    EFI_DHCP4_MODE_DATA Dhcp4ModeData;
    UINT32 DiscoverTimeout = 6;
    UINT32 RequestTimeout = 6;

    EFI_HTTP_PROTOCOL *Http;
    EFI_HANDLE HttpChildHandle = NULL;
    EFI_SERVICE_BINDING_PROTOCOL *HttpServiceBinding;
    EFI_HTTP_CONFIG_DATA HttpConfigData;
    EFI_HTTP_REQUEST_DATA RequestData;
    EFI_HTTP_TOKEN RequestToken, ResponseToken;

    EFI_HTTP_MESSAGE RequestMessage, ResponseMessage;
    CHAR16 *BootFileUri = L"http://" BOOT_SERVER_IP BOOT_FILE_PATH;

    // Step 1: Locate DHCP Protocol for network configuration
    Status = gBS->LocateProtocol(&gEfiDhcp4ProtocolGuid, NULL, (VOID **)&Dhcp4);
    if (EFI_ERROR(Status))
    {
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
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to configure DHCP: %r\n", Status);
        return Status;
    }

    Status = Dhcp4->Start(Dhcp4, NULL);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to start DHCP: %r\n", Status);
        return Status;
    }

    Status = Dhcp4->GetModeData(Dhcp4, &Dhcp4ModeData);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to get DHCP mode data: %r\n", Status);
        return Status;
    }

    EFI_IPv4_ADDRESS *AssignedIp = &Dhcp4ModeData.ClientAddress;
    Print(L"Assigned IP address: %d.%d.%d.%d\n",
          AssignedIp->Addr[0],
          AssignedIp->Addr[1],
          AssignedIp->Addr[2],
          AssignedIp->Addr[3]);

    // Step 2: Locate and initialize HTTP Protocol
    Status = gBS->LocateProtocol(&gEfiHttpServiceBindingProtocolGuid, NULL, (VOID **)&HttpServiceBinding);
    if (EFI_ERROR(Status))
    {
        Print(L"Error locating HTTP Service Binding Protocol: %r\n", Status);
        return Status;
    }

    Status = HttpServiceBinding->CreateChild(HttpServiceBinding, &HttpChildHandle);
    if (EFI_ERROR(Status))
    {
        Print(L"Error creating HTTP child handle: %r\n", Status);
        return Status;
    }

    Status = gBS->OpenProtocol(HttpChildHandle, &gEfiHttpProtocolGuid, (VOID **)&Http, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to open HTTP Protocol: %r\n", Status);
        HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpChildHandle);
        return Status;
    }

    // Configure HTTP Protocol
    ZeroMem(&HttpConfigData, sizeof(HttpConfigData));
    HttpConfigData.HttpVersion = HttpVersion11;
    HttpConfigData.TimeOutMillisec = 0;
    HttpConfigData.LocalAddressIsIPv6 = FALSE;

    // Allocate memory for the structure
    HttpConfigData.AccessPoint.IPv4Node = AllocatePool(sizeof(EFI_HTTPv4_ACCESS_POINT));
    if (!HttpConfigData.AccessPoint.IPv4Node)
    {
        Print(L"Failed to allocate memory for the structure: %r\n", Status);
        return Status; // or handle the error appropriately
    }

    CopyMem(&HttpConfigData.AccessPoint.IPv4Node->LocalAddress, AssignedIp, sizeof(EFI_IPv4_ADDRESS));
    CopyMem(&HttpConfigData.AccessPoint.IPv4Node->LocalSubnet, &Dhcp4ModeData.SubnetMask, sizeof(EFI_IPv4_ADDRESS));
    HttpConfigData.AccessPoint.IPv4Node->LocalPort = 80;

    Status = Http->Configure(Http, &HttpConfigData);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to configure HTTP: %r\n", Status);
        HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpChildHandle);
        return Status;
    }

    // Step 3: Set up HTTP Request
    ZeroMem(&RequestData, sizeof(RequestData));
    RequestData.Method = HttpMethodGet;
    RequestData.Url = BootFileUri;

    ZeroMem(&RequestMessage, sizeof(RequestMessage));
    RequestMessage.Data.Request = &RequestData;
    RequestMessage.HeaderCount = 0;
    RequestMessage.BodyLength = 0;
    RequestMessage.Body = NULL;

    ZeroMem(&RequestToken, sizeof(RequestToken));
    RequestToken.Event = NULL; // Synchronous request
    RequestToken.Message = &RequestMessage;

    // Step 4: Send HTTP Request
    Status = Http->Request(Http, &RequestToken);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to send HTTP request: %r\n", Status);
        Http->Configure(Http, NULL);
        HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpChildHandle);
        return Status;
    }

    // Step 5: Receive HTTP Response
    ZeroMem(&ResponseMessage, sizeof(ResponseMessage));
    ZeroMem(&ResponseToken, sizeof(ResponseToken));
    ResponseToken.Event = NULL;
    ResponseToken.Message = &ResponseMessage;

    Status = Http->Response(Http, &ResponseToken);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to receive HTTP response: %r\n", Status);
        Http->Configure(Http, NULL);
        HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpChildHandle);
        return Status;
    }

    // Step 6: Save the Boot File Locally
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *Root, *BootFile;
    Status = gBS->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, NULL, (VOID **)&FileSystem);
    if (EFI_ERROR(Status))
    {
        Print(L"Error locating file system protocol: %r\n", Status);
        return Status;
    }

    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status))
    {
        Print(L"Error opening root volume: %r\n", Status);
        return Status;
    }

    Status = Root->Open(Root, &BootFile, L"bootx64.efi", EFI_FILE_MODE_CREATE | EFI_FILE_MODE_WRITE, 0);
    if (EFI_ERROR(Status))
    {
        Print(L"Error creating boot file: %r\n", Status);
        return Status;
    }

    Status = BootFile->Write(BootFile, &ResponseMessage.BodyLength, ResponseMessage.Body);
    if (EFI_ERROR(Status))
    {
        Print(L"Error writing to boot file: %r\n", Status);
        BootFile->Close(BootFile);
        return Status;
    }

    BootFile->Close(BootFile);

    // Step 7: Load and Start the Boot Image
    EFI_HANDLE BootImageHandle;
    Status = gBS->LoadImage(FALSE, ImageHandle, NULL, ResponseMessage.Body, ResponseMessage.BodyLength, &BootImageHandle);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to load boot image: %r\n", Status);
        return Status;
    }

    Status = gBS->StartImage(BootImageHandle, NULL, NULL);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to start boot image: %r\n", Status);
    }

    // Clean up
    Http->Configure(Http, NULL);
    HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpChildHandle);

    return Status;
}

EFI_STATUS
EFIAPI
sendHttpRequest(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;

    EFI_HTTP_PROTOCOL *Http;
    EFI_SERVICE_BINDING_PROTOCOL *HttpServiceBinding;
    EFI_HANDLE HttpHandle = NULL;
    EFI_HTTP_CONFIG_DATA HttpConfigData;
    EFI_HTTP_REQUEST_DATA RequestData;
    EFI_HTTP_RESPONSE_DATA ResponseData;
    EFI_HTTP_TOKEN RequestToken, ResponseToken;
    EFI_HTTP_MESSAGE RequestMessage, ResponseMessage;
    EFI_HTTP_HEADER Headers[2];
    CHAR16 *RequestUrl = L"http://" BOOT_SERVER_IP BOOT_CONFIG_PATH;
    CHAR8 *Buffer;
    BOOLEAN response_done;
    BOOLEAN request_done;
    UINTN Index;

    Print(L"Starting HttpBootClient default DHCP example\r\n");

    Status = gBS->LocateProtocol(&gEfiHttpServiceBindingProtocolGuid, NULL, (VOID **)&HttpServiceBinding);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to locate HTTP service binding protocol: %r\n", Status);
        return Status;
    }

    Status = HttpServiceBinding->CreateChild(HttpServiceBinding, &HttpHandle);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to create HTTP child: %r\n", Status);
        return Status;
    }

    Status = gBS->HandleProtocol(HttpHandle, &gEfiHttpProtocolGuid, (VOID **)&Http);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to locate HTTP protocol: %r\n", Status);
        return Status;
    }

    ZeroMem(&HttpConfigData, sizeof(HttpConfigData));
    HttpConfigData.HttpVersion = HttpVersion11;
    HttpConfigData.TimeOutMillisec = 0;
    HttpConfigData.LocalAddressIsIPv6 = FALSE;
    HttpConfigData.AccessPoint.IPv4Node = AllocatePool(sizeof(EFI_HTTPv4_ACCESS_POINT));
    if (!HttpConfigData.AccessPoint.IPv4Node)
    {
        Print(L"Failed to allocate memory for the structure\n");
        return EFI_OUT_OF_RESOURCES;
    }
    HttpConfigData.AccessPoint.IPv4Node->UseDefaultAddress = TRUE;

    Status = Http->Configure(Http, &HttpConfigData);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to configure HTTP protocol: %r\n", Status);
        return Status;
    }

    // CHAR8* bufferLocal = NULL;
    // UINT64 BufferSize;
    // send_http_request(Http, BOOT_SERVER_IP, L"http://" BOOT_SERVER_IP BOOT_CONFIG_PATH);
    // AsciiPrint("Request Done\n");
    // receive_http_response(Http, &bufferLocal, &BufferSize);
    // AsciiPrint("Ash: %a\n", bufferLocal);

    ZeroMem(&RequestData, sizeof(RequestData));
    RequestData.Method = HttpMethodGet;
    RequestData.Url = RequestUrl;

    ZeroMem(Headers, sizeof(Headers));
    Headers[0].FieldName = "Host";
    Headers[0].FieldValue = BOOT_SERVER_IP;
    Headers[1].FieldName = "User-Agent";
    Headers[1].FieldValue = "UEFI-Client";

    ZeroMem(&RequestMessage, sizeof(RequestMessage));
    RequestMessage.Data.Request = &RequestData;
    RequestMessage.Headers = Headers;
    RequestMessage.HeaderCount = 2;
    RequestMessage.BodyLength = 0;
    RequestMessage.Body = NULL;

    ZeroMem(&RequestToken, sizeof(RequestToken));
    RequestToken.Event = NULL;
    RequestToken.Message = &RequestMessage;

    request_done = FALSE;
    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, httpnotify, &request_done, &RequestToken.Event);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to create request event: %r\n", Status);
        Http->Configure(Http, NULL);
        HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpHandle);
        return Status;
    }
    else{
        Print(L"Event value : %p\n", RequestToken.Event);
    }

    Status = Http->Request(Http, &RequestToken);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to send HTTP request: %r\n", Status);
        Http->Configure(Http, NULL);
        HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpHandle);
        return Status;
    }

    while (!request_done)
    {
        Status = Http->Poll(Http);
        Print(L"HTTP Poll: %r\n", Status);
    }
    Print(L"Request Sent : %r\n", Status);

    // Create an event for the response token

    ZeroMem(&ResponseMessage, sizeof(ResponseMessage));
    ResponseData.StatusCode = HTTP_STATUS_UNSUPPORTED_STATUS;
    ResponseMessage.Data.Response = &ResponseData;
    ResponseMessage.HeaderCount = 0;
    ResponseMessage.Headers = NULL;
    ResponseMessage.BodyLength = BUFFER_SIZE;
    Status = gBS->AllocatePool(
        EfiBootServicesData,
        ResponseMessage.BodyLength,
        (VOID **)&Buffer);
    if (!Buffer)
    {
        Print(L"Failed to allocate memory for response body\n");
        Http->Configure(Http, NULL);
        HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpHandle);
        return EFI_OUT_OF_RESOURCES;
    }
    ZeroMem(Buffer, sizeof(BUFFER_SIZE));
    ResponseMessage.Body = Buffer;

    ZeroMem(&ResponseToken, sizeof(ResponseToken));
    ResponseToken.Status = EFI_SUCCESS;
    ResponseToken.Message = &ResponseMessage;
    ResponseToken.Event = NULL;
    response_done = FALSE;
    Print(L"Event value : %p\n", ResponseToken.Event);
    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, httpnotify, &response_done, &ResponseToken.Event);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to create response event: %r\n", Status);
        Http->Configure(Http, NULL);
        HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpHandle);
        return Status;
    }
    else{
        Print(L"Event value : %p\n", ResponseToken.Event);
    }

    Status = Http->Response(Http, &ResponseToken);

    if (!EFI_ERROR(Status))
    {
        while (!response_done)
        {
		    Status = Http->Poll(Http);
        }

        Print(L"HTTP response token: %r\n", ResponseToken.Status);

        Print(L"Status Code : %d\n", ResponseToken.Message->Data.Response->StatusCode);
        for (Index = 0; Index < ResponseMessage.HeaderCount; ++Index)
        {
            Print(L"%a: %a\n", ResponseMessage.Headers[Index].FieldName, ResponseMessage.Headers[Index].FieldValue);
        }

        if (ResponseMessage.Body && ResponseMessage.BodyLength > 0)
        {            
            Print(L"HTTP Response Body: %a\n", Buffer);
            Print(L"Body Length: %d\n", ResponseMessage.BodyLength);
        }
        else
        {
            Print(L"Response body is empty\n");
        }

    }
    else
    {
        Print(L"Failed to receive HTTP response: %r\n", Status);
    }

    if (Buffer)
    {
        FreePool(Buffer);
    }
    Http->Configure(Http, NULL);
    HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpHandle);

    return EFI_SUCCESS;
}

#endif



#if 0
EFI_STATUS
EFIAPI
HttpClientMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    
    
    EFI_HANDLE Handle = NULL;
    EFI_SERVICE_BINDING_PROTOCOL *ServiceBinding;
    EFI_HTTP_PROTOCOL *HttpProtocol;
    EFI_HTTP_CONFIG_DATA ConfigData;
    EFI_HTTPv4_ACCESS_POINT IPv4Node;
    EFI_HTTP_REQUEST_DATA RequestData;
    EFI_HTTP_HEADER RequestHeader;
    EFI_HTTP_MESSAGE RequestMessage;
    EFI_HTTP_TOKEN RequestToken;
    EFI_HTTP_RESPONSE_DATA ResponseData;
    EFI_HTTP_MESSAGE ResponseMessage;
    EFI_HTTP_TOKEN ResponseToken;
    UINT8 *Buffer;
    EFI_TIME Baseline;
    EFI_TIME Current;
    UINTN Timer;
    UINTN Index;
    UINTN ContentDownloaded;
    UINTN ContentLength;
    Status = gBS->AllocatePool(
        EfiBootServicesData,
        BUFFER_SIZE,
        (VOID **)&Buffer);
    // TODO: Handle error...
    Status = gBS->LocateProtocol(
        &gEfiHttpServiceBindingProtocolGuid,
        NULL,
        &ServiceBinding);
    // TODO: Handle error...
    Status = ServiceBinding->CreateChild(ServiceBinding, &Handle);
    // TODO: Handle error...
    Status = gBS->HandleProtocol(Handle, &gEfiHttpProtocolGuid, &HttpProtocol);
    // TODO: Handle error...
    ConfigData.HttpVersion = HttpVersion11;
    ConfigData.TimeOutMillisec = 0; // Indicates default timeout period
    ConfigData.LocalAddressIsIPv6 = FALSE;
    ZeroMem(&IPv4Node, sizeof(IPv4Node));
    IPv4Node.UseDefaultAddress = TRUE; // Obtain IP address from DHCP
    ConfigData.AccessPoint.IPv4Node = &IPv4Node;
    // The HTTP driver must first be configured before requests or responses can
    // be processed. This is the same for other network protocols such as TCP.
    // This request message is initialized to request a sample driver bundle
    // from Intel's driver download center. To download a file, we use HTTP GET.
    RequestData.Method = HttpMethodGet;
    // URI where the file is located that we want to download.
    RequestData.Url = L"http://192.168.1.4/boot/image.efi";
    // This header tells the HTTP driver to relay the HTTP request
    // via a proxy server. This header is just used to demonstrate
    // how to relay through a proxy with this driver. The method
    // for obtaining the proxy address is up to the client. The
    // HTTP driver does NOT resolve this on its own.
    RequestHeader.FieldName = "Host";
    RequestHeader.FieldValue = "192.168.1.4";
    // Message format just contains a pointer to the request data
    // and body info, if applicable. In the case of HTTP GET, body
    // is not relevant.
    RequestMessage.Data.Request = &RequestData;
    // Just one header being provided in the HTTP message.
    RequestMessage.HeaderCount = 1;
    RequestMessage.Headers = &RequestHeader;
    RequestMessage.BodyLength = 0;
    RequestMessage.Body = NULL;
    // Token format is similar to the token format in EFI TCP protocol.
    RequestToken.Event = NULL;
    Status = gBS->CreateEvent(
        EVT_NOTIFY_SIGNAL,
        TPL_CALLBACK,
        RequestCallback,
        NULL,
        &RequestToken.Event);
    // TODO: Handle error...
    RequestToken.Status = EFI_SUCCESS;
    RequestToken.Message = &RequestMessage;
    gRequestCallbackComplete = FALSE;
    // Finally, make HTTP request.
    Status = HttpProtocol->Request(HttpProtocol, &RequestToken);
    Print(L"Request Sent\n");
    // TODO: Handle error...
    Status = gRT->GetTime(&Baseline, NULL);
    Print(L"Waiting\n");
    // TODO: Handle error...
    // Optionally, wait for a certain amount of time before cancelling
    // the request. In this case, we'll allow the network stack 10
    // seconds to send the request successfully.
    for (Timer = 0; !gRequestCallbackComplete && Timer < 10;)
    {
        // Give the HTTP driver some motivation...
        HttpProtocol->Poll(HttpProtocol);
        // In practice, a call to GetTime() only fails when the total
        // elapsed time between the last call to to GetTime() is less
        // than the resolution of one tick (e.g. 1 second, depending
        // on capabilities of hardware). We only care to check the time
        // when the call succeeds.
        if (!EFI_ERROR(gRT->GetTime(&Current, NULL)) &&
            Current.Second != Baseline.Second)
        {
            // One second has passed, so update Current time and
            // increment the counter.
            Baseline = Current;
            ++Timer;
        }
    }
    // Cancel request if we did not get a notification from the HTTP
    // driver in a timely manner.
    if (!gRequestCallbackComplete)
    {
        Print(L"Request Callback not Cpmpleted\n");
        Status = HttpProtocol->Cancel(HttpProtocol, &RequestToken);
        // TODO: Handle error and exit condition...
    }
    Print(L"Callback Complete\n");
    // Assuming we succeed in our request...
    // This response message is different that request in that the
    // HTTP driver is responsible for allocating the headers during
    // a response instead of the caller.
    ResponseData.StatusCode = HTTP_STATUS_UNSUPPORTED_STATUS;
    ResponseMessage.Data.Response = &ResponseData;
    // HeaderCount will be updated by the HTTP driver on response.
    ResponseMessage.HeaderCount = 0;
    // Headers will be populated by the driver on response.
    ResponseMessage.Headers = NULL;
    // BodyLength maximum limit is defined by the caller. On response,
    // the HTTP driver will update BodyLength to the total number of
    // bytes copied to Body. This number will never exceed the initial
    // maximum provided by the caller.
    ResponseMessage.BodyLength = BUFFER_SIZE;
    ResponseMessage.Body = Buffer;
    // Token format is similar to the token format in EFI TCP protocol.
    ResponseToken.Event = NULL;
    Status = gBS->CreateEvent(
        EVT_NOTIFY_SIGNAL,
        TPL_CALLBACK,
        NULL,
        &ResponseToken,
        &ResponseToken.Event);
    ResponseToken.Status = EFI_SUCCESS;
    ResponseToken.Message = &ResponseMessage;
    gResponseCallbackComplete = FALSE;
    // Finally, make HTTP request.
    Status = HttpProtocol->Response(HttpProtocol, &ResponseToken);
    Print(L"Response Requested\n");
    // TODO: Handle error...
    Status = gRT->GetTime(&Baseline, NULL);
    Print(L"Waiting\n");
    // TODO: Handle error...
    // Optionally, wait for a certain amount of time before cancelling.
    for (Timer = 0; !gResponseCallbackComplete && Timer < 10;)
    {
        HttpProtocol->Poll(HttpProtocol);
        if (!EFI_ERROR(gRT->GetTime(&Current, NULL)) &&
            Current.Second != Baseline.Second)
        {
            // One second has passed, so update Current time and
            // increment the counter.
            Baseline = Current;
            ++Timer;
        }
    }
    // Remove response token from queue if we did not get a notification
    // from the remote host in a timely manner.
    if (!gResponseCallbackComplete)
    {
        Print(L"Response Callback not Cpmpleted\n");
        Status = HttpProtocol->Cancel(HttpProtocol, &ResponseToken);
        // TODO: Handle error and exit condition...
    }
    Print(L"Response Cpmpleted\n");
    // Assuming we successfully received a response...
    Print(L"Header Count : %d\n", ResponseMessage.HeaderCount);
    for (Index = 0; Index < ResponseMessage.HeaderCount; ++Index)
    {
        // We can parse the length of the file from the ContentLength header.
        AsciiPrint("%a: %a\n", ResponseMessage.Headers[Index].FieldName, ResponseMessage.Headers[Index].FieldValue);
        if (!AsciiStriCmp(ResponseMessage.Headers[Index].FieldName, "ContentLength"))
        {
            ContentLength =
                AsciiStrDecimalToUintn(ResponseMessage.Headers[Index].FieldValue);
            Print(L"Content Length: %d\n", ContentLength);
        }
    }
    
    ContentDownloaded = ResponseMessage.BodyLength;
    // TODO:
    // Downloaded data exists in Buffer[0..ResponseMessage.BodyLength].
    // At this point, depending on business use case, the content can
    // be written to a file, stored on the heap, etc.
    while (ContentDownloaded < ContentLength)
    {
        // If we make it here, we haven't yet downloaded the whole file and
        // need to keep going.
        ResponseMessage.Data.Response = NULL;
        if (ResponseMessage.Headers != NULL)
        {
            // No sense hanging onto this anymore.
            FreePool(ResponseMessage.Headers);
        }
        ResponseMessage.HeaderCount = 0;
        ResponseMessage.BodyLength = BUFFER_SIZE;
        ZeroMem(Buffer, BUFFER_SIZE);
        // ResponseMessage.Body still points to Buffer.
        gResponseCallbackComplete = FALSE;
        // The HTTP driver accepts a token where Data, Headers, and
        // HeaderCount are all 0 or NULL. The driver will wait for a
        // response from the last remote host which a transaction occurred
        // and copy the response directly into Body, updating BodyLength
        // with the total amount copied (downloaded).
        Status = HttpProtocol->Response(HttpProtocol, &ResponseToken);
        // TODO: Handle error...
        Status = gRT->GetTime(&Baseline, NULL);
        // TODO: Handle error...
        // Optionally, wait for a certain amount of time before cancelling.
        for (Timer = 0; !gResponseCallbackComplete && Timer < 10;)
        {
            HttpProtocol->Poll(HttpProtocol);
            if (!EFI_ERROR(gRT->GetTime(&Current, NULL)) &&
                Current.Second != Baseline.Second)
            {
                // One second has passed, so update Current time and
                // increment the counter.
                Baseline = Current;
                ++Timer;
            }
        }
        // Remove response token from queue if we did not get a notification
        // from the remote host in a timely manner.
        if (!gResponseCallbackComplete)
        {
            Status = HttpProtocol->Cancel(HttpProtocol, &ResponseToken);
            // TODO: Handle error and exit condition...
        }
        // Assuming we successfully received a response...
        ContentDownloaded += ResponseMessage.BodyLength;
        // TODO:
        // Downloaded data exists in Buffer[0..ResponseMessage.BodyLength].
        // Append data to a file, heap memory, etc.
        if (ResponseMessage.BodyLength > 0)
        {
            AsciiPrint("Downloaded Content:\n%.*a\n", ResponseMessage.BodyLength, Buffer);
        }
    }
    // Perform any necessary cleanup and handling of downloaded file
    // assuming we succeeded at downloading the content. Depending on
    // where the data was stored as per business need, that data can
    // be consumed at this point. For example, if the data was stored
    // to a file system, the file can be opened and consumed.
    return EFI_SUCCESS;
}
#endif
EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    NTFSSequence(ImageHandle, SystemTable);
    return waitForButtonPress(ImageHandle, SystemTable);
}
