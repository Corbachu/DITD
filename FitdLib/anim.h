//----------------------------------------------------------------------------
//  Dream In The Dark anim (Animation)
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

#define INFO_TRI 1
#define INFO_ANIM 2
#define INFO_TORTUE 4
#define INFO_OPTIMISE 8


struct sFrame
{
    u16 m_timestamp;
    point3dStruct m_animStep;
    std::vector<sGroupState> m_groups;
};

struct sAnimation
{
    void* m_raw;

    u16 m_numFrames;
    u16 m_numGroups;
    std::vector<sFrame> m_frames;
};

extern hqrEntryStruct<sAnimation>* HQ_Anims;

#define NB_BUFFER_ANIM 25 // AITD1 was  20
#define SIZE_BUFFER_ANIM (8*41) // AITD1 was 4*31

extern std::vector<sFrame> BufferAnim;

int InitAnim(int animNum,int animType, int animInfo);
int SetAnimObjet(int frame, sAnimation* anim, sBody* body);
s16 SetInterAnimObjet(int frame, sAnimation* animPtr, sBody* bodyPtr);
s16 GetNbFramesAnim(sAnimation* animPtr);
void StockInterAnim(sFrame& animBuffer, sBody* bodyPtr);
void ResetStartAnim(sBody* bodyPtr);
void GereAnim(void);
void InitCopyBox(char* var0, char* var1);

