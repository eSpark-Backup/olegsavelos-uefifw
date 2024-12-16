#include <Uefi.h>
#include <Protocol/IScsiInitiatorName.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>

EFI_STATUS ConfigureIscsiInitiator(EFI_HANDLE ImageHandle) {
    EFI_ISCSI_INITIATOR_NAME_PROTOCOL *IScsiProtocol;
    CHAR8 InitiatorName[256];
    UINTN NameSize = sizeof(InitiatorName);
    EFI_STATUS Status;

    // Locate the iSCSI Initiator Protocol
    Status = gBS->LocateProtocol(&gEfiIScsiInitiatorNameProtocolGuid, NULL, (VOID **)&IScsiProtocol);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to locate iSCSI Protocol: %r\n", Status);
        return Status;
    }

    // Get the current Initiator Name
    Status = IScsiProtocol->Get(IScsiProtocol, &NameSize, InitiatorName);
    if (!EFI_ERROR(Status)) {
        Print(L"Current iSCSI Initiator Name: %a\n", InitiatorName);
    } else {
        Print(L"Failed to get iSCSI Initiator Name: %r\n", Status);
    }

    // Set a new Initiator Name
    AsciiStrCpyS(InitiatorName, sizeof(InitiatorName), "iqn.2024-12.com.example:initiator");
    Status = IScsiProtocol->Set(IScsiProtocol, &NameSize, InitiatorName);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to set iSCSI Initiator Name: %r\n", Status);
    }

    return Status;
}

#if 0 
EFI_STATUS StartIScsiBoot(EFI_HANDLE ImageHandle) {
    EFI_ISCSI_PROTOCOL *IScsiProtocol;
    EFI_ISCSI_SESSION_CONFIG_DATA ConfigData;
    EFI_STATUS Status;

    // Locate the iSCSI Protocol
    Status = gBS->LocateProtocol(&gEfiIScsiProtocolGuid, NULL, (VOID **)&IScsiProtocol);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to locate iSCSI Protocol: %r\n", Status);
        return Status;
    }

    // Setup the session configuration data
    ZeroMem(&ConfigData, sizeof(ConfigData));
    ConfigData.HeaderVersion = EFI_ISCSI_PROTOCOL_REVISION;
    ConfigData.TargetAddress = "192.168.1.100"; // Example target
    ConfigData.TargetPort = 3260;               // Default iSCSI port
    ConfigData.TargetLun = 0;
    AsciiStrCpyS(ConfigData.InitiatorName, sizeof(ConfigData.InitiatorName), "iqn.2024-12.com.example:initiator");
    AsciiStrCpyS(ConfigData.TargetName, sizeof(ConfigData.TargetName), "iqn.2024-12.com.example:target");

    Status = IScsiProtocol->SetConfigData(IScsiProtocol, &ConfigData);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to set iSCSI Config Data: %r\n", Status);
        return Status;
    }

    // Initiate the boot process
    Status = IScsiProtocol->Start(IScsiProtocol);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to start iSCSI Boot: %r\n", Status);
    }

    return Status;
}
#endif