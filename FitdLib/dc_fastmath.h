//----------------------------------------------------------------------------
//  Dream In The Dark SH-4 Math Code (Dreamcast / KOS)
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

// Dreamcast SH-4 fast-math helpers.
//
// KOS provides SH-4 math intrinsics in <dc/fmath.h> (fsin/fcos/fsqrt/etc).
// These are generally much faster than libm on the target.

#ifdef DREAMCAST
extern "C" {
#include <dc/fmath.h>
}
#endif

#include <cstdint>
#include <cmath>

#ifdef DREAMCAST
static inline uint32_t fitd_fpscr_get() {
    uint32_t v;
    asm volatile("sts fpscr, %0" : "=r"(v));
    return v;
}

static inline void fitd_fpscr_set(uint32_t v) {
    asm volatile("lds %0, fpscr" : : "r"(v) : "memory");
}

static inline uint32_t fitd_fpscr_force_single() {
    // FPSCR.PR (bit 19): 0=single precision, 1=double precision
    uint32_t old = fitd_fpscr_get();
    uint32_t forced = old & ~(1u << 19);
    if (forced != old) {
        fitd_fpscr_set(forced);
    }
    return old;
}

static inline void fitd_fpscr_restore(uint32_t old) {
    fitd_fpscr_set(old);
}
#endif

static inline float fitd_sinf(float r) {
#ifdef DREAMCAST
    uint32_t old = fitd_fpscr_force_single();
    float out = fsin(r);
    fitd_fpscr_restore(old);
    return out;
#else
    return std::sinf(r);
#endif
}

static inline float fitd_cosf(float r) {
#ifdef DREAMCAST
    uint32_t old = fitd_fpscr_force_single();
    float out = fcos(r);
    fitd_fpscr_restore(old);
    return out;
#else
    return std::cosf(r);
#endif
}

static inline float fitd_sqrtf(float v) {
#ifdef DREAMCAST
    uint32_t old = fitd_fpscr_force_single();
    float out = fsqrt(v);
    fitd_fpscr_restore(old);
    return out;
#else
    return std::sqrtf(v);
#endif
}

static inline double fitd_sin(double r) {
#ifdef DREAMCAST
    return (double)fitd_sinf((float)r);
#else
    return std::sin(r);
#endif
}

static inline double fitd_cos(double r) {
#ifdef DREAMCAST
    return (double)fitd_cosf((float)r);
#else
    return std::cos(r);
#endif
}
