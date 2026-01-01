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

#include <cmath>

static inline float fitd_sinf(float r) {
#ifdef DREAMCAST
    return fsin(r);
#else
    return std::sinf(r);
#endif
}

static inline float fitd_cosf(float r) {
#ifdef DREAMCAST
    return fcos(r);
#else
    return std::cosf(r);
#endif
}

static inline float fitd_sqrtf(float v) {
#ifdef DREAMCAST
    return fsqrt(v);
#else
    return std::sqrtf(v);
#endif
}

static inline double fitd_sin(double r) {
#ifdef DREAMCAST
    return (double)fsin((float)r);
#else
    return std::sin(r);
#endif
}

static inline double fitd_cos(double r) {
#ifdef DREAMCAST
    return (double)fcos((float)r);
#else
    return std::cos(r);
#endif
}
