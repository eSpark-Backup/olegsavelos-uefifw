[Defines]
  PLATFORM_NAME                   = HttpBootClientPlatform
  PLATFORM_GUID                   = A1E44F72-4567-1234-B0B1-0805B4895A9E
  PLATFORM_VERSION                = 1.0
  DSC_SPECIFICATION               = 0x0001001C
  OUTPUT_DIRECTORY                = D:\GitHub\olegsavelos-uefifw\Firmware-Development\HttpBootClient\Build\HttpBootClient
  SUPPORTED_ARCHITECTURES         = IA32|X64|ARM|AARCH64
  BUILD_TARGETS                   = DEBUG|RELEASE
  SKUID_IDENTIFIER                = DEFAULT

[LibraryClasses]
  UefiLib                         | MdePkg/Library/UefiLib/UefiLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  BaseMemoryLib                   | MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf

  JsonLib|RedfishPkg/Library/JsonLib/JsonLib.inf
#   RestExLib|RedfishPkg/Library/DxeRestExLib/DxeRestExLib.inf
  Ucs2Utf8Lib|RedfishPkg/Library/BaseUcs2Utf8Lib/BaseUcs2Utf8Lib.inf
  RedfishCrtLib|RedfishPkg/PrivateLibrary/RedfishCrtLib/RedfishCrtLib.inf
#   JsonLib|RedfishPkg/Library/JsonLib/JsonLib.inf
#   RedfishLib|RedfishPkg/PrivateLibrary/RedfishLib/RedfishLib.inf
#   RedfishDebugLib|RedfishPkg/Library/RedfishDebugLib/RedfishDebugLib.inf
#   RedfishPlatformCredentialLib|RedfishPkg/Library/RedfishPlatformCredentialIpmiLib/RedfishPlatformCredentialIpmiLib.inf
#   HiiUtilityLib|RedfishPkg/Library/HiiUtilityLib/HiiUtilityLib.inf
#   RedfishPlatformConfigLib|RedfishPkg/Library/RedfishPlatformConfigLib/RedfishPlatformConfigLib.inf
#   RedfishHttpLib|RedfishPkg/Library/RedfishHttpLib/RedfishHttpLib.inf
  SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf

[Components]
HttpBootClient/HttpBootClient.inf

[BuildOptions]
  MSFT:*_*_X64_CC_FLAGS = /GS-
  MSFT:*_*_X64_LINK_FLAGS = /SAFESEH:NO /GS-