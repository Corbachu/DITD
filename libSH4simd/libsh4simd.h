//----------------------------------------------------------------------------
//  libSH4simd Main Header Include
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
//  ---------------------------------------------------------------------------
//  - Add to your KOS ports as a header-only library:
//  - Add -I/path/to/libSH4simd/include to your CFLAGS/CXXFLAGS.
//  - Include via #include <libsh4simd/libsh4simd.h>.

#pragma once

#include "simd_array.h"
#include "tuple.h"
#include "matrix.h"
#include "sh4_matrix_kernels.h"
#include "audio.h"
#include "vertex_dc.h"
#include "gldc_pipeline.h"