//----------------------------------------------------------------------------
//  libSH4simd - SIMD vertex transform pipeline
//  vertex_dc.h
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
//  This is intentionally simple and pipeline-focused, so you can wire it into 
//  your existing PVR submit path.
//  ----------------------------------------------------
#pragma once
#include <cstddef>
#include <cstdint>
#include "simd_array.h"
#include "matrix.h"
#include "matrix_kernels.h"

namespace sh4simd
{
    struct vec3f
    {
        float3 v;

        vec3f() : v{0.f,0.f,0.f} {}
        vec3f(float x, float y, float z) : v{x,y,z} {}

        float &x()       { return v[0]; }
        float &y()       { return v[1]; }
        float &z()       { return v[2]; }
        float  x() const { return v[0]; }
        float  y() const { return v[1]; }
        float  z() const { return v[2]; }
    };

    struct vec4f
    {
        float4 v;

        vec4f() : v{0.f,0.f,0.f,0.f} {}
        vec4f(float x, float y, float z, float w) : v{x,y,z,w} {}

        float &x()       { return v[0]; }
        float &y()       { return v[1]; }
        float &z()       { return v[2]; }
        float &w()       { return v[3]; }

        float  x() const { return v[0]; }
        float  y() const { return v[1]; }
        float  z() const { return v[2]; }
        float  w() const { return v[3]; }
    };

    // Generic DC vertex structure you can adapt to your PVR format.
    struct dc_vertex
    {
        float x, y, z;
        float u, v;
        uint32_t argb;   // or ARGB8888
    };

    // Transform an array of positions by MVP, producing clip-space positions.
    inline void transform_positions(
        dc_vertex       *dst,
        const dc_vertex *src,
        std::size_t      count,
        const mat4f     &mvp)
    {
        for (std::size_t i=0;i<count;i++)
        {
            float4 p{ src[i].x, src[i].y, src[i].z, 1.0f };
            float4 r = mul(mvp, p);

            dst[i] = src[i];
            dst[i].x = r[0];
            dst[i].y = r[1];
            dst[i].z = r[2]; // keep w in separate buffer if you want
        }
    }

    // Same, but with perspective divide (NDC) and simple viewport mapping
    inline void transform_positions_project(
        dc_vertex       *dst,
        const dc_vertex *src,
        std::size_t      count,
        const mat4f     &mvp,
        float            vp_x,
        float            vp_y,
        float            vp_w,
        float            vp_h)
    {
        for (std::size_t i=0;i<count;i++)
        {
            float4 p{ src[i].x, src[i].y, src[i].z, 1.0f };
            float4 r = mul(mvp, p);

            float inv_w = 1.0f / r[3];
            float nx = r[0] * inv_w;
            float ny = r[1] * inv_w;
            float nz = r[2] * inv_w;

            // Map from NDC [-1,1] to viewport
            float sx = vp_x + (nx * 0.5f + 0.5f) * vp_w;
            float sy = vp_y + (-ny * 0.5f + 0.5f) * vp_h; // Y-flip

            dst[i] = src[i];
            dst[i].x = sx;
            dst[i].y = sy;
            dst[i].z = nz;
        }
    }
}