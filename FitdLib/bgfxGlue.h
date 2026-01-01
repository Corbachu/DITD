//----------------------------------------------------------------------------
//  Dream In The Dark bgfx Glue (Rendering)
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

#ifndef DREAMCAST
#include <SDL.h>

int initBgfxGlue(int argc, char* argv[]);
void deleteBgfxGlue();

void StartFrame();
void EndFrame();

extern int gFrameLimit;
extern bool gCloseApp;

extern SDL_Window* gWindowBGFX;
#endif

