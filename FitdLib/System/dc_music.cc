//----------------------------------------------------------------------------
//  Dream in the Dark Music Streaming Support
//  ADL/OPL music streaming via KOS snd_stream
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

#include "System/dc_music.h"
#include "System/i_sound_dc.h"
#include "vars.h"

#include <array>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdio>

#include <dc/sound/stream.h>

extern "C" {
#include <kos/dbgio.h>
}

#include <kos/thread.h>

// Engine callback.
extern "C" int musicUpdate(void* udata, uint8* stream, int len);

//------------------------------------------------------------------------------
// Dream in the Dark Music Streaming Support
// ADL/OPL music streaming via KOS snd_stream
//------------------------------------------------------------------------------
static snd_stream_hnd_t g_adlibStream = SND_STREAM_INVALID;
static bool g_adlibStreamStarted = false;

static volatile bool g_adlibPollThreadRun = false;
static kthread_t* g_adlibPollThread = nullptr;

static volatile bool g_adlibSynthThreadRun = false;
static kthread_t* g_adlibSynthThread = nullptr;

// Bumped when a new song is loaded so the synth thread can drop any buffered
// audio from the previous track (prevents "out of order" playback).
static volatile uint32_t g_adlibSongSerial = 0;

// Flushing the ring on song change must be coordinated with both producer
// (synth thread) and consumer (snd_stream callback) to avoid corruption and
// accidental "old track" bleed.
static volatile bool g_adlibFlushRequested = false;
static volatile int g_adlibRingLock = 0;

// AICA output is easy to overdrive; keep some headroom.
static constexpr int kAdlibAicaVolMax = 200; // <= 255
// Additional fixed attenuation on PCM to reduce harsh clipping.
static constexpr int kAdlibPcmAttenShift = 1; // 1 => -6dB

static inline void dc_adlib_ring_lock()
{
    while (__sync_lock_test_and_set(&g_adlibRingLock, 1))
        thd_pass();
}

static inline void dc_adlib_ring_unlock()
{
    __sync_lock_release(&g_adlibRingLock);
}

// Empirically, on this target `snd_stream_start(freq, ...)` plays at ~2x the
// requested frequency (e.g. 22050 -> ~44100, 44100 -> ~88200). We pass half
// the desired output rate so the effective playback rate is ~44100Hz.
static constexpr uint32_t kAdlibHz = 22050;
// Buffer size is per-channel bytes; 16-bit mono => bytes = samples * 2.
static constexpr int kAdlibBufBytes = 16 * 1024;
static uint8_t g_adlibPcmBuf[kAdlibBufBytes] __attribute__((aligned(32)));

// Ring buffer for pre-generated PCM.
static constexpr uint32_t kAdlibRingSamples = 65536; // ~1.48s @ 44100Hz
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

static FORCEINLINE void dc_adlib_ring_flush_locked()
{
    g_adlibRingRead = 0;
    g_adlibRingWrite = 0;
}

