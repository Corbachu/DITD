//----------------------------------------------------------------------------
//  EDGE SDL Controller Stuff
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2025  The EDGE Team.
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

#include "System/dc_timing.h"

#include <kos.h>

#include <cstdarg>
#include <cstdio>

extern "C" {
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/maple/purupuru.h>
#include <dc/maple/mouse.h>
#include <dc/maple/keyboard.h>
}

#undef DEBUG_KB

// -------------------------------------------------------------------------
// DITD compatibility layer
//
// The original dreamEDGE implementation generates key/mouse events into an
// internal event queue (E_PostEvent). DITD's engine expects three globals:
// `JoyD` (held d-pad), `Click` (held click), and `key` (held key code).
//
// We keep the overall structure (controller/mouse/keyboard polling + delta
// tracking) but translate posted events directly into those globals.
// -------------------------------------------------------------------------

// The engine-defined globals live in vars.cpp.
extern char JoyD;
extern char Click;
extern char key;

// Treat everything as "menu active" for now so START behaves like Enter and
// D-pad maps to navigation consistently while debugging.
static bool menuactive = false;

void DC_SetMenuActive(bool active)
{
    menuactive = active;
}

bool DC_IsMenuActive()
{
    return menuactive;
}

enum
{
    ev_keydown = 1,
    ev_keyup   = 2,
    ev_mouse   = 3,
};

struct event_t
{
    int type;
    union
    {
        struct
        {
            int sym;
            int unicode;
        } key;
        struct
        {
            short dx;
            short dy;
        } mouse;
    } value;
};

// Minimal key constants used by this file.
static constexpr int KEYD_ENTER      = 0x1C;
static constexpr int KEYD_ESCAPE     = 0x1B;
static constexpr int KEYD_BACKSPACE  = 0x0E;
static constexpr int KEYD_TAB        = 0x0F;

static constexpr int KEYD_UPARROW    = 0x1001;
static constexpr int KEYD_DOWNARROW  = 0x1002;
static constexpr int KEYD_LEFTARROW  = 0x1003;
static constexpr int KEYD_RIGHTARROW = 0x1004;

static constexpr int KEYD_MOUSE1     = 0x1101;
static constexpr int KEYD_MOUSE2     = 0x1102;
static constexpr int KEYD_MOUSE3     = 0x1103;
static constexpr int KEYD_WHEEL_UP   = 0x1111;
static constexpr int KEYD_WHEEL_DN   = 0x1112;

// Keep these defined so the original key_table compiles.
static constexpr int KEYD_F1      = 0x1201;
static constexpr int KEYD_F2      = 0x1202;
static constexpr int KEYD_F3      = 0x1203;
static constexpr int KEYD_F4      = 0x1204;
static constexpr int KEYD_F5      = 0x1205;
static constexpr int KEYD_F6      = 0x1206;
static constexpr int KEYD_F7      = 0x1207;
static constexpr int KEYD_F8      = 0x1208;
static constexpr int KEYD_F9      = 0x1209;
static constexpr int KEYD_F10     = 0x120A;
static constexpr int KEYD_F11     = 0x120B;
static constexpr int KEYD_F12     = 0x120C;
static constexpr int KEYD_PRTSCR  = 0x1210;
static constexpr int KEYD_SCRLOCK = 0x1211;
static constexpr int KEYD_PAUSE   = 0x1212;
static constexpr int KEYD_INSERT  = 0x1213;
static constexpr int KEYD_HOME    = 0x1214;
static constexpr int KEYD_PGUP    = 0x1215;
static constexpr int KEYD_DELETE  = 0x1216;
static constexpr int KEYD_END     = 0x1217;
static constexpr int KEYD_PGDN    = 0x1218;
static constexpr int KEYD_NUMLOCK = 0x1219;
static constexpr int KEYD_KP_SLASH = 0x1220;
static constexpr int KEYD_KP_STAR  = 0x1221;
static constexpr int KEYD_KP_MINUS = 0x1222;
static constexpr int KEYD_KP_PLUS  = 0x1223;
static constexpr int KEYD_KP_ENTER = 0x1224;
static constexpr int KEYD_KP1      = 0x1225;
static constexpr int KEYD_KP2      = 0x1226;
static constexpr int KEYD_KP3      = 0x1227;
static constexpr int KEYD_KP4      = 0x1228;
static constexpr int KEYD_KP5      = 0x1229;
static constexpr int KEYD_KP6      = 0x122A;
static constexpr int KEYD_KP7      = 0x122B;
static constexpr int KEYD_KP8      = 0x122C;
static constexpr int KEYD_KP9      = 0x122D;
static constexpr int KEYD_KP0      = 0x122E;
static constexpr int KEYD_KP_DOT   = 0x122F;
static constexpr int KEYD_RALT     = 0x1230;
static constexpr int KEYD_RCTRL    = 0x1231;
static constexpr int KEYD_RSHIFT   = 0x1232;

