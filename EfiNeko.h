#include <Uefi.h>

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
