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
#include <algorithm>
#include <cstring>
#include <cstdio>

#include <dc/sound/stream.h>

extern "C" {
#include <kos/dbgio.h>
}

#include <kos/thread.h>
#include <arch/timer.h>

// Engine callback.
extern "C" int musicUpdate(void* udata, uint8* stream, int len);

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
static volatile const char* g_adlibFlushReason = nullptr;
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

// Empirically, KOS's `snd_stream` callback is being driven at ~44.1k samples/sec
// on this target. Run the whole path at 44100Hz to keep pitch/tempo correct.
static constexpr uint32_t kAdlibHz = 44100;

// Stream callback cadence depends on how often we poll and internal buffering.
// It's useful for debug, but do not use it to change resampling at runtime.
// Buffer size is per-channel bytes; 16-bit mono => bytes = samples * 2.
static constexpr int kAdlibBufBytes = 16 * 1024;
static uint8_t g_adlibPcmBuf[kAdlibBufBytes] __attribute__((aligned(32)));

// Ring buffer for pre-generated PCM.
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

static FORCEINLINE void dc_adlib_ring_flush_locked()
{
    static uint32_t s_flushSeq = 0;
    ++s_flushSeq;
    const char* reason = (const char*)g_adlibFlushReason;
    const uint32_t rd = g_adlibRingRead;
    const uint32_t wr = g_adlibRingWrite;
    const uint32_t avail = dc_ring_avail(rd, wr);
    dbgio_printf("[dc] [adlib] flush #%lu reason=%s avail=%lu rd=%lu wr=%lu\n",
            (unsigned long)s_flushSeq,
            reason ? reason : "(none)",
            (unsigned long)avail,
            (unsigned long)rd,
            (unsigned long)wr);
    g_adlibFlushReason = nullptr;

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

static FORCEINLINE void dc_adlib_atten(const int16_t* in, int16_t* out, uint32_t samples)
{
    for (uint32_t i = 0; i < samples; ++i)
        out[i] = (int16_t)(((int)in[i]) >> kAdlibPcmAttenShift);
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

    // KOS passes/receives BYTES here (despite the header naming).
    // For 16-bit mono, samples = bytes / 2.
    int bytes = smp_req;
    if (bytes > kAdlibBufBytes)
        bytes = kAdlibBufBytes;
    if (bytes < 0)
        bytes = 0;
    bytes &= ~1; // keep 16-bit aligned

    const int samples = bytes / 2;

    int16_t* out = (int16_t*)g_adlibPcmBuf;
    int needSamples = samples;

    // Debug-only: log how much data KOS requests per second.
    // This is NOT guaranteed to equal the actual playback rate.
    static uint64_t s_lastMs = 0;
    static uint32_t s_accumBytes = 0;
    static uint32_t s_accumCalls = 0;
    const uint64_t nowMs = timer_ms_gettime64();
    if (s_lastMs == 0)
        s_lastMs = nowMs;
    if (smp_req > 0)
    {
        s_accumBytes += (uint32_t)smp_req;
        s_accumCalls++;
    }
    const uint64_t dtMs = nowMs - s_lastMs;
    if (dtMs >= 1000)
    {
        const uint32_t bytesPerSec = (uint32_t)((uint64_t)s_accumBytes * 1000ULL / (dtMs ? dtMs : 1));
        const uint32_t samplesPerSec = bytesPerSec / 2;
        const uint32_t callsPerSec = (uint32_t)((uint64_t)s_accumCalls * 1000ULL / (dtMs ? dtMs : 1));
        s_accumBytes = 0;
        s_accumCalls = 0;
        s_lastMs = nowMs;

        const uint32_t avail = dc_ring_avail(g_adlibRingRead, g_adlibRingWrite);
        dbgio_printf("[dc] [music] stream stats: req=%dB calls/s~%u reqB/s~%u reqS/s~%u ring_avail=%u\n",
                     smp_req,
                     (unsigned int)callsPerSec,
                     (unsigned int)bytesPerSec,
                     (unsigned int)samplesPerSec,
                     (unsigned int)avail);
    }

    while (needSamples > 0)
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
        uint32_t chunk = (uint32_t)needSamples;
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
        needSamples -= (int)chunk;
    }

    // If underrun, zero-fill remainder.
    if (needSamples > 0)
        fitd_memset(out, 0, (size_t)needSamples * 2);

    *smp_recv = bytes;
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

    // YM3812 generates at 44100Hz; keep the stream at 44100Hz (no resampling).
    static constexpr int kChunkSamples = 1024;
    static constexpr int kChunkBytes = kChunkSamples * 2;
    static int16_t s_out[kChunkSamples] __attribute__((aligned(32)));

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
            chunks = 1 + (int)((deficit + (uint32_t)kChunkSamples - 1) / (uint32_t)kChunkSamples);
            if (chunks > 12) chunks = 12;
        }

        while (g_adlibSynthThreadRun && chunks-- > 0)
        {
            const uint32_t space = dc_ring_space(g_adlibRingRead, g_adlibRingWrite);
            if (space < (uint32_t)kChunkSamples)
                break;

            fitd_memset(s_out, 0, (size_t)kChunkBytes);
            musicUpdate(nullptr, (uint8*)s_out, kChunkBytes);

            const uint32_t serialAfter = g_adlibSongSerial;
            if (serialAfter != localSerial || g_adlibFlushRequested)
            {
				g_adlibFlushReason = (serialAfter != localSerial) ? "song_serial_change" : "flush_requested";
                dc_adlib_ring_lock();
                dc_adlib_ring_flush_locked();
                dc_adlib_ring_unlock();
                g_adlibFlushRequested = false;
                localSerial = serialAfter;
                continue;
            }

            dc_adlib_atten(s_out, s_out, (uint32_t)kChunkSamples);
            dc_adlib_ring_write_chunk(s_out, (uint32_t)kChunkSamples);
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
    g_adlibFlushReason = "song_change";
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
		g_adlibFlushReason = "prime_start";
        dc_adlib_ring_flush_locked();
        dc_adlib_ring_unlock();
        g_adlibFlushRequested = false;

        static constexpr int kPrimeSamples = 1024;
        static constexpr int kPrimeBytes = kPrimeSamples * 2;
        static int16_t s_prime[kPrimeSamples] __attribute__((aligned(32)));

        const uint32_t primeSamples = 16384;
        int tries = 0;
        const int maxTries = 32;
        while (dc_ring_avail(g_adlibRingRead, g_adlibRingWrite) < primeSamples && tries++ < maxTries)
        {
            fitd_memset(s_prime, 0, (size_t)kPrimeBytes);
            musicUpdate(nullptr, (uint8*)s_prime, kPrimeBytes);
            dc_adlib_atten(s_prime, s_prime, (uint32_t)kPrimeSamples);
            dc_adlib_ring_write_chunk(s_prime, (uint32_t)kPrimeSamples);
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
