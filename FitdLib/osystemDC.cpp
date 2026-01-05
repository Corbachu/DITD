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

#include "System/i_sound_dc.h"
#include "System/dc_music.h"

#include "System/i_video_dc.h"

#include "System/dc_palette.h"
#include "System/r_units.h"
#include "System/r_render.h"

#include <dc/maple.h>
#include <dc/maple/controller.h>

extern "C" {
#include <kos/dbgio.h>
}

#include <kos/thread.h>

#include "System/dc_timing.h"

// Start building a native PVR paletted path (PAL8)
// Enable with -DUSE_PVR_PAL8 to switch from GLdc blit to PVR renderer.
// Sound/music moved into System/i_sound_dc.cc + System/dc_music.cc

static bool g_renderDebugHud = true;
static int g_renderDebugCableType = -1;

// Software 2D raster helpers used by the engine (8-bit indexed into frontBuffer).
extern void fillpoly(s16* datas, int n, unsigned char c);
extern void line(int x1, int y1, int x2, int y2, unsigned char c);
extern void hline(int x1, int x2, int y, unsigned char c);
extern unsigned char* polyBackBuffer;

// Shared Dreamcast palette state.
std::array<u8, 256 * 3> g_palette = {};
std::array<u16, 256> g_palette565 = {};

static bool g_glInited = false;



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

unsigned char frontBuffer[320 * 200] = {0};
unsigned char physicalScreen[320 * 200] = {0};
std::array<unsigned char, 320 * 200> uiLayer = {0};

void osystem_init()
{
    dbgio_printf("[FitdMain] Initialising DC startup!\n");
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

    // NOTE: Forcing full-frame scanout can cause cropping/offset issues on FlyCast.
#ifdef DC_FORCE_FULLFRAME_SCANOUT
    dc_force_fullframe_scanout_if_needed(cable);
#endif

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
    dc_video_gl_init();
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

#ifndef USE_PVR_PAL8
// Uploading the 320x200 composited texture every present is expensive on GLdc.
// Track whether the background/palette/UI changed since last upload.
static bool g_dcCompositedDirty = true;
static bool g_dcUiHadNonZeroLastUpload = false;
#endif

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
    // Texture upload is expensive on GLdc; skip when nothing changed.
    // uiLayer is composited into the texture, so we must detect when UI pixels
    // appear/disappear even if the background stayed the same.
    bool uiHasNonZero = false;

    if (!g_dcCompositedDirty)
    {
        for (const u8 px : uiLayer)
        {
            if (px != 0)
            {
                uiHasNonZero = true;
                break;
            }
        }

        if (uiHasNonZero != g_dcUiHadNonZeroLastUpload)
            g_dcCompositedDirty = true;
    }

    if (g_dcCompositedDirty)
    {
        if (!uiHasNonZero)
        {
            for (const u8 px : uiLayer)
            {
                if (px != 0)
                {
                    uiHasNonZero = true;
                    break;
                }
            }
        }

        dc_video_gl_upload_composited();
        g_dcUiHadNonZeroLastUpload = uiHasNonZero;
        g_dcCompositedDirty = false;
    }
#endif
}

#ifndef USE_PVR_PAL8
// Dreamcast-specific: some loops present 2D-only screens (menus, fades) without
// re-rendering 3D. Keep a per-present flag so we can avoid drawing stale 3D
// geometry from a previous screen.
// True once something has started submitting 3D overlay primitives for the
// current present. This gates per-frame clearing of RGL batches so we don't
// accumulate geometry across frames (OOM) and also don't wipe it in tight
// process_events() loops.
bool g_dcExpect3DOverlayThisFrame = false;

// Track engine "logic frames" so we can reliably begin (clear) RGL batches once
// per tick, even if some paths render multiple times before an explicit present.
// This prevents unbounded growth of g_rglTriVtx/g_rglLineVtx (OOM).
static uint32_t s_dcLogicFrameId = 0;
static uint32_t s_dcRglBegunFrameId = 0xFFFFFFFFu;

// Called by the GLdc batching helpers on the first primitive submission of a
// given logic frame.
void dc_rgl_ensure_frame_started()
{
    if (s_dcRglBegunFrameId != s_dcLogicFrameId)
    {
        osystem_cleanScreenKeepZBuffer();
        s_dcRglBegunFrameId = s_dcLogicFrameId;
    }
}

// Track the active clip region (set by SetClip/osystem_setClip) so background
// masks can be clipped to actor bounding boxes.
static bool g_dcClipActive = false;
static int g_dcClipX1 = 0;
static int g_dcClipY1 = 0;
static int g_dcClipX2 = 320;
static int g_dcClipY2 = 200;
#endif

