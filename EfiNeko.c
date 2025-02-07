#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

#include <Protocol/SimplePointer.h>
#include <Protocol/LoadedImage.h>

#include "EfiNeko.h"
#include "Util.h"
#include "Anim.h"

#define FASTFAIL() \
   if (EFI_ERROR(Status)) { \
      return Status; \
   }

#define EC(expr) do { EFI_STATUS Status = (expr); if (EFI_ERROR(Status)) return Status; } while (0)

#define NEKO_ANIM_INTERVAL 700000
#define NEKO_INPUT_INTERVAL 50000

#define CURSOR_WIDTH 16
#define CURSOR_HEIGHT 16

#define SPRITE_DIRECTORY   "sprites"
#define IMAGE_DIRECTORY    "img"
#define CONFIG_FILE        "EfiNeko.ini"

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
   INT32 NekoXPrev;
   INT32 NekoYPrev;

   INT32 PtrX;
   INT32 PtrY;
   INT32 PtrXPrev;
   INT32 PtrYPrev;

   INT32 ScrX;
   INT32 ScrY;

   EFI_GRAPHICS_OUTPUT_BLT_PIXEL *PrevBuffer;

   BOOLEAN ShouldQuit;
   BOOLEAN NekoPaused;

   EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CursorImage;
   UINTN CursorWidth;
   UINTN CursorHeight;

   EFI_GRAPHICS_OUTPUT_BLT_PIXEL *SpsImage;
   UINTN SpsWidth;
   UINTN SpsHeight;

   NekoAnimationType CurrentAnimation;
   UINT8 CurrentFrame;
   UINT8 LoopIndex;
   UINTN TicksElapsed;
   UINT8 SpriteSheetX;
   UINT8 SpriteSheetY;

   EFI_HANDLE ImageHandle;
} NekoState;

EFI_STATUS EFIAPI
EfiNekoInit(EFINEKO_SETUP *Setup, EFINEKO_STATE *State) {
   EFI_EVENT MouseEvent;
   EC(gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &MouseEvent));
   EC(gBS->SetTimer(MouseEvent, TimerPeriodic, NEKO_INPUT_INTERVAL));

   EFI_EVENT NekoTickEvent;
   UINTN TickSpeed = Setup->NekoTickSpeed == 0 ? NEKO_ANIM_INTERVAL
                                               : Setup->NekoTickSpeed;
   EC(gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &NekoTickEvent));
   EC(gBS->SetTimer(NekoTickEvent, TimerPeriodic, TickSpeed));

   State->WaitList[0] = MouseEvent;
   State->WaitList[1] = NekoTickEvent;
   if (Setup->EnableKeyboardEvents) {
      State->WaitList[2] = gST->ConIn->WaitForKey;
   }

   return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
EfiNekoDestroy(EFINEKO_STATE *State) {
   if (!State) {
      return EFI_INVALID_PARAMETER;
   }

   EC(gBS->SetTimer(State->WaitList[0], TimerCancel, 0));
   EC(gBS->CloseEvent(State->WaitList[0]));
   EC(gBS->SetTimer(State->WaitList[1], TimerCancel, 0));
   EC(gBS->CloseEvent(State->WaitList[1]));

   return EFI_SUCCESS;
}

VOID EFIAPI
NekoHardReset(NekoState *State) {

}

static EFI_STATUS EFIAPI
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

static EFI_STATUS EFIAPI
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

static EFI_STATUS EFIAPI
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

