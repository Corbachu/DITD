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

#include "common.h"

#ifdef DREAMCAST

#include "System/r_units.h"

#include "osystem.h"

// Defined in osystemDC.cpp (Dreamcast backend). Exposed so the unit batching
// helpers can ensure a 3D overlay frame is started on first submission.
extern bool g_dcExpect3DOverlayThisFrame;

#include <algorithm>
#include <cmath>

// Software 2D raster helpers used by the engine (8-bit indexed into polyBackBuffer).
extern void fillpoly(s16* datas, int n, unsigned char c);
extern void line(int x1, int y1, int x2, int y2, unsigned char c);
extern void hline(int x1, int x2, int y, unsigned char c);
extern unsigned char* polyBackBuffer;

std::vector<RGL_Vtx> g_rglTriVtx;
std::vector<RGL_Vtx> g_rglLineVtx;
std::array<int, 8> g_rglPolyTypeCounts = {0};

void RGL_BeginFrame()
{
    g_rglTriVtx.clear();
    g_rglLineVtx.clear();
    g_rglPolyTypeCounts.fill(0);
}

void RGL_AddTri(float x1, float y1, float z1,
                    float x2, float y2, float z2,
                    float x3, float y3, float z3,
                    u8 colorIdx, u8 alpha)
{
    if (!g_dcExpect3DOverlayThisFrame)
        osystem_cleanScreenKeepZBuffer();

    g_rglTriVtx.push_back({x1, y1, z1, colorIdx, alpha});
    g_rglTriVtx.push_back({x2, y2, z2, colorIdx, alpha});
    g_rglTriVtx.push_back({x3, y3, z3, colorIdx, alpha});
}

void RGL_AddLine(float x1, float y1, float z1,
                     float x2, float y2, float z2,
                     u8 colorIdx, u8 alpha)
{
    if (!g_dcExpect3DOverlayThisFrame)
        osystem_cleanScreenKeepZBuffer();

    g_rglLineVtx.push_back({x1, y1, z1, colorIdx, alpha});
    g_rglLineVtx.push_back({x2, y2, z2, colorIdx, alpha});
}

static FORCEINLINE int dc_iround(float v)
{
    return (int)std::lround(v);
}

static void dc_fill_circle(int cx, int cy, int r, unsigned char color)
{
    if (r <= 0)
        return;

    // Simple scanline circle fill (matches the original osystemDC implementation).
    for (int dy = -r; dy <= r; ++dy)
    {
        const int y = cy + dy;
        const double rr = (double)r * (double)r;
        const double ddy = (double)dy * (double)dy;
        const int dxMax = (int)std::floor(std::sqrt(std::max(0.0, rr - ddy)));
        hline(cx - dxMax, cx + dxMax, y, color);
    }
}

void osystem_draw3dLine(float x1, float y1, float z1, float x2, float y2, float z2, unsigned char color)
{
#ifndef USE_PVR_PAL8
    RGL_AddLine(x1, y1, z1, x2, y2, z2, color, 255);
    return;
#endif

    // PAL8 path: fall back to software line into polyBackBuffer.
    line(dc_iround(x1), dc_iround(y1), dc_iround(x2), dc_iround(y2), color);
}

void osystem_draw3dQuad(float x1, float y1, float /*z1*/, float x2, float y2, float /*z2*/, float x3, float y3,
                        float /*z3*/, float x4, float y4, float /*z4*/, unsigned char color, int /*transparency*/)
{
    // Mostly used by debug helpers; outline is sufficient.
    osystem_draw3dLine(x1, y1, 0, x2, y2, 0, color);
    osystem_draw3dLine(x2, y2, 0, x3, y3, 0, color);
    osystem_draw3dLine(x3, y3, 0, x4, y4, 0, color);
    osystem_draw3dLine(x4, y4, 0, x1, y1, 0, color);
}

void osystem_drawSphere(float X, float Y, float Z, u8 color, u8 /*material*/, float size)
{
#ifndef USE_PVR_PAL8
    (void)size;
    RGL_AddLine(X - 1.0f, Y, Z, X + 1.0f, Y, Z, color, 255);
    RGL_AddLine(X, Y - 1.0f, Z, X, Y + 1.0f, Z, color, 255);
    return;
#endif

    // 'size' is already projected to screen space by the renderer.
    const int r = std::max(1, dc_iround(size));
    dc_fill_circle(dc_iround(X), dc_iround(Y), r, color);
}

void osystem_drawPoint(float X, float Y, float Z, u8 color, u8 /*material*/, float size)
{
#ifndef USE_PVR_PAL8
    (void)size;
    RGL_AddLine(X - 1.0f, Y, Z, X + 1.0f, Y, Z, color, 255);
    RGL_AddLine(X, Y - 1.0f, Z, X, Y + 1.0f, Z, color, 255);
    return;
#endif

    const int r = std::max(1, dc_iround(size));
    dc_fill_circle(dc_iround(X), dc_iround(Y), r, color);
}

#endif // DREAMCAST
