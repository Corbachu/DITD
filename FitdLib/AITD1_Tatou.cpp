//----------------------------------------------------------------------------
//  Dream In The Dark Tattoo/Armadillo Splash (Game Logic)
//----------------------------------------------------------------------------
//  Copyright (c) 2025  Corbin Annis
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

#include "AITD1.h"

#ifdef DREAMCAST
extern "C" {
#include <kos/dbgio.h>
}
#endif

#ifdef DREAMCAST
static void dc_debug_stage(const char* msg)
{
    if (!PtrFont)
        return;

    const int bandH = std::min(200, fontHeight + 2);
    const int y0 = std::max(0, 200 - bandH);

    for (int y = y0; y < 200; ++y)
    {
        unsigned char* row = uiLayer.data() + y * 320;
        std::memset(row, 0, 320);
    }

    SetFont(PtrFont, 1);
    PrintFont(3, y0 + 1, (char*)uiLayer.data(), (u8*)msg);
    SetFont(PtrFont, 255);
    PrintFont(2, y0, (char*)uiLayer.data(), (u8*)msg);
}
#endif

void clearScreenTatou(void)
{
	for(int i=0;i<45120;i++)
	{
		frontBuffer[i] = 0;
	}
}

int make3dTatou(void)
{
#ifdef DREAMCAST
    dbgio_printf("[dc] make3dTatou: begin\n");
#endif
    int zoom;
    int deltaTime;
    int beta;
    int alpha;
    unsigned int localChrono;
    palette_t tatouPal;
    palette_t paletteBackup;

    char* tatou2d = CheckLoadMallocPak("ITD_RESS",AITD1_TATOU_MCG);

#ifdef DREAMCAST
    dbgio_printf("[dc] make3dTatou: loaded tatou2d=%p\n", (void*)tatou2d);
#endif

    char* tatou3dRaw = CheckLoadMallocPak("ITD_RESS", AITD1_TATOU_3DO);

#ifdef DREAMCAST
    dbgio_printf("[dc] make3dTatou: loaded tatou3dRaw=%p\n", (void*)tatou3dRaw);
#endif
    sBody* tatou3d = createBodyFromPtr(tatou3dRaw);

#ifdef DREAMCAST
    dbgio_printf("[dc] make3dTatou: createBodyFromPtr -> %p\n", (void*)tatou3d);
#endif

    char* tatouPalRaw = CheckLoadMallocPak("ITD_RESS",AITD1_TATOU_PAL);
    copyPalette(tatouPalRaw, tatouPal);

#ifdef DREAMCAST
    dbgio_printf("[dc] make3dTatou: loaded palette raw=%p\n", (void*)tatouPalRaw);
#endif

#ifdef DREAMCAST
    dc_debug_stage("tatou: loaded");
    osystem_drawBackground();
#endif

    zoom = 8920;
    deltaTime = 50;
    beta = 256;
    alpha = 8;

    SetProjection(160,100,128,500,490);

    copyPalette(currentGamePalette,paletteBackup);

    paletteFill(currentGamePalette,0,0,0);

    setPalette(currentGamePalette);

    copyPalette(tatouPal,currentGamePalette);
    FastCopyScreen(tatou2d+770,frontBuffer);
    FastCopyScreen(frontBuffer,aux2);

    osystem_CopyBlockPhys(frontBuffer,0,0,320,200);

#ifdef DREAMCAST
    dbgio_printf("[dc] make3dTatou: before FadeInPhys\n");
#endif

    FadeInPhys(8,0);

#ifdef DREAMCAST
    dbgio_printf("[dc] make3dTatou: after FadeInPhys\n");
#endif

#ifdef DREAMCAST
    dc_debug_stage("tatou: fade-in");
    osystem_drawBackground();
#endif

    startChrono(&localChrono);

#ifdef DREAMCAST
    dbgio_printf("[dc] make3dTatou: entering wait loop (pre-lightning)\n");
#endif

    do
    {
        process_events();

        //timeGlobal++;
        timer = timeGlobal;

        const int elapsed = evalChrono(&localChrono);

#ifdef DREAMCAST
        // Keep the screen alive during this timed wait (otherwise it can look like a hang).
        // Also emit occasional logs so we can see progress in the console.
        if ((elapsed % 30) == 0)
        {
            dbgio_printf("[dc] make3dTatou: pre-lightning elapsed=%d key=%d click=%d joy=%d\n",
                         elapsed, key, Click, JoyD);
            osystem_drawBackground();
        }
#endif

        if(elapsed<=180) 
        {
            // before lightning strike
            if(key || Click)
            {
#ifdef DREAMCAST
                dbgio_printf("[dc] make3dTatou: pre-lightning skip requested (key=%d click=%d joy=%d)\n",
                             key, Click, JoyD);
                // For debugging on Dreamcast: treat any input here as 'skip ahead'
                // to the lightning/3D part rather than aborting the whole sequence.
                localChrono = timer - 181;
                key = 0;
                Click = 0;
                JoyD = 0;
                continue;
#endif
                break;
            }
        }
        else
        {
            // lightning strike

#ifdef DREAMCAST
            dbgio_printf("[dc] make3dTatou: lightning begin\n");
#endif

#ifdef DREAMCAST
            dc_debug_stage("tatou: lightning");
            osystem_drawBackground();
#endif

            /*  LastSample = -1;
            LastPriority = -1; */

            int thunder = CVars[getCVarsIdx(SAMPLE_TONNERRE)];
#ifdef DREAMCAST
			dbgio_printf("[dc] make3dTatou: playSound thunder=%d\n", thunder);
#endif
            playSound(thunder);
#ifdef DREAMCAST
			dbgio_printf("[dc] make3dTatou: playSound returned\n");
#endif

            /*     LastSample = -1;
            LastPriority = -1;*/

            paletteFill(currentGamePalette,63,63,63);
            setPalette(currentGamePalette);
            /*  setClipSize(0,0,319,199);*/

            clearScreenTatou();

            setCameraTarget(0,0,0,alpha,beta,0,zoom);

#ifdef DREAMCAST
            dc_debug_stage("tatou: AffObjet");
            osystem_drawBackground();
#endif

            AffObjet(0,0,0,0,0,0,tatou3d);

#ifdef DREAMCAST
			dbgio_printf("[dc] make3dTatou: AffObjet returned\n");
#endif

#ifdef DREAMCAST
            dc_debug_stage("tatou: drew");
            osystem_drawBackground();
#endif

            //blitScreenTatou();
            osystem_CopyBlockPhys((unsigned char*)frontBuffer,0,0,320,200);

			process_events();

            copyPalette(tatouPal,currentGamePalette);
            setPalette(currentGamePalette);
			osystem_CopyBlockPhys((unsigned char*)frontBuffer,0,0,320,200);


            while(key==0 && Click == 0 && JoyD == 0)
            {
                // Armadillo rotation loop

                process_events();

#ifdef DREAMCAST
				static int dc_rotFrames = 0;
				if (((dc_rotFrames++) % 60) == 0)
					dbgio_printf("[dc] make3dTatou: rotation loop zoom=%d beta=%d\n", zoom, beta);
#endif

                zoom += deltaTime;

                if(zoom>16000)
                    break;

                beta -=8;

                clearScreenTatou();

                setCameraTarget(0,0,0,alpha,beta,0,zoom);

                AffObjet(0,0,0,0,0,0,tatou3d);

                //blitScreenTatou();

                osystem_CopyBlockPhys((unsigned char*)frontBuffer,0,0,320,200);

                // Present explicitly here; calling osystem_stopFrame() adds an
                // extra endOfFrame on Dreamcast and can stall in tight loops.
                osystem_drawBackground();
            }

            break;
        }
    }while(1);

    delete[] tatou3dRaw;
    delete tatou3d;

    free(tatou2d);

    if(key || Click || JoyD)
    {
        while(key)
        {
          process_events();
        }

        FadeOutPhys(32,0);
        copyPalette(paletteBackup,currentGamePalette);
        return(1);
    }
    else
    {
        FadeOutPhys(16,0);
        copyPalette(paletteBackup,currentGamePalette);
        return(0);
    }

    return(0);
}
