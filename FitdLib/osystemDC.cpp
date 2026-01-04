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
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>

// GLdc
#include <GL/gl.h>
#include <GL/glkos.h>

#include <dc/video.h>
#include <dc/sound/sound.h>
#include <dc/sound/stream.h>
#include <dc/sound/sfxmgr.h>

#include <dc/maple.h>
#include <dc/maple/controller.h>

extern "C" {
#include <kos/dbgio.h>
}

#include <kos/thread.h>

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

// Simple hardware 3D overlay: submit flat-colored triangles via PVR.
struct DcTri
{
    float x1, y1;
    float x2, y2;
    float x3, y3;
    uint32_t argb;
};

static std::vector<DcTri> g_pvrTris;
static pvr_poly_cxt_t g_pvrColCxt;
static pvr_poly_hdr_t g_pvrColHdr;
#endif

#ifndef USE_PVR_PAL8
static GLuint g_backgroundTex = 0;
static constexpr int GL_TEX_W = 512;
static constexpr int GL_TEX_H = 256;
static float g_gl_uMax = 320.0f / (float)GL_TEX_W;
static float g_gl_vMax = 200.0f / (float)GL_TEX_H;
#endif
static bool g_glInited = false;
static std::array<u8, 256 * 3> g_palette = {0};
static std::array<uint16_t, 256> g_palette565 = {0};

static bool g_soundInited = false;

//------------------------------------------------------------------------------
// ADL/OPL music streaming via KOS snd_stream
// TODO: move this into system/musicDC.cc and system/s_musicdc.cc 
//------------------------------------------------------------------------------
static bool g_streamInited = false;
static snd_stream_hnd_t g_adlibStream = SND_STREAM_INVALID;
static bool g_adlibStreamStarted = false;

static volatile bool g_adlibPollThreadRun = false;
static kthread_t* g_adlibPollThread = nullptr;

static volatile bool g_adlibSynthThreadRun = false;
static kthread_t* g_adlibSynthThread = nullptr;

static constexpr uint32_t kAdlibHz = 22050;
// Buffer size is per-channel bytes; 16-bit mono => bytes = samples * 2.
// Keep this relatively small so the callback doesn't try to synthesize
// huge bursts of audio at once (which can miss real-time deadlines).
static constexpr int kAdlibBufBytes = 8 * 1024;
static uint8_t g_adlibPcmBuf[kAdlibBufBytes] __attribute__((aligned(32)));

// Ring buffer for pre-generated PCM (single producer: synth thread, single
// consumer: snd_stream callback). Keep a decent amount buffered to survive
// renderer/model load spikes.
static constexpr uint32_t kAdlibRingSamples = 65536; // ~2.97s @ 22050Hz
static int16_t g_adlibRing[kAdlibRingSamples] __attribute__((aligned(32)));
static volatile uint32_t g_adlibRingRead = 0;
static volatile uint32_t g_adlibRingWrite = 0;

static FORCEINLINE uint32_t dc_ring_avail(uint32_t rd, uint32_t wr)
{
    return (wr >= rd) ? (wr - rd) : (kAdlibRingSamples - (rd - wr));
}

static FORCEINLINE uint32_t dc_ring_space(uint32_t rd, uint32_t wr)
{
    // Keep one slot empty to distinguish full vs empty.
    return (kAdlibRingSamples - 1) - dc_ring_avail(rd, wr);
}

static void* dc_adlib_poll_thread(void* /*param*/)
{
    thd_set_label(thd_get_current(), "adlib_stream");

    while (g_adlibPollThreadRun)
    {
        if (g_adlibStreamStarted && g_adlibStream != SND_STREAM_INVALID)
            snd_stream_poll(g_adlibStream);

        // Poll frequently so we don't under-run when the render thread stalls.
        thd_sleep(1);
    }

    return nullptr;
}

static void ensure_adlib_poll_thread()
{
    if (g_adlibPollThreadRun)
        return;

    g_adlibPollThreadRun = true;
    g_adlibPollThread = thd_create(true, dc_adlib_poll_thread, nullptr);
    if (!g_adlibPollThread)
    {
        g_adlibPollThreadRun = false;
        return;
    }

    // PRIO_DEFAULT is 10; lower is higher priority.
    // Give audio a small boost so heavy rendering doesn't starve polling.
    thd_set_prio(g_adlibPollThread, 3);
}

