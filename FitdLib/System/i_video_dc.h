//----------------------------------------------------------------------------
//  Dream in the Dark Texture Upload (OpenGL)
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

#include "common.h"

#ifdef DREAMCAST

#ifndef USE_PVR_PAL8

// GLdc-based Dreamcast video backend.
void dc_video_gl_init();
void dc_video_gl_upload_composited();
void dc_video_gl_present();

// Dreamcast GLdc: background mask overlays (used for actor occlusion).
void dc_video_gl_create_mask(const u8* mask320x200, int roomId, int maskId,
							 int maskX1, int maskY1, int maskX2, int maskY2);
void dc_video_gl_queue_mask_draw(int roomId, int maskId,
								 bool hasClip, int clipX1, int clipY1, int clipX2, int clipY2);
void dc_video_gl_clear_mask_queue();

#endif // !USE_PVR_PAL8

#endif // DREAMCAST
