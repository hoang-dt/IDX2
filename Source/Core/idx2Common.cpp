#include "idx2Common.h"
#include "Math.h"


#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wnested-anon-types"
#endif
#endif
#define SEXPR_IMPLEMENTATION
#include "sexpr.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif
#include "zstd/zstd.c"
#include "zstd/zstd.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif


namespace idx2
{


free_list_allocator BrickAlloc_;


void
Dealloc(params* P)
{
  Dealloc(&P->RdoLevels);
}


void
SetName(idx2_file* Idx2, cstr Name)
{
  snprintf(Idx2->Name, sizeof(Idx2->Name), "%s", Name);
}


void
SetField(idx2_file* Idx2, cstr Field)
{
  snprintf(Idx2->Field, sizeof(Idx2->Field), "%s", Field);
}


void
SetVersion(idx2_file* Idx2, const v2i& Ver)
{
  Idx2->Version = Ver;
}


void
SetDimensions(idx2_file* Idx2, const v3i& Dims3)
{
  Idx2->Dims3 = Dims3;
}


void
SetDataType(idx2_file* Idx2, dtype DType)
{
  Idx2->DType = DType;
}


void
SetBrickSize(idx2_file* Idx2, const v3i& BrickDims3)
{
  Idx2->BrickDims3 = BrickDims3;
}


void
SetNumIterations(idx2_file* Idx2, i8 NLevels)
{
  Idx2->NLevels = NLevels;
}


void
SetAccuracy(idx2_file* Idx2, f64 Accuracy)
{
  Idx2->Accuracy = Accuracy;
}


void
SetChunksPerFile(idx2_file* Idx2, int ChunksPerFile)
{
  Idx2->ChunksPerFileIn = ChunksPerFile;
}


void
SetBricksPerChunk(idx2_file* Idx2, int BricksPerChunk)
{
  Idx2->BricksPerChunkIn = BricksPerChunk;
}


void
SetFilesPerDirectory(idx2_file* Idx2, int FilesPerDir)
{
  Idx2->FilesPerDir = FilesPerDir;
}


void
SetDir(idx2_file* Idx2, cstr Dir)
{
  Idx2->Dir = Dir;
}


void
SetGroupLevels(idx2_file* Idx2, bool GroupLevels)
{
  Idx2->GroupLevels = GroupLevels;
}


void
SetGroupSubLevels(idx2_file* Idx2, bool GroupSubLevels)
{
  Idx2->GroupSubLevels = GroupSubLevels;
}


void
SetGroupBitPlanes(idx2_file* Idx2, bool GroupBitPlanes)
{
  Idx2->GroupBitPlanes = GroupBitPlanes;
}


void
SetQualityLevels(idx2_file* Idx2, const array<int>& QualityLevels)
{
  Clear(&Idx2->QualityLevelsIn);
  idx2_ForEach (It, QualityLevels)
  {
    PushBack(&Idx2->QualityLevelsIn, (int)*It);
  };
}


void
SetDownsamplingFactor(idx2_file* Idx2, const v3i& DownsamplingFactor3)
{
  Idx2->DownsamplingFactor3 = DownsamplingFactor3;
}


static error<idx2_err_code>
CheckBrickSize(idx2_file* Idx2, const params& P)
{
  if (!(IsPow2(Idx2->BrickDims3.X) && IsPow2(Idx2->BrickDims3.Y) && IsPow2(Idx2->BrickDims3.Z)))
    return idx2_Error(
      idx2_err_code::BrickSizeNotPowerOfTwo, idx2_PrStrV3i "\n", idx2_PrV3i(Idx2->BrickDims3));
  if (!(Idx2->Dims3 >= Idx2->BrickDims3))
    return idx2_Error(idx2_err_code::BrickSizeTooBig,
                      " total dims: " idx2_PrStrV3i ", brick dims: " idx2_PrStrV3i "\n",
                      idx2_PrV3i(Idx2->Dims3),
                      idx2_PrV3i(Idx2->BrickDims3));
  return idx2_Error(idx2_err_code::NoError);
}


static void
ComputeTransformOrder(idx2_file* Idx2, const params& P, char* TformOrder)
{ /* try to repeat XYZ+, depending on the BrickDims */
  int J = 0;
  idx2_For (int, D, 0, 3)
  {
    if (Idx2->BrickDims3[D] > 1)
      TformOrder[J++] = char('X' + D);
  }
  TformOrder[J++] = '+';
  TformOrder[J++] = '+';
  Idx2->TformOrder = EncodeTransformOrder(TformOrder);
  Idx2->TransformOrderFull.Len =
    DecodeTransformOrder(Idx2->TformOrder, Idx2->NTformPasses, Idx2->TransformOrderFull.Data);
}


/* Build the subbands, including which subbands to decode, depending on P.DownsamplingFactor3*/
static void
BuildSubbands(idx2_file* Idx2, const params& P)
{
  Idx2->BrickDimsExt3 = idx2_ExtDims(Idx2->BrickDims3);
  BuildSubbands(Idx2->BrickDimsExt3, Idx2->NTformPasses, Idx2->TformOrder, &Idx2->Subbands);
  BuildSubbands(Idx2->BrickDims3, Idx2->NTformPasses, Idx2->TformOrder, &Idx2->SubbandsNonExt);

  // Compute the decode subband mask based on DownsamplingFactor3
  v3i Df3 = P.DownsamplingFactor3;
  idx2_For (int, I, 0, Idx2->NLevels)
  {
    if (Df3.X > 0 && Df3.Y > 0 && Df3.Z > 0)
    {
      Idx2->DecodeSubbandMasks[I] = 0;
      --Df3.X;
      --Df3.Y;
      --Df3.Z;
      if (Df3.X == 0 && Df3.Y == 0 && Df3.Z == 0)
        Idx2->DecodeSubbandMasks[I] = 1;
      continue;
    }
    u8 Mask = 0xFF;
    idx2_For (int, Sb, 0, Size(Idx2->Subbands))
    {
      const v3i& Lh3 = Idx2->Subbands[Sb].LowHigh3;
      if ((Lh3.X == 1 && Df3.X > 0) || (Lh3.Y == 1 && Df3.Y > 0) || (Lh3.Z == 1 && Df3.Z > 0))
        Mask = UnsetBit(Mask, Sb);
    }
    Idx2->DecodeSubbandMasks[I] = Mask;
    if (Df3.X > 0) --Df3.X;
    if (Df3.Y > 0) --Df3.Y;
    if (Df3.Z > 0) --Df3.Z;
  }
  // TODO: maybe decode the first (0, 0, 0) subband?
}


static void
ComputeNumBricksPerLevel(idx2_file* Idx2, const params& P)
{
  Idx2->GroupBrick3 = Idx2->BrickDims3 / Dims(Idx2->SubbandsNonExt[0].Grid);
  v3i NBricks3 = (Idx2->Dims3 + Idx2->BrickDims3 - 1) / Idx2->BrickDims3;
  v3i NBricksPerLevel3 = NBricks3;
  idx2_For (int, I, 0, Idx2->NLevels)
  {
    Idx2->NBricks3[I] = NBricksPerLevel3;
    NBricksPerLevel3 = (NBricksPerLevel3 + Idx2->GroupBrick3 - 1) / Idx2->GroupBrick3;
  }
}


/* compute the brick order, by repeating the (per brick) transform order */
static error<idx2_err_code>
ComputeGlobalBricksOrder(idx2_file* Idx2, const params& P, const char* TformOrder)
{
  Resize(&Idx2->BricksOrderStr, Idx2->NLevels);
  idx2_For (int, I, 0, Idx2->NLevels)
  {
    v3i N3 = Idx2->NBricks3[I];
    v3i LogN3 = v3i(Log2Ceil(N3.X), Log2Ceil(N3.Y), Log2Ceil(N3.Z));
    int MinLogN3 = Min(LogN3.X, LogN3.Y, LogN3.Z);
    v3i LeftOver3 =
      LogN3 -
      v3i(Idx2->BrickDims3.X > 1, Idx2->BrickDims3.Y > 1, Idx2->BrickDims3.Z > 1) * MinLogN3;
    char BrickOrder[128];
    int J = 0;
    idx2_For (int, D, 0, 3)
    {
      if (Idx2->BrickDims3[D] == 1)
      {
        while (LeftOver3[D]-- > 0)
          BrickOrder[J++] = char('X' + D);
      }
    }
    while (!(LeftOver3 <= 0))
    {
      idx2_For (int, D, 0, 3)
      {
        if (LeftOver3[D]-- > 0)
          BrickOrder[J++] = char('X' + D);
      }
    }
    if (J > 0)
      BrickOrder[J++] = '+';
    idx2_For (size_t, K, 0, sizeof(TformOrder))
      BrickOrder[J++] = TformOrder[K];
    Idx2->BricksOrder[I] = EncodeTransformOrder(BrickOrder);
    Idx2->BricksOrderStr[I].Len =
      DecodeTransformOrder(Idx2->BricksOrder[I], N3, Idx2->BricksOrderStr[I].Data);

    if (Idx2->BricksOrderStr[I].Len < Idx2->TransformOrderFull.Len)
      return idx2_Error(idx2_err_code::TooManyLevels);
  }

  return idx2_Error(idx2_err_code::NoError);
}


static error<idx2_err_code>
ComputeLocalBricksChunksFilesOrders(idx2_file* Idx2, const params& P)
{
  Idx2->BricksPerChunk[0] = Idx2->BricksPerChunkIn;
  Idx2->ChunksPerFile[0] = Idx2->ChunksPerFileIn;

  if (!(Idx2->BricksPerChunk[0] <= idx2_file::MaxBricksPerChunk))
    return idx2_Error(idx2_err_code::TooManyBricksPerChunk);
  if (!IsPow2(Idx2->BricksPerChunk[0]))
    return idx2_Error(idx2_err_code::BricksPerChunkNotPowerOf2);
  if (!(Idx2->ChunksPerFile[0] <= idx2_file::MaxChunksPerFile))
    return idx2_Error(idx2_err_code::TooManyChunksPerFile);
  if (!IsPow2(Idx2->ChunksPerFile[0]))
    return idx2_Error(idx2_err_code::ChunksPerFileNotPowerOf2);

  idx2_For (int, I, 0, Idx2->NLevels)
  {
    stack_string<64> BricksOrderInChunk;
    stack_string<64> ChunksOrderInFile;
    stack_string<64> FilesOrder;

    /* bricks order in chunk */
    {
      Idx2->BricksPerChunk[I] =
        1 << Min((u8)Log2Ceil(Idx2->BricksPerChunk[0]), Idx2->BricksOrderStr[I].Len);
      BricksOrderInChunk.Len = Log2Ceil(Idx2->BricksPerChunk[I]);
      Idx2->BricksPerChunk3s[I] = v3i(1);
      idx2_For (int, J, 0, BricksOrderInChunk.Len)
      {
        char C = Idx2->BricksOrderStr[I][Idx2->BricksOrderStr[I].Len - J - 1];
        Idx2->BricksPerChunk3s[I][C - 'X'] *= 2;
        BricksOrderInChunk[BricksOrderInChunk.Len - J - 1] = C;
      }
      Idx2->BricksOrderInChunk[I] =
        EncodeTransformOrder(stref(BricksOrderInChunk.Data, BricksOrderInChunk.Len));
      idx2_Assert(Idx2->BricksPerChunk[I] = Prod(Idx2->BricksPerChunk3s[I]));
    }

    /* chunks order in file */
    {
      Idx2->NChunks3[I] =
        (Idx2->NBricks3[I] + Idx2->BricksPerChunk3s[I] - 1) / Idx2->BricksPerChunk3s[I];
      Idx2->ChunksPerFile[I] = 1 << Min((u8)Log2Ceil(Idx2->ChunksPerFile[0]),
                                         (u8)(Idx2->BricksOrderStr[I].Len - BricksOrderInChunk.Len));
      idx2_Assert(Idx2->BricksOrderStr[I].Len >= BricksOrderInChunk.Len);
      ChunksOrderInFile.Len = Log2Ceil(Idx2->ChunksPerFile[I]);
      Idx2->ChunksPerFile3s[I] = v3i(1);
      idx2_For (int, J, 0, ChunksOrderInFile.Len)
      {
        char C = Idx2->BricksOrderStr[I][Idx2->BricksOrderStr[I].Len - BricksOrderInChunk.Len - J - 1];
        Idx2->ChunksPerFile3s[I][C - 'X'] *= 2;
        ChunksOrderInFile[ChunksOrderInFile.Len - J - 1] = C;
      }
      Idx2->ChunksOrderInFile[I] =
        EncodeTransformOrder(stref(ChunksOrderInFile.Data, ChunksOrderInFile.Len));
      idx2_Assert(Idx2->ChunksPerFile[I] == Prod(Idx2->ChunksPerFile3s[I]));
      Idx2->NFiles3[I] =
        (Idx2->NChunks3[I] + Idx2->ChunksPerFile3s[I] - 1) / Idx2->ChunksPerFile3s[I];
    }

    /* global chunk orders (not being used for now) */
    {
      stack_string<64> ChunksOrder;
      Idx2->ChunksPerVol[I] = 1 << (Idx2->BricksOrderStr[I].Len - BricksOrderInChunk.Len);
      idx2_Assert(Idx2->BricksOrderStr[I].Len >= BricksOrderInChunk.Len);
      ChunksOrder.Len = Log2Ceil(Idx2->ChunksPerVol[I]);
      idx2_For (int, J, 0, ChunksOrder.Len)
      {
        char C = Idx2->BricksOrderStr[I][Idx2->BricksOrderStr[I].Len - BricksOrderInChunk.Len - J - 1];
        ChunksOrder[ChunksOrder.Len - J - 1] = C;
      }
      Idx2->ChunksOrder[I] = EncodeTransformOrder(stref(ChunksOrder.Data, ChunksOrder.Len));
      Resize(&Idx2->ChunksOrderStr, Idx2->NLevels);
      Idx2->ChunksOrderStr[I].Len = DecodeTransformOrder(
        Idx2->ChunksOrder[I], Idx2->NChunks3[I], Idx2->ChunksOrderStr[I].Data);
    }

    /* files order */
    {
      Idx2->FilesPerVol[I] =
        1 << (Idx2->BricksOrderStr[I].Len - BricksOrderInChunk.Len - ChunksOrderInFile.Len);
      // TODO: the following check may fail if the brick size is too close to the size of the
      // volume, and we set NLevels too high
      idx2_Assert(Idx2->BricksOrderStr[I].Len >= BricksOrderInChunk.Len + ChunksOrderInFile.Len);
      FilesOrder.Len = Log2Ceil(Idx2->FilesPerVol[I]);
      idx2_For (int, J, 0, FilesOrder.Len)
      {
        char C = Idx2->BricksOrderStr[I][Idx2->BricksOrderStr[I].Len - BricksOrderInChunk.Len -
                                         ChunksOrderInFile.Len - J - 1];
        FilesOrder[FilesOrder.Len - J - 1] = C;
      }
      Idx2->FilesOrder[I] = EncodeTransformOrder(stref(FilesOrder.Data, FilesOrder.Len));
      Resize(&Idx2->FilesOrderStr, Idx2->NLevels);
      Idx2->FilesOrderStr[I].Len =
        DecodeTransformOrder(Idx2->FilesOrder[I], Idx2->NFiles3[I], Idx2->FilesOrderStr[I].Data);
    }
  }

  return idx2_Error(idx2_err_code::NoError);
}


static error<idx2_err_code>
ComputeFileDirDepths(idx2_file* Idx2, const params& P)
{
  if (!(Idx2->FilesPerDir <= idx2_file::MaxFilesPerDir))
    return idx2_Error(idx2_err_code::TooManyFilesPerDir, "%d", Idx2->FilesPerDir);

  idx2_For (int, I, 0, Idx2->NLevels)
  {
    Idx2->BricksPerFile[I] = Idx2->BricksPerChunk[I] * Idx2->ChunksPerFile[I];
    Idx2->FilesDirsDepth[I].Len = 0;
    i8 DepthAccum = Idx2->FilesDirsDepth[I][Idx2->FilesDirsDepth[I].Len++] =
      Log2Ceil(Idx2->BricksPerFile[I]);
    i8 Len = Idx2->BricksOrderStr[I].Len /* - Idx2->TformOrderFull.Len*/;
    while (DepthAccum < Len)
    {
      i8 Inc = Min(i8(Len - DepthAccum), Log2Ceil(Idx2->FilesPerDir));
      DepthAccum += (Idx2->FilesDirsDepth[I][Idx2->FilesDirsDepth[I].Len++] = Inc);
    }
    if (Idx2->FilesDirsDepth[I].Len > idx2_file::MaxSpatialDepth)
      return idx2_Error(idx2_err_code::TooManyFilesPerDir);
    Reverse(Begin(Idx2->FilesDirsDepth[I]),
            Begin(Idx2->FilesDirsDepth[I]) + Idx2->FilesDirsDepth[I].Len);
  }

  return idx2_Error(idx2_err_code::NoError);
}


/* compute the transform details, for both the normal transform and for extrapolation */
static void
ComputeWaveletTransformDetails(idx2_file* Idx2)
{
  ComputeTransformDetails(&Idx2->Td, Idx2->BrickDimsExt3, Idx2->NTformPasses, Idx2->TformOrder);
  int NLevels = Log2Floor(Max(Max(Idx2->BrickDims3.X, Idx2->BrickDims3.Y), Idx2->BrickDims3.Z));
  ComputeTransformDetails(&Idx2->TdExtrpolate, Idx2->BrickDims3, NLevels, Idx2->TformOrder);
}


/* compute actual number of bytes for each rdo (rate-distortion optimization) quality level (not used for now) */
static void
ComputeRdoQualityLevels(idx2_file* Idx2, const params& P)
{
  i64 TotalUncompressedSize = Prod<i64>(Idx2->Dims3) * SizeOf(Idx2->DType);
  Reserve(&Idx2->RdoLevels, Size(Idx2->QualityLevelsIn));
  idx2_ForEach (It, Idx2->QualityLevelsIn)
  {
    PushBack(&Idx2->RdoLevels, TotalUncompressedSize / *It);
  }
}


error<idx2_err_code>
Finalize(idx2_file* Idx2, const params& P)
{
  idx2_PropagateIfError(CheckBrickSize(Idx2, P));

  if (!(Idx2->NLevels <= idx2_file::MaxLevels))
    return idx2_Error(idx2_err_code::TooManyLevels, "Max # of levels = %d\n", Idx2->MaxLevels);

  char TformOrder[8] = {};
  ComputeTransformOrder(Idx2, P, TformOrder);

  BuildSubbands(Idx2, P);

  ComputeNumBricksPerLevel(Idx2, P);

  idx2_PropagateIfError(ComputeGlobalBricksOrder(Idx2, P, TformOrder));
  idx2_PropagateIfError(ComputeLocalBricksChunksFilesOrders(Idx2, P));

  idx2_PropagateIfError(ComputeFileDirDepths(Idx2, P));

  ComputeWaveletTransformDetails(Idx2);
  ComputeRdoQualityLevels(Idx2, P);

  return idx2_Error(idx2_err_code::NoError);
}


void
Dealloc(idx2_file* Idx2)
{
  Dealloc(&Idx2->BricksOrderStr);
  Dealloc(&Idx2->ChunksOrderStr);
  Dealloc(&Idx2->FilesOrderStr);
  Dealloc(&Idx2->Subbands);
  Dealloc(&Idx2->SubbandsNonExt);
  Dealloc(&Idx2->QualityLevelsIn);
  Dealloc(&Idx2->RdoLevels);
}


// TODO: handle the case where the query extent is larger than the domain itself
grid
GetGrid(const idx2_file& Idx2, const extent& Ext)
{
  auto CroppedExt = Crop(Ext, extent(Idx2.Dims3));
  v3i Strd3(1); // start with stride (1, 1, 1)
  idx2_For (int, D, 0, 3)
    Strd3[D] <<= Idx2.DownsamplingFactor3[D];

  v3i First3 = From(CroppedExt);
  v3i Last3 = Last(CroppedExt);
  Last3 = ((Last3 + Strd3 - 1) / Strd3) * Strd3; // move last to the right
  First3 = (First3 / Strd3) * Strd3; // move first to the left

  return grid(First3, (Last3 - First3) / Strd3 + 1, Strd3);
}


} // namespace idx2