// Joystick key range (unused by DITD; keep for compilation).
static constexpr int KEYD_JOY_1_1 = 0x2000;

static int I_GetTime(void);
static int I_GetMillies(void);

void I_Printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    dbgio_printf("%s", buf);

    va_end(ap);
}

// KOS controller button polarity varies across versions/builds.
// Some expose `buttons` as active-low (0 == pressed), others as active-high
// (1 == pressed). Auto-detect using a simple heuristic (fewer pressed bits is
// more plausible at rest) and use that consistently.
static int g_kosButtonsActiveLow = -1; // -1 unknown, 0 active-high, 1 active-low

static inline bool KOS_ButtonPressed(uint32_t rawButtons, uint32_t mask)
{
    if (g_kosButtonsActiveLow < 0)
    {
        const uint32_t masks[] = {
            (uint32_t)CONT_DPAD_UP,
            (uint32_t)CONT_DPAD_DOWN,
            (uint32_t)CONT_DPAD_LEFT,
            (uint32_t)CONT_DPAD_RIGHT,
            (uint32_t)CONT_START,
            (uint32_t)CONT_A,
            (uint32_t)CONT_B,
            (uint32_t)CONT_X,
            (uint32_t)CONT_Y,
        };

        int pressedLow = 0;
        int pressedHigh = 0;
        for (uint32_t m : masks)
        {
            pressedLow  += ((rawButtons & m) == 0) ? 1 : 0;
            pressedHigh += ((rawButtons & m) != 0) ? 1 : 0;
        }

        // Choose the interpretation that yields fewer pressed buttons.
        // Tie-break toward active-high.
        g_kosButtonsActiveLow = (pressedLow < pressedHigh) ? 1 : 0;
        I_Printf("[dc-input] KOS buttons polarity: %s (raw=0x%08X low=%d high=%d)\n",
                 g_kosButtonsActiveLow ? "active-low" : "active-high",
                 (unsigned)rawButtons,
                 pressedLow,
                 pressedHigh);
    }

    return g_kosButtonsActiveLow ? ((rawButtons & mask) == 0) : ((rawButtons & mask) != 0);
}

static int M_CheckParm(const char * /*parm*/)
{
    // DITD doesn't use the dreamEDGE-style argv parsing.
    return 0;
}

static void E_PostEvent(const event_t *event)
{
    if (!event)
        return;

    if (event->type == ev_mouse)
        return;

    const bool down = (event->type == ev_keydown);
    const int sym = event->value.key.sym;

    switch (sym)
    {
        case KEYD_UPARROW:
            if (down) JoyD |= 1; else JoyD &= ~1;
            break;
        case KEYD_DOWNARROW:
            if (down) JoyD |= 2; else JoyD &= ~2;
            break;
        case KEYD_LEFTARROW:
            if (down) JoyD |= 4; else JoyD &= ~4;
            break;
        case KEYD_RIGHTARROW:
            if (down) JoyD |= 8; else JoyD &= ~8;
            break;

        case KEYD_MOUSE1:
            if (down)
            {
                Click = 1;
                key = (char)KEYD_ENTER;
            }
            else
            {
                Click = 0;
                if (key == (char)KEYD_ENTER)
                    key = 0;
            }
            break;

        case KEYD_ENTER:
            if (down)
                key = (char)KEYD_ENTER;
            else if (key == (char)KEYD_ENTER)
                key = 0;
            break;

        case KEYD_ESCAPE:
            if (down)
                key = (char)KEYD_ESCAPE;
            else if (key == (char)KEYD_ESCAPE)
                key = 0;
            break;

        default:
            // Ignore everything else for now.
            break;
    }
}

