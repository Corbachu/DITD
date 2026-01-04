//----------------------------------------------------------------------------
//  Dream in the Dark Sound Decode/Playback
//----------------------------------------------------------------------------
//
//  Copyright (c) 2025  Corbin Annis/yaz0r/jimmu/FITD Team
//  Copyright (c) 2025  The EDGE Team.
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

#pragma once

#ifdef DREAMCAST

// Ensure KOS sound (and optionally snd_stream) is initialized.
void dc_sound_ensure();

// True when snd_stream_init() succeeded.
bool dc_sound_stream_inited();

// Play an in-memory SFX sample using KOS sfxmgr.
void dc_sound_play_sample(char* samplePtr, int size);

// Optional on-screen diagnostic message (for SFX load/parse failures).
const char* dc_sound_debug_msg();
void dc_sound_debug_msg_tick();

#endif // DREAMCAST
