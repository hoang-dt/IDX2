#pragma once

#include "Common.h"
#include "String.h"

namespace idx2
{

/* Only support the forward slash '/' separator. */
struct path
{
  constexpr static int NPartsMax = 64;
  stref Parts[NPartsMax] = {}; /* e.g. home, dir, file.txt */
  int NParts = 0;
  path();
  path(const stref& Str);
};

/* General */
void  Init(path* Path, const stref& Str);
void  Append(path* Path, const stref& Part);
bool  IsRelative(const stref& Path);
cstr  ToString(const path& Path);

/* File related */
stref GetFileName(const stref& Path);
stref GetExtension(const stref& Path);
i64   GetSize(const stref& Path);

/* Directory related */
stref GetDirName(const stref& Path);
bool  CreateFullDir(const stref& Path);
bool  DirExists(const stref& Path);
void  RemoveDir(cstr path);

} // namespace idx2