static EFI_STATUS EFIAPI
NekoPngToGopBlt(UINT8 *ImageData, 
                UINTN ImageSize, 
                EFI_GRAPHICS_OUTPUT_BLT_PIXEL **BltBuffer,
                UINTN *Width,
                UINTN *Height) {
   unsigned char *Image = NULL;
   unsigned int W, H;

   UINT32 Error = lodepng_decode32(&Image, &W, &H, ImageData, ImageSize);
   if (Error) {
      return EFI_INVALID_PARAMETER;
   }

   *Width = W;
   *Height = H;

   *BltBuffer = AllocatePool(W * H * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
   if (*BltBuffer == NULL) {
      lodepng_free(Image);
      return EFI_OUT_OF_RESOURCES;
   }

   EFI_GRAPHICS_OUTPUT_BLT_PIXEL Filter;
   Filter.Red = Image[0];
   Filter.Green = Image[1];
   Filter.Blue = Image[2];
   Filter.Reserved = Image[3];

   for (UINTN y = 0; y < H; y++) {
      for (UINTN x = 0; x < W; x++) {
         UINTN idx = (y * W + x) * 4;

         if (Image[idx + 0] == Filter.Red && 
             Image[idx + 1] == Filter.Green &&
             Image[idx + 2] == Filter.Blue) {
            (*BltBuffer)[y * W + x].Red = 0;
            (*BltBuffer)[y * W + x].Green = 0;
            (*BltBuffer)[y * W + x].Blue = 0;
            (*BltBuffer)[y * W + x].Reserved = 0;
         } else {
            (*BltBuffer)[y * W + x].Red = Image[idx];
            (*BltBuffer)[y * W + x].Green = Image[idx + 1];
            (*BltBuffer)[y * W + x].Blue = Image[idx + 2];
            (*BltBuffer)[y * W + x].Reserved = Image[idx + 3];
         }
      }
   }

   lodepng_free(Image);

   return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
NekoDrawBackground(NekoState *State) {
   EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = State->Gop;
   // we'll use a black screen for now, add support for images etc later.
   EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pix = {0, 0, 0, 0};

   UINTN Width = Gop->Mode->Info->HorizontalResolution;
   UINTN Height = Gop->Mode->Info->VerticalResolution;

   Gop->Blt(Gop, &Pix, EfiBltVideoFill, 0, 0, 0, 0, Width, Height, 0);

   return EFI_SUCCESS;
}

static VOID EFIAPI
NekoUpdateCursorPos(EFI_SIMPLE_POINTER_STATE Ptr, NekoState *State) {
   // FIXME: add adaptive scaling
   INT32 NewX = State->PtrX + (Ptr.RelativeMovementX / 10000);
   INT32 NewY = State->PtrY + (Ptr.RelativeMovementY / 10000);

   State->PtrX = MAX(0, MIN(NewX, (INT32)State->ScrX - CURSOR_WIDTH));
   State->PtrY = MAX(0, MIN(NewY, (INT32)State->ScrY - CURSOR_HEIGHT));
}

static VOID EFIAPI
NekoDrawCursor(NekoState *State) {
   EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = State->Gop;

   EFI_GRAPHICS_OUTPUT_BLT_PIXEL Blk = {0, 0, 0, 0};
   Gop->Blt(Gop, &Blk, EfiBltVideoFill, 0, 0, 
            State->PtrXPrev, State->PtrYPrev,
            State->CursorWidth, State->CursorHeight,
            State->CursorWidth * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));

   Gop->Blt(Gop, State->CursorImage, EfiBltBufferToVideo, 0, 0,
            State->PtrX, State->PtrY,
            State->CursorWidth, State->CursorHeight,
            State->CursorWidth * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));

   State->PtrXPrev = State->PtrX;
   State->PtrYPrev = State->PtrY;
}

