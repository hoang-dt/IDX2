#pragma once

#include "Common.h"
#include "idx2Common.h"


namespace idx2
{


static constexpr i16 BitPlaneKeyBias_ = 1024;
static constexpr i16 ExponentBitPlane_ = -1024 + BitPlaneKeyBias_;

/* ---------------------- TYPES ----------------------*/
idx2_Inline u64
GetAddress(u64 Brick, int BrickShift, i8 Level, i8 Subband, i16 BitPlane)
{
  return (u64(Level) << 60) +               // 4 bits
         u64((Brick >> BrickShift) << 18) + // 42 bits
         (u64(Subband << 12)) +             // 6 bits
         (u64(BitPlane) & 0xFFF);           // 12 bits
}


idx2_Inline void
UnpackFileAddress(const idx2_file& Idx2, u64 Address, u64* Brick, i8* Level, i8* Subband, i16* BitPlane)
{
  *BitPlane = i16((i32(Address & 0xFFF) << 20) >> 20); // this convoluted double shifts is to keep the sign of BitPlane
  *Subband = (Address >> 12) & 0x3F;
  *Level = (Address >> 60) & 0xF;
  i8 BrickBitsPerFile =
    Size(Idx2.BrickIndexing[*Level].IndexTemplateInChunk) * Size(Idx2.ChunkIndexing[*Level].IndexTemplateInFile);
  *Brick = ((Address >> 18) & 0x3FFFFFFFFFFull) << BrickBitsPerFile;
}


idx2_Inline void
UnpackChunkAddress(const idx2_file& Idx2, u64 Address, u64* Brick, i8* Level, i8* Subband, i16* BitPlane)
{
  // TODO NEXT
  *BitPlane = i16((i32(Address & 0xFFF) << 20) >> 20); // this convoluted double shifts is to keep the sign of BitPlane
  *Subband = (Address >> 12) & 0x3F;
  *Level = (Address >> 60) & 0xF;
  i8 BrickBitsPerChunk = Size(Idx2.BrickIndexing[*Level].IndexTemplateInChunk);
  *Brick = ((Address >> 18) & 0x3FFFFFFFFFFull) << BrickBitsPerChunk;
}


// BpKey is (bit plane + BitPlaneKeyBias) / BitPlanesPerChunk
idx2_Inline u64
GetChunkAddress(const idx2_file& Idx2, u64 Brick, i8 Level, i8 Subband, i16 BpKey)
{
  // TODO NEXT
  i8 BrickBitsPerChunk = Size(Idx2.BrickIndexing[Level].IndexTemplateInChunk);
  return GetAddress(Brick, BrickBitsPerChunk, Level, Subband, BpKey);
}


idx2_Inline u64
GetFileAddress(const idx2_file& Idx2, u64 Brick, i8 Level, i8 Subband, i16 BpFileKey)
{
  Subband = 0; // all subbands are stored in the same file
  BpFileKey = 0;// all bit planes are stored in the same file
  // TODO NEXT
  i8 BrickBitsPerFile =
    Size(Idx2.BrickIndexing[Level].IndexTemplateInChunk) * Size(Idx2.ChunkIndexing[Level].IndexTemplateInFile);
  return GetAddress(Brick, BrickBitsPerFile, Level, Subband, BpFileKey);
}


/* Get the linear index of the input brick on the given level */
u64
GetLinearBrick(const idx2_file& Idx2, int Level, v3i Brick3);


v3i
GetSpatialBrick(const idx2_file& Idx2, int Level, u64 LinearBrick);


extent
ChunkAddressToSpatial(const idx2_file& Idx2, int Level, u64 LinearBrick);


file_id
ConstructFilePath(const idx2_file& Idx2, u64 Brick, i8 Level, i8 Subband, i16 BitPlane);


file_id
ConstructFilePath(const idx2_file& Idx2, u64 BrickAddress);


enum class file_type
{
  MainDataFile,
  MetadataFile
};

// Compose a key from Brick + Level
idx2_Inline u64
GetBrickKey(i8 Level, u64 Brick)
{
  return (Brick << 4) + Level;
}


// Get the Brick from a Key composed of Brick + Level
idx2_Inline u64
GetBrickFromBrickKey(u64 BrickKey)
{
  return BrickKey >> 4;
}


idx2_Inline i8
GetLevelFromBrickKey(u64 BrickKey)
{ return i8(BrickKey & 0xF); }


// Compose a Key from Level + Subband + BitPlane
idx2_Inline u32
GetChannelKey(i16 BitPlane, i8 Level, i8 Subband)
{
  return (u32(BitPlane) << 16) + (u32(Subband) << 4) + Level;
}


// Get Level from a Key composed of Iteration + Level + BitPlane
idx2_Inline i8
GetSubbandFromChannelKey(u64 ChannelKey)
{
  return i8((ChannelKey >> 4) & 0xFFFF);
}


// Get Iteration from a Key composed of Iteration + Level + BitPlane
idx2_Inline i8
GetLevelFromChannelKey(u64 ChannelKey)
{
  return i8(ChannelKey & 0xF);
}


// Get BitPlane from a Key composed of Iteration + Level + BitPlane
idx2_Inline i16
BitPlaneFromChannelKey(u64 ChannelKey)
{
  return i16(ChannelKey >> 16);
}



/* not used for now
static v3i
GetPartialResolution(const v3i& Dims3, u8 Mask, const array<subband>& Subbands) {
  v3i Div(0);
  idx2_For(u8, Sb, 0, 8) {
    if (!BitSet(Mask, Sb)) continue;
    v3i Lh3 = Subbands[Sb].LowHigh3;
    idx2_For(int, D, 0, 3) Div[D] = Max(Div[D], Lh3[D]);
  }
  v3i OutDims3 = Dims3;
  idx2_For(int, D, 0, 3) if (Div[D] == 0) OutDims3[D] = (Dims3[D] + 1) >> 1;
  return OutDims3;
}*/


} // namespace idx2

