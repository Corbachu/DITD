//----------------------------------------------------------------------------
//  libSH4simd - core SIMD array (std::array parity)
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
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <stdexcept>

#if !defined(__GNUC__)
#   error "libSH4simd requires GCC (vector_size attribute)."
#endif

namespace sh4simd
{
    template<typename T>
    using is_supported_scalar =
        std::bool_constant<std::is_arithmetic_v<T> && !std::is_same_v<T,bool>>;

    template<typename T, std::size_t VecBytes>
    struct gcc_vector_type
    {
        static_assert(is_supported_scalar<T>::value,
            "SIMD scalar type must be arithmetic.");
        using type = T __attribute__((vector_size(VecBytes), may_alias));
    };

    template<typename T, std::size_t VecBytes>
    using gcc_vector_t = typename gcc_vector_type<T,VecBytes>::type;

    template<typename T, std::size_t N, std::size_t VecBytes = 8>
    struct alignas(VecBytes) simd_array
    {
        static_assert(is_supported_scalar<T>::value,
            "simd_array only supports arithmetic types.");

        using value_type      = T;
        using size_type       = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference       = T&;
        using const_reference = const T&;
        using pointer         = T*;
        using const_pointer   = const T*;

        static constexpr size_type vector_bytes = VecBytes;
        static constexpr size_type scalar_size  = sizeof(T);

        static_assert(vector_bytes % scalar_size == 0,
            "VecBytes must be multiple of sizeof(T).");

        static constexpr size_type lanes      = vector_bytes / scalar_size;
        static constexpr size_type vec_count  = (N + lanes - 1) / lanes;
        static constexpr size_type padded_len = vec_count * lanes;

        using vector_type = gcc_vector_t<T,VecBytes>;

        union
        {
            vector_type vec[vec_count];
            T           scalars[padded_len];
        };

        // ------------------------------------------------------------
        // Constructors
        // ------------------------------------------------------------
        constexpr simd_array() : scalars{} {}

        constexpr explicit simd_array(const T &v)
        {
            for (size_type i=0;i<N;i++) scalars[i] = v;
            for (size_type i=N;i<padded_len;i++) scalars[i] = T{};
        }

        constexpr simd_array(std::initializer_list<T> init)
        {
            size_type i=0;
            for (T v : init)
            {
                if (i < N) scalars[i++] = v;
                else break;
            }
            for (; i<padded_len; i++) scalars[i] = T{};
        }

        // ------------------------------------------------------------
        // std::array API parity
        // ------------------------------------------------------------
        constexpr reference at(size_type i)
        {
            if (i >= N) throw std::out_of_range("simd_array::at");
            return scalars[i];
        }

        constexpr const_reference at(size_type i) const
        {
            if (i >= N) throw std::out_of_range("simd_array::at");
            return scalars[i];
        }

        constexpr reference operator[](size_type i)             { return scalars[i]; }
        constexpr const_reference operator[](size_type i) const { return scalars[i]; }

        constexpr reference front()             { return scalars[0]; }
        constexpr const_reference front() const { return scalars[0]; }

        constexpr reference back()              { return scalars[N-1]; }
        constexpr const_reference back() const  { return scalars[N-1]; }

        constexpr pointer data() noexcept             { return scalars; }
        constexpr const_pointer data() const noexcept { return scalars; }

        // Iterators
        constexpr pointer begin() noexcept             { return scalars; }
        constexpr const_pointer begin() const noexcept { return scalars; }
        constexpr const_pointer cbegin() const noexcept{ return scalars; }

        constexpr pointer end() noexcept               { return scalars + N; }
        constexpr const_pointer end() const noexcept   { return scalars + N; }
        constexpr const_pointer cend() const noexcept  { return scalars + N; }

        // Capacity
        static constexpr size_type size() noexcept     { return N; }
        static constexpr size_type max_size() noexcept { return N; }
        static constexpr bool empty() noexcept         { return N == 0; }

        // Modifiers
        constexpr void fill(const T &value)
        {
            for (size_type i=0;i<N;i++) scalars[i] = value;
            for (size_type i=N;i<padded_len;i++) scalars[i] = T{};
        }

        constexpr void swap(simd_array &other) noexcept
        {
            for (size_type i=0;i<N;i++)
                std::swap(scalars[i], other.scalars[i]);
        }

