//----------------------------------------------------------------------------
//  libSH4simd - GLdc + SH-4 SIMD helpers
//  gldc_pipeline.h
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
//  Optimized for GLdc library. You can push these outputs directly through GLdc’s 
//  glVertexPointer/glNormalPointer/glTexCoordPointer workflows
//  ----------------------------------------------------
#pragma once
#include <cstddef>
#include <cstdint>
#include "simd_array.h"
#include "matrix.h"
#include "matrix_kernels.h"

namespace sh4simd
{
    struct gldc_vertex
    {
        float x, y, z;
        float u, v;
        float nx, ny, nz;
    };

    // Simple “transform and write to GLdc vertex array” helper.
    inline void gldc_transform_vertices(
        gldc_vertex       *dst,
        const gldc_vertex *src,
        std::size_t        count,
        const mat4f       &modelviewproj)
    {
        for (std::size_t i=0;i<count;i++)
        {
            float4 p{ src[i].x, src[i].y, src[i].z, 1.0f };
            float4 r = mul(modelviewproj, p);

            float inv_w = 1.0f / r[3];

            dst[i] = src[i];
            dst[i].x = r[0] * inv_w;
            dst[i].y = r[1] * inv_w;
            dst[i].z = r[2] * inv_w;
        }
    }

    // Transform normals by 3x3 part (no renormalization for speed)
    inline void gldc_transform_normals(
        gldc_vertex       *dst,
        const gldc_vertex *src,
        std::size_t        count,
        const mat3f       &normal_matrix)
    {
        for (std::size_t i=0;i<count;i++)
        {
            float3 n{ src[i].nx, src[i].ny, src[i].nz };

            float3 r;
            // n' = N * n; N is column-major 3x3
            r[0] = n[0] * normal_matrix(0,0) +
                   n[1] * normal_matrix(0,1) +
                   n[2] * normal_matrix(0,2);

            r[1] = n[0] * normal_matrix(1,0) +
                   n[1] * normal_matrix(1,1) +
                   n[2] * normal_matrix(1,2);

            r[2] = n[0] * normal_matrix(2,0) +
                   n[1] * normal_matrix(2,1) +
                   n[2] * normal_matrix(2,2);

            dst[i] = src[i];
            dst[i].nx = r[0];
            dst[i].ny = r[1];
            dst[i].nz = r[2];
        }
    }
}