u32 osystem_startOfFrame()
{
    // Frame pacing: keep the engine logic deterministic.
    // Returning >1 here causes the game to advance multiple logic ticks in a
    // single rendered frame, which can make fades/text feel like they vanish.
    
    // Without throttling here, Dreamcast will run as fast as the CPU allows,
    // causing cutscenes, book pages, and gameplay timing to fly by.
    // 30 FPS matches DC-friendly pacing better than 25 on this port.
    constexpr uint32_t kFramesPerSecond = (uint32_t)DC_LOGIC_FPS;
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

    // Count logic ticks. Used by dc_rgl_ensure_frame_started() to prevent
    // unbounded RGL batch growth when multiple ticks occur without a present.
    s_dcLogicFrameId++;
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
                I_Printf("[rgl] tris:%d lines:%d z:[%.1f..%.1f] t0:%d t1:%d t2:%d t3:%d tri_vec:%p line_vec:%p\n",
                         tris, lines, zMin, zMax,
                         g_rglPolyTypeCounts[0], g_rglPolyTypeCounts[1], g_rglPolyTypeCounts[2], g_rglPolyTypeCounts[3],
                         (void*)&g_rglTriVtx,
                         (void*)&g_rglLineVtx);
            else
                I_Printf("[rgl] tris:%d lines:%d z:(none) t0:%d t1:%d t2:%d t3:%d tri_vec:%p line_vec:%p\n",
                         tris, lines,
                         g_rglPolyTypeCounts[0], g_rglPolyTypeCounts[1], g_rglPolyTypeCounts[2], g_rglPolyTypeCounts[3],
                         (void*)&g_rglTriVtx,
                         (void*)&g_rglLineVtx);
#else
            I_Printf("[rgl] (pvr) tris:%d\n", (int)g_pvrTris.size());
#endif
        }
    }

    // If we had an SFX load/parse failure recently, render a tiny overlay message.
    // We draw into uiLayer so it goes through the normal 320x200 compositing path.
    const char* sfxMsg = dc_sound_debug_msg();
    if (sfxMsg && PtrFont)
    {
        g_dcCompositedDirty = true;
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
        PrintFont(x + 1, y + 1, (char*)uiLayer.data(), (u8*)sfxMsg);
        SetFont(PtrFont, 255);
        PrintFont(x, y, (char*)uiLayer.data(), (u8*)sfxMsg);
        dc_sound_debug_msg_tick();
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

    dc_video_gl_present();

    // Always clear after presenting. This avoids re-drawing stale geometry on
    // subsequent 2D-only presents and keeps memory bounded.
    RGL_BeginFrame();
    dc_video_gl_clear_mask_queue();

    // Force a re-begin if any primitives are submitted before the next
    // osystem_startOfFrame() tick.
    s_dcRglBegunFrameId = 0xFFFFFFFFu;

    // Reset for the next present.
    g_dcExpect3DOverlayThisFrame = false;
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

#ifndef USE_PVR_PAL8
    g_dcCompositedDirty = true;
#endif
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

#ifndef USE_PVR_PAL8
    g_dcCompositedDirty = true;
#endif
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

#ifndef USE_PVR_PAL8
    g_dcCompositedDirty = true;
#endif
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

void osystem_createMask(const std::array<u8, 320 * 200>& mask, int roomId, int maskId,
                        int maskX1, int maskY1, int maskX2, int maskY2)
{
#ifndef USE_PVR_PAL8
    dc_video_gl_create_mask(mask.data(), roomId, maskId, maskX1, maskY1, maskX2, maskY2);
#else
    (void)mask; (void)roomId; (void)maskId; (void)maskX1; (void)maskY1; (void)maskX2; (void)maskY2;
#endif
}

void osystem_drawMask(int roomId, int maskId)
{
    if (g_gameId == TIMEGATE)
        return;

#ifndef USE_PVR_PAL8
    dc_video_gl_queue_mask_draw(roomId, maskId,
                                g_dcClipActive,
                                g_dcClipX1, g_dcClipY1, g_dcClipX2, g_dcClipY2);
#else
    (void)roomId; (void)maskId;
#endif
}

void osystem_startFrame() {}
void osystem_stopFrame() { osystem_endOfFrame(); }
void osystem_setClip(float left, float top, float right, float bottom)
{
#ifndef USE_PVR_PAL8
    g_dcClipActive = true;

    // Engine clip is specified with inclusive right/bottom (320x200 space).
    // For masks, we want half-open [x1,x2) and add a 1px safety border
    // (matches the BGFX backend's scissor expansion).
    const int lx = (int)std::lround(left);
    const int ty = (int)std::lround(top);
    const int rx = (int)std::lround(right);
    const int by = (int)std::lround(bottom);

    g_dcClipX1 = std::max(0, std::min(320, lx - 1));
    g_dcClipY1 = std::max(0, std::min(200, ty - 1));
    g_dcClipX2 = std::max(0, std::min(320, rx + 1));
    g_dcClipY2 = std::max(0, std::min(200, by + 1));
#else
    (void)left; (void)top; (void)right; (void)bottom;
#endif
}

void osystem_clearClip()
{
#ifndef USE_PVR_PAL8
    g_dcClipActive = false;
    g_dcClipX1 = 0;
    g_dcClipY1 = 0;
    g_dcClipX2 = 320;
    g_dcClipY2 = 200;
#endif
}
void osystem_cleanScreenKeepZBuffer()
{
#ifndef USE_PVR_PAL8
    // Clear queued 3D primitives. This must NOT live in osystem_startOfFrame(),
    // because process_events() calls startOfFrame in tight loops (fades/menus)
    // and would wipe out geometry before it gets presented.
    g_dcExpect3DOverlayThisFrame = true;
    RGL_BeginFrame();
    dc_video_gl_clear_mask_queue();
#endif
}

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

    // Start a 3D overlay pass on first submission this present.
    // Some gameplay paths submit 3D without explicitly calling
    // osystem_cleanScreenKeepZBuffer(); without this, batches either get wiped
    // at endOfFrame (missing 3D) or accumulate across frames (OOM).
    if (!g_dcExpect3DOverlayThisFrame)
        osystem_cleanScreenKeepZBuffer();

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

        // Keep the original behavior: per-vertex shade mapping.
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

void osystem_flushPendingPrimitives()
{
    // Nothing to do: primitives are drawn directly into frontBuffer.
}

int osystem_playTrack(int) { return 0; }
void osystem_playAdlib()
{
    dc_music_play_adlib(musicVolume);
}
void osystem_playSample(char* samplePtr, int size)
{
    dc_sound_play_sample(samplePtr, size);
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
