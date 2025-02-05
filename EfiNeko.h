#ifndef __EFINEKO_H__
#define __EFINEKO_H__

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

typedef unsigned long size_t;

typedef struct {
   BOOLEAN EnableKeyboardEvents;
   CHAR16 *SpriteSheetPath;
   UINTN NekoTickSpeed;
} EFINEKO_SETUP;

typedef struct {
   EFI_EVENT WaitList[3];
} EFINEKO_STATE;

EFI_STATUS EFIAPI
EfiNekoInit(EFINEKO_SETUP *Setup, EFINEKO_STATE *State);

EFI_STATUS EFIAPI
EfiNekoDestroy(EFINEKO_STATE *State);

#define LODEPNG_NO_COMPILE_ALLOCATORS

void* lodepng_malloc(size_t Size) {
   return AllocatePool(Size);
}

void lodepng_free(void *Addr) {
   if (Addr) {
      FreePool(Addr);
   }
}

void* lodepng_realloc(void *Addr, size_t Size) {
   if (Size == 0) {
      lodepng_free(Addr);
      return NULL;
   }

   if (Addr == NULL) {
      return lodepng_malloc(Size);
   }

   VOID *NewAddr = AllocatePool(Size);
   if (NewAddr == NULL) {
      return NULL;
   }

   CopyMem(NewAddr, Addr, Size);

   lodepng_free(Addr);
   return NewAddr;
}

#include "lodepng.h"

#endif // __EFINEKO_H__