static bool nojoy;  // joysticks completely disabled
static bool ff_enable;

static bool has_mouse;
static bool has_keyboard;

static int num_joys;
static int joystick_device;   // 0 for none
static int joystick_device2;  // 0 for none
static int twinstick_device;  // 0 for none
static int twinstick_device2; // 0 for none

static int ff_timeout[2]   = { 0, 0 };
static int ff_frequency[2] = { 0, 0 };
static int ff_intensity[2] = { 0, 0 };
static int ff_select[2]    = { 0, 0 };

// KOS button mapping into a compact bit-index space expected by the original
// dreamEDGE event generation code.
enum
{
    PAD_UP = 0,
    PAD_DOWN,
    PAD_LEFT,
    PAD_RIGHT,
    PAD_START,
    PAD_A,
    PAD_B,
    PAD_X,
    PAD_Y,
    PAD_D,
    PAD_LTRIGGER,
    PAD_RTRIGGER,

    // Analogue stick fakes (used only for menu nav mapping).
    PAD_UPS,
    PAD_DOWNS,
    PAD_LEFTS,
    PAD_RIGHTS,

    // Legacy/unused indices kept so helper code compiles.
    PAD_UP2,
    PAD_DOWN2,
    PAD_LEFT2,
    PAD_RIGHT2,
    PAD_ALT_X,
};

static maple_device_t *cont, *kbd, *mky;
static cont_state_t *cstate;
static kbd_state_t *kstate;
static mouse_state_t *mstate;

int key_table[128] = {
 0, 0, 0, 0, 'a', 'b', 'c', 'd',                // 00-07
 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',        // 08-0F
 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',        // 10-17
 'u', 'v', 'w', 'x', 'y', 'z', '1', '2',        // 18-1F
 '3', '4', '5', '6', '7', '8', '9', '0',        // 20-27
 KEYD_ENTER, KEYD_ESCAPE, KEYD_BACKSPACE, KEYD_TAB, ' ', '-', '=', '[',        // 28-2F
 ']', '\\', 0, ';', '\'', '`', ',', '.',        // 30-37
 '/', 0, KEYD_F1, KEYD_F2, KEYD_F3, KEYD_F4, KEYD_F5, KEYD_F6,        // 38-3F
 KEYD_F7, KEYD_F8, KEYD_F9, KEYD_F10, KEYD_F11, KEYD_F12, KEYD_PRTSCR, KEYD_SCRLOCK,        // 40-47
 KEYD_PAUSE, KEYD_INSERT, KEYD_HOME, KEYD_PGUP, KEYD_DELETE, KEYD_END, KEYD_PGDN, KEYD_RIGHTARROW,        // 48-4F
 KEYD_LEFTARROW, KEYD_DOWNARROW, KEYD_UPARROW, KEYD_NUMLOCK, KEYD_KP_SLASH, KEYD_KP_STAR, KEYD_KP_MINUS, KEYD_KP_PLUS,        // 50-57
 KEYD_KP_ENTER, KEYD_KP1, KEYD_KP2, KEYD_KP3, KEYD_KP4, KEYD_KP5, KEYD_KP6, KEYD_KP7,        // 58-5F
 KEYD_KP8, KEYD_KP9, KEYD_KP0, KEYD_KP_DOT, 0, 0, 0, 0,        // 60-67
 0, 0, 0, 0, 0, 0, 0, 0,        // 68-6F
 0, 0, 0, 0, 0, 0, 0, 0,        // 70-77
 0, 0, 0, 0, 0, 0, 0, 0         // 78-7F
};

static unsigned int previous[2];
static int prev_mkyb, prev_mkyw, prev_shift;
static int tshift[2];
static bool prev_valid[2];

static inline bool IsMoveForwardBackPadButton(int pad_button)
{
    switch (pad_button)
    {
        case PAD_UP:
        case PAD_DOWN:
        case PAD_UP2:
        case PAD_DOWN2:
        case PAD_UPS:
        case PAD_DOWNS:
            return true;

        default:
            return false;
    }
}

