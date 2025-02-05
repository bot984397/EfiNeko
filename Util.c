#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>

#define FASTFAIL() \
   if (EFI_ERROR(Status)) { \
      return Status; \
   }

EFI_STATUS EFIAPI
UtLoadFileFromRoot(EFI_HANDLE ImageHandle, 
                   CHAR16 *FileName, 
                   VOID **FileBuffer, 
                   UINTN *FileSize) {
   EFI_STATUS Status;
   EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
   EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
   EFI_FILE_PROTOCOL *Root;
   EFI_FILE_PROTOCOL *File;
   EFI_FILE_INFO *FileInfo;
   UINTN BufferSize;

   if (!ImageHandle || !FileName || !FileBuffer || !FileSize) {
      return EFI_INVALID_PARAMETER;
   }

   *FileBuffer = NULL;
   *FileSize = 0;

   Status = gBS->HandleProtocol(
         ImageHandle,
         &gEfiLoadedImageProtocolGuid,
         (VOID**)&LoadedImage
   );
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = gBS->HandleProtocol(
         LoadedImage->DeviceHandle,
         &gEfiSimpleFileSystemProtocolGuid,
         (VOID**)&FileSystem
   );
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = FileSystem->OpenVolume(FileSystem, &Root);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = Root->Open(Root, &File, FileName, EFI_FILE_MODE_READ, 0);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   BufferSize = sizeof(EFI_FILE_INFO) + 256;
   FileInfo = AllocatePool(BufferSize);
   if (!FileInfo) {
      return EFI_OUT_OF_RESOURCES;
   }

   Status = File->GetInfo(
         File,
         &gEfiFileInfoGuid,
         &BufferSize,
         FileInfo
   );
   if (EFI_ERROR(Status)) {
      FreePool(FileInfo);
      File->Close(File);
      Root->Close(Root);
      return Status;
   }

   *FileSize = FileInfo->FileSize;
   FreePool(FileInfo);

   *FileBuffer = AllocatePool(*FileSize);
   if (!*FileBuffer) {
      File->Close(File);
      Root->Close(Root);
      return EFI_OUT_OF_RESOURCES;
   }

   BufferSize = *FileSize;
   Status = File->Read(File, &BufferSize, *FileBuffer);

   File->Close(File);
   Root->Close(Root);

   if (EFI_ERROR(Status) || BufferSize != *FileSize) {
      FreePool(*FileBuffer);
      *FileBuffer = NULL;
      *FileSize = 0;
      return EFI_DEVICE_ERROR;
   }

   return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UtAllocatePool(VOID** Buffer, UINTN Size) {
   *Buffer = AllocatePool(Size);
   return *Buffer == NULL ? EFI_OUT_OF_RESOURCES : EFI_SUCCESS;
}
