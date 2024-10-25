#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/Http.h>
#include <Protocol/ServiceBinding.h>
#include <Protocol/Ip4Config2.h>
#include <Protocol/Ip4.h>

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
sendHttpRequest(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_HTTP_PROTOCOL *Http;
    EFI_SERVICE_BINDING_PROTOCOL *HttpServiceBinding;
    EFI_HANDLE HttpHandle = NULL;
    EFI_HTTP_CONFIG_DATA HttpConfigData;
    EFI_HTTP_REQUEST_DATA HttpRequestData;
    // EFI_HTTP_RESPONSE_DATA HttpResponseData;
    EFI_HTTP_TOKEN HttpRequestToken;
    EFI_HTTP_TOKEN HttpResponseToken;
    EFI_HTTP_HEADER Headers[1];
    CHAR16 *RequestUrl = L"http://example.com";
    CHAR8 *ResponseData;
    UINTN ResponseDataSize = 1024;
    // EFI_IP4_CONFIG2_PROTOCOL *Ip4Config;
    Print(L"Starting HTTP Client example\r\n");

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
    HttpConfigData.TimeOutMillisec = 0;  // No timeout

    Status = Http->Configure(Http, &HttpConfigData);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to configure HTTP protocol: %r\n", Status);
        return Status;
    }

    // Prepare the HTTP GET request
    ZeroMem(Headers, sizeof(Headers));
    Headers[0].FieldName = "Host";
    Headers[0].FieldValue = "example.com";

    HttpRequestData.Method = HttpMethodGet;
    HttpRequestData.Url = RequestUrl;  // Use CHAR16 string for the URL

    // Set up the HTTP request token
    ZeroMem(&HttpRequestToken, sizeof(HttpRequestToken));
    HttpRequestToken.Status = EFI_SUCCESS;
    HttpRequestToken.Message = AllocateZeroPool(sizeof(EFI_HTTP_MESSAGE));
    HttpRequestToken.Message->Headers = Headers;
    HttpRequestToken.Message->HeaderCount = 1;
    HttpRequestToken.Message->BodyLength = 0;
    HttpRequestToken.Message->Body = NULL;

    HttpRequestToken.Message->Data.Request = &HttpRequestData;  // Pass the address of HttpRequestData

    Status = Http->Request(Http, &HttpRequestToken);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to send HTTP GET request: %r\n", Status);
        return Status;
    }

    // Wait for request completion (you may want to use gBS->CheckEvent() to check event status)
    while (HttpRequestToken.Status == EFI_NOT_READY) {
        gBS->Stall(100000);  // Wait for 100 milliseconds
    }

    if (EFI_ERROR(HttpRequestToken.Status)) {
        Print(L"HTTP Request failed: %r\n", HttpRequestToken.Status);
        return HttpRequestToken.Status;
    }

    // Receive the HTTP response
    ResponseData = AllocateZeroPool(ResponseDataSize);
    ZeroMem(&HttpResponseToken, sizeof(HttpResponseToken));
    HttpResponseToken.Status = EFI_SUCCESS;
    HttpResponseToken.Message = AllocateZeroPool(sizeof(EFI_HTTP_MESSAGE));
    HttpResponseToken.Message->Body = ResponseData;
    HttpResponseToken.Message->BodyLength = ResponseDataSize;

    Status = Http->Response(Http, &HttpResponseToken);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to receive HTTP response: %r\n", Status);
        return Status;
    }

    // Wait for response completion
    while (HttpResponseToken.Status == EFI_NOT_READY) {
        gBS->Stall(100000);  // Wait for 100 milliseconds
    }

    if (EFI_ERROR(HttpResponseToken.Status)) {
        Print(L"HTTP Response failed: %r\n", HttpResponseToken.Status);
        return HttpResponseToken.Status;
    }

    // Print the response body
    Print(L"HTTP Response: %a\n", ResponseData);

    // Clean up
    FreePool(ResponseData);
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