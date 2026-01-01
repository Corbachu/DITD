//----------------------------------------------------------------------------
//  Dream In The Dark AITD1 (Game Logic)
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

// ITD_RESS mapping
#define AITD1_TATOU_3DO		0
#define AITD1_TATOU_PAL		1
#define AITD1_TATOU_MCG		2
#define AITD1_PALETTE_JEU	3
#define AITD1_CADRE_SPF		4
#define AITD1_ITDFONT		5
#define AITD1_LETTRE		6
#define AITD1_LIVRE			7
#define AITD1_CARNET		8
#define AITD1_TEXT_GRAPH	9
#define AITD1_PERSO_CHOICE	10
#define AITD1_GRENOUILLE	11
#define AITD1_DEAD_END		12
#define AITD1_TITRE			13
#define AITD1_FOND_INTRO	14
#define AITD1_CAM07000		15
#define AITD1_CAM07001		16
#define AITD1_CAM06000		17
#define AITD1_CAM06005		18
#define AITD1_CAM06008		19

void startAITD1();
void AITD1_ReadBook(int index, int type);