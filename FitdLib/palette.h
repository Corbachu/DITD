//----------------------------------------------------------------------------
//  Dream In The Dark palette (Rendering)
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
#pragma once

#include <array>
typedef std::array<std::array<unsigned char, 3>, 256> palette_t;
void paletteFill(palette_t& palette, unsigned char r, unsigned char g, unsigned b);
void setPalette(palette_t& palette);
void copyPalette(void* source, palette_t& dest);
void copyPalette(palette_t& source, palette_t& dest);
void convertPaletteIfRequired(palette_t& lpalette);
void SetLevelDestPal(palette_t& inPalette, palette_t& outPalette, int coef);

extern palette_t currentGamePalette;