void DC_Tactile (int freq, int intensity, int select)
{
    ff_frequency[select] = freq;
    ff_intensity[select] = intensity;
    ff_select[select] = 0;
    ff_timeout[select] = I_GetTime() + 35;

#ifdef DEBUG_DREAMEDGE
    I_Printf("I_Tactile: %d, %d, %d\n", freq, intensity, select);
#endif
    // FF will be handled at the same time as the events
}

static void dc_handle_ff(int joy)
{
    maple_device_t *jump;
    int ix = ((joy + 1) == joystick_device) ? 0 : 1;

    jump = maple_enum_type(joy, MAPLE_FUNC_PURUPURU);

    if (ff_timeout[ix] && jump)
    {
        if (I_GetTime() >= ff_timeout[ix])
        {
            static purupuru_effect_t effect[2];
            // stop force feedback if still on
            effect[ix].duration = 0x00;
            effect[ix].effect2 = 0x00;
            effect[ix].effect1 = 0x00;
            effect[ix].special = PURUPURU_SPECIAL_MOTOR1;
            purupuru_rumble(jump, &effect[ix]);
            ff_timeout[ix] = 0;
            ff_intensity[ix] = 0;
        }
    }

    if (ff_intensity[ix] && jump)
    {
        static purupuru_effect_t effect[2];
        // start force-feedback
        effect[ix].duration = 30;
        effect[ix].effect2 = ff_frequency[ix];
        effect[ix].effect1 = PURUPURU_EFFECT1_INTENSITY(ff_intensity[ix]);
        effect[ix].special = PURUPURU_SPECIAL_MOTOR1 | ff_select[ix];
        purupuru_rumble(jump, &effect[ix]);
        ff_intensity[ix] = 0;
    }
}

static void dc_handle_ts(int joy)
{
    event_t event;
    unsigned int buttons;
    int i;
    unsigned int j;
    int ix = ((joy + 1) == twinstick_device) ? 0 : 1;

    cont = maple_enum_type(joy, MAPLE_FUNC_CONTROLLER);

    if (cont)
        cstate = (cont_state_t *)maple_dev_status(cont);
    else
        cstate = NULL;

    if (!cstate)
    {
        buttons = previous[ix] = 0;

        // Force re-sync when the device disappears.
        prev_valid[ix] = false;
		return;
    }

    // Convert KOS button bitfield into our active-high (1 == pressed) format.
    buttons = 0;
    const uint32_t raw = (uint32_t)cstate->buttons;
    if (KOS_ButtonPressed(raw, CONT_DPAD_UP))    buttons |= 1u << PAD_UP;
    if (KOS_ButtonPressed(raw, CONT_DPAD_DOWN))  buttons |= 1u << PAD_DOWN;
    if (KOS_ButtonPressed(raw, CONT_DPAD_LEFT))  buttons |= 1u << PAD_LEFT;
    if (KOS_ButtonPressed(raw, CONT_DPAD_RIGHT)) buttons |= 1u << PAD_RIGHT;
    if (KOS_ButtonPressed(raw, CONT_START))      buttons |= 1u << PAD_START;
    if (KOS_ButtonPressed(raw, CONT_A))          buttons |= 1u << PAD_A;
    if (KOS_ButtonPressed(raw, CONT_B))          buttons |= 1u << PAD_B;
    if (KOS_ButtonPressed(raw, CONT_X))          buttons |= 1u << PAD_X;
    if (KOS_ButtonPressed(raw, CONT_Y))          buttons |= 1u << PAD_Y;

    // First valid read: prime state without emitting events (avoids startup
    // garbage/glitches looking like a burst of button presses).
    if (!prev_valid[ix])
    {
        previous[ix] = buttons;
        prev_valid[ix] = true;
        return;
    }

    // now handle controller buttons
    if (buttons == previous[ix])
        return; // no changes

    for (i=0, j=1u; i<18; i++, j<<=1)
    {
        if ((buttons ^ previous[ix]) & j)
        {
            // button changed
            event.type = (buttons & j) ? ev_keydown : ev_keyup;
            switch (i)
            {
                case PAD_B:
                    event.value.key.sym = KEYD_ESCAPE;
                    break;
                case PAD_A:
                case PAD_X:
                    event.value.key.sym = KEYD_MOUSE1;
                    break;
                case PAD_START:
                    event.value.key.sym = menuactive ? KEYD_ENTER : KEYD_ESCAPE;
                    break;
                case PAD_UP:
                case PAD_UP2:
                    event.value.key.sym = KEYD_UPARROW;
                    break;
                case PAD_DOWN:
                case PAD_DOWN2:
                    event.value.key.sym = KEYD_DOWNARROW;
                    break;
                case PAD_LEFT:
                case PAD_LEFT2:
                    event.value.key.sym = KEYD_LEFTARROW;
                    break;
                case PAD_RIGHT:
                case PAD_RIGHT2:
                    event.value.key.sym = KEYD_RIGHTARROW;
                    break;
                default:
                    event.value.key.sym = KEYD_JOY_1_1 + i + (ix * 18);
                    break;
            }
			E_PostEvent(&event);
        }
    }
    previous[ix] = buttons;
}

