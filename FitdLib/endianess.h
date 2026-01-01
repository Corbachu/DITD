//----------------------------------------------------------------------------
//  Dream In The Dark endianess (Game Code)
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

#ifndef _ENDIANESS_H_
#define _ENDIANESS_H_

#ifdef __GCC__
#define FORCEINLINE static inline
#else
#ifdef WIN32
#define FORCEINLINE __forceinline
#else
#define FORCEINLINE static inline
#endif
#endif

FORCEINLINE u16 READ_LE_U16(void *ptr)
{
#ifdef MACOSX
  return (((u8*)ptr)[1]<<8)|((u8*)ptr)[0];
#else
  return *(u16*)ptr;
#endif
}

FORCEINLINE s16 READ_LE_S16(void *ptr)
{
  return (s16)READ_LE_U16(ptr);
}

FORCEINLINE u16 READ_BE_U16(void *ptr)
{
#ifdef MACOSX
  return *(u16*)ptr;
#else
  return (((u8*)ptr)[1]<<8)|((u8*)ptr)[0];
#endif
}

FORCEINLINE s16 READ_BE_S16(void *ptr)
{
  return (s16)READ_BE_S16(ptr);
}

FORCEINLINE u32 READ_LE_U32(void *ptr)
{
#ifdef MACOSX
  return (((u8*)ptr)[3]<<24)|(((u8*)ptr)[2]<<16)|(((u8*)ptr)[1]<<8)|((u8*)ptr)[0];
#else
  return *(u32*)ptr;
#endif
}

FORCEINLINE s32 READ_LE_S32(void *ptr)
{
  return (s32)READ_LE_U32(ptr);
}

FORCEINLINE u32 READ_BE_U32(void *ptr)
{
#ifdef MACOSX
  return *(u32*)ptr;
#else
  return (((u8*)ptr)[3]<<24)|(((u8*)ptr)[2]<<16)|(((u8*)ptr)[1]<<8)|((u8*)ptr)[0];
#endif
}

FORCEINLINE s32 READ_BE_S32(void *ptr)
{
  return (s32)READ_LE_U32(ptr);
}

#endif