static NekoAnimationType EFIAPI
NekoGetDirection(INT32 Dx, INT32 Dy) {
   if (ABS(Dx) < 2 && ABS(Dy) < 2) {
      return NEKO_ANIM_IDLE;
   }

   INT32 Ratio;
   if (Dx != 0) {
      Ratio = (Dy * 1000) / Dx;
   } else {
      return (Dy > 0) ? NEKO_ANIM_RUN_DOWN : NEKO_ANIM_RUN_UP;
   }

   const INT32 THRESH_STEEP = 2414;
   const INT32 THRESH_SHALLOW = 414;

   if (Dx > 0) {
      if (ABS(Ratio) > THRESH_STEEP) {
         return (Dy > 0) ? NEKO_ANIM_RUN_DOWN : NEKO_ANIM_RUN_UP;
      } else if (ABS(Ratio) < THRESH_SHALLOW) {
         return NEKO_ANIM_RUN_RIGHT;
      } else {
         return (Dy > 0) ? NEKO_ANIM_RUN_DOWN_RIGHT : NEKO_ANIM_RUN_UP_RIGHT;
      }
   } else {
      if (ABS(Ratio) > THRESH_STEEP) {
         return (Dy > 0) ? NEKO_ANIM_RUN_DOWN : NEKO_ANIM_RUN_UP;
      } else if (ABS(Ratio) < THRESH_SHALLOW) {
         return NEKO_ANIM_RUN_LEFT;
      } else {
         return (Dy > 0) ? NEKO_ANIM_RUN_DOWN_LEFT : NEKO_ANIM_RUN_UP_LEFT;
      }
   }
}

static UINT32 EFIAPI
NekoIntSqrt(UINT32 Value) {
    if (Value <= 1) return Value;
    
    UINT32 Result = 0;
    UINT32 Bit = 1UL << 30;  // Start with highest power of 4 <= Value
    
    while (Bit > Value) {
        Bit >>= 2;
    }
    
    while (Bit != 0) {
        if (Value >= Result + Bit) {
            Value -= Result + Bit;
            Result = (Result >> 1) + Bit;
        } else {
            Result >>= 1;
        }
        Bit >>= 2;
    }
    
    return Result;
}

static VOID EFIAPI
NekoUpdateAnimation(NekoState *State) {
   const AnimationSequence *Sequence = 
      &AnimationSequences[State->CurrentAnimation];
   const AnimationFrame *Frame = &Sequence->Frames[State->CurrentFrame];

   State->TicksElapsed += 1;

   if (State->TicksElapsed == Frame->Duration) {
      if (Frame->Flags == NEKO_FRAME_FLAG_LOOP_END) {
         if (State->LoopIndex < Frame->LoopIterations || Frame->LoopIterations == 0) {
            for (UINTN i = State->CurrentFrame; i >= 0; i--) {
               if (Sequence->Frames[i].Flags == NEKO_FRAME_FLAG_LOOP_BEGIN) {
                  State->CurrentFrame = i;
                  State->TicksElapsed = 0;
                  break;
               }
            }
            State->LoopIndex++;
         } else {
            State->CurrentFrame++;
            State->TicksElapsed = 0;
            State->LoopIndex = 0;
         }
      } else {
         State->CurrentFrame++;
         State->TicksElapsed = 0;
      }

      if (State->CurrentFrame >= Sequence->FrameCount) {
         State->CurrentFrame = 0;
         State->TicksElapsed = 0;
      }
   }

   const AnimationSequence *NewSequence = 
      &AnimationSequences[State->CurrentAnimation];
   const AnimationFrame *NewFrame = &NewSequence->Frames[State->CurrentFrame];

   State->SpriteSheetX = NewFrame->SpriteSheetX;
   State->SpriteSheetY = NewFrame->SpriteSheetY;
}

