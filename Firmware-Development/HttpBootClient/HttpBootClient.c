#include "Button.h"
#include "ISCSIApp.h"
#include "Ntfs.h"

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/HttpLib.h>
#include <Library/UefiLib.h>
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


// #include <Protocol/SimpleFileSystem.h>

#define BOOT_SERVER_IP_NAT "192.168.82.132"
#define BOOT_SERVER_IP_BRIDGE "192.168.1.4"
#define BOOT_SERVER_IP BOOT_SERVER_IP_NAT
#define BOOT_CONFIG_PATH "/boot-config"
#define BOOT_IMAGE_PATH "/boot/image.efi"
#define BOOT_FILE_PATH "/bootx64.efi"

static VOID EFIAPI
HttpNotify(EFI_EVENT Event, VOID *Context)
{
    *((BOOLEAN *)Context) = TRUE;
}

static EFI_STATUS EFIAPI
HttpSendRequest(EFI_HTTP_PROTOCOL *Http, CHAR8 *Hostname, CHAR16 *Url)
{
    EFI_HTTP_TOKEN TxToken;
    EFI_HTTP_MESSAGE TxMessage;
    EFI_HTTP_REQUEST_DATA Request;
    EFI_HTTP_HEADER Headers[3];
    BOOLEAN RequestDone;
    EFI_STATUS EventStatus;
    EFI_STATUS Status;

    Request.Method = HttpMethodGet;
    Request.Url = Url;

    /* Prepare the HTTP Headers */
    Headers[0].FieldName = (CHAR8 *)"Host";
    Headers[0].FieldValue = Hostname;
    Headers[1].FieldName = (CHAR8 *)"Accept";
    Headers[1].FieldValue = (CHAR8 *)"*/*";
    Headers[2].FieldName = (CHAR8 *)"User-Agent";
    Headers[2].FieldValue = (CHAR8 *)"UefiHttpBoot/1.0";

    TxMessage.Data.Request = &Request;
    TxMessage.HeaderCount = 3;
    TxMessage.Headers = Headers;
    TxMessage.BodyLength = 0;
    TxMessage.Body = NULL;

    TxToken.Status = EFI_NOT_READY;
    TxToken.Message = &TxMessage;
    TxToken.Event = NULL;
    RequestDone = FALSE;
    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_NOTIFY,
                                 HttpNotify, &RequestDone,
                                 &TxToken.Event);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to Create Event for HTTP Request: %r\n", Status);
        goto no_event;
    }

    /* Send out the Request */
    Status = Http->Request(Http, &TxToken);
    if (EFI_ERROR(Status))
    {
        Print(L"HTTP Request failed: %r\n", Status);
        goto error;
    }

    /* Wait for the Response */
    while (!RequestDone)
        Http->Poll(Http);

    if (EFI_ERROR(TxToken.Status))
    {
        Print(L"HTTP Request: %r\n", TxToken.Status);
        Status = TxToken.Status;
    }

error:
    EventStatus = gBS->CloseEvent(TxToken.Event);
    if (EFI_ERROR(EventStatus))
    {
        Print(L"Failed to close Event for HTTP Request: %r\n",
              EventStatus);
    }

no_event:
    return Status;
}

