//----------------------------------------------------------------------------
//  Dream In The Dark ZV (Game Logic)
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
#ifndef _ZV_H_
#define _ZV_H_
void getZvCube(sBody* bodyPtr, ZVStruct* zvPtr);
void GiveZVObjet(sBody* bodyPtr, ZVStruct* zvPtr);
void getZvMax(sBody* bodyPtr, ZVStruct* zvPtr);
void getZvRot(sBody* bodyPtr, ZVStruct* zvPtr, int alpha, int beta, int gamma);
void makeDefaultZV(ZVStruct* zvPtr);
#endif
