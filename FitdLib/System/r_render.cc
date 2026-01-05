//----------------------------------------------------------------------------
//  Dream in the Dark Screen Setup (OpenGL)
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

#include "System/r_render.h"

#include "System/dc_palette.h"
#include "System/r_clipping.h"
#include "System/r_units.h"

// GLdc
#include <GL/gl.h>
#include <GL/glkos.h>

#include <vector>


void RGL_RenderPolys()
{
    if (g_rglTriVtx.empty() && g_rglLineVtx.empty())
        return;

    // Use the engine's projected X/Y (320x200 game-space) and preserve Z for ordering.
    dc_gl_ensure_fullscreen_viewport();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // The engine feeds already-projected X/Y in 320x200 game-space.
    // Z is carried through mostly for ordering/debugging, and values can be
    // 0 or have a wide dynamic range depending on scene.
    // Use a symmetric, very wide Z range to avoid clipping everything away.
    glOrtho(0.0f, 320.0f, 200.0f, 0.0f, -100000.0f, 100000.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    dc_gl_force_fullscreen_scissor();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    // The engine already orders primitives for correct painter-style rendering.
    glDisable(GL_DEPTH_TEST);

    bool needsBlend = false;
    for (const auto& v : g_rglTriVtx)
    {
        if (v.alpha != 255)
        {
            needsBlend = true;
            break;
        }
    }
    if (!needsBlend)
    {
        for (const auto& v : g_rglLineVtx)
        {
            if (v.alpha != 255)
            {
                needsBlend = true;
                break;
            }
        }
    }

    if (needsBlend)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glDisable(GL_BLEND);
    }

    if (!g_rglTriVtx.empty())
    {
        struct RGL_GLVertex
        {
            float x, y, z;
            u8 r, g, b, a;
        };

        static std::vector<RGL_GLVertex> s_triVerts;
        s_triVerts.resize(g_rglTriVtx.size());

        for (size_t i = 0; i < g_rglTriVtx.size(); ++i)
        {
            const auto& v = g_rglTriVtx[i];
            const u8 r = g_palette[v.colorIdx * 3 + 0];
            const u8 g = g_palette[v.colorIdx * 3 + 1];
            const u8 b = g_palette[v.colorIdx * 3 + 2];
            s_triVerts[i] = { v.x, v.y, v.z, r, g, b, v.alpha };
        }

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, sizeof(RGL_GLVertex), &s_triVerts[0].x);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(RGL_GLVertex), &s_triVerts[0].r);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)s_triVerts.size());
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    if (!g_rglLineVtx.empty())
    {
        struct RGL_GLVertex
        {
            float x, y, z;
            u8 r, g, b, a;
        };

        static std::vector<RGL_GLVertex> s_lineVerts;
        s_lineVerts.resize(g_rglLineVtx.size());

        for (size_t i = 0; i < g_rglLineVtx.size(); ++i)
        {
            const auto& v = g_rglLineVtx[i];
            const u8 r = g_palette[v.colorIdx * 3 + 0];
            const u8 g = g_palette[v.colorIdx * 3 + 1];
            const u8 b = g_palette[v.colorIdx * 3 + 2];
            s_lineVerts[i] = { v.x, v.y, v.z, r, g, b, v.alpha };
        }

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, sizeof(RGL_GLVertex), &s_lineVerts[0].x);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(RGL_GLVertex), &s_lineVerts[0].r);
        glDrawArrays(GL_LINES, 0, (GLsizei)s_lineVerts.size());
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    glEnable(GL_TEXTURE_2D);
}

#endif // DREAMCAST
