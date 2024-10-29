#pragma once

#include <Uefi.h>

#define MAX_BUFFER_SIZE 128

typedef struct BootServerInfo
{
    BOOLEAN Available;
    CHAR8 ServerIp[MAX_BUFFER_SIZE];
    CHAR8 BootfileName[MAX_BUFFER_SIZE];
} BootServerInfo;

EFI_STATUS
EFIAPI
getDCHP_Configs(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIAPI DhcpCallback(
    IN EFI_DHCP4_PROTOCOL *This,
    IN VOID *Context,
    IN EFI_DHCP4_STATE CurrentState,
    IN EFI_DHCP4_EVENT DhcpEvent,
    IN EFI_DHCP4_PACKET *Packet,
    OUT EFI_DHCP4_PACKET **NewPacket
);