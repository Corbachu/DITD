//----------------------------------------------------------------------------
//  Dream in the Dark Palette Handling (Dreamcast)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2025  Corbin Annis/yaz0r/jimmu/FITD Team
//  Copyright (c) 2025  The EDGE Team.
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

#include "common.h"

#ifdef DREAMCAST

#include <array>

// Shared Dreamcast palette state used by multiple Dreamcast rendering units.
// 256 entries, RGB888 packed as RGBRGB...
extern std::array<u8, 256 * 3> g_palette;

// Precomputed RGB565 palette for fast 16-bit conversion.
extern std::array<u16, 256> g_palette565;

#endif // DREAMCAST
