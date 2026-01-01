//----------------------------------------------------------------------------
//  Dream In The Dark inventory (Game Logic)
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

#define INVENTORY_SIZE      30
#define NUM_MAX_INVENTORY	2
extern s16 currentInventory;
extern s16 numObjInInventoryTable[NUM_MAX_INVENTORY];
extern s16 inHandTable[NUM_MAX_INVENTORY];
extern s16 inventoryTable[NUM_MAX_INVENTORY][INVENTORY_SIZE];

extern int statusLeft;
extern int statusTop;
extern int statusRight;
extern int statusBottom;

void processInventory(void);
