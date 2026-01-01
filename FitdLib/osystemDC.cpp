//----------------------------------------------------------------------------
//  Dream In The Dark OS System (Dreamcast) (Dreamcast / KOS)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2025  yaz0r/jimmu/FITD Team
//  Copyright (C) 1999-2025  The EDGE Team
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
#include "osystem.h"
#include "vars.h"
#include "font.h"
#include <array>
#include <vector>
#include <cstring>
#include <cstdio>

// GLdc
#include <GL/gl.h>
#include <GL/glkos.h>

#include <dc/video.h>
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>

// Start building a native PVR paletted path (PAL8)
// Enable with -DUSE_PVR_PAL8 to switch from GLdc blit to PVR renderer.
#ifdef USE_PVR_PAL8
#include <dc/pvr.h>
static bool g_pvrInited = false;
static pvr_ptr_t g_pal8Texture = nullptr;
static constexpr int TEX_W = 512; // PVR requires power-of-two
static constexpr int TEX_H = 256;
static float g_uMax = 320.0f / (float)TEX_W;
static float g_vMax = 200.0f / (float)TEX_H;
static std::vector<u8> g_pal8Buffer(TEX_W * TEX_H, 0);
static pvr_poly_cxt_t g_pvrCxt;
static pvr_poly_hdr_t g_pvrHdr;
#endif

static GLuint g_backgroundTex = 0;
static bool g_glInited = false;
static std::array<u8, 256 * 3> g_palette = {0};
static std::array<uint16_t, 256> g_palette565 = {0};

static bool g_soundInited = false;

static char g_debugSfxMsg[128] = {0};
static int g_debugSfxMsgFramesLeft = 0;

static void dc_set_sfx_debug_msg(const char* msg)
{
    if (!msg)
        return;

    std::snprintf(g_debugSfxMsg, sizeof(g_debugSfxMsg), "%s", msg);
    g_debugSfxMsg[sizeof(g_debugSfxMsg) - 1] = '\0';
    g_debugSfxMsgFramesLeft = 180; // ~a few seconds at ~25-60fps
}

static void ensure_sound()
{
    if (!g_soundInited)
    {
        snd_init();
        g_soundInited = true;
    }
}

static void ensure_fullscreen_viewport()
{
    const vid_mode_t* mode = vid_mode;
    const int vw = mode ? mode->width : 640;
    const int vh = mode ? mode->height : 480;
    glViewport(0, 0, vw, vh);
}

unsigned char frontBuffer[320 * 200] = {0};
unsigned char physicalScreen[320 * 200] = {0};
std::array<unsigned char, 320 * 200> uiLayer = {0};

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
        // allocate initial 320x200 texture store in RGB565
        std::vector<uint16_t> blank(320 * 200, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 320, 200, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, blank.data());
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, g_backgroundTex);
    }
}

void osystem_init()
{
    if (g_glInited)
        return;

#ifdef USE_PVR_PAL8
    if (!g_pvrInited)
    {
        pvr_init_defaults();
        pvr_set_pal_format(PVR_PAL_ARGB1555);
        g_pal8Texture = pvr_mem_malloc(TEX_W * TEX_H);
        pvr_poly_cxt_txr(&g_pvrCxt, PVR_LIST_OP_POLY,
                         PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_TWIDDLED,
                         TEX_W, TEX_H, g_pal8Texture, PVR_FILTER_NEAREST);
        pvr_poly_compile(&g_pvrHdr, &g_pvrCxt);
        g_pvrInited = true;
    }
#else
    glKosInit();

    // GLdc doesn't always leave the viewport matching the current video mode.
    // If it's left at 320x200 (or similar), our 320x200 quad only fills a corner.
    // Force the viewport to the full screen so the game scales correctly.
    ensure_fullscreen_viewport();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // 4:3 surface; keep game space at 320x200
    glOrtho(0.0f, 320.0f, 200.0f, 0.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);

    ensureTexture();
#endif

    g_glInited = true;
}

void osystem_initGL(int /*screenWidth*/, int /*screenHeight*/)
{
    // Handled by osystem_init for Dreamcast
}

void osystem_delay(int /*time*/)
{
    // noop on Dreamcast
}

static FORCEINLINE uint16_t rgb565_from_palette(u8 idx)
{
    return g_palette565[idx];
}

static void uploadComposited()
{
    // Compose UI over physicalScreen (non-zero UI pixels override)
#ifdef USE_PVR_PAL8
    // Compose into 512x256 pal8 buffer with padding
    for (int y = 0; y < 200; ++y)
    {
        u8* dst = g_pal8Buffer.data() + y * TEX_W;
        const u8* scr = physicalScreen + y * 320;
        const u8* ui  = uiLayer.data() + y * 320;
        for (int x = 0; x < 320; ++x)
        {
            const u8 u = ui[x];
            dst[x] = u ? u : scr[x];
        }
        // remaining [320..TEX_W) already zeroed or left as is
    }
    // Upload full texture to VRAM (8bpp paletted, twiddled)
    uint32_t txrFlags = PVR_TXRLOAD_8BPP;
    // KOS flag naming differs between versions.
    // Newer KOS: PVR_TXRLOAD_FMT_TWIDDLED
    // Older KOS: PVR_TXRLOAD_TWIDDLED
    #ifdef PVR_TXRLOAD_TWIDDLED
        txrFlags |= PVR_TXRLOAD_TWIDDLED;
    #else
        txrFlags |= PVR_TXRLOAD_FMT_TWIDDLED;
    #endif
    pvr_txr_load_ex(g_pal8Buffer.data(), g_pal8Texture, TEX_W, TEX_H, txrFlags);
#else
    static std::vector<uint16_t> rgb(320 * 200);
    for (int i = 0; i < 320 * 200; ++i)
    {
        u8 src = uiLayer[i] ? uiLayer[i] : physicalScreen[i];
        rgb[i] = rgb565_from_palette(src);
    }

    ensureTexture();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 320, 200, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, rgb.data());
#endif
}

u32 osystem_startOfFrame()
{
    // Single frame advance; engine targets ~25fps
    return 1;
}

void osystem_endOfFrame()
{
    // If we had an SFX load/parse failure recently, render a tiny overlay message.
    // We draw into uiLayer so it goes through the normal 320x200 compositing path.
    if (g_debugSfxMsgFramesLeft > 0 && g_debugSfxMsg[0] != '\0' && PtrFont)
    {
        const int bandH = std::min(200, fontHeight + 2);
        const int y0 = std::max(0, 200 - bandH);

        for (int y = y0; y < 200; ++y)
        {
            unsigned char* row = uiLayer.data() + y * 320;
            std::memset(row, 0, 320);
        }

        const int x = 2;
        const int y = y0 + 1;

        // Shadow + main color for readability with unknown palettes.
        SetFont(PtrFont, 1);
        PrintFont(x + 1, y + 1, (char*)logicalScreen, (u8*)g_debugSfxMsg);
        SetFont(PtrFont, 255);
        PrintFont(x, y, (char*)logicalScreen, (u8*)g_debugSfxMsg);

        g_debugSfxMsgFramesLeft--;
    }

    // Draw full-screen quad
    uploadComposited();

#ifdef USE_PVR_PAL8
    // Submit a PAL8 textured quad using PVR lists (triangle strip)
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&g_pvrHdr, sizeof(g_pvrHdr));

    pvr_vertex_t v;
    // v0
    v.flags = PVR_CMD_VERTEX; v.x = 0.0f;   v.y = 0.0f;   v.z = 1.0f;
    v.u = 0.0f; v.v = 0.0f; v.argb = 0xFFFFFFFF; v.oargb = 0;
    pvr_prim(&v, sizeof(v));
    // v1
    v.flags = PVR_CMD_VERTEX; v.x = 320.0f; v.y = 0.0f;   v.z = 1.0f;
    v.u = g_uMax; v.v = 0.0f; v.argb = 0xFFFFFFFF; v.oargb = 0;
    pvr_prim(&v, sizeof(v));
    // v2
    v.flags = PVR_CMD_VERTEX; v.x = 0.0f;   v.y = 200.0f; v.z = 1.0f;
    v.u = 0.0f; v.v = g_vMax; v.argb = 0xFFFFFFFF; v.oargb = 0;
    pvr_prim(&v, sizeof(v));
    // v3 (end)
    v.flags = PVR_CMD_VERTEX_EOL; v.x = 320.0f; v.y = 200.0f; v.z = 1.0f;
    v.u = g_uMax; v.v = g_vMax; v.argb = 0xFFFFFFFF; v.oargb = 0;
    pvr_prim(&v, sizeof(v));

    pvr_list_finish();
    pvr_scene_finish();
