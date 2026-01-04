//----------------------------------------------------------------------------
//  Dream In The Dark music (Audio)
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
#ifndef _MUSIC_H_
#define _MUSIC_H_
extern unsigned int musicChrono;
int initMusicDriver(void);

extern "C" {
int musicUpdate(void *udata, uint8 *stream, int len);
void playMusic(int musicNumber);
extern bool g_gameUseCDA;
extern int musicVolume;
};

void callMusicUpdate(void);
void destroyMusicDriver(void);
int fadeMusic(int param1, int param2, int param3);
#endif
