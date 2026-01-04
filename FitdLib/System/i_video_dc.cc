//----------------------------------------------------------------------------
//  Dream in the Dark Texture Upload (OpenGL)
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

#ifndef USE_PVR_PAL8

#include "System/i_video_dc.h"

#include "System/dc_palette.h"
#include "System/r_render.h"

#include "vars.h"

// GLdc
#include <GL/gl.h>
#include <GL/glkos.h>

#include <dc/video.h>

#include <vector>

static GLuint g_backgroundTex = 0;
static constexpr int GL_TEX_W = 512;
static constexpr int GL_TEX_H = 256;
static float g_gl_uMax = 320.0f / (float)GL_TEX_W;
static float g_gl_vMax = 200.0f / (float)GL_TEX_H;

static void ensure_fullscreen_viewport()
{
    if (vid_mode)
        glViewport(0, 0, vid_mode->width, vid_mode->height);
    else
        glViewport(0, 0, 640, 480);
}

static void dc_force_fullscreen_scissor()
{
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

static void ensure_game_ortho()
{
    ensure_fullscreen_viewport();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, 320.0f, 200.0f, 0.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    dc_force_fullscreen_scissor();
}

static void ensureTexture()
{
    if (!g_backgroundTex)
    {
        glGenTextures(1, &g_backgroundTex);
        glBindTexture(GL_TEXTURE_2D, g_backgroundTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        std::vector<uint16_t> blank(GL_TEX_W * GL_TEX_H, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GL_TEX_W, GL_TEX_H, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, blank.data());
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, g_backgroundTex);
    }
}

static FORCEINLINE uint16_t rgb565_from_palette(u8 idx)
{
    return g_palette565[idx];
}

void dc_video_gl_init()
{
    glKosInit();

    // Ensure our 320x200 quad fills the screen.
    ensure_fullscreen_viewport();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, 320.0f, 200.0f, 0.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);

    dc_force_fullscreen_scissor();

    // Fixed texture coordinate range (we only update the 320x200 top-left).
    g_gl_uMax = 320.0f / (float)GL_TEX_W;
    g_gl_vMax = 200.0f / (float)GL_TEX_H;

    ensureTexture();
}

void dc_video_gl_upload_composited()
{
    static std::vector<uint16_t> rgb(320 * 200);
    for (int i = 0; i < 320 * 200; ++i)
    {
        const u8 src = uiLayer[i] ? uiLayer[i] : physicalScreen[i];
        rgb[i] = rgb565_from_palette(src);
    }

    ensureTexture();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 320, 200, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, rgb.data());
}

void dc_video_gl_present()
{
    ensure_game_ortho();
    glClear(GL_COLOR_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, g_backgroundTex);

    // Ensure 2D blits are not darkened by leftover 3D state.
    glDisable(GL_BLEND);
    glColor4ub(255, 255, 255, 255);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.f,       0.f);       glVertex3f(0.f,   0.f,   0.f);
    glTexCoord2f(g_gl_uMax, 0.f);       glVertex3f(320.f, 0.f,   0.f);
    glTexCoord2f(0.f,       g_gl_vMax); glVertex3f(0.f,   200.f, 0.f);
    glTexCoord2f(g_gl_uMax, g_gl_vMax); glVertex3f(320.f, 200.f, 0.f);
    glEnd();

    // Draw queued 3D primitives on top of the background.
    RGL_RenderPolys();

    glKosSwapBuffers();
}

#endif // !USE_PVR_PAL8

#endif // DREAMCAST
