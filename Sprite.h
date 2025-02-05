#ifndef __NEKO_SPRITE_H__
#define __NEKO_SPRITE_H__

#include <Protocol/GraphicsOutput.h>

#define C_TRANSPARENT   ((EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0,   0,   0,   0  })
#define C_WHITE         ((EFI_GRAPHICS_OUTPUT_BLT_PIXEL){255, 255, 255, 255})
#define C_BLACK         ((EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0,   0,   0,   255})
#define C_BLUE          ((EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0,   0,   255, 255})
#define C_RED           ((EFI_GRAPHICS_OUTPUT_BLT_PIXEL){255, 0,   0,   255})

EFI_GRAPHICS_OUTPUT_BLT_PIXEL EFIAPI
SpCreatePixel(UINT8 R, UINT8 B, UINT8 G, UINT8 A);

#endif // __NEKO_SPRITE_H__