static void dc_handle_pad(int joy)
{
    event_t event;
    unsigned int buttons;
    int i, j;
    int ix = ((joy + 1) == joystick_device) ? 0 : 1;

	if ((joy + 1) == twinstick_device || (joy + 1) == twinstick_device2)
	{
		dc_handle_ts(joy);
		return;
	}

    cont = maple_enum_type(joy, MAPLE_FUNC_CONTROLLER);

    if (cont)
        cstate = (cont_state_t *)maple_dev_status(cont);
    else
        cstate = NULL;

    if (!cstate)
    {
        // Force re-sync when the device disappears.
        prev_valid[ix] = false;
        return;
    }

    // Build an active-high bitfield (1 == pressed) using KOS masks.
    buttons = 0;

    const uint32_t raw = (uint32_t)cstate->buttons;
    if (KOS_ButtonPressed(raw, CONT_DPAD_UP))    buttons |= 1u << PAD_UP;
    if (KOS_ButtonPressed(raw, CONT_DPAD_DOWN))  buttons |= 1u << PAD_DOWN;
    if (KOS_ButtonPressed(raw, CONT_DPAD_LEFT))  buttons |= 1u << PAD_LEFT;
    if (KOS_ButtonPressed(raw, CONT_DPAD_RIGHT)) buttons |= 1u << PAD_RIGHT;

    if (KOS_ButtonPressed(raw, CONT_START))      buttons |= 1u << PAD_START;
    if (KOS_ButtonPressed(raw, CONT_A))          buttons |= 1u << PAD_A;
    if (KOS_ButtonPressed(raw, CONT_B))          buttons |= 1u << PAD_B;
    if (KOS_ButtonPressed(raw, CONT_X))          buttons |= 1u << PAD_X;
    if (KOS_ButtonPressed(raw, CONT_Y))          buttons |= 1u << PAD_Y;

    // Triggers are treated as digital.
    buttons |= (cstate->ltrig > 32) ? (1u << PAD_LTRIGGER) : 0;
    buttons |= (cstate->rtrig > 32) ? (1u << PAD_RTRIGGER) : 0;

    // IMPORTANT: Do not map the analogue stick into d-pad directions.
    // Many pads report a non-zero stick offset at rest (or the type/sign of
    // joyx/joyy varies), which can look like "stuck" navigation.

    // For debugging "ghost input" on boot: dump raw state briefly.
    {
        static uint32_t last_log_ms = 0;
        const uint32_t now_ms = (uint32_t)timer_ms_gettime64();
        if (buttons != 0 && now_ms < 3000 && (now_ms - last_log_ms) > 200)
        {
            last_log_ms = now_ms;
            I_Printf("[dc-input] t=%ums raw=0x%08X btns=0x%08X joyx=%d joyy=%d l=%d r=%d menu=%d JoyD=%d Click=%d key=%d\n",
                     (unsigned)now_ms,
                     (unsigned)cstate->buttons,
                     (unsigned)buttons,
                     (int)cstate->joyx,
                     (int)cstate->joyy,
                     (int)cstate->ltrig,
                     (int)cstate->rtrig,
                     menuactive ? 1 : 0,
                     (int)JoyD,
                     (int)Click,
                     (int)key);
        }
    }

    // First valid read: prime state without emitting events (avoids startup
    // garbage/glitches looking like a burst of button presses).
    if (!prev_valid[ix])
    {
        previous[ix] = buttons;
        prev_valid[ix] = true;
        return;
    }

    // now handle controller buttons
    if (buttons == previous[ix])
        return; // no changes

    for (i=0, j=1; i<32; i++, j<<=1)
    {
        if ((buttons ^ previous[ix]) & j)
        {
            // button changed
            event.type = (buttons & j) ? ev_keydown : ev_keyup;
            switch (i)
            {
                case PAD_B:
                    event.value.key.sym = KEYD_ESCAPE;
                    break;
                case PAD_A:
                    event.value.key.sym = KEYD_MOUSE1;
                    break;
                case PAD_START:
                    event.value.key.sym = menuactive ? KEYD_ENTER : KEYD_ESCAPE;
                    break;
                case PAD_UP:
                    event.value.key.sym = KEYD_UPARROW;
                    break;
                case PAD_DOWN:
                    event.value.key.sym = KEYD_DOWNARROW;
                    break;
                case PAD_LEFT:
                    event.value.key.sym = KEYD_LEFTARROW;
                    break;
                case PAD_RIGHT:
                    event.value.key.sym = KEYD_RIGHTARROW;
                    break;
                default:
                    event.value.key.sym = KEYD_JOY_1_1 + i + (ix * 32);
                    break;
            }

#ifdef DEBUG_CTRL_VALUES
            if (!menuactive && IsMoveForwardBackPadButton(i))
            {
                // Only log forward/back related inputs to avoid spam.
                I_Printf("[PAD%d] %s btn=%d sym=0x%X joyx=%d joyy=%d ltrig=%d rtrig=%d buttons=0x%08X\n",
                        ix + 1,
                        (event.type == ev_keydown) ? "DOWN" : "UP",
                        i,
                        (unsigned)event.value.key.sym,
                        joyx,
                        joyy,
                        (int)cstate->ltrig,
                        (int)cstate->rtrig,
                        (unsigned)buttons);
            }
#endif
			E_PostEvent(&event);
        }
    }
    previous[ix] = buttons;
}