static FORCEINLINE void dc_adlib_ring_write_chunk(const int16_t* src, uint32_t samples)
{
    dc_adlib_ring_lock();

    uint32_t remaining = samples;
    while (remaining > 0)
    {
        const uint32_t rd = g_adlibRingRead;
        uint32_t wr = g_adlibRingWrite;
        const uint32_t space = dc_ring_space(rd, wr);
        if (space == 0)
            break;

        uint32_t toWrite = remaining;
        if (toWrite > space)
            toWrite = space;

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

    dc_adlib_ring_unlock();
}

static FORCEINLINE void dc_adlib_upsample_x2(const int16_t* in, int16_t* out, uint32_t inSamples)
{
    // Cheap 2x upsample (linear). Reduces harsh imaging vs nearest-neighbor,
    // while remaining very low CPU.
    if (inSamples == 0)
        return;

    for (uint32_t i = 0; i + 1 < inSamples; i++)
    {
        const int16_t a = in[i];
        const int16_t b = in[i + 1];

        // Apply a small attenuation to avoid distortion on AICA.
        out[i * 2 + 0] = (int16_t)((int)a >> kAdlibPcmAttenShift);
        out[i * 2 + 1] = (int16_t)((((int)a + (int)b) / 2) >> kAdlibPcmAttenShift);
    }

    // Last sample: hold.
    const int16_t last = in[inSamples - 1];
    const int16_t lastA = (int16_t)((int)last >> kAdlibPcmAttenShift);
    out[(inSamples - 1) * 2 + 0] = lastA;
    out[(inSamples - 1) * 2 + 1] = lastA;
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

    while (need > 0)
    {
        dc_adlib_ring_lock();
        const uint32_t rd0 = g_adlibRingRead;
        const uint32_t wr0 = g_adlibRingWrite;
        const uint32_t avail = dc_ring_avail(rd0, wr0);

        if (avail == 0)
        {
            dc_adlib_ring_unlock();
            break;
        }

        uint32_t rd = rd0;
        uint32_t chunk = (uint32_t)need;
        if (chunk > avail)
            chunk = avail;

        const uint32_t tillWrap = kAdlibRingSamples - rd;
        if (chunk > tillWrap)
            chunk = tillWrap;

        fitd_memcpy(out, &g_adlibRing[rd], (size_t)chunk * 2);

        rd += chunk;
        if (rd >= kAdlibRingSamples)
            rd = 0;

        g_adlibRingRead = rd;
        dc_adlib_ring_unlock();

        out += chunk;
        need -= (int)chunk;
    }

    // If underrun, zero-fill remainder.
    if (need > 0)
        fitd_memset(out, 0, (size_t)need * 2);

    *smp_recv = samples;
    return g_adlibPcmBuf;
}

static void* dc_adlib_poll_thread(void* /*param*/)
{
    thd_set_label(thd_get_current(), "adlib_stream");

    while (g_adlibPollThreadRun)
    {
        if (g_adlibStreamStarted && g_adlibStream != SND_STREAM_INVALID)
            snd_stream_poll(g_adlibStream);

        thd_sleep(5);
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

    thd_set_prio(g_adlibPollThread, 10);
}

static void* dc_adlib_synth_thread(void* /*param*/)
{
    thd_set_label(thd_get_current(), "adlib_synth");

    uint32_t localSerial = g_adlibSongSerial;

    // YM3812 runs at 22050Hz; AICA consumes ~44100Hz, so upsample 2x.
    static constexpr int kOutChunkSamples = 1024;
    static constexpr int kInChunkSamples = kOutChunkSamples / 2;
    static constexpr int kInChunkBytes = kInChunkSamples * 2;
    static uint8_t s_inBuf[kInChunkBytes] __attribute__((aligned(32)));
    static int16_t s_outBuf[kOutChunkSamples] __attribute__((aligned(32)));

    while (g_adlibSynthThreadRun)
    {
        const uint32_t serialNow = g_adlibSongSerial;
        if (g_adlibFlushRequested || serialNow != localSerial)
        {
            dc_adlib_ring_lock();
            dc_adlib_ring_flush_locked();
            dc_adlib_ring_unlock();
            g_adlibFlushRequested = false;
            localSerial = serialNow;
        }

        if (g_adlibStream == SND_STREAM_INVALID)
        {
            thd_sleep(10);
            continue;
        }

        const uint32_t avail = dc_ring_avail(g_adlibRingRead, g_adlibRingWrite);

        static constexpr uint32_t kLowWater = 16384;
        static constexpr uint32_t kHighWater = 49152;

        if (avail >= kHighWater)
        {
            thd_sleep(10);
            continue;
        }

        if (avail >= kLowWater)
            thd_sleep(2);

        int chunks = 1;
        if (avail < kLowWater)
        {
            const uint32_t deficit = kLowWater - avail;
            chunks = 1 + (int)((deficit + (uint32_t)kOutChunkSamples - 1) / (uint32_t)kOutChunkSamples);
            if (chunks > 12) chunks = 12;
        }

        while (g_adlibSynthThreadRun && chunks-- > 0)
        {
            const uint32_t space = dc_ring_space(g_adlibRingRead, g_adlibRingWrite);
            if (space < (uint32_t)kOutChunkSamples)
                break;

            fitd_memset(s_inBuf, 0, (size_t)kInChunkBytes);
            musicUpdate(nullptr, s_inBuf, kInChunkBytes);

            const uint32_t serialAfter = g_adlibSongSerial;
            if (serialAfter != localSerial || g_adlibFlushRequested)
            {
                dc_adlib_ring_lock();
                dc_adlib_ring_flush_locked();
                dc_adlib_ring_unlock();
                g_adlibFlushRequested = false;
                localSerial = serialAfter;
                continue;
            }

            dc_adlib_upsample_x2((const int16_t*)s_inBuf, s_outBuf, (uint32_t)kInChunkSamples);
            dc_adlib_ring_write_chunk(s_outBuf, (uint32_t)kOutChunkSamples);
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

    thd_set_prio(g_adlibSynthThread, 10);
}

void dc_adlib_notify_song_changed()
{
    const uint32_t serial = g_adlibSongSerial;
    g_adlibSongSerial = serial + 1;
    g_adlibFlushRequested = true;
}

void dc_music_poll()
{
    // If the dedicated poll thread is running, never poll from the render thread.
    if (g_adlibPollThreadRun)
        return;

    if (g_adlibStreamStarted && g_adlibStream != SND_STREAM_INVALID)
        snd_stream_poll(g_adlibStream);
}

void dc_music_play_adlib(int musicVolume)
{
    dc_sound_ensure();

    if (!dc_sound_stream_inited())
        return;

    if (g_adlibStream == SND_STREAM_INVALID)
    {
        g_adlibStream = snd_stream_alloc(dc_adlib_stream_cb, kAdlibBufBytes);
        if (g_adlibStream == SND_STREAM_INVALID)
            return;
    }

    // Prime a small amount synchronously so the first callback doesn't underrun.
    if (!g_adlibStreamStarted)
    {
        dc_adlib_ring_lock();
        dc_adlib_ring_flush_locked();
        dc_adlib_ring_unlock();
        g_adlibFlushRequested = false;

        static constexpr int kPrimeOutSamples = 1024;
        static constexpr int kPrimeInSamples = kPrimeOutSamples / 2;
        static constexpr int kPrimeInBytes = kPrimeInSamples * 2;
        static uint8_t s_primeIn[kPrimeInBytes] __attribute__((aligned(32)));
        static int16_t s_primeOut[kPrimeOutSamples] __attribute__((aligned(32)));

        const uint32_t primeSamples = 16384;
        int tries = 0;
        const int maxTries = 32;
        while (dc_ring_avail(g_adlibRingRead, g_adlibRingWrite) < primeSamples && tries++ < maxTries)
        {
            fitd_memset(s_primeIn, 0, (size_t)kPrimeInBytes);
            musicUpdate(nullptr, s_primeIn, kPrimeInBytes);
            dc_adlib_upsample_x2((const int16_t*)s_primeIn, s_primeOut, (uint32_t)kPrimeInSamples);
            dc_adlib_ring_write_chunk(s_primeOut, (uint32_t)kPrimeOutSamples);
        }
    }

    if (!g_adlibStreamStarted)
    {
        snd_stream_start(g_adlibStream, kAdlibHz, 0 /*mono*/);
        g_adlibStreamStarted = true;

        ensure_adlib_poll_thread();
        snd_stream_poll(g_adlibStream);
    }

    ensure_adlib_synth_thread();

    int vol = musicVolume * 2;
    if (vol < 0) vol = 0;
    if (vol > 255) vol = 255;
    if (vol > kAdlibAicaVolMax) vol = kAdlibAicaVolMax;
    snd_stream_volume(g_adlibStream, vol);
}

#endif // DREAMCAST
