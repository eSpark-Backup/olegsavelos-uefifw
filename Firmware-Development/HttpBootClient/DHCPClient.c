#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/Dhcp4.h>
#include "DHCPClient.h"

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

EFI_STATUS configureDHCP(EFI_DHCP4_MODE_DATA* l_Dhcp4ModeData)
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

    CopyMem(l_Dhcp4ModeData, &Dhcp4ModeData, sizeof(Dhcp4ModeData));

    EFI_IPv4_ADDRESS* AssignedIp = &Dhcp4ModeData.ClientAddress;
    Print(L"Assigned IP address: %d.%d.%d.%d\n",
        AssignedIp->Addr[0],
        AssignedIp->Addr[1],
        AssignedIp->Addr[2],
        AssignedIp->Addr[3]);

    return EFI_SUCCESS;
}

EFI_STATUS getBootInfo(EFI_DHCP4_MODE_DATA* pDhcp4ModeData, BootServerInfo* bootserverInfo)
{
    EFI_STATUS Status = EFI_NOT_FOUND;
    EFI_DHCP4_PACKET *ReplyPacket = pDhcp4ModeData->ReplyPacket;
    UINT8 *OptionData = ReplyPacket->Dhcp4.Option;
    UINTN OptionLength = ReplyPacket->Length - sizeof(EFI_DHCP4_HEADER) - sizeof(UINT32);

    while (OptionLength > 0) {
        UINT8 OptionType = *OptionData;
        UINT8 OptionLen = *(OptionData + 1);
        UINTN bufferSize = MAX_BUFFER_SIZE - 1;
        UINTN LengthToCopy = OptionLen < bufferSize ? OptionLen : bufferSize;

        if (OptionType == 66) { 
            CopyMem(bootserverInfo->ServerIp, OptionData + 2, LengthToCopy);
            bootserverInfo->ServerIp[LengthToCopy] = '\0';         
        }

        if (OptionType == 67) { 
            CopyMem(bootserverInfo->BootfileName, OptionData + 2, LengthToCopy);
            bootserverInfo->BootfileName[LengthToCopy] = '\0';            
            bootserverInfo->Available = TRUE;
            Status = EFI_SUCCESS;
            break;
        }

        OptionData += OptionLen + 2;
        OptionLength -= OptionLen + 2;
    }

    return Status;
}

EFI_STATUS
EFIAPI
getDCHP_Configs (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
)
{
    EFI_STATUS Status;

    EFI_DHCP4_MODE_DATA Dhcp4ModeData;
    Status = configureDHCP(&Dhcp4ModeData);
    if (Status != EFI_SUCCESS)
    {
        Print(L"Failed to configure DHCP: %r\n", Status);
        return Status;
    }

    BootServerInfo bootserverInfo;
    Status = getBootInfo(&Dhcp4ModeData, &bootserverInfo);
    if (Status != EFI_SUCCESS)
    {
        Print(L"Failed to get Bootinfo: %r\n", Status);
        return Status;
    }

    if (bootserverInfo.Available) 
    {
        AsciiPrint("Boot Server: %a\n", bootserverInfo.ServerIp); 
        AsciiPrint("Boot Server Path: %a\n", bootserverInfo.BootfileName);
    } 
    else 
    {
        Print(L"Bootfile Name not found in DHCP options\n");
    }

    return EFI_SUCCESS;
}
