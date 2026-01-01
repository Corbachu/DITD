//----------------------------------------------------------------------------
//  Dream In The Dark screen (Rendering)
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
// seg 048

#include "common.h"

void setupScreen(void)
{
    logicalScreen = (char*)malloc(64800);

    screenBufferSize = 64800;

    unkScreenVar2 = 3;

    // TODO: remain of screen init

}

void flushScreen(void)
{
    for(int i=0;i<200;i++)
    {
        for(int j=0;j<320;j++)
        {
            *(uiLayer.data()+i*320+j) = 0;
            *(logicalScreen + i * 320 + j) = 0;
        }
    }
}
