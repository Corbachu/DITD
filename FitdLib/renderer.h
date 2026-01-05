//----------------------------------------------------------------------------
//  Dream In The Dark renderer (Rendering)
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
#ifndef _RENDERER_H_
#define _RENDERER_H_

extern int BBox3D1;
extern int BBox3D2;
extern int BBox3D3;
extern int BBox3D4;

#define NUM_MAX_POINT_IN_POINT_BUFFER 800
#define NUM_MAX_BONES 50

extern std::array<point3dStruct, NUM_MAX_POINT_IN_POINT_BUFFER> pointBuffer;
extern int numOfPoints;

void transformPoint(float* ax, float* bx, float* cx);

int DisplayObject(int x, int y, int z, int alpha, int beta, int gamma, sBody* pBody);

// Compatibility wrapper (old French name).
int AffObjet(int x, int y, int z, int alpha, int beta, int gamma, sBody* pBody);

void computeScreenBox(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr);

#endif
