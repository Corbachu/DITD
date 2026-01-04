//----------------------------------------------------------------------------
//  libSH4simd - mat2/mat3/mat4 built on simd_array
//----------------------------------------------------------------------------
//
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

#pragma once
#include <cstddef>
#include <cmath>
#include "simd_array.h"

namespace sh4simd
{
    // Column-major matrices, like GL/GLdc
    template<typename T, std::size_t ROWS, std::size_t COLS>
    struct matrix
    {
        static constexpr std::size_t rows = ROWS;
        static constexpr std::size_t cols = COLS;
        static constexpr std::size_t elements = ROWS * COLS;

        using value_type = T;
        using storage_type = simd_array<T, elements, 8>; // 8-byte vector default

        storage_type m; // column-major, m[col * rows + row]

        constexpr matrix() : m{} {}

        constexpr explicit matrix(T diag)
        {
            m.fill(T{0});
            constexpr std::size_t min_dim = (ROWS < COLS ? ROWS : COLS);
            for (std::size_t i=0;i<min_dim;i++)
                m[i * ROWS + i] = diag;
        }

        constexpr T &operator()(std::size_t r, std::size_t c)
        {
            return m[c * ROWS + r];
        }

        constexpr const T &operator()(std::size_t r, std::size_t c) const
        {
            return m[c * ROWS + r];
        }

        static constexpr std::size_t size() { return elements; }

        // row/col access (column major)
        constexpr simd_array<T,ROWS,8> column(std::size_t c) const
        {
            simd_array<T,ROWS,8> out;
            for (std::size_t r=0;r<ROWS;r++)
                out[r] = (*this)(r,c);
            return out;
        }

        constexpr void set_column(std::size_t c, const simd_array<T,ROWS,8> &v)
        {
            for (std::size_t r=0;r<ROWS;r++)
                (*this)(r,c) = v[r];
        }
    };

    using mat2f = matrix<float,2,2>;
    using mat3f = matrix<float,3,3>;
    using mat4f = matrix<float,4,4>;

    // Some basic constructors/helpers for mat4f
    inline mat4f identity4f()
    {
        return mat4f(1.0f);
    }

    inline mat4f translation4f(float x, float y, float z)
    {
        mat4f r(1.0f);
        r(0,3) = x;
        r(1,3) = y;
        r(2,3) = z;
        return r;
    }

    inline mat4f scale4f(float sx, float sy, float sz)
    {
        mat4f r(0.0f);
        r(0,0) = sx;
        r(1,1) = sy;
        r(2,2) = sz;
        r(3,3) = 1.0f;
        return r;
    }

    inline mat4f rotation_z4f(float radians)
    {
        mat4f r(1.0f);
        float c = std::cos(radians);
        float s = std::sin(radians);

        r(0,0) = c;  r(1,0) = s;
        r(0,1) = -s; r(1,1) = c;
        return r;
    }
}