static void* dc_adlib_synth_thread(void* /*param*/)
{
    thd_set_label(thd_get_current(), "adlib_synth");

    // Generate audio in small chunks so we can interleave with the rest of the
    // system without long stalls.
    static constexpr int kChunkSamples = 1024;
    static constexpr int kChunkBytes = kChunkSamples * 2;
    static uint8_t s_chunkBuf[kChunkBytes] __attribute__((aligned(32)));

    while (g_adlibSynthThreadRun)
    {
        // Wait until the stream is allocated so we know someone wants audio.
        if (g_adlibStream == SND_STREAM_INVALID)
        {
            thd_sleep(10);
            continue;
        }

        // Keep the ring buffer mostly full so brief stalls don't underrun.
        const uint32_t targetAvail = (kAdlibRingSamples * 3) / 4;

        uint32_t rd = g_adlibRingRead;
        uint32_t wr = g_adlibRingWrite;
        uint32_t avail = dc_ring_avail(rd, wr);

        if (avail >= targetAvail)
        {
            thd_sleep(2);
            continue;
        }

        // Fill loop: generate and push chunks until we hit target fill.
        while (g_adlibSynthThreadRun)
        {
            rd = g_adlibRingRead;
            wr = g_adlibRingWrite;
            const uint32_t space = dc_ring_space(rd, wr);
            avail = dc_ring_avail(rd, wr);

            if (avail >= targetAvail)
                break;
            if (space < (uint32_t)kChunkSamples)
                break;

            fitd_memset(s_chunkBuf, 0, (size_t)kChunkBytes);
            musicUpdate(nullptr, s_chunkBuf, kChunkBytes);

            const int16_t* src = (const int16_t*)s_chunkBuf;
            uint32_t remaining = (uint32_t)kChunkSamples;

            while (remaining > 0)
            {
                rd = g_adlibRingRead;
                wr = g_adlibRingWrite;
                const uint32_t space2 = dc_ring_space(rd, wr);
                if (space2 == 0)
                    break;

                uint32_t toWrite = remaining;
                if (toWrite > space2)
                    toWrite = space2;

                uint32_t chunk = toWrite;
                const uint32_t tillWrap = kAdlibRingSamples - wr;
                if (chunk > tillWrap)
                    chunk = tillWrap;

                fitd_memcpy(&g_adlibRing[wr], src, (size_t)chunk * 2);

                wr += chunk;
                if (wr >= kAdlibRingSamples)
                    wr = 0;

                g_adlibRingWrite = wr;
                src += chunk;
                remaining -= chunk;
            }

            // Let other threads run between chunks.
            thd_sleep(0);
        }
    }

    return nullptr;
}

static void ensure_adlib_synth_thread()
{
    if (g_adlibSynthThreadRun)
        return;

    g_adlibSynthThreadRun = true;
    g_adlibSynthThread = thd_create(true, dc_adlib_synth_thread, nullptr);
    if (!g_adlibSynthThread)
    {
        g_adlibSynthThreadRun = false;
        return;
    }

    // Higher priority than most game work; synth must keep up.
    thd_set_prio(g_adlibSynthThread, 4);
}

static void* dc_adlib_stream_cb(snd_stream_hnd_t /*hnd*/, int smp_req, int* smp_recv)
{
    if (!smp_recv)
        return nullptr;

    static bool s_loggedOnce = false;
    if (!s_loggedOnce)
    {
        s_loggedOnce = true;
        dbgio_printf("[dc] [music] adlib stream cb active (req=%d)\n", smp_req);
    }

    int maxSamples = kAdlibBufBytes / 2;
    int samples = smp_req;
    if (samples > maxSamples)
        samples = maxSamples;
    if (samples < 0)
        samples = 0;

    int16_t* out = (int16_t*)g_adlibPcmBuf;
    int need = samples;
    int underrun = 0;

    while (need > 0)
    {
        const uint32_t rd0 = g_adlibRingRead;
        const uint32_t wr0 = g_adlibRingWrite;
        const uint32_t avail = dc_ring_avail(rd0, wr0);
        if (avail == 0)
        {
            fitd_memset(out, 0, (size_t)need * 2);
            underrun += need;
            break;
        }

        uint32_t toCopy = (uint32_t)need;
        if (toCopy > avail)
            toCopy = avail;

        uint32_t chunk = toCopy;
        const uint32_t tillWrap = kAdlibRingSamples - rd0;
        if (chunk > tillWrap)
            chunk = tillWrap;

        fitd_memcpy(out, &g_adlibRing[rd0], (size_t)chunk * 2);

        uint32_t rd1 = rd0 + chunk;
        if (rd1 >= kAdlibRingSamples)
            rd1 = 0;
        g_adlibRingRead = rd1;

        out += (int)chunk;
        need -= (int)chunk;
    }

    if (underrun > 0)
    {
        static uint64_t s_lastLog = 0;
        const uint64_t now = timer_ms_gettime64();
        if (now - s_lastLog > 1000)
        {
            s_lastLog = now;
            dbgio_printf("[dc] [music] underrun: %d samples\n", underrun);
        }
    }

    *smp_recv = samples;
    return g_adlibPcmBuf;
}

static bool g_renderDebugHud = true;
static int g_renderDebugCableType = -1;

static char g_debugSfxMsg[128] = {0};
static int g_debugSfxMsgFramesLeft = 0;

