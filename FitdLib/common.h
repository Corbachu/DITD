//----------------------------------------------------------------------------
//  Dream In The Dark common (Game Code)
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

#ifndef _COMMON_H_
#define _COMMON_H_

#include "config.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

//////////////// GAME SPECIFIC DEFINES

#define NUM_MAX_CAMERA_IN_ROOM 20
//#define NUM_MAX_OBJ         300
#define NUM_MAX_OBJECT       50
#define NUM_MAX_TEXT        40
#define NUM_MAX_MESSAGE     5


// 250
#define NUM_MAX_TEXT_ENTRY  1000

//////////////////

enum enumCVars
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
	JET_SARBACANE,
	TIR_CANON,
	JET_SCALPEL,
	POIVRE,
	DORTOIR,
	EXT_JACK,
	NUM_MATRICE_PROTECT_1,
	NUM_MATRICE_PROTECT_2,
	NUM_PERSO,
	TYPE_INVENTAIRE,
	PROLOGUE,
	POIGNARD,
	MATRICE_FORME,
	MATRICE_COULEUR,

	UNKNOWN_CVAR // for table padding, shouldn't be called !
};

typedef enum enumCVars enumCVars;

extern int AITD1KnownCVars[];
extern int AITD2KnownCVars[];
extern int* currentCVarTable;

int getCVarsIdx(enumCVars);
int getCVarsIdx(int);

//////////////////////

#define	SAMPLE_PAGE				0
#define	BODY_FLAMME				1
#define	MAX_WEIGHT_LOADABLE		2
#define	TEXTE_CREDITS			3
#define	SAMPLE_TONNERRE			4
#define	INTRO_DETECTIVE			5
#define	INTRO_HERITIERE			6
#define	WORLD_NUM_PERSO			7
#define	CHOOSE_PERSO			8
#define	SAMPLE_CHOC				9
#define	SAMPLE_PLOUF			10
#define	REVERSE_OBJECT			11
#define	KILLED_SORCERER			12
#define	LIGHT_OBJECT			13
#define	FOG_FLAG				14
#define	DEAD_PERSO				15


//////////////////

#define TYPE_MASK 0x1D1

#define ANIM_ONCE             0
#define ANIM_REPEAT           1
#define ANIM_UNINTERRUPTABLE  2
#define ANIM_RESET            4

#include "room.h"
#include "vars.h"
#include "main.h"
#include "fileAccess.h"

#include "dc_fastmem.h"

#ifdef DREAMCAST
#include "dc_stdio_shim.h"
#endif
#include "screen.h"
#include "videoMode.h"
#include "pak.h"
#include "unpack.h"
#include "tatou.h"
#include "threadCode.h"
#include "renderer.h"
#include "input.h"
#include "version.h"
#include "cosTable.h"
#include "hqr.h"
#include "gameTime.h"
#include "font.h"
#include "aitdBox.h"
#include "save.h"
#include "anim.h"
#include "animAction.h"
#include "actorList.h"
#include "mainLoop.h"
#include "inventory.h"
#include "startupMenu.h"
#include "systemMenu.h"
#include "floor.h"
#include "object.h"
#include "zv.h"
#include "music.h"
#include "fmopl.h"
#include "main.h"
#include "sequence.h"
#include "palette.h"

// include game specific stuff
#include "AITD1.h"
#include "AITD2.h"
#include "AITD3.h"
#include "JACK.h"

// debugger
#ifdef FITD_DEBUGGER
#include "debugger.h"
#endif

// scripting
#include "track.h"
#include "life.h"
#include "evalVar.h"

#include "osystem.h"


////

//typedef unsigned char byte;

#ifdef UNIX
#define FORCEINLINE static inline
#else
#ifdef WIN32
#define FORCEINLINE __forceinline
#else
#define FORCEINLINE inline
#endif
#endif

#endif
