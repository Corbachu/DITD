//------------------------------------------------------------------------
//  libfastmem Dreamcast Shim
//------------------------------------------------------------------------
//
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
//------------------------------------------------------------------------

// Simple memory shim to centralize fastmem usage on Dreamcast
// Falls back to standard libc on other platforms

#pragma once

#ifdef _arch_dreamcast
#include <fastmem/fastmem.h>
#ifndef fm_memcpy
#define fm_memcpy memcpy
#endif
#ifndef fm_memset
#define fm_memset memset
#endif
#else
#include <string.h>
#define fm_memcpy memcpy
#define fm_memset memset
#endif