// Software 2D raster helpers used by the engine (8-bit indexed into frontBuffer).
extern void fillpoly(s16* datas, int n, unsigned char c);
extern void line(int x1, int y1, int x2, int y2, unsigned char c);
extern void hline(int x1, int x2, int y, unsigned char c);
extern unsigned char* polyBackBuffer;

#ifndef USE_PVR_PAL8
//------------------------------------------------------------------------------
// GLdc 3D batching (Dreamcast)
//------------------------------------------------------------------------------
static void ensure_fullscreen_viewport();
static void dc_force_fullscreen_scissor();

struct RGL_Vtx
{
    float x;
    float y;
    float z;
    u8 colorIdx;
    u8 alpha;
};

static std::vector<RGL_Vtx> g_rglTriVtx;
static std::vector<RGL_Vtx> g_rglLineVtx;
static std::array<int, 8> g_rglPolyTypeCounts = {0};

static FORCEINLINE void RGL_BeginFrame()
{
    g_rglTriVtx.clear();
    g_rglLineVtx.clear();
    g_rglPolyTypeCounts.fill(0);
}

static FORCEINLINE void RGL_AddTri(float x1, float y1, float z1,
                                  float x2, float y2, float z2,
                                  float x3, float y3, float z3,
                                  u8 colorIdx, u8 alpha)
{
    g_rglTriVtx.push_back({x1, y1, z1, colorIdx, alpha});
    g_rglTriVtx.push_back({x2, y2, z2, colorIdx, alpha});
    g_rglTriVtx.push_back({x3, y3, z3, colorIdx, alpha});
}

static FORCEINLINE void RGL_AddLine(float x1, float y1, float z1,
                                   float x2, float y2, float z2,
                                   u8 colorIdx, u8 alpha)
{
    g_rglLineVtx.push_back({x1, y1, z1, colorIdx, alpha});
    g_rglLineVtx.push_back({x2, y2, z2, colorIdx, alpha});
}

static void RGL_RenderPolys()
{
    if (g_rglTriVtx.empty() && g_rglLineVtx.empty())
        return;

    // Use the engine's projected X/Y (320x200 game-space) and preserve Z for
    // ordering. This mirrors the old SDL_GL implementation from Jimmu.
    ensure_fullscreen_viewport();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, 320.0f, 200.0f, 0.0f, 0.2f, -50000.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    dc_force_fullscreen_scissor();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    // The engine already orders primitives for correct painter-style rendering.
    // Avoid depth buffer work on Dreamcast for speed.
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
#endif

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
        // Use the stream init path so we can play OPL music and SFX together.
        // NOTE: snd_stream_init() implicitly calls snd_init() and may reset the AICA.
        if (!g_streamInited)
        {
            if (snd_stream_init() == 0)
                g_streamInited = true;
        }

        if (!g_streamInited)
        {
            // Fallback: at least bring up basic sound so SFX works.
            snd_init();
        }
        g_soundInited = true;
    }
}

static void dc_music_poll()
{
    // If the dedicated poll thread is running, never poll from the render thread.
    // snd_stream_poll() isn't guaranteed to be re-entrant.
    if (g_adlibPollThreadRun)
        return;

    if (g_adlibStreamStarted && g_adlibStream != SND_STREAM_INVALID)
    {
        snd_stream_poll(g_adlibStream);
    }
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static void dc_force_fullframe_scanout_if_needed(int cable)
{
    // KOS built-in VGA modes intentionally center the bitmap window within the
    // scan area (e.g. bmp 172,40). Force a full-frame bitmap + border window 
    // so the output fills the actual viewport.
    if (cable != CT_VGA)
        return;

    if (!vid_mode)
        return;

    if (vid_mode->width != 640 || vid_mode->height != 480)
        return;

    vid_mode_t mode = *vid_mode;

    mode.bitmapx = 0;
    mode.bitmapy = 0;

    // Expand border window to full scanout area.
    // These values are in scan timing units (clocks/scanlines), not framebuffer pixels.
    if (mode.clocks > 0)
    {
        mode.borderx1 = 0;
        mode.borderx2 = mode.clocks - 1;
    }

    if (mode.scanlines > 0)
    {
        mode.bordery1 = 0;
        mode.bordery2 = mode.scanlines - 1;
    }

    // Apply updated scanout/window parameters.
    vid_set_mode_ex(&mode);
}

#ifndef USE_PVR_PAL8
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
#endif

unsigned char frontBuffer[320 * 200] = {0};
unsigned char physicalScreen[320 * 200] = {0};
std::array<unsigned char, 320 * 200> uiLayer = {0};

#ifndef USE_PVR_PAL8
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
        // Allocate a POT texture store (GLdc/PVR constraints). We'll update only the
        // top-left 320x200 sub-rectangle each frame.
        std::vector<uint16_t> blank(GL_TEX_W * GL_TEX_H, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GL_TEX_W, GL_TEX_H, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, blank.data());
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, g_backgroundTex);
    }
}
#endif

