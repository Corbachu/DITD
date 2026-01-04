//----------------------------------------------------------------------------
//  Dream in the Dark Music Streaming Support
//  ADL/OPL music streaming via KOS snd_stream
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

#include <cstdint>

// Notify the music backend that a new song was loaded; causes a coordinated flush.
void dc_adlib_notify_song_changed();

// Poll the stream from the main/render thread if no dedicated poll thread exists.
void dc_music_poll();

// Start/maintain AdLib music playback. Call this from osystem_playAdlib().
void dc_music_play_adlib(int musicVolume);

#endif // DREAMCAST
