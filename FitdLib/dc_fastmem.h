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

// Uses libfastmem on Dreamcast (via KOS-ports) when available.
// Provides fm_memcpy / fm_memset with safe fallbacks elsewhere.
#include "memshim.h"

#include <cstring>

static inline void *fitd_memcpy(void *dst, const void *src, size_t n) {
#ifdef DREAMCAST
    return fm_memcpy(dst, src, n);
#else
    return std::memcpy(dst, src, n);
#endif
}

static inline void *fitd_memset(void *dst, int c, size_t n) {
#ifdef DREAMCAST
    return fm_memset(dst, c, n);
#else
    return std::memset(dst, c, n);
#endif
}
