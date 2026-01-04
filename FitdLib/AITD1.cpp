//----------------------------------------------------------------------------
//  Dream In The Dark AITD1 (Game Logic)

//----------------------------------------------------------------------------
//  
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

#ifdef DREAMCAST
extern "C" {
#include <kos/dbgio.h>
}
#endif

// DEMO mapping
/*
#define PALETTE_JEU		0
#define ITDFONT			1
*/

int AITD1KnownCVars[] =
{
    SAMPLE_PAGE,
    BODY_FLAMME,
    MAX_WEIGHT_LOADABLE,
    TEXTE_CREDITS,
    SAMPLE_TONNERRE,
    INTRO_DETECTIVE,
    INTRO_HERITIERE,
    WORLD_NUM_PERSO,
    CHOOSE_PERSO,
    SAMPLE_CHOC,
    SAMPLE_PLOUF,
    REVERSE_OBJECT,
    KILLED_SORCERER,
    LIGHT_OBJECT,
    FOG_FLAG,
    DEAD_PERSO,
    -1
};

enumLifeMacro AITD1LifeMacroTable[] =
{
    LM_DO_MOVE,
    LM_ANIM_ONCE,
    LM_ANIM_ALL_ONCE,
    LM_BODY,
    LM_IF_EGAL,
    LM_IF_DIFFERENT,
    LM_IF_SUP_EGAL,
    LM_IF_SUP,
    LM_IF_INF_EGAL,
    LM_IF_INF,
    LM_GOTO,
    LM_RETURN,
    LM_END,
    LM_ANIM_REPEAT,
    LM_ANIM_MOVE,
    LM_MOVE,
    LM_HIT,
    LM_MESSAGE,
    LM_MESSAGE_VALUE,
    LM_VAR,
    LM_INC,
    LM_DEC,
    LM_ADD,
    LM_SUB,
    LM_LIFE_MODE,
    LM_SWITCH,
    LM_CASE,
    LM_CAMERA,
    LM_START_CHRONO,
    LM_MULTI_CASE,
    LM_FOUND,
    LM_LIFE,
    LM_DELETE,
    LM_TAKE,
    LM_IN_HAND,
    LM_READ,
    LM_ANIM_SAMPLE,
    LM_SPECIAL,
    LM_DO_REAL_ZV,
    LM_SAMPLE,
    LM_TYPE,
    LM_GAME_OVER,
    LM_MANUAL_ROT,
    LM_RND_FREQ,
    LM_MUSIC,
    LM_SET_BETA,
    LM_DO_ROT_ZV,
    LM_STAGE,
    LM_FOUND_NAME,
    LM_FOUND_FLAG,
    LM_FOUND_LIFE,
    LM_CAMERA_TARGET,
    LM_DROP,
    LM_FIRE,
    LM_TEST_COL,
    LM_FOUND_BODY,
    LM_SET_ALPHA,
    LM_STOP_BETA,
    LM_DO_MAX_ZV,
    LM_PUT,
    LM_C_VAR,
    LM_DO_NORMAL_ZV,
    LM_DO_CARRE_ZV,
    LM_SAMPLE_THEN,
    LM_LIGHT,
    LM_SHAKING,
    LM_INVENTORY,
    LM_FOUND_WEIGHT,
    LM_UP_COOR_Y,
    LM_SPEED,
    LM_PUT_AT,
    LM_DEF_ZV,
    LM_HIT_OBJECT,
    LM_GET_HARD_CLIP,
    LM_ANGLE,
    LM_REP_SAMPLE,
    LM_THROW,
    LM_WATER,
    LM_PICTURE,
    LM_STOP_SAMPLE,
    LM_NEXT_MUSIC,
    LM_FADE_MUSIC,
    LM_STOP_HIT_OBJECT,
    LM_COPY_ANGLE,
    LM_END_SEQUENCE,
    LM_SAMPLE_THEN_REPEAT,
    LM_WAIT_GAME_OVER,
};

