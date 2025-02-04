#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Protocol/SimplePointer.h>

#define FASTFAIL() \
   if (EFI_ERROR(Status)) { \
      return Status; \
   }

typedef enum {
   NEKO_SLEEPING,
   NEKO_WALKING,
   NEKO_RUNNING,
   NEKO_WAITING,
   NEKO_STARTLED,
} NekoAnim;

typedef struct {
   INT32 NekoX;
   INT32 NekoY;

   INT32 PtrX;
   INT32 PtrY;

   NekoAnim Anim;
   BOOLEAN ShouldQuit;
} NekoState;

EFI_STATUS EFIAPI
NekoInitSpp(EFI_SIMPLE_POINTER_PROTOCOL **Spp) {
   EFI_STATUS Status;

   if (Spp == NULL) {
      return EFI_INVALID_PARAMETER;
   }

   Status = gBS->LocateProtocol(&gEfiSimplePointerProtocolGuid,
                                NULL,
                                (VOID**)Spp);
   FASTFAIL();

   Status = (*Spp)->Reset(*Spp, FALSE);
   FASTFAIL();

   return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
NekoSetGopMode(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop) {
   EFI_STATUS Status;
   UINTN MaxMode = Gop->Mode->MaxMode;
   UINTN BestIdx = 0;
   UINTN BestRes = 0;

   for (UINTN i = 0; i < MaxMode; i++) {
      UINTN OutSize = 0;
      EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *ModeInfo = NULL;

      Status = Gop->QueryMode(Gop, i, &OutSize, &ModeInfo);
      ModeInfo = AllocatePool(OutSize);
      if (ModeInfo == NULL) {
         continue;
      }

      Status = Gop->QueryMode(Gop, i, &OutSize, &ModeInfo);
      if (!EFI_ERROR(Status)) {
         UINTN CurrentRes = ModeInfo->HorizontalResolution
                          * ModeInfo->VerticalResolution;
         if (CurrentRes > BestRes) {
            BestRes = CurrentRes;
            BestIdx = i;
         }
      }

      FreePool(ModeInfo);
   }

   Status = Gop->SetMode(Gop, BestIdx);
   FASTFAIL();

   return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
NekoInitGop(EFI_GRAPHICS_OUTPUT_PROTOCOL **Gop, NekoState *State) {
   EFI_STATUS Status;

   if (Gop == NULL) {
      return EFI_INVALID_PARAMETER;
   }

   Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,
                                NULL,
                                (VOID**)Gop);
   FASTFAIL();

   Status = NekoSetGopMode(*Gop);
   FASTFAIL();

   return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
NekoSetBackground(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop, NekoState *State) {
   // we'll use a black screen for now, add support for images etc later.
   EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pix = {0, 0, 0, 0};
   UINTN Width = Gop->Mode->Info->HorizontalResolution;
   UINTN Height = Gop->Mode->Info->VerticalResolution;

   Gop->Blt(Gop, &Pix, EfiBltVideoFill, 0, 0, 0, 0, Width, Height, 0);

   return EFI_SUCCESS;
}

VOID EFIAPI
NekoHandleKeyEvent(EFI_INPUT_KEY Key, NekoState *State) {
   switch (Key.UnicodeChar) {
   case 'q':
      State->ShouldQuit = TRUE;
      break;
   }
}

VOID EFIAPI
NekoHandleMouseEvent(EFI_SIMPLE_POINTER_STATE Ptr, NekoState *State) {
   if (Ptr.LeftButton == TRUE) {
      Print(L"Left button pressed\n");
   } 
}

EFI_STATUS EFIAPI 
NekoMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
   EFI_STATUS Status;
   EFI_SIMPLE_POINTER_PROTOCOL *Spp = NULL;
   EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;

   NekoState State = {0};

   if (NekoInitSpp(&Spp) != EFI_SUCCESS) {
      return Status;
   }

   if (NekoInitGop(&Gop, &State) != EFI_SUCCESS) {
      return Status;
   }

   NekoSetBackground(Gop, &State);

   EFI_EVENT PtrEvent;
   EFI_EVENT WaitList[2];

   Status = gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &PtrEvent);
   FASTFAIL();
   Status = gBS->SetTimer(PtrEvent, TimerPeriodic, 50000);
   FASTFAIL();

   WaitList[0] = PtrEvent;
   WaitList[1] = gST->ConIn->WaitForKey;

   State.ShouldQuit = FALSE;

   while (State.ShouldQuit == FALSE) {
      UINTN Idx;
      Status = gBS->WaitForEvent(2, WaitList, &Idx);

      switch (Idx) {
      case 0: // mouse event
         EFI_SIMPLE_POINTER_STATE Ptr;
         if (Spp->GetState(Spp, &Ptr) == EFI_SUCCESS) {
            NekoHandleMouseEvent(Ptr, &State);
         }
         break;
      case 1: // keyboard event
         EFI_INPUT_KEY Key;
         if (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_SUCCESS) {
            NekoHandleKeyEvent(Key, &State);
         }
      }
   }

   return EFI_SUCCESS;
}