static EFI_STATUS EFIAPI
HttpReceiveResponse(EFI_HTTP_PROTOCOL *Http, CHAR8 **Buf, UINT64 *BufLen)
{
    EFI_HTTP_TOKEN RxToken;
    EFI_HTTP_MESSAGE RxMessage;
    EFI_HTTP_RESPONSE_DATA Response;
    EFI_HTTP_STATUS_CODE HttpStatus;
    BOOLEAN ResponseDone;
    UINTN i, ContentDownloaded;
    CHAR8 RxBuffer[9216];
    EFI_STATUS EfiStatus;
    EFI_STATUS EventStatus;

    /* Initialize the rx message and Buf */
    Response.StatusCode = HTTP_STATUS_UNSUPPORTED_STATUS;
    RxMessage.Data.Response = &Response;
    RxMessage.HeaderCount = 0;
    RxMessage.Headers = 0;
    RxMessage.BodyLength = sizeof(RxBuffer);
    RxMessage.Body = RxBuffer;

    RxToken.Status = EFI_NOT_READY;
    RxToken.Message = &RxMessage;
    RxToken.Event = NULL;
    ResponseDone = FALSE;
    EfiStatus = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_NOTIFY,
                                 HttpNotify, &ResponseDone,
                                 &RxToken.Event);
    if (EFI_ERROR(EfiStatus))
    {
        Print(L"Failed to Create Event for HTTP Response: %r\n",
              EfiStatus);
        goto no_event;
    }

    /* Notify the firmware to receive the HTTP messages */
    EfiStatus = Http->Response(Http, &RxToken);
    if (EFI_ERROR(EfiStatus))
    {
        Print(L"HTTP Response failed: %r\n", EfiStatus);
        goto error;
    }

    /* Wait for the Response */
    while (!ResponseDone)
        Http->Poll(Http);

    if (EFI_ERROR(RxToken.Status))
    {
        Print(L"HTTP Response: %r\n", RxToken.Status);
        EfiStatus = RxToken.Status;
        goto error;
    }

    /* Check the HTTP status code */
    HttpStatus = RxToken.Message->Data.Response->StatusCode;
    if (HttpStatus != HTTP_STATUS_200_OK)
    {
        Print(L"HTTP Status Code: %r\n", HttpStatus);
        EfiStatus = EFI_ABORTED;
        goto error;
    }

    /* Check the length of the file */
    for (i = 0; i < RxMessage.HeaderCount; i++)
    {
        if (!AsciiStrCmp(RxMessage.Headers[i].FieldName, (CHAR8 *)"Content-Length"))
        {
            *BufLen = AsciiStrDecimalToUint64(RxMessage.Headers[i].FieldValue);
        }
    }

    if (*BufLen == 0)
    {
        Print(L"Failed to get Content-Lenght\n");
        goto error;
    }

    *Buf = AllocatePool(*BufLen);
    if (!*Buf)
    {
        Print(L"Failed to allocate new rx Buf\n");
        goto error;
    }

    ContentDownloaded = RxMessage.BodyLength;

    CopyMem(*Buf, RxBuffer, ContentDownloaded);

    /* Retreive the rest of the message */
    while (ContentDownloaded < *BufLen)
    {
        if (RxMessage.Headers)
        {
            FreePool(RxMessage.Headers);
        }
        RxMessage.Headers = NULL;
        RxMessage.HeaderCount = 0;
        RxMessage.Data.Response = NULL;
        RxMessage.BodyLength = sizeof(RxBuffer);
        RxMessage.Body = RxBuffer;

        RxToken.Status = EFI_NOT_READY;
        ResponseDone = FALSE;

        EfiStatus = Http->Response(Http, &RxToken);
        if (EFI_ERROR(EfiStatus))
        {
            Print(L"HTTP Response failed: %r\n", EfiStatus);
            goto error;
        }

        while (!ResponseDone)
            Http->Poll(Http);

        if (EFI_ERROR(RxToken.Status))
        {
            Print(L"HTTP Response: %r\n", RxToken.Status);
            EfiStatus = RxToken.Status;
            goto error;
        }

        if (RxMessage.BodyLength + ContentDownloaded > *BufLen)
        {
            EfiStatus = EFI_BAD_BUFFER_SIZE;
            goto error;
        }

        CopyMem(*Buf + ContentDownloaded, RxBuffer, RxMessage.BodyLength);

        ContentDownloaded += RxMessage.BodyLength;
    }

error:
    EventStatus = gBS->CloseEvent(RxToken.Event);
    if (EFI_ERROR(EventStatus))
    {
        Print(L"Failed to close Event for HTTP Response: %r\n",
              EventStatus);
    }

no_event:
    if (EFI_ERROR(EfiStatus) && *Buf)
        FreePool(*Buf);

    return EfiStatus;
}

EFI_STATUS
EFIAPI
StartBoot(IN EFI_HANDLE ImageHandle, VOID *FileBuf, UINTN FileLen)
{
    EFI_STATUS Status;
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

    Status = BootFile->Write(BootFile, &FileLen, FileBuf);
    if (EFI_ERROR(Status))
    {
        Print(L"Error writing to boot file: %r\n", Status);
        BootFile->Close(BootFile);
        return Status;
    }

    BootFile->Close(BootFile);

    EFI_HANDLE BootImageHandle;
    Status = gBS->LoadImage(FALSE, ImageHandle, NULL, FileBuf, FileLen, &BootImageHandle);
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

    return Status;
}

