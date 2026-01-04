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
#include "System/r_units.h"

// GLdc
#include <GL/gl.h>
#include <GL/glkos.h>

#include <dc/video.h>

static void ensure_fullscreen_viewport()
{
    // GLdc's glViewport math depends on GetVideoMode()->height (backed by KOS
    // vid_mode->height). Always match the current KOS video mode to avoid
    // incorrect scaling/offset.
    if (vid_mode)
        glViewport(0, 0, vid_mode->width, vid_mode->height);
    else
        glViewport(0, 0, 640, 480);
}

static void dc_force_fullscreen_scissor()
{
    // GLdc's scissor implementation emits a PVR tile-clip command only when
    // GL_SCISSOR_TEST is enabled. If any earlier code enabled scissor and set
    // a small clip, then later disabled scissor, the last tile-clip can
    // effectively "stick" on hardware/emulators. Force a full-screen clip.
    int vw = 640;
    int vh = 480;
    if (vid_mode)
    {
        vw = vid_mode->width;
        vh = vid_mode->height;
    }

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, vw, vh);
}

void RGL_RenderPolys()
{
    if (g_rglTriVtx.empty() && g_rglLineVtx.empty())
        return;

    // Use the engine's projected X/Y (320x200 game-space) and preserve Z for ordering.
    ensure_fullscreen_viewport();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // The engine feeds already-projected X/Y in 320x200 game-space.
    // Z is carried through mostly for ordering/debugging, and values can be
    // 0 or have a wide dynamic range depending on scene.
    // Use a symmetric, very wide Z range to avoid clipping everything away.
    glOrtho(0.0f, 320.0f, 200.0f, 0.0f, -100000.0f, 100000.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    dc_force_fullscreen_scissor();

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
        glBegin(GL_TRIANGLES);
        for (const auto& v : g_rglTriVtx)
        {
            const u8 r = g_palette[v.colorIdx * 3 + 0];
            const u8 g = g_palette[v.colorIdx * 3 + 1];
            const u8 b = g_palette[v.colorIdx * 3 + 2];
            glColor4ub(r, g, b, v.alpha);
            glVertex3f(v.x, v.y, v.z);
        }
        glEnd();
    }

    if (!g_rglLineVtx.empty())
    {
        glBegin(GL_LINES);
        for (const auto& v : g_rglLineVtx)
        {
            const u8 r = g_palette[v.colorIdx * 3 + 0];
            const u8 g = g_palette[v.colorIdx * 3 + 1];
            const u8 b = g_palette[v.colorIdx * 3 + 2];
            glColor4ub(r, g, b, v.alpha);
            glVertex3f(v.x, v.y, v.z);
        }
        glEnd();
    }

    glEnable(GL_TEXTURE_2D);
}

#endif // DREAMCAST
