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
#include "common.h"
#include "sprite.h"

void sHybrid_Sprite::read(uint8_t* buffer, int bufferSize) {
    flags = READ_LE_U16(buffer); buffer+=2;
    dx = READ_LE_U8(buffer); buffer++;
    dy = READ_LE_U8(buffer); buffer++;

    lines.reserve(dy);
    for (int i = 0; i < dy; i++) {
        lines.emplace_back();
        auto& line = lines.back();
        int numBlocks = READ_LE_U8(buffer); buffer++;
        line.blocks.reserve(numBlocks);
        for (int j = 0; j < numBlocks; j++) {
            line.blocks.emplace_back();
            auto& block = line.blocks.back();
            block.unk0 = READ_LE_U8(buffer); buffer++;
            block.numWords = READ_LE_U8(buffer); buffer++;
            block.numBytes = READ_LE_U8(buffer); buffer++;

            int dataSize = block.numWords * 4 + block.numBytes;
            block.data.resize(dataSize);
            for (int k = 0; k < dataSize; k++) {
                block.data[k] = READ_LE_U8(buffer); buffer++;
            }
        }
        line.unk = READ_LE_U8(buffer); buffer++;
    }
}

void AffSpr(int spriteNumber, int X, int Y, char* screen, std::vector<sHybrid_Sprite>& sprites) {
    sHybrid_Sprite& sprite = sprites[spriteNumber];

    Y -= sprite.dy;
    for (int y = 0; y < sprite.dy; y++) {
        auto& lineData = sprite.lines[y];
        
        char* lineStart = screen + ((y + Y) * 320 + X);
        for (int i = 0; i < lineData.blocks.size(); i++) {
            auto& block = lineData.blocks[i];
            lineStart += block.unk0;
            memcpy(lineStart, block.data.data(), block.data.size());
            lineStart += block.data.size();
        }
    }
}