void osystem_init()
{
    if (g_glInited)
        return;

    // Pick a deterministic video mode early.
    // NOTE: KOS built-in VGA modes intentionally center the bitmap window.
    // Some emulators show the unused border as large black bars, so we can
    // optionally force a full-frame scanout.
    const int cable = vid_check_cable();
    g_renderDebugCableType = cable;

    // Use the explicit VGA timing/mode for 640x480.
    if (cable == CT_VGA || cable < 0)
        vid_set_mode(DM_640x480_VGA, PM_RGB565);
    else
        vid_set_mode(DM_320x240, PM_RGB565);

    dc_force_fullframe_scanout_if_needed(cable);

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

        pvr_poly_cxt_col(&g_pvrColCxt, PVR_LIST_OP_POLY);
        g_pvrColCxt.gen.culling = PVR_CULLING_NONE;
        pvr_poly_compile(&g_pvrColHdr, &g_pvrColCxt);

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

    // Ensure the PVR tile-clip covers the full screen from the start.
    dc_force_fullscreen_scissor();

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
    // Frame pacing: keep the engine logic deterministic (30FPS, for now 25FPS..).
    // Returning >1 here causes the game to advance multiple logic ticks in a
    // single rendered frame, which can make fades/text feel like they vanish.
    
    // Without throttling here, Dreamcast will run as fast as the CPU allows,
    // causing cutscenes, book pages, and gameplay timing to fly by.
    constexpr uint32_t kFramesPerSecond = 30;
    constexpr uint32_t kFrameMs = 1000 / kFramesPerSecond;

    static bool s_firstFrame = true;
    static uint64_t s_nextFrameMs = 0;

    const uint64_t now0 = timer_ms_gettime64();
    if (s_firstFrame)
    {
        s_nextFrameMs = now0 + kFrameMs;
        s_firstFrame = false;
    }

    // If we're running faster than real time, yield until the next frame.
    uint64_t now = now0;
    if (now < s_nextFrameMs)
    {
        const uint32_t sleepMs = (uint32_t)(s_nextFrameMs - now);
        if (sleepMs > 0)
            thd_sleep((int)sleepMs);
        now = timer_ms_gettime64();
    }

    // If we fell behind (slow frame), resync to avoid "catching up" logic.
    if (now > s_nextFrameMs + (uint64_t)kFrameMs)
        s_nextFrameMs = now + kFrameMs;
    else
        s_nextFrameMs += kFrameMs;

#ifndef USE_PVR_PAL8
    RGL_BeginFrame();
#endif
    return 1;
}