static VOID EFIAPI
NekoUpdateSpritePos(NekoState *State) {
   if (State->NekoPaused == TRUE) {
      return;
   }

   INT32 Dx = ((INT32)State->PtrX - State->CursorWidth / 2) - 
      (INT32)State->NekoX;
   INT32 Dy = ((INT32)State->PtrY - State->CursorHeight / 2) - 
      (INT32)State->NekoY;

   UINT32 Dist = (Dx * Dx) + (Dy * Dy);

   if (Dist <= 1600) {
      Dx = 0;
      Dy = 0;
   }

   NekoAnimationType AnimationType = NekoGetDirection(Dx, Dy);

   const AnimationSequence *Sequence = 
      &AnimationSequences[State->CurrentAnimation];
   const AnimationFrame *Frame = &Sequence->Frames[State->CurrentFrame];

   if (Sequence->Flags & NEKO_ANIM_FLAG_NO_INTERRUPT) {
      if (State->CurrentFrame == Sequence->FrameCount - 1 &&
          State->TicksElapsed == Frame->Duration - 1) {
         State->TicksElapsed = 0;
         State->CurrentFrame = 0;
         State->LoopIndex = 0;
         State->CurrentAnimation = AnimationType;
      }
   } else {
      if (AnimationType != State->CurrentAnimation) {
         if (State->CurrentAnimation == NEKO_ANIM_IDLE) {
            AnimationType = NEKO_ANIM_STARTLED;
         }
         State->TicksElapsed = 0;
         State->CurrentFrame = 0;
         State->CurrentAnimation = AnimationType;
      }
   }

   /*
   if (AnimationType != State->CurrentAnimation) {
      if (State->CurrentAnimation == NEKO_ANIM_IDLE) {
         AnimationType = NEKO_ANIM_STARTLED;
      }
      State->TicksElapsed = 0;
      State->CurrentFrame = 0;
      State->LoopIndex = 0;
      State->CurrentAnimation = AnimationType;
   }
   */

   if (State->NekoX == 0) {
      State->CurrentAnimation = NEKO_ANIM_SCRATCH_LEFT;
   } else if (State->NekoX == State->ScrX) {
      State->CurrentAnimation = NEKO_ANIM_SCRATCH_RIGHT;
   } else if (State->NekoY == 0) {
      State->CurrentAnimation = NEKO_ANIM_SCRATCH_DOWN;
   } else if (State->NekoY == State->ScrY) {
      State->CurrentAnimation = NEKO_ANIM_SCRATCH_UP;
   }

   NekoUpdateAnimation(State);

   if (State->CurrentAnimation == NEKO_ANIM_STARTLED) {
      return;
   }

   const INT32 NEKO_SPEED = 5;

   INT32 Length = (INT32)NekoIntSqrt(Dist);
   if (Length > 0) {
      INT32 StepX = (Dx * NEKO_SPEED * 1000) / Length;
      INT32 StepY = (Dy * NEKO_SPEED * 1000) / Length;
        
      State->NekoX = (UINT32)((INT32)State->NekoX + (StepX + 500) / 1000);
      State->NekoY = (UINT32)((INT32)State->NekoY + (StepY + 500) / 1000);
   }
}

VOID EFIAPI
NekoDrawSprite(NekoState *State) {
   EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = State->Gop;
   #define SPRITE_SIZE 32
   #define BORDER_SIZE 1

   UINT8 SpriteX = State->SpriteSheetX;
   UINT8 SpriteY = State->SpriteSheetY;

   EFI_GRAPHICS_OUTPUT_BLT_PIXEL Blk = {0, 0, 0, 0};
   Gop->Blt(Gop, &Blk, EfiBltVideoFill, 0, 0, 
            State->NekoXPrev, State->NekoYPrev,
            SPRITE_SIZE, SPRITE_SIZE,
            SPRITE_SIZE * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));

   for (UINT32 y = 0; y < SPRITE_SIZE; y++) {
      EFI_GRAPHICS_OUTPUT_BLT_PIXEL *RowStart = &State->SpsImage[
         ((SpriteY * (SPRITE_SIZE + BORDER_SIZE) + y) * State->SpsWidth) + 
         (SpriteX * (SPRITE_SIZE + BORDER_SIZE))
      ];

      Gop->Blt(
         Gop,
         RowStart,
         EfiBltBufferToVideo,
         0,
         0,
         State->NekoX,
         State->NekoY + y,
         SPRITE_SIZE,
         1,
         State->SpsWidth
      );
   }

   State->NekoXPrev = State->NekoX;
   State->NekoYPrev = State->NekoY;
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
      break;
   case 'p':
   case 'P':
      State->NekoPaused = TRUE;
      break;
   }
}

