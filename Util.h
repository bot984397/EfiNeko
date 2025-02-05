#ifndef __NEKO_UTIL_H__
#define __NEKO_UTIL_H__

EFI_STATUS EFIAPI
UtLoadFileFromRoot(EFI_HANDLE Handle, CHAR16 *Name, VOID **Buf, UINTN *Size);

EFI_STATUS EFIAPI
UtAllocatePool(VOID** Buffer, UINTN Size);

#endif // __NEKO_UTIL_H__
