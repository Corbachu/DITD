//----------------------------------------------------------------------------
//  Dream In The Dark JACK (Game Code)
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

// ITD_RESS mapping
#define JACK_CADRE_SPF					0
#define JACK_ITDFONT					1
#define JACK_LIVRE						2
#define JACK_IM_EXT_JACK				3

void startJACK()
{
	fontHeight = 16; // TODO: check
	startGame(16,1,1);
}

void JACK_ReadBook(int index, int type)
{
	switch(type)
	{
	case 1: // READ_BOOK
		{
			unsigned char* pImage = (unsigned char*)loadPak("ITD_RESS", JACK_LIVRE);
			memcpy(aux, pImage, 320*200);
            palette_t lpalette;
            copyPalette(pImage + 320*200, lpalette);
			convertPaletteIfRequired(lpalette);
			copyPalette(lpalette,currentGamePalette);
			setPalette(lpalette);
			free(pImage);
			turnPageFlag = 1;
			Lire(index, 60, 10, 245, 190, 0, 124, 124);
			break;
		}
	default:
		assert(0);
	}
}