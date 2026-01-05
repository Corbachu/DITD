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
#include "System/r_clipping.h"
#include "System/r_render.h"

#include "vars.h"

// GLdc
#include <GL/gl.h>
#include <GL/glkos.h>

#include <dc/video.h>

#include <vector>

struct DcMaskRun
{
    uint16_t y;
    uint16_t x1;
    uint16_t x2; // exclusive
};

struct DcMask
{
    int x1 = 0;
    int y1 = 0;
    int x2 = 0; // inclusive
    int y2 = 0; // inclusive
    std::vector<DcMaskRun> runs;
};

struct DcMaskCmd
{
    int roomId = 0;
    int maskId = 0;
    bool hasClip = false;
    int clipX1 = 0;
    int clipY1 = 0;
    int clipX2 = 320;
    int clipY2 = 200;
};

static std::vector<std::vector<DcMask>> g_dcMasks; // [room][mask]
static std::vector<DcMaskCmd> g_dcMaskQueue;

static GLuint g_backgroundTex = 0;
static constexpr int GL_TEX_W = 512;
static constexpr int GL_TEX_H = 256;
static float g_gl_uMax = 320.0f / (float)GL_TEX_W;
static float g_gl_vMax = 200.0f / (float)GL_TEX_H;

static void ensureMaskStorage(int roomId, int maskId)
{
    if ((int)g_dcMasks.size() < roomId + 1)
        g_dcMasks.resize(roomId + 1);
    if ((int)g_dcMasks[roomId].size() < maskId + 1)
        g_dcMasks[roomId].resize(maskId + 1);
}

void dc_video_gl_create_mask(const u8* mask320x200, int roomId, int maskId,
                             int maskX1, int maskY1, int maskX2, int maskY2)
{
    if (!mask320x200)
        return;

    ensureMaskStorage(roomId, maskId);

    DcMask& dst = g_dcMasks[roomId][maskId];
    dst.x1 = std::max(0, std::min(319, maskX1));
    dst.y1 = std::max(0, std::min(199, maskY1));
    dst.x2 = std::max(0, std::min(319, maskX2));
    dst.y2 = std::max(0, std::min(199, maskY2));
    dst.runs.clear();

    for (int y = dst.y1; y <= dst.y2; ++y)
    {
        const int row = y * 320;
        int x = dst.x1;
        while (x <= dst.x2)
        {
            while (x <= dst.x2 && mask320x200[row + x] == 0)
                ++x;
            if (x > dst.x2)
                break;
            const int runX1 = x;
            while (x <= dst.x2 && mask320x200[row + x] != 0)
                ++x;
            const int runX2 = x; // exclusive
            dst.runs.push_back({(uint16_t)y, (uint16_t)runX1, (uint16_t)runX2});
        }
    }
}

void dc_video_gl_queue_mask_draw(int roomId, int maskId,
                                 bool hasClip, int clipX1, int clipY1, int clipX2, int clipY2)
{
    DcMaskCmd cmd;
    cmd.roomId = roomId;
    cmd.maskId = maskId;
    cmd.hasClip = hasClip;
    cmd.clipX1 = clipX1;
    cmd.clipY1 = clipY1;
    cmd.clipX2 = clipX2;
    cmd.clipY2 = clipY2;
    g_dcMaskQueue.push_back(cmd);
}

void dc_video_gl_clear_mask_queue()
{
    g_dcMaskQueue.clear();
}

static void dc_video_gl_draw_mask_cmds()
{
    if (g_dcMaskQueue.empty())
        return;

    dc_gl_ensure_game_ortho();

    glBindTexture(GL_TEXTURE_2D, g_backgroundTex);
    glDisable(GL_BLEND);
    glColor4ub(255, 255, 255, 255);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    for (const DcMaskCmd& cmd : g_dcMaskQueue)
    {
        if (cmd.roomId < 0 || cmd.maskId < 0)
            continue;
        if (cmd.roomId >= (int)g_dcMasks.size())
            continue;
        if (cmd.maskId >= (int)g_dcMasks[cmd.roomId].size())
            continue;

        const DcMask& m = g_dcMasks[cmd.roomId][cmd.maskId];
        if (m.runs.empty())
            continue;

        const int clipX1 = cmd.hasClip ? std::max(0, std::min(320, cmd.clipX1)) : 0;
        const int clipY1 = cmd.hasClip ? std::max(0, std::min(200, cmd.clipY1)) : 0;
        const int clipX2 = cmd.hasClip ? std::max(0, std::min(320, cmd.clipX2)) : 320;
        const int clipY2 = cmd.hasClip ? std::max(0, std::min(200, cmd.clipY2)) : 200;

        for (const DcMaskRun& r : m.runs)
        {
            const int y = (int)r.y;
            if (y < clipY1 || y >= clipY2)
                continue;

            int x1 = (int)r.x1;
            int x2 = (int)r.x2;
            if (x2 <= clipX1 || x1 >= clipX2)
                continue;
            x1 = std::max(x1, clipX1);
            x2 = std::min(x2, clipX2);
            if (x1 >= x2)
                continue;

            const float u1 = (float)x1 / (float)GL_TEX_W;
            const float u2 = (float)x2 / (float)GL_TEX_W;
            const float v1 = (float)y / (float)GL_TEX_H;
            const float v2 = (float)(y + 1) / (float)GL_TEX_H;

            const float fx1 = (float)x1;
            const float fx2 = (float)x2;
            const float fy1 = (float)y;
            const float fy2 = (float)(y + 1);

            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(u1, v1); glVertex3f(fx1, fy1, 0.f);
            glTexCoord2f(u2, v1); glVertex3f(fx2, fy1, 0.f);
            glTexCoord2f(u1, v2); glVertex3f(fx1, fy2, 0.f);
            glTexCoord2f(u2, v2); glVertex3f(fx2, fy2, 0.f);
            glEnd();
        }
    }

    g_dcMaskQueue.clear();
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
    dc_gl_ensure_fullscreen_viewport();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, 320.0f, 200.0f, 0.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);

    dc_gl_force_fullscreen_scissor();

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
    dc_gl_ensure_game_ortho();
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

    // Draw queued background masks over 3D (occlusion).
    dc_video_gl_draw_mask_cmds();

    glKosSwapBuffers();
}

#endif // !USE_PVR_PAL8

#endif // DREAMCAST
