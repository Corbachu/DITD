//----------------------------------------------------------------------------
//  Dream In The Dark dc fastmem (Dreamcast / KOS)
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

#pragma once

// Dreamcast SH-4 fast memory helpers.
//
// This is the "aggressive" path: we opportunistically use the SH-4 Store Queue
// API (KOS: <dc/sq.h>) for large copies/sets when alignment allows.
//
// We intentionally do NOT override libc memcpy/memset globally; instead call
// fitd_memcpy/fitd_memset in hotspots.

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef DREAMCAST
extern "C" {
#include <dc/sq.h>
}
#endif

static inline void *fitd_memcpy(void *dst, const void *src, size_t n) {
#ifdef DREAMCAST
    auto *d = static_cast<uint8_t *>(dst);
    auto *s = static_cast<const uint8_t *>(src);

    size_t remaining = n;

    // Small copies: libc is fine.
    if (remaining >= 128) {
        // Align dest to 32 bytes for SQ.
        uintptr_t dmis = reinterpret_cast<uintptr_t>(d) & 31u;
        if (dmis) {
            size_t lead = 32u - static_cast<size_t>(dmis);
            if (lead > remaining) lead = remaining;
            std::memcpy(d, s, lead);
            d += lead;
            s += lead;
            remaining -= lead;
        }

        // SQ requires src at least 4-byte aligned; 8-byte aligned uses fast path.
        if ((reinterpret_cast<uintptr_t>(s) & 3u) == 0u) {
            size_t bulk = remaining & ~static_cast<size_t>(31u);
            if (bulk) {
                sq_cpy(d, s, bulk);
                d += bulk;
                s += bulk;
                remaining -= bulk;
            }
        }
    }

    if (remaining) {
        std::memcpy(d, s, remaining);
    }

    return dst;
#else
    return std::memcpy(dst, src, n);
#endif
}

static inline void *fitd_memset(void *dst, int c, size_t n) {
#ifdef DREAMCAST
    auto *d = static_cast<uint8_t *>(dst);

    size_t remaining = n;

    if (remaining >= 128) {
        uintptr_t dmis = reinterpret_cast<uintptr_t>(d) & 31u;
        if (dmis) {
            size_t lead = 32u - static_cast<size_t>(dmis);
            if (lead > remaining) lead = remaining;
            std::memset(d, c, lead);
            d += lead;
            remaining -= lead;
        }

        size_t bulk = remaining & ~static_cast<size_t>(31u);
        if (bulk) {
            // sq_set only uses low 8-bits of c.
            sq_set(d, static_cast<uint32_t>(static_cast<uint8_t>(c)), bulk);
            d += bulk;
            remaining -= bulk;
        }
    }

    if (remaining) {
        std::memset(d, c, remaining);
    }

    return dst;
#else
    return std::memset(dst, c, n);
#endif
}
