//----------------------------------------------------------------------------
//  libSH4simd - tuple_size/tuple_element/get<> for structured bindings
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

#pragma once
#include <tuple>
#include "simd_array.h"

namespace std
{
    template<typename T, std::size_t N, std::size_t VecBytes>
    struct tuple_size<sh4simd::simd_array<T,N,VecBytes>>
        : std::integral_constant<std::size_t, N> {};

    template<std::size_t I, typename T, std::size_t N, std::size_t VecBytes>
    struct tuple_element<I, sh4simd::simd_array<T,N,VecBytes>>
    {
        static_assert(I < N, "tuple_element index out of range for simd_array");
        using type = T;
    };
}

namespace sh4simd
{
    template<std::size_t I, typename T, std::size_t N, std::size_t VecBytes>
    constexpr T &get(simd_array<T,N,VecBytes> &a) noexcept
    {
        static_assert(I < N, "get index out of range for simd_array");
        return a[I];
    }

    template<std::size_t I, typename T, std::size_t N, std::size_t VecBytes>
    constexpr const T &get(const simd_array<T,N,VecBytes> &a) noexcept
    {
        static_assert(I < N, "get index out of range for simd_array");
        return a[I];
    }

    template<std::size_t I, typename T, std::size_t N, std::size_t VecBytes>
    constexpr T &&get(simd_array<T,N,VecBytes> &&a) noexcept
    {
        static_assert(I < N, "get index out of range for simd_array");
        return static_cast<T &&>(a[I]);
    }

    template<std::size_t I, typename T, std::size_t N, std::size_t VecBytes>
    constexpr const T &&get(const simd_array<T,N,VecBytes> &&a) noexcept
    {
        static_assert(I < N, "get index out of range for simd_array");
        return static_cast<const T &&>(a[I]);
    }
}