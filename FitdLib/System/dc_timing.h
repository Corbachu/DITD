//----------------------------------------------------------------------------
//  Dream in the Dark Timer Code
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

#ifdef DREAMCAST

#include <cstdint>

// Dreamcast logic/frame pacing for this port.
// Used by both video frame pacing and any code that needs "tics".
static constexpr int DC_LOGIC_FPS = 30;

static inline int DC_MsToTics(uint64_t ms)
{
    // Equivalent to floor(ms * DC_LOGIC_FPS / 1000).
    return (int)((ms / 1000ULL) * (uint64_t)DC_LOGIC_FPS + ((ms % 1000ULL) * (uint64_t)DC_LOGIC_FPS) / 1000ULL);
}

#endif // DREAMCAST
