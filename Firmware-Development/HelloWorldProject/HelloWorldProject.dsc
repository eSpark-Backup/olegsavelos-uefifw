[Defines]
  PLATFORM_NAME                   = HelloWorldPlatform
  PLATFORM_GUID                   = A1E44F72-4567-1234-B0B1-0805B4895A9E
  PLATFORM_VERSION                = 1.0
  DSC_SPECIFICATION               = 0x0001001C
  OUTPUT_DIRECTORY                = D:\HelloWorldProject\Build/HelloWorldPackage
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

[Components]
  HelloWorldProject/HelloWorld.inf

[BuildOptions]
  MSFT:*_*_X64_CC_FLAGS = /GS-
  MSFT:*_*_X64_LINK_FLAGS = /SAFESEH:NO /GS-