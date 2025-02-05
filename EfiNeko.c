#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BmpSupportLib.h>

#include <Protocol/SimplePointer.h>

#include "EfiNeko.h"
#include "Util.h"
#include "Sprite.h"
#include "Png.h"

#define FASTFAIL() \
   if (EFI_ERROR(Status)) { \
      return Status; \
   }

#define EC(expr) do { EFI_STATUS Status = (expr); if (EFI_ERROR(Status)) return Status; } while (0)

#define NEKO_ANIM_INTERVAL 8000000
#define NEKO_INPUT_INTERVAL 50000

#define CURSOR_WIDTH 16
#define CURSOR_HEIGHT 16

#define SPRITE_DIRECTORY   "sprites"
#define IMAGE_DIRECTORY    "img"
#define CONFIG_FILE        "EfiNeko.ini"

typedef enum {
   NEKO_SLEEPING,
   NEKO_WALKING,
   NEKO_RUNNING,
   NEKO_WAITING,
   NEKO_STARTLED,
} NekoAnim;

typedef struct {
   EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buffer;
   UINTN Width;
   UINTN Height;
} NEKO_SPRITE;

typedef struct {
   EFI_SIMPLE_POINTER_PROTOCOL *Spp;
   EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;

   INT32 NekoX;
   INT32 NekoY;

   INT32 PtrX;
   INT32 PtrY;
   INT32 PtrXPrev;
   INT32 PtrYPrev;

   INT32 ScrX;
   INT32 ScrY;

   EFI_GRAPHICS_OUTPUT_BLT_PIXEL *PrevBuffer;

   NekoAnim Anim;
   BOOLEAN ShouldQuit;

   EFI_HANDLE ImageHandle;
} NekoState;

EFI_STATUS EFIAPI
EfiNekoInit(EFINEKO_SETUP *Setup, EFINEKO_STATE *State) {
   EFI_EVENT EventList[3];

   EFI_EVENT MouseEvent;
   EC(gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &MouseEvent));
   EC(gBS->SetTimer(MouseEvent, TimerPeriodic, NEKO_INPUT_INTERVAL));

   EFI_EVENT NekoTickEvent;
   UINTN TickSpeed = Setup->NekoTickSpeed == 0 ? NEKO_ANIM_INTERVAL
                                               : Setup->NekoTickSpeed;
   EC(gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &NekoTickEvent));
   EC(gBS->SetTimer(NekoTickEvent, TimerPeriodic, TickSpeed));

   EventList[0] = MouseEvent;
   EventList[1] = NekoTickEvent;
   if (Setup->EnableKeyboardEvents) {
      EventList[2] = gST->ConIn->WaitForKey;
   }
   State->WaitList[0] = EventList[0];

   return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
EfiNekoDestroy(EFINEKO_STATE *State) {
   EFI_EVENT EventList[3];
   EventList[0] = State->WaitList[0];

   EC(gBS->SetTimer(EventList[0], TimerCancel, 0));
   EC(gBS->CloseEvent(EventList[0]));
   EC(gBS->SetTimer(EventList[1], TimerCancel, 0));
   EC(gBS->CloseEvent(EventList[1]));

   return EFI_SUCCESS;
}

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
   INT32 NewX = State->PtrX + (Ptr.RelativeMovementX / 10000);
   INT32 NewY = State->PtrY + (Ptr.RelativeMovementY / 10000);

   State->PtrX = MAX(0, MIN(NewX, (INT32)State->ScrX - CURSOR_WIDTH));
   State->PtrY = MAX(0, MIN(NewY, (INT32)State->ScrY - CURSOR_HEIGHT));
}

VOID EFIAPI
NekoDrawCursor(NekoState *State) {
   EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = State->Gop;

   /*
   Gop->Blt(Gop, State->PrevBuffer, EfiBltBufferToVideo, 0, 0,
            State->PtrXPrev, State->PtrYPrev, CURSOR_WIDTH, CURSOR_HEIGHT, 0);

   Gop->Blt(Gop, State->PrevBuffer, EfiBltVideoToBltBuffer, 0, 0,
            State->PtrX, State->PtrY, CURSOR_WIDTH, CURSOR_HEIGHT, 0);
   */

   EFI_GRAPHICS_OUTPUT_BLT_PIXEL Blk = {0, 0, 0, 0};
   Gop->Blt(Gop, &Blk, EfiBltVideoFill, 0, 0, State->PtrXPrev, State->PtrYPrev,
            CURSOR_WIDTH, CURSOR_HEIGHT, 0);

   EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pix = {255, 255, 255, 0};
   Gop->Blt(Gop, &Pix, EfiBltVideoFill, 0, 0, State->PtrX, State->PtrY,
            CURSOR_WIDTH, CURSOR_HEIGHT, 0);

   State->PtrXPrev = State->PtrX;
   State->PtrYPrev = State->PtrY;
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

static VOID test(EFI_HANDLE ImageHandle) {
   EFI_STATUS Status;
   UINT8 *Buffer;
   UINTN Size;
   UtLoadFileFromRoot(ImageHandle, L"sample.png", (VOID**)&Buffer, &Size);

   Status = PngDecodeImage(Buffer, Size);
   Print(L"Decode status: %r", Status);
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
   State.PtrX = 0;
   State.PtrY = 0;
   State.PtrXPrev = 0;
   State.PtrYPrev = 0;
   State.ShouldQuit = FALSE;
   State.ImageHandle = ImageHandle;
   
   EC(UtAllocatePool((VOID**)&State.PrevBuffer, CURSOR_WIDTH * CURSOR_HEIGHT
            * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL)));

   EFI_EVENT MouseEvent;
   EC(gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &MouseEvent));
   EC(gBS->SetTimer(MouseEvent, TimerPeriodic, NEKO_INPUT_INTERVAL));

   EFI_EVENT NekoTickEvent;
   EC(gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &NekoTickEvent));
   EC(gBS->SetTimer(NekoTickEvent, TimerPeriodic, NEKO_ANIM_INTERVAL));

   EFI_EVENT WaitList[3] = { 
      MouseEvent,
      NekoTickEvent,
      gST->ConIn->WaitForKey
   };

   NekoDrawBackground(&State);

   /// TEMPORARY BEGIN

   test(ImageHandle);

   while (TRUE) {}

   /// TEMPORARY END

   while (State.ShouldQuit == FALSE) {
      UINTN Idx;
      Status = gBS->WaitForEvent(2, WaitList, &Idx);

      switch (Idx) {
      case 0: // pointer interval
         EFI_SIMPLE_POINTER_STATE Ptr;
         if (Spp->GetState(Spp, &Ptr) == EFI_SUCCESS) {
            NekoHandleMouseEvent(Ptr, &State);
         }
         break;
      case 1: // animation interval
         EFI_INPUT_KEY Key;
         if (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_SUCCESS) {
            NekoHandleKeyEvent(Key, &State);
         }
         break;
      case 2: // keyboard input

      }
      NekoDrawCursor(&State);
      NekoDrawSprite(&State);
   }

   return EFI_SUCCESS;
}
