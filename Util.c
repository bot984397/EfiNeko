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
UtLoadFileFromRoot(EFI_HANDLE Handle, CHAR16 *Name, VOID **Buf, UINTN *Size) {
   EFI_STATUS Status;
   EFI_LOADED_IMAGE_PROTOCOL *Lip;
   EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Sfsp;
   EFI_FILE_PROTOCOL *Root, *File;
   EFI_FILE_INFO *FileInfo;
   UINTN BufferSize;

   Status = gBS->HandleProtocol(Handle,
                                &gEfiLoadedImageProtocolGuid,
                                (VOID**)&Lip);
   FASTFAIL();

   Status = gBS->HandleProtocol(Lip->DeviceHandle,
                                &gEfiSimpleFileSystemProtocolGuid,
                                (VOID**)&Sfsp);
   FASTFAIL();

   Status = Sfsp->OpenVolume(Sfsp, &Root);
   FASTFAIL();

   Status = Root->Open(Root, &File, Name, EFI_FILE_MODE_READ, 0);
   FASTFAIL();

   BufferSize = sizeof(EFI_FILE_INFO) + 512;
   FileInfo = AllocatePool(BufferSize);
   if (!FileInfo) {
      File->Close(File);
      return EFI_OUT_OF_RESOURCES;
   }

   Status = File->GetInfo(File, &gEfiFileInfoGuid, &BufferSize, FileInfo);
   if (EFI_ERROR(Status)) {
      FreePool(FileInfo);
      File->Close(File);
      return Status;
   }

   *Size = FileInfo->FileSize;
   FreePool(FileInfo);

   *Buf = AllocatePool(*Size);
   if (!*Buf) {
      File->Close(File);
      return EFI_OUT_OF_RESOURCES;
   }

   Status = File->Read(File, Size, *Buf);
   File->Close(File);

   return Status;
}

EFI_STATUS EFIAPI UtAllocatePool(VOID** Buffer, UINTN Size) {
   *Buffer = AllocatePool(Size);
   return *Buffer == NULL ? EFI_OUT_OF_RESOURCES : EFI_SUCCESS;
}
