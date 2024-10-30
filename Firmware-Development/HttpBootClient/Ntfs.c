#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/FileInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

#define NTFS_DRIVER_PATH L"\\EFI\\BOOT\\ntfs_x64.efi"

EFI_STATUS
EFIAPI
LoadNtfsDriver(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
EFI_STATUS Status;
    EFI_HANDLE LoadedImageHandle;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;

    // Initialize the library (EDK2 specific)
    gST = SystemTable;
    gBS = SystemTable->BootServices;

    // Get the Loaded Image Protocol for the current image to access its device handle
    Status = gBS->OpenProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID **)&LoadedImage,
        ImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get LoadedImageProtocol: %r\n", Status);
        return Status;
    }

    // Create a device path for the NTFS driver file
    DevicePath = FileDevicePath(LoadedImage->DeviceHandle, NTFS_DRIVER_PATH);
    if (DevicePath == NULL) {
        Print(L"Failed to create device path for %s\n", NTFS_DRIVER_PATH);
        return EFI_NOT_FOUND;
    }

    // Load the NTFS driver (ntfs_x64.efi)
    Status = gBS->LoadImage(
        FALSE,               // Not a Boot Image
        ImageHandle,         // Parent image handle
        DevicePath,          // Path to the NTFS driver
        NULL,                // Optional source buffer
        0,                   // Size of the source buffer
        &LoadedImageHandle   // Image handle for the loaded driver
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to load %s: %r\n", NTFS_DRIVER_PATH, Status);
        FreePool(DevicePath);
        return Status;
    }
    Print(L"%s loaded successfully.\n", NTFS_DRIVER_PATH);

    // Start the NTFS driver
    Status = gBS->StartImage(
        LoadedImageHandle,
        NULL,   // Optional exit data size
        NULL    // Optional exit data
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to start %s: %r\n", NTFS_DRIVER_PATH, Status);
    } else {
        Print(L"%s started successfully.\n", NTFS_DRIVER_PATH);
    }

    // Free allocated device path memory
    FreePool(DevicePath);

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ListNtfsPartitions(
    EFI_HANDLE **HandleBuffer,
    UINTN *HandleCount)
{
    // Locate handles that support the Simple File System Protocol
    return gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, HandleCount, HandleBuffer);
}

EFI_STATUS
EFIAPI
OpenNtfsFile(
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FsProtocol,
    CHAR16 *FileName,
    EFI_FILE_PROTOCOL **FileHandle,
    UINT64 OpenMode)
{
    EFI_FILE_PROTOCOL *Root;
    EFI_STATUS Status = FsProtocol->OpenVolume(FsProtocol, &Root);
    if (EFI_ERROR(Status))
        return Status;

    return Root->Open(Root, FileHandle, FileName, OpenMode, 0);
}

EFI_STATUS
EFIAPI
ReadFileContents(
    EFI_FILE_PROTOCOL *FileHandle,
    VOID *Buffer,
    UINTN *BufferSize)
{
    return FileHandle->Read(FileHandle, BufferSize, Buffer);
}

EFI_STATUS
EFIAPI
WriteFileContents(
    EFI_FILE_PROTOCOL *FileHandle,
    VOID *Buffer,
    UINTN BufferSize)
{
    return FileHandle->Write(FileHandle, &BufferSize, Buffer);
}

EFI_STATUS
EFIAPI
ListFilesInRootDirectory(
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FsProtocol)
{
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *Root;
    CHAR16 *FileName;
    UINTN FileNameSize = sizeof(CHAR16) * 256;
    UINTN BufferSize = sizeof(EFI_FILE_INFO) + 256 * sizeof(CHAR16);
    EFI_FILE_INFO *FileInfo = AllocatePool(BufferSize);
    UINTN FileInfoSize = BufferSize;

    // Open the root volume
    Status = FsProtocol->OpenVolume(FsProtocol, &Root);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // Use a loop to read all entries in the root directory
    while (TRUE) {
        // Create space for the next file name
        FileName = AllocatePool(FileNameSize);
        if (FileName == NULL) {
            break; // Memory allocation failed, exit loop
        }

        // Read the next file info
        Status = Root->Read(Root, &FileInfoSize, FileInfo);
        if (EFI_ERROR(Status) || FileInfoSize == 0) {
            FreePool(FileName);
            break; // No more files or error
        }

        // Print the file name
        Print(L"Found file: %s\n", FileInfo->FileName);

        FreePool(FileName);
    }

    FreePool(FileInfo);
    Root->Close(Root);
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
NTFSSequence(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    CHAR16 FileName[] = L"\\TestFile.txt";
    CHAR8 WriteData[] = "Hello from EFI Ash!";
    CHAR8 ReadData[100];
    UINTN ReadSize = sizeof(ReadData);

    // Status = LoadNtfsDriver(ImageHandle, SystemTable);
    // Locate all NTFS partitions
    Status = ListNtfsPartitions(&HandleBuffer, &HandleCount);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to locate NTFS partitions: %r\n", Status);
        return Status;
    }

    // Display partitions and select one (for simplicity, we’ll use the first one here)
    Print(L"Available NTFS Partitions: %u\n", HandleCount);
    if (HandleCount == 0)
    {
        Print(L"No NTFS partitions found.\n");
        return EFI_NOT_FOUND;
    }

    // Get the Simple File System Protocol of the first NTFS partition
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FsProtocol;
    Status = gBS->HandleProtocol(HandleBuffer[1], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&FsProtocol);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to get file system protocol: %r\n", Status);
        return Status;
    }
    
    // Open file for reading and writing
    EFI_FILE_PROTOCOL *FileHandle;
    Status = OpenNtfsFile(FsProtocol, FileName, &FileHandle, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open file: %r\n", Status);
        return Status;
    }

    // Write data to the file
    Status = WriteFileContents(FileHandle, WriteData, sizeof(WriteData) - 1);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to write to file: %r\n", Status);
        FileHandle->Close(FileHandle);
        return Status;
    }
    Print(L"Data written successfully.\n");

    // Set file position to beginning for reading
    Status = FileHandle->SetPosition(FileHandle, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to set file position: %r\n", Status);
        FileHandle->Close(FileHandle);
        return Status;
    }

    // Read data from the file
    Status = ReadFileContents(FileHandle, ReadData, &ReadSize);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read from file: %r\n", Status);
        FileHandle->Close(FileHandle);
        return Status;
    }
    Print(L"Read Data: %a\n", ReadData);

    // Close the file handle
    FileHandle->Close(FileHandle);

    Status = ListFilesInRootDirectory(FsProtocol);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to list files in root directory: %r\n", Status);
        return Status;
    }
    
    return EFI_SUCCESS;
}
