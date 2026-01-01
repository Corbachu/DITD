//----------------------------------------------------------------------------
//  Dream In The Dark anim2d (Animation)
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

#include "hybrid.h"

struct sAnim2d {
    uint16_t frame;
    uint32_t time;
    uint16_t flagEndInterAnim;
    std::vector<sHybrid_Anim>::iterator pAnim;
    sHybrid_Animation* pAnimation;
    int16_t ScreenXMin;
    int16_t ScreenYMin;
    int16_t ScreenXMax;
    int16_t ScreenYMax;
};

extern std::array<sAnim2d, 20> TabAnim2d;
extern int NbAnim2D;
extern sHybrid* PtrAnim2D;

void resetAnim2D();
void load2dAnims(int cameraIdx);
void startAnim2d(int index);
void handleAnim2d();