static void dc_handle_mky(void)
{
    event_t event;
    short mousex, mousey;
    int mouseb, mousew;

    mky = maple_enum_type(0, MAPLE_FUNC_MOUSE);

    if (mky)
        mstate = (mouse_state_t *)maple_dev_status(mky);
    else
        mstate = NULL;

    // use mouse as mouse if found
    if (mstate)
    {
        mousex = mstate->dx;
        mousey = -mstate->dy;
        // handle mouse move
        if (mousex || mousey)
        {
            event.type = ev_mouse;
            event.value.mouse.dx = mousex;
            event.value.mouse.dy = mousey;
            E_PostEvent(&event);
        }

		// find wheel direction
        mousew = (mstate->dz < 0) ? 1 : (mstate->dz > 0) ? 2 : 0;
        if (prev_mkyw == -1)
            prev_mkyw = mousew;

        // handle mouse wheel
        if (mousew != prev_mkyw)
        {
			//I_Printf("Mouse wheel = %d\n", mstate->dz);
            if ((mousew ^ prev_mkyw) & 1)
            {
                // mouse wheel up
                if (mousew & 1)
                {
                    event.type = ev_keydown;
                    event.value.key.sym = KEYD_WHEEL_UP;
                    E_PostEvent(&event);
                }
                else
                {
                    event.type = ev_keyup;
                    event.value.key.sym = KEYD_WHEEL_UP;
                    E_PostEvent(&event);
                }
            }
            if ((mousew ^ prev_mkyw) & 2)
            {
                // mouse wheel down
                if (mousew & 2)
                {
                    event.type = ev_keydown;
                    event.value.key.sym = KEYD_WHEEL_DN;
                    E_PostEvent(&event);
                }
                else
                {
                    event.type = ev_keyup;
                    event.value.key.sym = KEYD_WHEEL_DN;
                    E_PostEvent(&event);
                }
            }
            prev_mkyw = mousew;
        }

        mouseb = (mstate->buttons & MOUSE_LEFTBUTTON ? 1 : 0) | (mstate->buttons & MOUSE_RIGHTBUTTON ? 2 : 0) | (mstate->buttons & MOUSE_SIDEBUTTON ? 4 : 0);
        if (prev_mkyb == -1)
            prev_mkyb = mouseb;

        // handle mouse buttons
        if (mouseb != prev_mkyb)
        {
            if ((mouseb ^ prev_mkyb) & 1)
            {
                // left mouse button changed
                if (mouseb & 1)
                {
                    event.type = ev_keydown;
                    event.value.key.sym = KEYD_MOUSE1;
                    E_PostEvent(&event);
                }
                else
                {
                    event.type = ev_keyup;
                    event.value.key.sym = KEYD_MOUSE1;
                    E_PostEvent(&event);
                }
            }
            if ((mouseb ^ prev_mkyb) & 2)
            {
                // right mouse button changed
                if (mouseb & 2)
                {
                    event.type = ev_keydown;
                    event.value.key.sym = KEYD_MOUSE2;
                    E_PostEvent(&event);
                }
                else
                {
                    event.type = ev_keyup;
                    event.value.key.sym = KEYD_MOUSE2;
                    E_PostEvent(&event);
                }
            }
            if ((mouseb ^ prev_mkyb) & 4)
            {
                // side mouse button changed
                if (mouseb & 4)
                {
                    event.type = ev_keydown;
                    event.value.key.sym = KEYD_MOUSE3;
                    E_PostEvent(&event);
                }
                else
                {
                    event.type = ev_keyup;
                    event.value.key.sym = KEYD_MOUSE3;
                    E_PostEvent(&event);
                }
            }
            prev_mkyb = mouseb;
        }
    }
}