void osystem_endOfFrame()
{
    // Keep music stream fed.
    dc_music_poll();

    // Keep cable type diagnostic live for HUD.
    g_renderDebugCableType = vid_check_cable();

    // Print RGL debug stats to the serial console instead of drawing them onto the game.
    if (g_renderDebugHud && !DC_IsMenuActive())
    {
        static uint32_t last_log_ms = 0;
        const uint32_t now_ms = (uint32_t)timer_ms_gettime64();
        if ((now_ms - last_log_ms) > 1000)
        {
            last_log_ms = now_ms;

#ifndef USE_PVR_PAL8
            const int tris = (int)(g_rglTriVtx.size() / 3);
            const int lines = (int)(g_rglLineVtx.size() / 2);

            bool hasZ = false;
            float zMin = 0.0f;
            float zMax = 0.0f;
            for (const auto& v : g_rglTriVtx)
            {
                if (!hasZ) { zMin = zMax = v.z; hasZ = true; }
                else { if (v.z < zMin) zMin = v.z; if (v.z > zMax) zMax = v.z; }
            }
            for (const auto& v : g_rglLineVtx)
            {
                if (!hasZ) { zMin = zMax = v.z; hasZ = true; }
                else { if (v.z < zMin) zMin = v.z; if (v.z > zMax) zMax = v.z; }
            }

            if (hasZ)
                I_Printf("[rgl] tris:%d lines:%d z:[%.1f..%.1f] t0:%d t1:%d t2:%d t3:%d\n",
                         tris, lines, zMin, zMax,
                         g_rglPolyTypeCounts[0], g_rglPolyTypeCounts[1], g_rglPolyTypeCounts[2], g_rglPolyTypeCounts[3]);
            else
                I_Printf("[rgl] tris:%d lines:%d z:(none) t0:%d t1:%d t2:%d t3:%d\n",
                         tris, lines,
                         g_rglPolyTypeCounts[0], g_rglPolyTypeCounts[1], g_rglPolyTypeCounts[2], g_rglPolyTypeCounts[3]);
#else
            I_Printf("[rgl] (pvr) tris:%d\n", (int)g_pvrTris.size());
#endif
        }
    }

    // If we had an SFX load/parse failure recently, render a tiny overlay message.
    // We draw into uiLayer so it goes through the normal 320x200 compositing path.
    if (g_debugSfxMsgFramesLeft > 0 && g_debugSfxMsg[0] != '\0' && PtrFont)
    {
        const int bandH = std::min(200, fontHeight + 2);
        const int y0 = std::max(0, 200 - bandH);

        for (int y = y0; y < 200; ++y)
        {
            unsigned char* row = uiLayer.data() + y * 320;
            fitd_memset(row, 0, 320);
        }

        const int x = 2;
        const int y = y0 + 1;

        // Shadow + main color for readability with unknown palettes.
        SetFont(PtrFont, 1);
        PrintFont(x + 1, y + 1, (char*)uiLayer.data(), (u8*)g_debugSfxMsg);
        SetFont(PtrFont, 255);
        PrintFont(x, y, (char*)uiLayer.data(), (u8*)g_debugSfxMsg);

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

    const float screenW = vid_mode ? (float)vid_mode->width : 640.0f;
    const float screenH = vid_mode ? (float)vid_mode->height : 480.0f;

    pvr_vertex_t v;
    // v0
    v.flags = PVR_CMD_VERTEX; v.x = 0.0f;    v.y = 0.0f;    v.z = 1.0f;
    v.u = 0.0f; v.v = 0.0f; v.argb = 0xFFFFFFFF; v.oargb = 0;
    pvr_prim(&v, sizeof(v));
    // v1
    v.flags = PVR_CMD_VERTEX; v.x = screenW; v.y = 0.0f;    v.z = 1.0f;
    v.u = g_uMax; v.v = 0.0f; v.argb = 0xFFFFFFFF; v.oargb = 0;
    pvr_prim(&v, sizeof(v));
    // v2
    v.flags = PVR_CMD_VERTEX; v.x = 0.0f;    v.y = screenH; v.z = 1.0f;
    v.u = 0.0f; v.v = g_vMax; v.argb = 0xFFFFFFFF; v.oargb = 0;
    pvr_prim(&v, sizeof(v));
    // v3 (end)
    v.flags = PVR_CMD_VERTEX_EOL; v.x = screenW; v.y = screenH; v.z = 1.0f;
    v.u = g_uMax; v.v = g_vMax; v.argb = 0xFFFFFFFF; v.oargb = 0;
    pvr_prim(&v, sizeof(v));

    // Submit flat-colored 3D overlay triangles.
    // Map from 320x200 game-space to the active video mode resolution.
    if (!g_pvrTris.empty())
    {
        const float sx = screenW / 320.0f;
        const float sy = screenH / 200.0f;

        pvr_prim(&g_pvrColHdr, sizeof(g_pvrColHdr));

        for (const DcTri& t : g_pvrTris)
        {
            pvr_vertex_t pv;

            pv.flags = PVR_CMD_VERTEX;
            pv.x = t.x1 * sx;
            pv.y = t.y1 * sy;
            pv.z = 1.0f;
            pv.u = 0.0f;
            pv.v = 0.0f;
            pv.argb = t.argb;
            pv.oargb = 0;
            pvr_prim(&pv, sizeof(pv));

            pv.flags = PVR_CMD_VERTEX;
            pv.x = t.x2 * sx;
            pv.y = t.y2 * sy;
            pvr_prim(&pv, sizeof(pv));

            pv.flags = PVR_CMD_VERTEX_EOL;
            pv.x = t.x3 * sx;
            pv.y = t.y3 * sy;
            pvr_prim(&pv, sizeof(pv));
        }

        g_pvrTris.clear();
    }

    pvr_list_finish();
    pvr_scene_finish();
#else
    ensure_game_ortho();
    glClear(GL_COLOR_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, g_backgroundTex);

    // Ensure 2D blits are not darkened by leftover 3D state.
    glDisable(GL_BLEND);
    glColor4ub(255, 255, 255, 255);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.f,      0.f);      glVertex3f(0.f,   0.f,   0.f);
    glTexCoord2f(g_gl_uMax, 0.f);      glVertex3f(320.f, 0.f,   0.f);
    glTexCoord2f(0.f,      g_gl_vMax); glVertex3f(0.f,   200.f, 0.f);
    glTexCoord2f(g_gl_uMax, g_gl_vMax); glVertex3f(320.f, 200.f, 0.f);
    glEnd();

    // Draw any queued 3D primitives on top of the background.
    RGL_RenderPolys();

    glKosSwapBuffers();
#endif
}

void osystem_flip(unsigned char* videoBuffer)
{
    dc_music_poll();

    // Many UI/intro paths call osystem_flip() as their present step.
    // If a buffer is provided, copy it; otherwise assume CopyBlockPhys
    // (or osystem_drawBackground) has already updated physicalScreen.
    if (videoBuffer)
    {
        osystem_CopyBlockPhys(videoBuffer, 0, 0, 320, 200);
    }
    else if (logicalScreen)
    {
        osystem_CopyBlockPhys((unsigned char*)logicalScreen, 0, 0, 320, 200);
    }

    osystem_endOfFrame();
}