int makeIntroScreens(void)
{
#ifdef DREAMCAST
    dbgio_printf("[dc] makeIntroScreens(): enter\n");
#endif
    char* data;
    unsigned int chrono;

    // Keep the main game palette intact for menus/character select.
    palette_t gamePaletteBackup;
    copyPalette(currentGamePalette, gamePaletteBackup);

    data = loadPak("ITD_RESS", AITD1_TITRE);
#ifdef DREAMCAST
    dbgio_printf("[dc] makeIntroScreens(): loaded title (AITD1_TITRE) ptr=%p\n", data);
#endif

    // AITD1_TITRE packs palette + pixels like other 320x200 resources:
    // 2 bytes header, 768 bytes palette, then 64000 bytes pixels (offset +770).
    palette_t titlePalette;
    copyPalette((unsigned char*)data + 2, titlePalette);
    convertPaletteIfRequired(titlePalette);
    copyPalette(titlePalette, currentGamePalette);
    setPalette(currentGamePalette);

    FastCopyScreen(data + 770, frontBuffer);
    osystem_CopyBlockPhys(frontBuffer, 0, 0, 320, 200);
    FadeInPhys(8, 0);
    memcpy(logicalScreen, frontBuffer, 320 * 200);
    osystem_flip(NULL);
    free(data);

    // Switch back to the normal palette for the book text/pages and subsequent menus.
    copyPalette(gamePaletteBackup, currentGamePalette);
    setPalette(currentGamePalette);

    LoadPak("ITD_RESS", AITD1_LIVRE, aux);

#ifdef DREAMCAST
    dbgio_printf("[dc] makeIntroScreens(): loaded book bg (AITD1_LIVRE)\n");
#endif

    // Ensure the book background is actually visible before we trigger the
    // page-turn effect in Lire(). The wipe should reveal the new page (book +
    // credits text) over the already-visible book background.
    osystem_CopyBlockPhys((unsigned char*)aux, 0, 0, 320, 200);
    osystem_drawBackground();

    startChrono(&chrono);

    do
    {
        int time;

        process_events();
        osystem_drawBackground();

        time = evalChrono(&chrono);

        if (time >= 0x30)
            break;

    } while (key == 0 && Click == 0);

    playSound(CVars[getCVarsIdx(SAMPLE_PAGE)]);

    // Prepare the book page, then let the engine page-turn logic reveal it.
    FastCopyScreen(aux, logicalScreen);

    /*  LastSample = -1;
    LastPriority = -1;
    LastSample = -1;
    LastPriority = 0; */
    turnPageFlag = 1;
    Lire(CVars[getCVarsIdx(TEXTE_CREDITS)] + 1, 48, 2, 260, 197, 1, 26, 0);

    // Ensure caller continues with the expected game palette.
    copyPalette(gamePaletteBackup, currentGamePalette);
    setPalette(currentGamePalette);

    return(0);
}

void CopyBox_Aux_Log(int x1, int y1, int x2, int y2)
{
    int i;
    int j;

    for (i = y1; i < y2; i++)
    {
        for (j = x1; j < x2; j++)
        {
            *(screenSm3 + i * 320 + j) = *(screenSm1 + i * 320 + j);
        }
    }
}

int ChoosePerso(void)
{
#ifdef DREAMCAST
    dbgio_printf("[dc] [CharacterSelect]ChoosePerso(): enter\n");
#endif
    int choice = 0;
    int firsttime = 1;
    int choiceMade = 0;

    uiLayer.fill(0);
    InitCopyBox(aux, logicalScreen);

    while (choiceMade == 0)
    {
        process_events();
        osystem_drawBackground();

        // TODO: missing code for music stop

        LoadPak("ITD_RESS", 10, aux);
        FastCopyScreen(aux, logicalScreen);
        FastCopyScreen(logicalScreen, aux2);

        if (choice == 0)
        {
            AffBigCadre(80, 100, 160, 200);
            CopyBox_Aux_Log(10, 10, 149, 190);
        }
        else
        {
            AffBigCadre(240, 100, 160, 200);
            CopyBox_Aux_Log(170, 10, 309, 190);
        }

        FastCopyScreen(logicalScreen, frontBuffer);
        osystem_CopyBlockPhys(frontBuffer, 0, 0, 320, 200);

        if (firsttime != 0)
        {
            FadeInPhys(0x40, 0);

            do
            {
                process_events();
            } while (Click || key);

            firsttime = 0;
        }

        while ((localKey = key) != 28 && Click == 0) // process input
        {
            process_events();
            osystem_drawBackground();

            if (JoyD & 4) // left
            {
                choice = 0;
                FastCopyScreen(aux2, logicalScreen);
                AffBigCadre(80, 100, 160, 200);
                CopyBox_Aux_Log(10, 10, 149, 190);
                osystem_CopyBlockPhys((unsigned char*)logicalScreen, 0, 0, 320, 200);

                while (JoyD != 0)
                {
                    process_events();
                }
            }

            if (JoyD & 8) // right
            {
                choice = 1;
                FastCopyScreen(aux2, logicalScreen);
                AffBigCadre(240, 100, 160, 200);
                CopyBox_Aux_Log(170, 10, 309, 190);
                osystem_CopyBlockPhys((unsigned char*)logicalScreen, 0, 0, 320, 200);

                while (JoyD != 0)
                {
                    process_events();
                }
            }

            if (localKey == 1)
            {
                InitCopyBox(aux2, logicalScreen);
                FadeOutPhys(0x40, 0);
#ifdef DREAMCAST
                dbgio_printf("[dc] ChoosePerso(): exit (cancel)\n");
#endif
                return(-1);
            }
        }

        FadeOutPhys(0x40, 0);
        turnPageFlag = 0;

        switch (choice)
        {
            case 0:
            {
                FastCopyScreen(frontBuffer, logicalScreen);
                SetClip(0, 0, 319, 199);
                LoadPak("ITD_RESS", AITD1_FOND_INTRO, aux);
                CopyBox_Aux_Log(160, 0, 319, 199);
                FastCopyScreen(logicalScreen, aux);
                Lire(CVars[getCVarsIdx(INTRO_HERITIERE)] + 1, 165, 5, 314, 194, 2, 15, 0);
                CVars[getCVarsIdx(CHOOSE_PERSO)] = 1;
                break;
            }
            case 1:
            {
                FastCopyScreen(frontBuffer, logicalScreen);
                SetClip(0, 0, 319, 199);
                LoadPak("ITD_RESS", AITD1_FOND_INTRO, aux);
                CopyBox_Aux_Log(0, 0, 159, 199);
                FastCopyScreen(logicalScreen, aux);
                Lire(CVars[getCVarsIdx(INTRO_DETECTIVE)] + 1, 5, 5, 154, 194, 2, 15, 0);
                CVars[getCVarsIdx(CHOOSE_PERSO)] = 0;
                break;
            }
        }

        if (localKey && 0x1C)
        {
            choiceMade = 1;
        }

    }

    FadeOutPhys(64, 0);
    InitCopyBox(aux2, logicalScreen);
#ifdef DREAMCAST
    dbgio_printf("[dc] ChoosePerso(): exit choice=%d\n", choice);
#endif
    return(choice);
}

