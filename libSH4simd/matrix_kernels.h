//----------------------------------------------------------------------------
//  libSH4simd - SH-4 Matrix Kernels
//  SIMD SH4 Matricies
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
//  This is “SIMD-aware” in the sense that float4 math uses the vectorized ops; you can progressively 
//	replace the loops with more SH-4 assembly if you want to chase every cycle.
//  ----------------------------------------------------------------------------

#pragma once
#include "simd_array.h"
#include "matrix.h"

namespace sh4simd
{
    // vec = mat4 * vec4 (column-major)
    inline float4 mul(const mat4f &m, const float4 &v)
    {
        // Each column is scaled by v component and summed
        float4 result{0.0f, 0.0f, 0.0f, 0.0f};

        for (std::size_t c = 0; c < 4; ++c)
        {
            float s = v[c];
            if (s != 0.0f)
            {
                // load column
                float4 col{
                    m(0,c), m(1,c), m(2,c), m(3,c)
                };
                result += col * s;
            }
        }
        return result;
    }

    // vec3 position transform with implicit w=1
    inline float3 transform_point(const mat4f &m, const float3 &p)
    {
        float4 v{p[0], p[1], p[2], 1.0f};
        float4 r = mul(m, v);
        return float3{r[0], r[1], r[2]};
    }

    // vec3 direction transform (w=0)
    inline float3 transform_direction(const mat4f &m, const float3 &d)
    {
        float4 v{d[0], d[1], d[2], 0.0f};
        float4 r = mul(m, v);
        return float3{r[0], r[1], r[2]};
    }

    // mat4 * mat4 (column-major)
    inline mat4f mul(const mat4f &a, const mat4f &b)
    {
        mat4f r(0.0f);

        for (std::size_t c = 0; c < 4; ++c)
        {
            // column of B
            float4 bc{
                b(0,c), b(1,c), b(2,c), b(3,c)
            };

            // r[:,c] = A * bc
            float4 tmp = mul(a, bc);
            r(0,c) = tmp[0];
            r(1,c) = tmp[1];
            r(2,c) = tmp[2];
            r(3,c) = tmp[3];
        }
        return r;
    }

    // You can later add SH-4 inline asm (ftrv) variants here if you
    // want to bypass GCC and hit the hardware matrix pipeline directly.
}