//----------------------------------------------------------------------------
//  Dreamcast EPI System Specifics
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2008  The EDGE Team.
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
//
#ifndef __DREAMCAST_EPI_HEADER__
#define __DREAMCAST_EPI_HEADER__

// Sanity checking...
#ifdef __EPI_HEADER_SYSTEM_SPECIFIC__
#error "Two different system specific EPI headers included"
#else
#define __EPI_HEADER_SYSTEM_SPECIFIC__
#endif

#define DIRSEPARATOR '/'

#define GCCATTR(xyz) __attribute__ (xyz)

// Provide portable case-insensitive compares
#ifdef _WIN32
#include <string.h>
#define stricmp   _stricmp
#define strnicmp  _strnicmp
#else
#include <strings.h>
#define stricmp   strcasecmp
#define strnicmp  strncasecmp
#endif

#ifndef O_BINARY
#define O_BINARY  0
#endif

#ifndef D_OK
#define D_OK  X_OK
#endif

// Avoid redefining C++11 nullptr keyword; only define for pre-C++11 or C code.
#if !defined(__cplusplus) || (defined(__cplusplus) && __cplusplus < 201103L)
#ifndef nullptr
#define nullptr NULL
#endif
#endif

#endif /* __DREAMCAST_EPI_HEADER__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