void startAITD1()
{
#ifdef DREAMCAST
    dbgio_printf("[dc] startAITD1(): enter\n");
#endif
    fontHeight = 16;
    g_gameUseCDA = true;
    setPalette(currentGamePalette);

#ifndef AITD_UE4
    if (!make3dTatou())
    {
        makeIntroScreens();
    }

    // Tatou/intros can leave the renderer palette faded; restore the game palette.
    setPalette(currentGamePalette);
#endif

#ifdef DREAMCAST
    dbgio_printf("[dc] startAITD1(): entering main loop\n");
#endif

    while (1)
    {
#ifndef AITD_UE4
        int startupMenuResult = MainMenu();
#else
        int startupMenuResult = 0;
#endif
        switch (startupMenuResult)
        {
        case -1: // timeout
        {
            CVars[getCVarsIdx(CHOOSE_PERSO)] = rand() & 1;
            startGame(7, 1, 0);

            if (!make3dTatou())
            {
                if (!makeIntroScreens())
                {
                    //makeSlideshow();
                }
            }

            break;
        }
        case 0: // new game
        {
            // here, original would ask for protection

#if !TARGET_OS_IOS && !AITD_UE4
            if(ChoosePerso()!=-1)
#endif
            {
                process_events();
                while (key)
                {
                    process_events();
                }

#if !TARGET_OS_IOS
                startGame(7, 1, 0);
#endif

                // here, original would quit if protection flag was false

                startGame(0, 0, 1);
            }

            break;
        }
        case 1: // continue
        {
            // here, original would ask for protection

            if (restoreSave(12, 0))
            {
                // here, original would quit if protection flag was false

                //          updateShaking();

                FlagInitView = 2;

                InitView();

                PlayWorld(1, 1);

                //          freeScene();

                FadeOutPhys(8, 0);
            }

            break;
        }
        case 2: // exit
        {
            freeAll();
            exit(-1);

            break;
        }
        }
    }
}

void AITD1_ReadBook(int index, int type)
{
#ifdef DREAMCAST
    dbgio_printf("[dc] AITD1_ReadBook(index=%d type=%d)\n", index, type);
#endif
    switch (type)
    {
    case 0: // READ_MESSAGE
    {
        LoadPak("ITD_RESS", AITD1_LETTRE, aux);
        turnPageFlag = 0;
        Lire(index, 60, 10, 245, 190, 0, 26, 0);
        break;
    }
    case 1: // READ_BOOK
    {
        LoadPak("ITD_RESS", AITD1_LIVRE, aux);
        turnPageFlag = 1;
        Lire(index, 48, 2, 260, 197, 0, 26, 0);
        break;
    }
    case 2: // READ_CARNET
    {
        LoadPak("ITD_RESS", AITD1_CARNET, aux);
        turnPageFlag = 0;
        Lire(index, 50, 20, 250, 199, 0, 26, 0);
        break;
    }
    default:
        assert(0);
    }
}
