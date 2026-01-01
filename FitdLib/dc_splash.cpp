//----------------------------------------------------------------------------
//  Dream In The Dark Dreamcast splash (Dreamcast / KOS)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2026  Corbin Annis
//  Copyright (C) 1999-2026  The EDGE Team
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

extern "C" {
#include <kos.h>
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// GLdc
#include <GL/gl.h>
#include <GL/glkos.h>

#include <dc/video.h>

#include "dc_splash.h"

#include "stb_image.h"

static bool read_whole_file(const char* path, unsigned char** outData, size_t* outSize)
{
    *outData = nullptr;
    *outSize = 0;

    FILE* fp = fopen(path, "rb");
    if (!fp)
        return false;

    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return false;
    }

    long sz = ftell(fp);
    if (sz <= 0)
    {
        fclose(fp);
        return false;
    }

    if (fseek(fp, 0, SEEK_SET) != 0)
    {
        fclose(fp);
        return false;
    }

    unsigned char* buf = (unsigned char*)malloc((size_t)sz);
    if (!buf)
    {
        fclose(fp);
        return false;
    }

    size_t got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    if (got != (size_t)sz)
    {
        free(buf);
        return false;
    }

    *outData = buf;
    *outSize = (size_t)sz;
    return true;
}

static FORCEINLINE uint16_t pack_rgb565(unsigned char r, unsigned char g, unsigned char b)
{
    uint16_t R = (uint16_t)((r >> 3) & 0x1F);
    uint16_t G = (uint16_t)((g >> 2) & 0x3F);
    uint16_t B = (uint16_t)((b >> 3) & 0x1F);
    return (uint16_t)((R << 11) | (G << 5) | B);
}

void dc_show_loading_splash()
{
    // Require a video context. osystem_init() is safe to call multiple times.
    osystem_init();

    // Try a couple of common filenames (relative paths resolve under /cd due to chdir).
    const char* candidates[] = {
        "LOADING.png",
        "LOADING.PNG",
    };

    unsigned char* data = nullptr;
    size_t size = 0;
    const char* used = nullptr;

    for (const char* path : candidates)
    {
        if (read_whole_file(path, &data, &size))
        {
            used = path;
            break;
        }
    }

    if (!data)
        return;

    int w = 0;
    int h = 0;
    int comp = 0;

    // Force RGBA for simplicity.
    unsigned char* rgba = stbi_load_from_memory((const stbi_uc*)data, (int)size, &w, &h, &comp, 4);
    free(data);

    if (!rgba)
        return;

    // Keep this strict and simple: require 512x512 input.
    static constexpr int W = 512;
    static constexpr int H = 512;
    if (w != W || h != H)
    {
        stbi_image_free(rgba);
        return;
    }

    std::vector<uint16_t> rgb565;
    rgb565.resize((size_t)W * (size_t)H);
    for (int y = 0; y < H; ++y)
    {
        const unsigned char* src = rgba + (size_t)y * (size_t)W * 4u;
        uint16_t* dst = rgb565.data() + (size_t)y * (size_t)W;
        for (int x = 0; x < W; ++x)
        {
            const unsigned char r = src[x * 4 + 0];
            const unsigned char g = src[x * 4 + 1];
            const unsigned char b = src[x * 4 + 2];
            dst[x] = pack_rgb565(r, g, b);
        }
    }

    stbi_image_free(rgba);

    // Upload to a temporary GL texture and draw it once.
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex)
        return;

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, W, H, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, rgb565.data());

    // Establish a known-good 2D state for the splash.
    if (vid_mode)
        glViewport(0, 0, vid_mode->width, vid_mode->height);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, 320.0f, 200.0f, 0.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.f, 0.f); glVertex3f(0.f,   0.f,   0.f);
    glTexCoord2f(1.f, 0.f); glVertex3f(320.f, 0.f,   0.f);
    glTexCoord2f(0.f, 1.f); glVertex3f(0.f,   200.f, 0.f);
    glTexCoord2f(1.f, 1.f); glVertex3f(320.f, 200.f, 0.f);
    glEnd();

    glKosSwapBuffers();

    // Ensure at least one VBlank passes so the image is visible even if we
    // immediately start loading and hog the CPU.
    vid_waitvbl();

    glDeleteTextures(1, &tex);

    // Restore the game's expected 2D state so we don't leave GLdc matrices/modes
    // in a surprising configuration.
    if (vid_mode)
        glViewport(0, 0, vid_mode->width, vid_mode->height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, 320.0f, 200.0f, 0.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    (void)used;
}

#endif // DREAMCAST
