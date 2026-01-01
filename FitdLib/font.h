//----------------------------------------------------------------------------
//  Dream In The Dark font (Rendering)
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

extern int fontHeight;
void SetFont(char* fontData, int color);
void SetFontSpace(int interWordSpace, int interLetterSpace);
int ExtGetSizeFont(u8* string);
void PrintFont(int x, int y, char* surface, u8* string);
void SelectedMessage(int x, int y, int index, int color1, int color2);
void SimpleMessage(int x, int y, int index, int color);