static void dc_handle_kbd(void)
{
    event_t event;

    kbd = maple_enum_type(0, MAPLE_FUNC_KEYBOARD);

    if (kbd)
        kstate = (kbd_state_t *)maple_dev_status(kbd);
    else
        kstate = NULL;

    // use keyboard if found
    if (kstate)
    {
        static uint8 prev_matrix[128];
        int i;

        if (prev_shift == -1)
        {
            prev_shift = kstate->last_modifiers.raw;
            memset(prev_matrix, 0, 128);
        }

        switch(kstate->last_modifiers.raw ^ prev_shift)
        {
            case KBD_MOD_LALT:
            case KBD_MOD_RALT:
                if (kstate->last_modifiers.raw & (KBD_MOD_LALT | KBD_MOD_RALT))
                {
                    event.type = ev_keydown;
                    event.value.key.sym = KEYD_RALT;
                    E_PostEvent (&event);
                }
                else
                {
                    event.type = ev_keyup;
                    event.value.key.sym = KEYD_RALT;
                    E_PostEvent (&event);
                }
                break;

            case KBD_MOD_LCTRL:
            case KBD_MOD_RCTRL:
                if (kstate->last_modifiers.raw & (KBD_MOD_LCTRL | KBD_MOD_RCTRL))
                {
                    event.type = ev_keydown;
                    event.value.key.sym = KEYD_RCTRL;
                    E_PostEvent (&event);
                }
                else
                {
                    event.type = ev_keyup;
                    event.value.key.sym = KEYD_RCTRL;
                    E_PostEvent (&event);
                }
                break;

            case KBD_MOD_LSHIFT:
            case KBD_MOD_RSHIFT:
                if (kstate->last_modifiers.raw & (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT))
                {
                    event.type = ev_keydown;
                    event.value.key.sym = KEYD_RSHIFT;
                    E_PostEvent (&event);
                }
                else
                {
                    event.type = ev_keyup;
                    event.value.key.sym = KEYD_RSHIFT;
                    E_PostEvent (&event);
                }
                break;
        }
        prev_shift = kstate->last_modifiers.raw;

        // handle regular keys
        for (i=4; i<0x65; i++)
        {
            const uint8 is_down = kstate->key_states[i].is_down ? 1 : 0;

            if (is_down != 0 && prev_matrix[i] == 0 && key_table[i] != 0)
            {
                event.type = ev_keydown;
                event.value.key.unicode = event.value.key.sym = key_table[i];
                E_PostEvent (&event);
            }
            else if (is_down == 0 && prev_matrix[i] != 0 && key_table[i] != 0)
            {
                event.type = ev_keyup;
                event.value.key.unicode = event.value.key.sym = key_table[i];
                E_PostEvent (&event);
            }

            prev_matrix[i] = is_down;
        }
    }
}