void osystem_drawBackground()
{
    dc_music_poll();

    // Used heavily by menus/intro screens as a present step.
    // Do not implicitly copy from logicalScreen here: many call-sites update
    // physicalScreen via osystem_CopyBlockPhys already, and forcing a copy can
    // clobber backgrounds.
    osystem_endOfFrame();
}

void osystem_drawUILayer()
{
    // Composition handled in endOfFrame
}

// Set the 256-color palette from a raw byte array (RGBRGB...).
void osystem_setPalette(unsigned char* palette)
{
    // ~CA: Change std::memcpy -> fitd_memcpy to avoid potential issues with
    // overlapping memory regions.
    fitd_memcpy(g_palette.data(), palette, 256 * 3);

#ifdef DREAMCAST
    // Mirror rendererBGFX: treat palette bytes as already 0..255.
    uint8_t maxc = 0;
    for (int i = 0; i < 256 * 3; ++i)
        if (g_palette[i] > maxc) maxc = g_palette[i];
    {
        static int s_lastMax = -1;
        if ((int)maxc != s_lastMax)
        {
            s_lastMax = (int)maxc;
            //dbgio_printf("[dc] [palette] max=%u\n", (unsigned)maxc);
        }
    }
#endif

    for (int i = 0; i < 256; ++i)
    {
        const uint16_t r = g_palette[i * 3 + 0];
        const uint16_t g = g_palette[i * 3 + 1];
        const uint16_t b = g_palette[i * 3 + 2];

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
    uint8_t maxc = 0;
    for (int i = 0; i < 256; ++i)
    {
        g_palette[i * 3 + 0] = (*palette)[i][0];
        g_palette[i * 3 + 1] = (*palette)[i][1];
        g_palette[i * 3 + 2] = (*palette)[i][2];
        if (g_palette[i * 3 + 0] > maxc) maxc = g_palette[i * 3 + 0];
        if (g_palette[i * 3 + 1] > maxc) maxc = g_palette[i * 3 + 1];
        if (g_palette[i * 3 + 2] > maxc) maxc = g_palette[i * 3 + 2];
    }

#ifdef DREAMCAST
    {
        static int s_lastMax = -1;
        if ((int)maxc != s_lastMax)
        {
            s_lastMax = (int)maxc;
            //dbgio_printf("[dc] [palette] max=%u\n", (unsigned)maxc);
        }
    }
#endif

    for (int i = 0; i < 256; ++i)
    {
        const uint16_t r = g_palette[i * 3 + 0];
        const uint16_t g = g_palette[i * 3 + 1];
        const uint16_t b = g_palette[i * 3 + 2];

        uint16_t R = (r >> 3) & 0x1F;
        uint16_t G = (g >> 2) & 0x3F;
        uint16_t B = (b >> 3) & 0x1F;
        g_palette565[i] = (R << 11) | (G << 5) | (B);
    #ifdef USE_PVR_PAL8
        uint16_t pr = (r >> 3) & 0x1F;
        uint16_t pg = (g >> 3) & 0x1F;
        uint16_t pb = (b >> 3) & 0x1F;
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
        fitd_memcpy(dst, src, (right - left));
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
void osystem_stopFrame() { osystem_endOfFrame(); }
void osystem_setClip(float, float, float, float) {}
void osystem_clearClip() {}
void osystem_cleanScreenKeepZBuffer() {}

static FORCEINLINE int dc_iround(float v)
{
    return (int)std::lround(v);
}

void osystem_fillPoly(float* buffer, int numPoint, unsigned char color, u8 polyType)
{
    if (!buffer || numPoint <= 0)
        return;

#ifdef USE_PVR_PAL8
    // Hardware path: triangulate and submit via PVR at endOfFrame.
    // Input vertices are already projected into 320x200 screen space.
    if (numPoint < 3)
        return;

    const u8 r = g_palette[color * 3 + 0];
    const u8 g = g_palette[color * 3 + 1];
    const u8 b = g_palette[color * 3 + 2];
    const uint32_t argb = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;

    const float x0 = buffer[0];
    const float y0 = buffer[1];

    for (int i = 2; i < numPoint; ++i)
    {
        const int i1 = (i - 1) * 3;
        const int i2 = i * 3;

        DcTri t;
        t.x1 = x0;
        t.y1 = y0;
        t.x2 = buffer[i1 + 0];
        t.y2 = buffer[i1 + 1];
        t.x3 = buffer[i2 + 0];
        t.y3 = buffer[i2 + 1];
        t.argb = argb;
        g_pvrTris.push_back(t);
    }
    return;

#else
    // GLdc path: approximate the BGFX material ramps by selecting a palette
    // index per-vertex (bank stays constant, shade varies 0..15).
    if (numPoint < 3)
        return;

    if (polyType < (u8)g_rglPolyTypeCounts.size())
        g_rglPolyTypeCounts[polyType]++;

    float polyMinX = 320.f;
    float polyMaxX = 0.f;
    float polyMinY = 200.f;
    float polyMaxY = 0.f;

    for (int i = 0; i < numPoint; ++i)
    {
        const float X = buffer[i * 3 + 0];
        const float Y = buffer[i * 3 + 1];
        polyMinX = std::min(polyMinX, X);
        polyMaxX = std::max(polyMaxX, X);
        polyMinY = std::min(polyMinY, Y);
        polyMaxY = std::max(polyMaxY, Y);
    }

    float polyW = polyMaxX - polyMinX;
    float polyH = polyMaxY - polyMinY;
    if (polyW <= 0.f) polyW = 1.f;
    if (polyH <= 0.f) polyH = 1.f;

    const int bank = (color & 0xF0) >> 4;
    const int startColor = (color & 0x0F);
    // NOTE: On Dreamcast we currently treat all polys as fully opaque.
    // Some sources label polyType==2 as "trans", but using constant half-alpha
    // causes key models (e.g. the armadillo) to appear incorrectly transparent.
    const u8 alpha = 255;

    auto shade_for_xy = [&](float X, float Y) -> int
    {
        switch (polyType)
        {
        default:
        case 0: // flat
        case 2: // trans (flat + alpha)
            return startColor;
        case 1: // dither (approx as alternating shade to avoid fullbright)
        {
            const int xi = (int)std::floor(X);
            const int yi = (int)std::floor(Y);
            const int shadeA = startColor;
            const int shadeB = startColor - 1;
            return ((xi ^ yi) & 1) ? shadeA : shadeB;
        }
        case 4: // copper (ramp top-to-bottom)
        case 5: // copper2 (2 scanlines per color; approximate by half-step)
        {
            float step = 1.0f;
            if (polyType == 5)
                step *= 0.5f;
            float shadef = (float)startColor + step * (Y - polyMinY);
            return (int)std::lround(shadef);
        }
        case 3: // marbre (ramp left-to-right)
        {
            float step = 15.0f / polyW;
            float shadef = (float)startColor + step * (X - polyMinX);
            return (int)std::lround(shadef);
        }
        case 6: // marbre2 (ramp right-to-left)
        {
            float step = 15.0f / polyW;
            float shadef = (float)startColor + step * (X - polyMinX);
            int s = (int)std::lround(shadef);
            return 15 - s;
        }
        }
    };

    auto vtx_color_idx = [&](float X, float Y) -> u8
    {
        int shade = shade_for_xy(X, Y);
        if (shade < 0) shade = 0;
        if (shade > 15) shade = 15;
        return (u8)((bank << 4) | (shade & 0x0F));
    };

    const float x0 = buffer[0];
    const float y0 = buffer[1];
    const float z0 = buffer[2];
    const u8 c0 = vtx_color_idx(x0, y0);

    for (int i = 2; i < numPoint; ++i)
    {
        const int i1 = (i - 1) * 3;
        const int i2 = i * 3;

        const float x1 = buffer[i1 + 0];
        const float y1 = buffer[i1 + 1];
        const float z1 = buffer[i1 + 2];
        const u8 c1 = vtx_color_idx(x1, y1);

        const float x2 = buffer[i2 + 0];
        const float y2 = buffer[i2 + 1];
        const float z2 = buffer[i2 + 2];
        const u8 c2 = vtx_color_idx(x2, y2);

        g_rglTriVtx.push_back({x0, y0, z0, c0, alpha});
        g_rglTriVtx.push_back({x1, y1, z1, c1, alpha});
        g_rglTriVtx.push_back({x2, y2, z2, c2, alpha});
    }
    return;
#endif

    // The engine provides screen-space projected vertices in 320x200.
    // Use the existing software polygon filler (expects s16 XY pairs).
    constexpr int kMaxPoints = 50;
    if (numPoint > kMaxPoints)
        numPoint = kMaxPoints;

    s16 coords[kMaxPoints * 2];
    for (int i = 0; i < numPoint; ++i)
    {
        const float x = buffer[i * 3 + 0];
        const float y = buffer[i * 3 + 1];
        coords[i * 2 + 0] = (s16)dc_iround(x);
        coords[i * 2 + 1] = (s16)dc_iround(y);
    }

    unsigned char* prev = polyBackBuffer;
    polyBackBuffer = frontBuffer;
    fillpoly(coords, numPoint, color);
    polyBackBuffer = prev;
}

void osystem_draw3dLine(float x1, float y1, float z1, float x2, float y2, float z2, unsigned char color)
{
#ifdef USE_PVR_PAL8
    // PAL8 path: fall back to software line into frontBuffer so it composes into the background.
    line(dc_iround(x1), dc_iround(y1), dc_iround(x2), dc_iround(y2), color);
#else
    // GLdc path: batch line.
    RGL_AddLine(x1, y1, z1, x2, y2, z2, color, 255);
#endif
}

void osystem_draw3dQuad(float x1, float y1, float /*z1*/, float x2, float y2, float /*z2*/, float x3, float y3, float /*z3*/, float x4, float y4, float /*z4*/, unsigned char color, int /*transparency*/)
{
    // Mostly used by debug helpers; outline is sufficient here.
    osystem_draw3dLine(x1, y1, 0, x2, y2, 0, color);
    osystem_draw3dLine(x2, y2, 0, x3, y3, 0, color);
    osystem_draw3dLine(x3, y3, 0, x4, y4, 0, color);
    osystem_draw3dLine(x4, y4, 0, x1, y1, 0, color);
}

static void dc_fill_circle(int cx, int cy, int r, unsigned char color)
{
    if (r <= 0)
        return;

    unsigned char* prev = polyBackBuffer;
    polyBackBuffer = frontBuffer;

    // Simple scanline circle fill.
    for (int dy = -r; dy <= r; ++dy)
    {
        const int y = cy + dy;
        const int dxMax = (int)std::floor(std::sqrt((double)r * (double)r - (double)dy * (double)dy));
        hline(cx - dxMax, cx + dxMax, y, color);
    }

    polyBackBuffer = prev;
}

void osystem_drawSphere(float X, float Y, float Z, u8 color, u8 /*material*/, float size)
{
#ifndef USE_PVR_PAL8
    // Approximate spheres as points for now (fast path).
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

void osystem_flushPendingPrimitives()
{
    // Nothing to do: primitives are drawn directly into frontBuffer.
}

int osystem_playTrack(int) { return 0; }
void osystem_playAdlib()
{
    ensure_sound();

    if (!g_streamInited)
        return;

    if (g_adlibStream == SND_STREAM_INVALID)
    {
        g_adlibStream = snd_stream_alloc(dc_adlib_stream_cb, kAdlibBufBytes);
        if (g_adlibStream == SND_STREAM_INVALID)
            return;
    }

    // Ensure the synth thread is running and pre-buffer some audio before the
    // stream starts pulling, to avoid startup underruns.
    ensure_adlib_synth_thread();
    {
        const uint64_t t0 = timer_ms_gettime64();
        const uint32_t primeSamples = 4096;
        while (dc_ring_avail(g_adlibRingRead, g_adlibRingWrite) < primeSamples)
        {
            if (timer_ms_gettime64() - t0 > 250)
                break;
            thd_sleep(1);
        }
    }

    // Start streaming the OPL synth output.
    if (!g_adlibStreamStarted)
    {
        snd_stream_start(g_adlibStream, kAdlibHz, 0 /*mono*/);
        g_adlibStreamStarted = true;

        // Keep the stream fed even if the main thread stalls.
        ensure_adlib_poll_thread();
        // Prime immediately.
        snd_stream_poll(g_adlibStream);
    }

    // Map engine 0..0x7F volume to KOS 0..255.
    int vol = musicVolume * 2;
    if (vol < 0) vol = 0;
    if (vol > 255) vol = 255;
    snd_stream_volume(g_adlibStream, vol);
}
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
            dc_set_sfx_debug_msg("I_Warning: SFX parse failed (VOC too small)");
            return;
        }

        if ((unsigned char)samplePtr[26] != 1)
        {
            dc_set_sfx_debug_msg("I_Warning: SFX parse failed (VOC block!=1)");
            return;
        }

        // Block header: [type:1][size:3] (24-bit LE), then block data.
        const uint32_t blockSize24 = (uint32_t)((unsigned char)samplePtr[27]) |
                                    ((uint32_t)((unsigned char)samplePtr[28]) << 8) |
                                    ((uint32_t)((unsigned char)samplePtr[29]) << 16);
        // For sound data blocks: data contains [freq_div:1][codec:1][samples...]
        if (blockSize24 < 2)
        {
            dc_set_sfx_debug_msg("I_Error: SFX parse failed (VOC block size)");
            return;
        }

        const int sampleBytes = (int)blockSize24 - 2;
        const unsigned char frequencyDiv = (unsigned char)samplePtr[30];
        const unsigned char codecId = (unsigned char)samplePtr[31];
        (void)codecId;

        const char* sampleData = samplePtr + 32;
        if (32 + sampleBytes > size)
        {
            dc_set_sfx_debug_msg("I_Warning: SFX parse failed (VOC bounds)");
            return;
        }

        const int sampleRate = 1000000 / (256 - (int)frequencyDiv);

        // KOS requires raw buffers be padded to 32 bytes per channel.
        size_t paddedLen = (size_t)sampleBytes;
        paddedLen = (paddedLen + 31u) & ~31u;

        std::vector<char> tmp(paddedLen, 0);
        fitd_memcpy(tmp.data(), sampleData, (size_t)sampleBytes);

        // VOC PCM is unsigned 8-bit. The AICA's 8-bit PCM behaves like signed
        // in practice; converting avoids the heavy DC offset distortion.
        for (size_t i = 0; i < (size_t)sampleBytes; ++i)
        {
            tmp[i] = (char)(((unsigned char)tmp[i]) ^ 0x80);
        }

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