#else
    ensure_fullscreen_viewport();
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    glBindTexture(GL_TEXTURE_2D, g_backgroundTex);

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.f, 0.f); glVertex3f(0.f,   0.f,   0.f);
    glTexCoord2f(1.f, 0.f); glVertex3f(320.f, 0.f,   0.f);
    glTexCoord2f(0.f, 1.f); glVertex3f(0.f,   200.f, 0.f);
    glTexCoord2f(1.f, 1.f); glVertex3f(320.f, 200.f, 0.f);
    glEnd();

    glKosSwapBuffers();
#endif
}

void osystem_flip(unsigned char* /*videoBuffer*/)
{
    // No separate flip step needed; endOfFrame performs swap.
}

void osystem_drawBackground()
{
    // Background drawn in endOfFrame after CopyBlockPhys updates physicalScreen
}

void osystem_drawUILayer()
{
    // Composition handled in endOfFrame
}

void osystem_setPalette(unsigned char* palette)
{
    std::memcpy(g_palette.data(), palette, 256 * 3);
    for (int i = 0; i < 256; ++i)
    {
        u8 r = g_palette[i * 3 + 0];
        u8 g = g_palette[i * 3 + 1];
        u8 b = g_palette[i * 3 + 2];
        uint16_t R = (r >> 3) & 0x1F;
        uint16_t G = (g >> 2) & 0x3F;
        uint16_t B = (b >> 3) & 0x1F;
        g_palette565[i] = (R << 11) | (G << 5) | (B);
#ifdef USE_PVR_PAL8
        // Upload ARGB1555 palette (alpha forced to 1)
        uint16_t pr = (r >> 3) & 0x1F;
        uint16_t pg = (g >> 3) & 0x1F;
        uint16_t pb = (b >> 3) & 0x1F;
        uint16_t packed = (1u << 15) | (pr << 10) | (pg << 5) | (pb);
        pvr_set_pal_entry(i, packed);
#endif
    }
}

void osystem_setPalette(palette_t* palette)
{
    for (int i = 0; i < 256; ++i)
    {
        g_palette[i * 3 + 0] = (*palette)[i][0];
        g_palette[i * 3 + 1] = (*palette)[i][1];
        g_palette[i * 3 + 2] = (*palette)[i][2];
        uint16_t R = (g_palette[i * 3 + 0] >> 3) & 0x1F;
        uint16_t G = (g_palette[i * 3 + 1] >> 2) & 0x3F;
        uint16_t B = (g_palette[i * 3 + 2] >> 3) & 0x1F;
        g_palette565[i] = (R << 11) | (G << 5) | (B);
    #ifdef USE_PVR_PAL8
        uint16_t pr = (g_palette[i * 3 + 0] >> 3) & 0x1F;
        uint16_t pg = (g_palette[i * 3 + 1] >> 3) & 0x1F;
        uint16_t pb = (g_palette[i * 3 + 2] >> 3) & 0x1F;
        uint16_t packed = (1u << 15) | (pr << 10) | (pg << 5) | (pb);
        pvr_set_pal_entry(i, packed);
    #endif
    }
}

void osystem_CopyBlockPhys(unsigned char* videoBuffer, int left, int top, int right, int bottom)
{
    // Copy 8-bit indices into physicalScreen
    for (int y = top; y < bottom; ++y)
    {
        unsigned char* src = videoBuffer + left + y * 320;
        unsigned char* dst = physicalScreen + left + y * 320;
        std::memcpy(dst, src, (right - left));
    }
}

void osystem_refreshFrontTextureBuffer() {}
void osystem_updateImage() {}
void osystem_fadeBlackToWhite() {}
void osystem_initBuffer() {}
void osystem_initVideoBuffer(char* /*buffer*/, int /*width*/, int /*height*/) {}
void osystem_putpixel(int /*x*/, int /*y*/, int /*pixel*/) {}
void osystem_setColor(unsigned char /*i*/, unsigned char /*R*/, unsigned char /*G*/, unsigned char /*B*/) {}
void osystem_setPalette320x200(unsigned char* /*palette*/) {}
void osystem_drawLine(int, int, int, int, unsigned char, unsigned char*) {}

void osystem_createMask(const std::array<u8, 320 * 200>&, int, int, int, int, int, int) {}
void osystem_drawMask(int, int) {}

void osystem_startFrame() {}
void osystem_stopFrame() {}
void osystem_setClip(float, float, float, float) {}
void osystem_clearClip() {}
void osystem_cleanScreenKeepZBuffer() {}

void osystem_fillPoly(float*, int, unsigned char, u8) {}
void osystem_draw3dLine(float, float, float, float, float, float, unsigned char) {}
void osystem_draw3dQuad(float, float, float, float, float, float, float, float, float, float, float, float, unsigned char, int) {}
void osystem_drawSphere(float, float, float, u8, u8, float) {}
void osystem_drawPoint(float, float, float, u8, u8, float) {}
void osystem_flushPendingPrimitives() {}

