//----------------------------------------------------------------------------
//  Dream in the Dark Sound Decode/Playback
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

#include "System/i_sound_dc.h"
#include "vars.h"

#include <vector>
#include <cstring>
#include <cstdio>

#include <dc/sound/sound.h>
#include <dc/sound/stream.h>
#include <dc/sound/sfxmgr.h>

extern "C" {
#include <kos/dbgio.h>
}

static bool g_soundInited = false;
static bool g_streamInited = false;

static char g_debugSfxMsg[128] = {0};
static int g_debugSfxMsgFramesLeft = 0;

static void dc_set_sfx_debug_msg(const char* msg)
{
    if (!msg)
        return;

    std::snprintf(g_debugSfxMsg, sizeof(g_debugSfxMsg), "%s", msg);
    g_debugSfxMsg[sizeof(g_debugSfxMsg) - 1] = '\0';
    g_debugSfxMsgFramesLeft = 180;
}

void dc_sound_ensure()
{
    if (g_soundInited)
        return;

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

bool dc_sound_stream_inited()
{
    return g_streamInited;
}

const char* dc_sound_debug_msg()
{
    if (g_debugSfxMsgFramesLeft <= 0)
        return nullptr;
    if (g_debugSfxMsg[0] == '\0')
        return nullptr;
    return g_debugSfxMsg;
}

void dc_sound_debug_msg_tick()
{
    if (g_debugSfxMsgFramesLeft > 0)
    {
        g_debugSfxMsgFramesLeft--;
        if (g_debugSfxMsgFramesLeft <= 0)
            g_debugSfxMsg[0] = '\0';
    }
}

void dc_sound_play_sample(char* samplePtr, int size)
{
    if (!samplePtr || size <= 0)
        return;

    dc_sound_ensure();

    // Keep a small cache so we don't keep re-uploading the same SFX into SPU RAM.
    struct CachedSfx {
        const void* ptr;
        int size;
        sfxhnd_t hnd;
    };
    static std::vector<CachedSfx> cache;
    static constexpr size_t kMaxCached = 48;

    for (auto& e : cache)
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

    if (hnd != SFXHND_INVALID)
    {
        snd_sfx_play(hnd, 255, 128);

        cache.push_back({samplePtr, size, hnd});
        if (cache.size() > kMaxCached)
        {
            // Drop oldest.
            if (cache.front().hnd != SFXHND_INVALID)
                snd_sfx_unload(cache.front().hnd);
            cache.erase(cache.begin());
        }
    }
}

#endif // DREAMCAST
