//----------------------------------------------------------------------------
//  Dream In The Dark tatou (Game Logic)
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
#ifndef _TATOU_H_
#define _TATOU_H_

int make3dTatou(void);
//////////////// to mode
void process_events( void );
void FastCopyScreen(void* source, void* dest);
void Rotate(unsigned int x, unsigned int y, unsigned int z, int* xOut, int* yOut);
void FadeInPhys(int var1,int var2);
void FadeOutPhys(int var1, int var2);
void playSound(int num);
void setCameraTarget(int x,int y,int z,int alpha,int beta,int gamma,int time);

void startChrono(unsigned int* chrono);
int evalChrono(unsigned int* chrono);
//////////////
#endif
