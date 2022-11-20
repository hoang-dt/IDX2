#include "idx2.h"
#include <stdio.h>


namespace idx2
{


error<idx2_err_code>
Init(idx2_file* Idx2, params& P)
{
#if VISUS_IDX2
  if (P.enable_visus)
    EnableVisus(Idx2);
#endif

  SetDir(Idx2, P.InDir);
  SetDownsamplingFactor(Idx2, P.DownsamplingFactor3);
  idx2_PropagateIfError(ReadMetaFile(Idx2, idx2_PrintScratch("%s", P.InputFile)));
  idx2_PropagateIfError(Finalize(Idx2, P));
  if (Dims(P.DecodeExtent) == v3i(0)) // TODO: this could conflate with the user wanting to decode a single sample (very unlikely though)
    P.DecodeExtent = extent(Idx2->Dims3);
  return idx2_Error(idx2_err_code::NoError);
}


idx2::grid
GetOutputGrid(const idx2_file& Idx2, const params& P)
{
  return GetGrid(Idx2, P.DecodeExtent);
}


error<idx2_err_code>
Decode(idx2_file* Idx2, params& P, buffer* OutBuf)
{
  Decode(*Idx2, P, OutBuf);
  return idx2_Error(idx2_err_code::NoError);
}


error<idx2_err_code>
Destroy(idx2_file* Idx2)
{
  Dealloc(Idx2);
  return idx2_Error(idx2_err_code::NoError);
}


} // namespace idx2