void dc_get_controllers()
{
    int i;

    // init DC controller stuff
    twinstick_device = twinstick_device2 = 0;
    for (i=num_joys=0; i<4; i++)
    {
        cont = maple_enum_type(i, MAPLE_FUNC_CONTROLLER);
        if (!cont)
            break;

        num_joys++;

        // look for Twin Sticks
        if (memcmp(cont->info.product_name, "Twin Stick", 10))
            continue; // not Twin Stick, skip
        if (!twinstick_device)
            twinstick_device = i + 1;
        else if (!twinstick_device2)
            twinstick_device2 = i + 1;
    }
	//default to using first two controllers
    joystick_device = num_joys ? 1 : 0;
    joystick_device2 = num_joys >= 2 ? 2 : 0;

	// if have Twin Stick controller, default to using it
    if (twinstick_device)
		joystick_device = twinstick_device;
    if (twinstick_device2)
		joystick_device2 = twinstick_device2;

	// look for conflict between sticks
	if (joystick_device == joystick_device2)
		joystick_device2 = joystick_device == 2 ? 1 : 2;

    kbd = maple_enum_type(0, MAPLE_FUNC_KEYBOARD);
    has_keyboard = kbd ? true : false;
    mky = maple_enum_type(0, MAPLE_FUNC_MOUSE);
    has_mouse = mky ? true : false;
}

int I_JoyGetAxis(int n)  // n begins at 0
{
    return 0;
}

void I_CentreMouse(void)
{
}

void I_ShowJoysticks(void)
{
    if (nojoy)
    {
        I_Printf("Joystick system is disabled.\n");
        return;
    }

    if (num_joys == 0)
    {
        I_Printf("No joysticks found.\n");
        return;
    }

    I_Printf("Joysticks:\n");

    for (int i = 0; i < num_joys; i++)
    {
        static const char *name[4] = {
            "JOY1", "JOY2", "JOY3", "JOY4"
        };
        I_Printf("  %2d : %s\n", i+1, name[i]);
    }
}

/****** Input Event Generation ******/

void I_StartupControl(void)
{
    ff_enable = M_CheckParm ("-rumble") ? true : false;
//  stick_disabled = M_CheckParm ("-noanalog") ? true : false;
    nojoy = M_CheckParm ("-nojoy") ? true : false;

    I_Printf("Num Joys: %d, Joy1: %d, Joy2: %d\n %s, %s, %s, %s\n",
        num_joys, joystick_device, joystick_device2, has_keyboard ? "KEYB" : "No KEYB",
        has_mouse ? "MKY" : "No MKY", ff_enable ? "FF" : "No FF",
        nojoy ? "Pads Disabled" : "Pads Enabled");

    I_Printf("Twin Stick 1: %d, Twin Stick 2: %d\n", twinstick_device, twinstick_device2);

    previous[0] = previous[1] = 0;
    prev_valid[0] = prev_valid[1] = false;
    prev_mkyb = prev_mkyw = prev_shift = -1;
    tshift[0] = tshift[1] = ev_keyup;
}

void I_ControlGetEvents(void)
{
    if (!nojoy && joystick_device)
        dc_handle_pad(joystick_device - 1);
    if (!nojoy && joystick_device2)
        dc_handle_pad(joystick_device2 - 1);
    if (ff_enable && joystick_device)
        dc_handle_ff(joystick_device - 1);
    if (ff_enable && joystick_device2)
        dc_handle_ff(joystick_device2 - 1);
    if (has_keyboard)
        dc_handle_kbd();
    if (has_mouse)
        dc_handle_mky();
}

void I_ShutdownControl(void)
{
}

static int I_GetTime(void)
{
    const uint64_t t = timer_ms_gettime64();
    return DC_MsToTics(t);
}


//
// Same as I_GetTime, but returns time in milliseconds
//
static int I_GetMillies(void)
{
    return (int)timer_ms_gettime64();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab