//----------------------------------------------------------------------------
//  Dream In The Dark input (Input)
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

#ifdef DREAMCAST

extern "C" {
#include <dc/maple.h>
#include <dc/maple/controller.h>
}

// Vendored dreamEDGE Dreamcast input handler (adapted for DITD).
extern void dc_get_controllers();
extern void I_StartupControl(void);
extern void I_ControlGetEvents(void);

void readKeyboard(void)
{
    static bool s_inited = false;
    if (!s_inited)
    {
        dc_get_controllers();
        I_StartupControl();
        s_inited = true;
    }

    // The dreamEDGE handler maintains held-state by posting key up/down events.
    // Do NOT clear JoyD/Click/key here.
    I_ControlGetEvents();
}

#else

#include <SDL.h>
#include <backends/imgui_impl_sdl3.h>

extern float nearVal;
extern float farVal;
extern float cameraZoom;
extern float fov;

extern bool debuggerVar_debugMenuDisplayed;

void handleKeyDown(SDL_Event& event)
{
    switch (event.key.key)
    {
    case SDL_SCANCODE_GRAVE:
        debuggerVar_debugMenuDisplayed ^= 1;
        break;
    }
}

#endif // DREAMCAST

#ifndef DREAMCAST
void readKeyboard(void)
{
    SDL_Event event;
    int size;
    int j;
    const bool *keyboard;

    JoyD = 0;
    Click = 0;
    key = 0;

    while (SDL_PollEvent(&event)) {

        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            handleKeyDown(event);
            break;
        case SDL_EVENT_QUIT:
            cleanupAndExit();
            break;
        }

    }

    debuggerVar_fastForward = false;

    keyboard = SDL_GetKeyboardState(&size);

    for (j = 0; j < size; j++)
    {
        if (keyboard[j])
        {
            switch (j)
            {
                /*        case SDLK_z:
                nearVal-=1;
                break;
                case SDLK_x:
                nearVal+=1;
                break;
                case SDLK_a:
                cameraZoom-=100;
                break;
                case SDLK_s:
                cameraZoom+=100;
                break;
                case SDLK_w:
                {
                float factor = (float)cameraY/(float)cameraZ;
                cameraY+=1;
                cameraZ=(float)cameraY/factor;
                break;
                }
                case SDLK_q:
                {
                float factor = (float)cameraY/(float)cameraZ;
                cameraY-=1;
                cameraZ=(float)cameraY/factor;
                break;
                }
                case SDLK_e:
                fov-=1;
                break;
                case SDLK_r:
                fov+=2;
                break; */
            case SDL_SCANCODE_RETURN:
                key = 0x1C;
                break;
            case SDL_SCANCODE_ESCAPE:
                key = 0x1B;
                break;
            case SDL_SCANCODE_UP:
                JoyD |= 1;
                break;

            case SDL_SCANCODE_DOWN:
                JoyD |= 2;
                break;

            case SDL_SCANCODE_RIGHT:
                JoyD |= 8;
                break;

            case SDL_SCANCODE_LEFT:
                JoyD |= 4;
                break;
            case SDL_SCANCODE_SPACE:
                Click = 1;
                break;
#ifdef FITD_DEBUGGER
            case SDL_SCANCODE_O:
                debufferVar_topCameraZoom += 100;
                break;
            case SDL_SCANCODE_P:
                debufferVar_topCameraZoom -= 100;
                break;
            case SDL_SCANCODE_T:
                debuggerVar_topCamera = true;
                backgroundMode = backgroundModeEnum_3D;
                break;
            case SDL_SCANCODE_Y:
                debuggerVar_topCamera = false;
                backgroundMode = backgroundModeEnum_2D;
                FlagInitView = 1;
                break;
            case SDL_SCANCODE_C:
                debuggerVar_noHardClip = !debuggerVar_noHardClip;
                break;
            case SDL_SCANCODE_B:
                backgroundMode = backgroundModeEnum_3D;
                break;
            case SDL_SCANCODE_N:
                backgroundMode = backgroundModeEnum_2D;
                break;
            case SDL_SCANCODE_F:
                debuggerVar_fastForward = true;
                break;


#endif
            }
        }
    }
#ifdef FITD_DEBUGGER
    if (debuggerVar_topCamera)
    {
        backgroundMode = backgroundModeEnum_3D;
    }
#endif
}
#endif // !DREAMCAST
