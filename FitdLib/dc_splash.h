//----------------------------------------------------------------------------
//  Dream In The Dark Dreamcast splash (Dreamcast / KOS)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2026  Corbin Annis
//  Copyright (C) 1999-2026  The EDGE Team
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

// Shows a single full-screen splash frame while the CPU loads data.
//
// Current simplest workflow (no external tools required):
// - Put a 512x512 PNG on the disc as LOADING.png (or LOADING.PNG).
// - The engine will decode it on boot and render it once.
//
// If the file is missing or cannot be loaded/decoded, this function is a no-op.
void dc_show_loading_splash();

#endif // DREAMCAST
