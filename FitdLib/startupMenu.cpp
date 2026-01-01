//----------------------------------------------------------------------------
//  Dream In The Dark startup Menu (UI)
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

void DrawMenu(int selectedEntry)
{
    if (g_gameId == AITD3) {
        LoadPak("ITD_RESS", 13, logicalScreen);
    }
    else {
        AffBigCadre(160, 100, 320, 80);
    }
    
    int currentY = 76;

    for(int i=0; i<3; i++)
    {
        if(i == selectedEntry) // highlight selected entry
        {
            if (g_gameId == AITD3) {
                SelectedMessage(160, currentY+1, i + 11, 1, 4);
            }
            else {
                AffRect(10, currentY, 309, currentY + 16, 100);
                SelectedMessage(160, currentY, i + 11, 15, 4);
            }
        }
        else
        {
            SimpleMessage(160,currentY,i+11,4);
        }

        currentY+=16; // next line
    }
}

int MainMenu(void)
{
    int currentSelectedEntry = 0;
    unsigned int chrono;
    int selectedEntry = -1;

    flushScreen();

    if (g_gameId == AITD3) {
        LoadPak("ITD_RESS", 47, aux);

        palette_t lpalette;
        copyPalette(aux, lpalette);
        convertPaletteIfRequired(lpalette);
        copyPalette(lpalette, currentGamePalette);
        setPalette(lpalette);
    }

    DrawMenu(0);

    osystem_startFrame();
    osystem_stopFrame();
    osystem_CopyBlockPhys((unsigned char*)logicalScreen,0,0,320,200);

    osystem_flip(NULL);
    FadeInPhys(16,0);
    startChrono(&chrono);

    while(evalChrono(&chrono) <= 0x10000) // exit loop only if time out or if choice made
    {
        osystem_CopyBlockPhys((unsigned char*)logicalScreen,0,0,320,200);
        osystem_startFrame();

        if(selectedEntry!=-1 || evalChrono(&chrono) > 0x10000)
        {
            break;
        }

        process_events();
		osystem_drawBackground();

        if(JoyD&1) // up key
        {
            currentSelectedEntry--;

            if(currentSelectedEntry<0)
            {
                currentSelectedEntry = 2;
            }

            DrawMenu(currentSelectedEntry);
            osystem_flip(NULL);
            //      menuWaitVSync();

            startChrono(&chrono);

            while(JoyD)
            {
                process_events();
            }
        }


        if(JoyD&2) // down key
        {
            currentSelectedEntry++;

            if(currentSelectedEntry>2)
            {
                currentSelectedEntry = 0;
            }

            DrawMenu(currentSelectedEntry);
            //menuWaitVSync();
            osystem_flip(NULL);

            startChrono(&chrono);

            while(JoyD)
            {
                process_events();
            }
        } 

        if(key == 28 || (key != 28 && Click!=0)) // select current entry
        {
            selectedEntry = currentSelectedEntry;
        }
        osystem_stopFrame();
        osystem_flip(NULL);
    }

    if(selectedEntry==2) // if exit game, do not fade
    {
        FadeOutPhys(16,0);
    }

    while(JoyD)
    {
        process_events();
    }

    return(selectedEntry);
}
