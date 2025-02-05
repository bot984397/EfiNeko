#ifndef __NEKO_PNG_H__
#define __NEKO_PNG_H__

#include <Uefi.h>

EFI_STATUS EFIAPI
PngDecodeImage(UINT8 *Buffer, UINTN Size);

#endif // __NEKO_PNG_H__
