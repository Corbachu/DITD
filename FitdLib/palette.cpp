//----------------------------------------------------------------------------
//  Dream In The Dark Palette Handling (Rendering)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2025  yaz0r/jimmu/FITD Team
//  Copyright (C) 1999-2025  The EDGE Team
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
#include "common.h"
#include "palette.h"

#include <cstring>

void paletteFill(palette_t& palette, unsigned char r, unsigned char g, unsigned b)
{
    r <<= 1;
    g <<= 1;
    b <<= 1;

    for (int i = 0; i < 256; i++)
    {
        palette[i][0] = r;
        palette[i][1] = g;
        palette[i][2] = b;
    }
}

void setPalette(palette_t& sourcePal)
{
    osystem_setPalette(&sourcePal);
}

void copyPalette(palette_t& source, palette_t& dest)
{
    dest = source;
}

void copyPalette(void* source, palette_t& dest)
{
    fitd_memcpy(dest.data(), source, sizeof(palette_t));
}

void convertPaletteIfRequired(palette_t& lpalette)
{
    if (g_gameId >= AITD1 && g_gameId <= AITD3)
    {
        // Palettes for these games are commonly stored as 6-bit (0..63)
        // and sometimes as 7-bit (0..127). The renderers expect 8-bit.
        // Avoid double-conversion (wrap) which would make output look dark.
        unsigned char maxc = 0;
        for (int i = 0; i < 256; i++)
        {
            if (lpalette[i][0] > maxc) maxc = lpalette[i][0];
            if (lpalette[i][1] > maxc) maxc = lpalette[i][1];
            if (lpalette[i][2] > maxc) maxc = lpalette[i][2];
        }

        // Already 8-bit.
        if (maxc > 127)
            return;

        const unsigned int denom = (maxc <= 63) ? 63u : 127u;

        for (int i = 0; i < 256; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                lpalette[i][j] = (((unsigned int)lpalette[i][j] * 255u) / denom) & 0xFF;
            }
        }
    }
}

void SetLevelDestPal(palette_t& inPalette, palette_t& outPalette, int coef)
{
    for (int i = 0; i < 256; i++)
    {
        for (int j = 0; j < 3; j++) {
            outPalette[i][j] = (inPalette[i][j] * coef) >> 8;
        }
    }
}