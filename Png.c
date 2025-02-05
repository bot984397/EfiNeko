#include <Base.h>

#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>

#include "Util.h"

#define EC(expr) do { \
   EFI_STATUS Status = (expr); \
   if (EFI_ERROR(Status)) return Status; \
} while (0)

typedef struct {
   UINT32 Width;
   UINT32 Height;
   UINT8 BitDepth;
   UINT8 ColorType;
   UINT8 CompressionMethod;
   UINT8 FilterMethod;
   UINT8 InterlaceMethod;
} PNG_IHDR;

typedef struct {
   UINT8 R;
   UINT8 G;
   UINT8 B;
} __attribute__((packed)) PLTE_CHUNK;
STATIC_ASSERT(sizeof(PLTE_CHUNK) == 3, "PLTE Chunk must be 3 bytes in size");

typedef struct {
   UINT8 *Data;
   UINTN DataSize;
   UINTN AllocSize;
   PNG_IHDR Header;
} PNG_IMAGE;

static BOOLEAN EFIAPI
PngCheckSignature(UINT8 *Buffer) {
   return Buffer != NULL && *(const UINT64*)Buffer == 0x0A1A0A0D474E5089ULL;
}

static UINT32 EFIAPI
PngGetBytesPerPixel(PNG_IHDR *Header) {
   UINT32 Bpp;

   switch (Header->ColorType) {
   case 0: // Grayscale
   case 3: // Indexed Color (Palette)
      Bpp = Header->BitDepth;
      break;
   case 2: // Truecolor (RGB)
      Bpp = Header->BitDepth * 3;
      break;
   case 4: // Grayscale + Alpha
      Bpp = Header->BitDepth * 2;
      break;
   case 6: // Truecolor (RGB) + Alpha
      Bpp = Header->BitDepth * 4;
      break;
   default:
      Bpp = 0;
   }

   return Bpp;
}

