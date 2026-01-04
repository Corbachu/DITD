//----------------------------------------------------------------------------
//  Dream in the Dark OpenGL Unit Batching
//----------------------------------------------------------------------------
//
//  Copyright (c) 2025  Corbin Annis/yaz0r/jimmu/FITD Team
//  Copyright (c) 2025  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#pragma once

#include "common.h"

#ifdef DREAMCAST

#include <array>
#include <vector>

struct RGL_Vtx
{
    float x;
    float y;
    float z;
    u8 colorIdx;
    u8 alpha;
};

extern std::vector<RGL_Vtx> g_rglTriVtx;
extern std::vector<RGL_Vtx> g_rglLineVtx;
extern std::array<int, 8> g_rglPolyTypeCounts;

void RGL_BeginFrame();
void RGL_AddTri(float x1, float y1, float z1,
                    float x2, float y2, float z2,
                    float x3, float y3, float z3,
                    u8 colorIdx, u8 alpha);

void RGL_AddLine(float x1, float y1, float z1,
                     float x2, float y2, float z2,
                     u8 colorIdx, u8 alpha);

#endif // DREAMCAST