EFI_STATUS
EFIAPI
SendHttpRequest(EFI_DHCP4_MODE_DATA *Dhcp4ModeData, CHAR8* Url)
{
    EFI_STATUS Status;
    EFI_HTTP_PROTOCOL *Http;
    EFI_SERVICE_BINDING_PROTOCOL *HttpServiceBinding;
    EFI_HANDLE HttpHandle = NULL;
    EFI_HTTP_CONFIG_DATA HttpConfigData;

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
    HttpConfigData.TimeOutMillisec = 1000;
    HttpConfigData.LocalAddressIsIPv6 = FALSE;
    HttpConfigData.AccessPoint.IPv4Node = AllocatePool(sizeof(EFI_HTTPv4_ACCESS_POINT));
    if (!HttpConfigData.AccessPoint.IPv4Node)
    {
        Print(L"Failed to allocate memory for the structure\n");
        return EFI_OUT_OF_RESOURCES;
    }
    HttpConfigData.AccessPoint.IPv4Node->UseDefaultAddress = TRUE;
    // CopyMem(&HttpConfigData.AccessPoint.IPv4Node->LocalAddress, &Dhcp4ModeData->ClientAddress, sizeof(EFI_IPv4_ADDRESS));
    // CopyMem(&HttpConfigData.AccessPoint.IPv4Node->LocalSubnet, &Dhcp4ModeData->SubnetMask, sizeof(EFI_IPv4_ADDRESS));
    // HttpConfigData.AccessPoint.IPv4Node->LocalPort = 80;

    Status = Http->Configure(Http, &HttpConfigData);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to configure HTTP protocol: %r\n", Status);
        return Status;
    }

    CHAR8 *BufLocal = NULL;
    UINT64 BufLen;

    VOID *UrlParser = NULL;
    CHAR8 *Hostname = NULL;

    Status = HttpParseUrl(Url, (UINT32)AsciiStrLen(Url), FALSE, &UrlParser);
    if (EFI_ERROR(Status))
    {
        goto error;
    }

    Status = HttpUrlGetHostName(Url, UrlParser, &Hostname);
    if (EFI_ERROR(Status))
    {
        goto error;
    }

    CHAR16 UrlUnicode[128];
    AsciiStrToUnicodeStrS (Url, UrlUnicode, (sizeof(UrlUnicode)/ sizeof(UrlUnicode[0])));
    Status = HttpSendRequest(Http, Hostname, UrlUnicode);
    if (EFI_ERROR(Status))
    {
        goto error;
    }

    Status = HttpReceiveResponse(Http, &BufLocal, &BufLen);
    if (EFI_ERROR(Status))
    {
        goto error;
    }

    if (BufLocal)
    {
        for (size_t i = 0; i < BufLen; i++)
        {
            AsciiPrint("%c", BufLocal[i]);
        }
    }

error:
    if (BufLocal)
        FreePool(BufLocal);
    if (Hostname)
        FreePool(Hostname);
    Http->Configure(Http, NULL);
    HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpHandle);

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
getDHCPInfo(EFI_DHCP4_MODE_DATA *Dhcp4ModeData)
{
    EFI_STATUS Status;
    EFI_DHCP4_PROTOCOL *Dhcp4;
    EFI_DHCP4_CONFIG_DATA DhcpConfig;
    UINT32 DiscoverTimeout = 6;
    UINT32 RequestTimeout = 6;

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

    Status = Dhcp4->GetModeData(Dhcp4, Dhcp4ModeData);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to get DHCP mode data: %r\n", Status);
        return Status;
    }

    EFI_IPv4_ADDRESS *AssignedIp = &Dhcp4ModeData->ClientAddress;
    Print(L"Assigned IP address: %d.%d.%d.%d\n",
          AssignedIp->Addr[0],
          AssignedIp->Addr[1],
          AssignedIp->Addr[2],
          AssignedIp->Addr[3]);

    return Status;
}

EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    Status = ConfigureIscsiInitiator(ImageHandle);
    // EFI_DHCP4_MODE_DATA Dhcp4ModeData;
    // Status = getDHCPInfo(&Dhcp4ModeData);
    // Status = SendHttpRequest(&Dhcp4ModeData, "Http://" BOOT_SERVER_IP BOOT_CONFIG_PATH);
    return waitForButtonPress(ImageHandle, SystemTable);
}
