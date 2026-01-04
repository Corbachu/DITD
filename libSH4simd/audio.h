//----------------------------------------------------------------------------
//  libSH4simd - SIMD audio mixing with saturation
//  
//----------------------------------------------------------------------------
//  Dreamcast SH-4 SIMD-Optimized helpers.
//
//  Copyright (c) 2025  Corbin Annis
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
//  ----------------------------------------------------------------------------
//  If you want a pure-int SIMD version later, you can add simd_array<int32_t,4,16>
//  as an intermediate accumulator and bitwise saturate down.
//  ----------------------------------------------------------------------------

#pragma once
#include <cstddef>
#include <cstdint>
#include "simd_array.h"

namespace sh4simd
{
    // Helper: saturate float -> int16_t
    inline int16_t saturate_to_i16(float x)
    {
        if (x >  32767.0f) x =  32767.0f;
        if (x < -32768.0f) x = -32768.0f;
        return static_cast<int16_t>(x);
    }

    // Mix N groups of 4 samples (e.g. stereo LRLR) from A and B:
    // dst[i] = clamp( srcA[i]*gainA + srcB[i]*gainB )
    inline void mix_i16x4_block(
        sample4i16       *dst,
        const sample4i16 *srcA,
        const sample4i16 *srcB,
        std::size_t       count,
        float             gainA,
        float             gainB)
    {
        for (std::size_t i=0;i<count;i++)
        {
            // convert to float SIMD
            float4 aF;
            float4 bF;

            for (std::size_t j=0;j<4;j++)
            {
                aF[j] = static_cast<float>(srcA[i][j]);
                bF[j] = static_cast<float>(srcB[i][j]);
            }

            float4 mixed = aF * gainA + bF * gainB;

            for (std::size_t j=0;j<4;j++)
                dst[i][j] = saturate_to_i16(mixed[j]);
        }
    }

    // Simple "add with saturation" for int16_t chunks of 4
    inline void add_i16x4_block(
        sample4i16       *dst,
        const sample4i16 *src,
        std::size_t       count)
    {
        for (std::size_t i=0;i<count;i++)
        {
            float4 acc;
            float4 add;

            for (std::size_t j=0;j<4;j++)
            {
                acc[j] = static_cast<float>(dst[i][j]);
                add[j] = static_cast<float>(src[i][j]);
            }

            float4 mixed = acc + add;

            for (std::size_t j=0;j<4;j++)
                dst[i][j] = saturate_to_i16(mixed[j]);
        }
    }
}