static EFI_STATUS EFIAPI
PngAppendIDAT(PNG_IMAGE *Image, UINT8 *ChunkData, UINT32 ChunkLen) {
   if (Image->Data == NULL) {
      Image->Data = AllocatePool(ChunkLen);
      if (Image->Data == NULL) {
         return EFI_OUT_OF_RESOURCES;
      }
      ZeroMem(Image->Data, ChunkLen);
   } else {
      UINT8 *NewData = ReallocatePool(Image->DataSize,
                                      Image->DataSize + ChunkLen,
                                      Image->Data);
      if (NewData == NULL) {
         return EFI_OUT_OF_RESOURCES;
      }
      Image->Data = NewData;
   }

   CopyMem(Image->Data + Image->DataSize, ChunkData, ChunkLen);
   Image->DataSize += ChunkLen;
   return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
PngParseIHDR(UINT8 *Buffer, PNG_IHDR *Header) {
   EFI_STATUS Status = EFI_SUCCESS;

   CopyMem(&Header->Height, Buffer, sizeof(UINT32));
   Header->Height = SwapBytes32(Header->Height);
   CopyMem(&Header->Width, Buffer + 4, sizeof(UINT32));
   Header->Width = SwapBytes32(Header->Width);

   Header->BitDepth           = Buffer[8];
   Header->ColorType          = Buffer[9];
   Header->CompressionMethod  = Buffer[10];
   Header->FilterMethod       = Buffer[11];
   Header->InterlaceMethod    = Buffer[12];

   switch (Header->ColorType) {
   case 0:
      if (Header->BitDepth != 1 && Header->BitDepth != 2 &&
          Header->BitDepth != 4 && Header->BitDepth != 8 &&
          Header->BitDepth != 16) {
         Status = EFI_UNSUPPORTED;
      }
      break;
   case 3:
      if (Header->BitDepth != 1 && Header->BitDepth != 2 &&
          Header->BitDepth != 4 && Header->BitDepth != 8) {
         Status = EFI_UNSUPPORTED;
      }
      break;
   case 2:
   case 4:
   case 16:
      if (Header->BitDepth != 8 && Header->BitDepth != 16) {
         Status = EFI_UNSUPPORTED;
      }
      break;
   }

   return Status;
}

static EFI_STATUS EFIAPI
PngParsePLTE(UINT8 *Buffer, PNG_IMAGE *Image) {
   return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
PngParseIDAT(UINT8 *Buffer, PNG_IMAGE *Image) {
   return EFI_SUCCESS;
}

static VOID EFIAPI
PngInitCRCTable(UINT32 *Table) {
   UINT32 c;
   for (UINT32 n = 0; n < 256; n++) {
      c = n;
      for (UINT32 k = 0; k < 8; k++) {
         if (c & 1) {
            c = 0xEDB88320 ^ (c >> 1);
         } else {
            c = c >> 1;
         }
      }
      Table[n] = c;
   }
}

static UINT32 EFIAPI
PngCalculateChunkCRC(UINT8 *ChunkType, UINT8 *ChunkData, UINT32 ChunkLen) {
   UINT32 CrcTable[256];
   PngInitCRCTable(CrcTable);

   UINT32 Crc = 0xFFFFFFFF;

   for (UINT32 i = 0; i < 4; i++) {
      Crc = CrcTable[(Crc ^ ChunkType[i]) & 0xFF] ^ (Crc >> 8);
   } 
   for (UINT32 i = 0; i < ChunkLen; i++) {
      Crc = CrcTable[(Crc ^ ChunkData[i]) & 0xFF] ^ (Crc >> 8);
   }
    
   return ~Crc; 
}

EFI_STATUS EFIAPI
PngInflateImage(PNG_IMAGE *Image) {
   return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
PngDecodeImage(UINT8 *Buffer, UINTN Size) { 
   if (Size < 8 || !PngCheckSignature(Buffer)) {
      return EFI_UNSUPPORTED;
   }

   PNG_IMAGE Image = {0};
   UINTN IDatSize = 0;

   UINT8 *CurrentChunk = Buffer + 8;
   UINT32 ChunkLen;
   UINT32 ChunkCRC;
   UINT32 CalcCRC;
   UINT8 *ChunkType;
   UINT8 *ChunkData;
   
   while (CurrentChunk < Buffer + Size) {
      CopyMem(&ChunkLen, CurrentChunk, sizeof(UINT32));
      ChunkLen = SwapBytes32(ChunkLen);

      ChunkType = CurrentChunk + 4;
      ChunkData = CurrentChunk + 8;

      CopyMem(&ChunkCRC, ChunkData + ChunkLen, sizeof(UINT32));
      ChunkCRC = SwapBytes32(ChunkCRC);

      CalcCRC = PngCalculateChunkCRC(ChunkType, ChunkData, ChunkLen);
      if (CalcCRC != ChunkCRC) {
         return EFI_UNSUPPORTED;
      }

      if (CompareMem(ChunkType, (UINT8*)"IHDR", 4) == 0) {
         if (ChunkLen < 13) {
            return EFI_UNSUPPORTED;
         }
         EC(PngParseIHDR(ChunkData, &Image.Header));
      }

      else if (CompareMem(ChunkType, (UINT8*)"PLTE", 4) == 0) {
         if (ChunkLen % 3 != 0 || ChunkLen > (3 * sizeof(PLTE_CHUNK))) {
            return EFI_UNSUPPORTED;
         }
         EC(PngParsePLTE(ChunkData, &Image));
      }

      else if (CompareMem(ChunkType, (UINT8*)"IDAT", 4) == 0) {
         // zero-bytes long IDAT chunks are allowed according to PNG spec
         if (ChunkLen == 0) {
            goto ADVANCE;
         }
         EC(PngParseIDAT(ChunkData, &Image));
         EC(PngAppendIDAT(&Image, ChunkData, ChunkLen));
         IDatSize += ChunkLen;
      }

      else if (CompareMem(ChunkType, (UINT8*)"IEND", 4) == 0) {
         if (ChunkLen != 0) {
            return EFI_UNSUPPORTED;
         }
         break;
      }

ADVANCE:
      CurrentChunk += ChunkLen + 12;
      if (CurrentChunk > Buffer + Size ) {
         return EFI_UNSUPPORTED;
      }
   }

   UINT32 Bpp = PngGetBytesPerPixel(&Image.Header);
   Bpp++;

   Print(L"Height: %d, Width: %d\n", Image.Header.Height, Image.Header.Width);
   Print(L"Image data size: %d, total IDAT size: %d\n", Image.DataSize, IDatSize);
   Print(L"Color type: %d\n", Image.Header.ColorType);
   Print(L"First bytes: 0x%02X, 0x%02X\n", Image.Data[0], Image.Data[1]);

   EC(PngInflateImage(&Image));

   return EFI_SUCCESS;
}
