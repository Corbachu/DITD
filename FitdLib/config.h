//----------------------------------------------------------------------------
//  Dream In The Dark config (Game Code)
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

#pragma once

#if __cplusplus
#include <vector>
#include <string>
#include <format>
#include <optional>
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#include <stdint.h>
#define HAS_STDINT
#endif

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
//#include "config.h"
#endif

// Config IMGUI
#ifdef __APPLE__
    #if TARGET_OS_OSX
        #define USE_SDL 1
        #define FITD_DEBUGGER
        #define USE_IMGUI
        #define USE_OPENGL_3_2
    #endif
    #if TARGET_OS_TV || TARGET_OS_IOS
        #define USE_SDL 1
        #define USE_OPENGLES_3_0
        #define RUN_FULLSCREEN  
        #define USE_IMGUI
        #define FITD_DEBUGGER
    #endif
#else
    #define USE_SDL 1
    #define FITD_DEBUGGER
    #define USE_IMGUI
    #define USE_OPENGL_3_2
#endif

#ifdef AITD_UE4
#undef USE_IMGUI
#endif

#ifdef DREAMCAST
#undef USE_IMGUI
#undef FITD_DEBUGGER
#undef USE_SDL
#undef USE_OPENGL_3_2
#endif

#ifdef MACOSX
#define UNIX
#endif

#define HAS_YM3812 1

#include <stdint.h>

// Prefer EPI's type definitions for core game types.
// Keep <stdint.h> for third-party code that uses uint*_t directly.
#include "types.h"

typedef u8_t  u8;
typedef u16_t u16;
typedef u32_t u32;

typedef s8_t  s8;
typedef s16_t s16;
typedef s32_t s32;

// Legacy aliases used throughout FitdLib.
typedef u8  U8;
typedef u16 U16;
typedef u32 U32;
typedef s8  S8;
typedef s16 S16;
typedef s32 S32;

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#ifdef WIN32
#include <search.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <assert.h>

#ifdef _DEBUG
#define ASSERT(exp) assert(exp)
#else
#define ASSERT(exp)
#endif

#ifdef _DEBUG
#define ASSERT_PTR(exp) assert(exp)
#else
#define ASSERT_PTR(exp)
#endif

