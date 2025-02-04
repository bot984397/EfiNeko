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

#define CURSOR_WIDTH 1
#define CURSOR_HEIGHT 1

typedef enum {
   NEKO_SLEEPING,
   NEKO_WALKING,
   NEKO_RUNNING,
   NEKO_WAITING,
   NEKO_STARTLED,
} NekoAnim;

typedef struct {
   EFI_SIMPLE_POINTER_PROTOCOL *Spp;
   EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;

   INT32 NekoX;
   INT32 NekoY;

   INT32 PtrX;
   INT32 PtrY;
   INT32 ScrX;
   INT32 ScrY;

   NekoAnim Anim;
   BOOLEAN ShouldQuit;
} NekoState;

VOID EFIAPI
NekoHardReset(NekoState *State) {

}

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

   State->ScrX = (*Gop)->Mode->Info->HorizontalResolution;
   State->ScrY = (*Gop)->Mode->Info->VerticalResolution;

   return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
NekoDrawBackground(NekoState *State) {
   EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = State->Gop;
   // we'll use a black screen for now, add support for images etc later.
   EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pix = {0, 0, 0, 0};

   UINTN Width = Gop->Mode->Info->HorizontalResolution;
   UINTN Height = Gop->Mode->Info->VerticalResolution;

   Gop->Blt(Gop, &Pix, EfiBltVideoFill, 0, 0, 0, 0, Width, Height, 0);

   return EFI_SUCCESS;
}

VOID EFIAPI
NekoUpdateCursorPos(EFI_SIMPLE_POINTER_STATE Ptr, NekoState *State) {
   // FIXME: add adaptive scaling
   INT32 NewX = State->PtrX + (Ptr.RelativeMovementX / 100);
   INT32 NewY = State->PtrY + (Ptr.RelativeMovementY / 100);

   State->PtrX = MAX(0, MIN(NewX, (INT32)State->ScrX - CURSOR_WIDTH));
   State->PtrY = MAX(0, MIN(NewY, (INT32)State->ScrY - CURSOR_HEIGHT));
}

VOID EFIAPI
NekoDrawCursor(NekoState *State) {
   EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = State->Gop;
   EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pix = {255, 255, 255, 0};
   Gop->Blt(Gop, &Pix, EfiBltVideoFill, 0, 0, State->PtrX, State->PtrY,
            CURSOR_WIDTH, CURSOR_HEIGHT, 0);
}

VOID EFIAPI
NekoUpdateSpritePos() {

}

VOID EFIAPI
NekoDrawSprite(NekoState *State) {

}

VOID EFIAPI
NekoHandleKeyEvent(EFI_INPUT_KEY Key, NekoState *State) {
   switch (Key.UnicodeChar) {
   case 'q':
   case 'Q':
      State->ShouldQuit = TRUE;
      break;
   case 'r':
   case 'R':
      NekoHardReset(State);
   }
}

VOID EFIAPI
NekoHandleMouseEvent(EFI_SIMPLE_POINTER_STATE Ptr, NekoState *State) {
   if (Ptr.LeftButton == TRUE) {
      Print(L"Left button pressed\n");
   } 

   NekoUpdateCursorPos(Ptr, State);
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

   State.Spp = Spp;
   State.Gop = Gop;

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

      NekoDrawBackground(&State);
      NekoDrawCursor(&State);
   }

   return EFI_SUCCESS;
}
