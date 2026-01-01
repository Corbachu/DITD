//----------------------------------------------------------------------------
//  Dream In The Dark sprite (Rendering)
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

struct sHybrid_SpriteBlock {
    uint8_t unk0;
    uint8_t numWords;
    uint8_t numBytes;
    std::vector<uint8_t> data;
};

struct sHybrid_SpriteLine {
    std::vector<sHybrid_SpriteBlock> blocks;
    uint8_t unk;
};

struct sHybrid_Sprite {
    uint16_t flags;
    uint8_t dx;
    uint8_t dy;
    std::vector<sHybrid_SpriteLine> lines;

    void read(uint8_t* buffer, int bufferSize);
};

void AffSpr(int spriteNumber, int X, int Y, char* screen, std::vector<sHybrid_Sprite>& sprites);