int osystem_playTrack(int) { return 0; }
void osystem_playAdlib() {}
void osystem_playSample(char* samplePtr, int size)
{
    if (!samplePtr || size <= 0)
        return;

    ensure_sound();

    // Keep a small cache so we don't keep re-uploading the same SFX into SPU RAM.
    struct CachedSfx {
        const void* ptr;
        int size;
        sfxhnd_t hnd;
    };
    static std::vector<CachedSfx> cache;
    static constexpr size_t kMaxCached = 48;

    for (auto &e : cache)
    {
        if (e.ptr == samplePtr && e.size == size && e.hnd != SFXHND_INVALID)
        {
            snd_sfx_play(e.hnd, 255, 128);
            return;
        }
    }

    sfxhnd_t hnd = SFXHND_INVALID;

    if (g_gameId >= TIMEGATE)
    {
        // Later titles store samples as WAV; KOS can load WAV-from-memory.
        hnd = snd_sfx_load_buf(samplePtr);
        if (hnd == SFXHND_INVALID)
            dc_set_sfx_debug_msg("SFX load failed (WAV)");
    }
    else
    {
        // AITD1/2/3 use VOC (Creative Voice). Extract the first audio block.
        // Header is 26 bytes; first block starts at offset 26.
        if (size < 32)
        {
            dc_set_sfx_debug_msg("SFX parse failed (VOC too small)");
            return;
        }

        if ((unsigned char)samplePtr[26] != 1)
        {
            dc_set_sfx_debug_msg("SFX parse failed (VOC block!=1)");
            return;
        }

        // Block header: [type:1][size:3] (24-bit LE), then block data.
        const uint32_t blockSize24 = (uint32_t)((unsigned char)samplePtr[27]) |
                                    ((uint32_t)((unsigned char)samplePtr[28]) << 8) |
                                    ((uint32_t)((unsigned char)samplePtr[29]) << 16);
        // For sound data blocks: data contains [freq_div:1][codec:1][samples...]
        if (blockSize24 < 2)
        {
            dc_set_sfx_debug_msg("SFX parse failed (VOC block size)");
            return;
        }

        const int sampleBytes = (int)blockSize24 - 2;
        const unsigned char frequencyDiv = (unsigned char)samplePtr[30];
        const unsigned char codecId = (unsigned char)samplePtr[31];
        (void)codecId;

        const char* sampleData = samplePtr + 32;
        if (32 + sampleBytes > size)
        {
            dc_set_sfx_debug_msg("SFX parse failed (VOC bounds)");
            return;
        }

        const int sampleRate = 1000000 / (256 - (int)frequencyDiv);

        // KOS requires raw buffers be padded to 32 bytes per channel.
        size_t paddedLen = (size_t)sampleBytes;
        paddedLen = (paddedLen + 31u) & ~31u;

        std::vector<char> tmp(paddedLen, 0);
        std::memcpy(tmp.data(), sampleData, (size_t)sampleBytes);

        hnd = snd_sfx_load_raw_buf(tmp.data(), paddedLen, (uint32_t)sampleRate, 8, 1);
        if (hnd == SFXHND_INVALID)
            dc_set_sfx_debug_msg("SFX load failed (raw)");
    }

    if (hnd == SFXHND_INVALID)
        return;

    snd_sfx_play(hnd, 255, 128);

    cache.push_back({ samplePtr, size, hnd });
    if (cache.size() > kMaxCached)
    {
        // Evict oldest.
        snd_sfx_unload(cache.front().hnd);
        cache.erase(cache.begin());
    }
}

void osystem_drawText(int X, int Y, char* text)
{
    if (!text || !PtrFont)
        return;
    SetFont(PtrFont, 255);
    PrintFont(X, Y, (char*)logicalScreen, (u8*)text);
}

void osystem_drawTextColor(int X, int Y, char* text, unsigned char R, unsigned char G, unsigned char B)
{
    if (!text || !PtrFont)
        return;

    // We don't have a reliable RGB->palette mapping here; pick a readable color.
    const unsigned int lum = (unsigned int)R + (unsigned int)G + (unsigned int)B;
    const int idx = (lum < 3u * 64u) ? 1 : 255;

    SetFont(PtrFont, idx);
    PrintFont(X, Y, (char*)logicalScreen, (u8*)text);
}

#endif // DREAMCAST