VOID EFIAPI
NekoHandleMouseEvent(EFI_SIMPLE_POINTER_STATE Ptr, NekoState *State) {
   NekoUpdateCursorPos(Ptr, State);
}

EFI_STATUS EFIAPI 
NekoMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
   EFI_STATUS Status;
   EFI_SIMPLE_POINTER_PROTOCOL *Spp = NULL;
   EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
   
   EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
   CHAR16 *CmdLine;
   //UINTN Argc;
   //CHAR16 **Argv;

   Status = gBS->HandleProtocol(
         ImageHandle,
         &gEfiLoadedImageProtocolGuid,
         (VOID**)&LoadedImage
   );
   if (EFI_ERROR(Status)) {
      return Status;
   }

   CmdLine = (CHAR16*)LoadedImage->LoadOptions;
   CmdLine++;

   NekoState State = {0};

   Status = NekoInitSpp(&Spp);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = NekoInitGop(&Gop, &State);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   State.Spp = Spp;
   State.Gop = Gop;
   State.SpriteSheetX = 0;
   State.SpriteSheetY = 0;
   State.PtrX = 0;
   State.PtrY = 0;
   State.PtrXPrev = 0;
   State.PtrYPrev = 0;
   State.NekoX = 0;
   State.NekoY = 0;
   State.NekoXPrev = 0;
   State.NekoYPrev = 0;
   State.LoopIndex = 0;
   State.ShouldQuit = FALSE;
   State.NekoPaused = FALSE;
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

   UINT8 *CursorData = NULL;
   UINTN CursorSize;
   Status = UtLoadFileFromRoot(ImageHandle, L"cursor.png", 
                               (VOID**)&CursorData, &CursorSize);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CurBltBuffer = NULL;
   UINTN CursorWidth;
   UINTN CursorHeight;

   Status = NekoPngToGopBlt(CursorData, CursorSize, &CurBltBuffer,
                            &CursorWidth, &CursorHeight);
   FreePool(CursorData);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   State.CursorImage = CurBltBuffer;
   State.CursorWidth = CursorWidth;
   State.CursorHeight = CursorHeight;

   UINT8 *SpsData = NULL;
   UINTN SpsSize;
   Status = UtLoadFileFromRoot(ImageHandle, L"neko.png",
                               (VOID**)&SpsData, &SpsSize);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   EFI_GRAPHICS_OUTPUT_BLT_PIXEL *SpsBltBuffer = NULL;
   UINTN SpsWidth;
   UINTN SpsHeight;

   Status = NekoPngToGopBlt(SpsData, SpsSize, &SpsBltBuffer, 
                            &SpsWidth, &SpsHeight);
   FreePool(SpsData);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   State.SpsImage = SpsBltBuffer;
   State.SpsWidth = SpsWidth;
   State.SpsHeight = SpsHeight;

   NekoDrawBackground(&State);

   while (State.ShouldQuit == FALSE) {
      UINTN Idx;
      Status = gBS->WaitForEvent(3, WaitList, &Idx);

      switch (Idx) {
      case 0: // pointer interval
         EFI_SIMPLE_POINTER_STATE Ptr;
         if (Spp->GetState(Spp, &Ptr) == EFI_SUCCESS) {
            NekoHandleMouseEvent(Ptr, &State);
         }
         break;
      case 1: // animation interval
         NekoUpdateSpritePos(&State);
         break;
      case 2: // keyboard input
         EFI_INPUT_KEY Key;
         if (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_SUCCESS) {
            NekoHandleKeyEvent(Key, &State);
         }
         break;
      }
      NekoDrawSprite(&State);
      NekoDrawCursor(&State);
   }

   return EFI_SUCCESS;
}