        // SIMD helpers (optional external use)
        static constexpr size_type simd_lanes()   { return lanes; }
        static constexpr size_type simd_vectors() { return vec_count; }

        constexpr vector_type &vector_at(size_type i)             { return vec[i]; }
        constexpr const vector_type &vector_at(size_type i) const { return vec[i]; }

        // ------------------------------------------------------------
        // SIMD binary ops
        // ------------------------------------------------------------
    private:
        template<typename Op>
        static constexpr simd_array apply_binary(const simd_array &a,
                                                 const simd_array &b,
                                                 Op op)
        {
            simd_array r;
            for (size_type i=0;i<vec_count;i++)
                r.vec[i] = op(a.vec[i], b.vec[i]);
            for (size_type i=N;i<padded_len;i++)
                r.scalars[i] = T{};
            return r;
        }

        template<typename Op>
        static constexpr simd_array apply_scalar(const simd_array &a,
                                                 const T &s,
                                                 Op op)
        {
            simd_array r;
            vector_type vs;
            for (size_type i=0;i<lanes;i++)
                reinterpret_cast<T*>(&vs)[i] = s;

            for (size_type i=0;i<vec_count;i++)
                r.vec[i] = op(a.vec[i], vs);

            for (size_type i=N;i<padded_len;i++)
                r.scalars[i] = T{};
            return r;
        }

    public:
        // elementwise ops
        friend constexpr simd_array operator+(const simd_array&a,const simd_array&b)
        { return apply_binary(a,b,[](auto x,auto y){return x+y;}); }

        friend constexpr simd_array operator-(const simd_array&a,const simd_array&b)
        { return apply_binary(a,b,[](auto x,auto y){return x-y;}); }

        friend constexpr simd_array operator*(const simd_array&a,const simd_array&b)
        { return apply_binary(a,b,[](auto x,auto y){return x*y;}); }

        friend constexpr simd_array operator/(const simd_array&a,const simd_array&b)
        { return apply_binary(a,b,[](auto x,auto y){return x/y;}); }

        // scalar ops
        friend constexpr simd_array operator+(const simd_array&a,const T&s)
        { return apply_scalar(a,s,[](auto x,auto y){return x+y;}); }

        friend constexpr simd_array operator-(const simd_array&a,const T&s)
        { return apply_scalar(a,s,[](auto x,auto y){return x-y;}); }

        friend constexpr simd_array operator*(const simd_array&a,const T&s)
        { return apply_scalar(a,s,[](auto x,auto y){return x*y;}); }

        friend constexpr simd_array operator/(const simd_array&a,const T&s)
        { return apply_scalar(a,s,[](auto x,auto y){return x/y;}); }

        // compound
        simd_array& operator+=(const simd_array&o)
        { for(size_type i=0;i<vec_count;i++) vec[i]+=o.vec[i]; return *this; }

        simd_array& operator-=(const simd_array&o)
        { for(size_type i=0;i<vec_count;i++) vec[i]-=o.vec[i]; return *this; }

        simd_array& operator*=(const simd_array&o)
        { for(size_type i=0;i<vec_count;i++) vec[i]*=o.vec[i]; return *this; }

        simd_array& operator/=(const simd_array&o)
        { for(size_type i=0;i<vec_count;i++) vec[i]/=o.vec[i]; return *this; }

        // math helpers
        T dot(const simd_array &o) const
        {
            simd_array p = (*this) * o;
            T sum{};
            for (size_type i=0;i<N;i++) sum += p.scalars[i];
            return sum;
        }

        T length_sq() const { return dot(*this); }

        T length() const
        {
            using std::sqrt;
            return sqrt(length_sq());
        }

        simd_array normalized() const
        {
            T len = length();
            return (len == T{}) ? simd_array{} : (*this / len);
        }

        // comparison
        friend bool operator==(const simd_array&a,const simd_array&b)
        {
            for (size_type i=0;i<N;i++)
                if (a.scalars[i] != b.scalars[i]) return false;
            return true;
        }

        friend bool operator!=(const simd_array&a,const simd_array&b)
        {
            return !(a == b);
        }
    };

    // Common typedefs
    using float2 = simd_array<float,2,8>;
    using float3 = simd_array<float,3,8>;
    using float4 = simd_array<float,4,8>;

    using sample4i16 = simd_array<int16_t,4,8>;

} // namespace sh4simd