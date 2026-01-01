//----------------------------------------------------------------------------
//  Dream In The Dark endian read helpers
//----------------------------------------------------------------------------
//
//  Centralized unaligned endian-safe reads used by legacy code.
//  Types come from EPI (via FitdLib/config.h).
//
//----------------------------------------------------------------------------

#pragma once

#include <string.h>

#ifndef FORCEINLINE
#define FORCEINLINE static inline
#endif

FORCEINLINE u8 READ_LE_U8(void *ptr)
{
    return *(u8*)ptr;
}

FORCEINLINE s8 READ_LE_S8(void *ptr)
{
    return *(s8*)ptr;
}

FORCEINLINE u16 READ_LE_U16(void *ptr)
{
#ifdef MACOSX
    return (((u8*)ptr)[1] << 8) | ((u8*)ptr)[0];
#else
    u16 temp;
    memcpy(&temp, ptr, 2);
    return temp;
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
    return (((u8*)ptr)[0] << 8) | ((u8*)ptr)[1];
#endif
}

FORCEINLINE s16 READ_BE_S16(void *ptr)
{
    return (s16)READ_BE_U16(ptr);
}

FORCEINLINE u32 READ_LE_U32(void *ptr)
{
#ifdef MACOSX
    return (((u8*)ptr)[3] << 24) | (((u8*)ptr)[2] << 16) | (((u8*)ptr)[1] << 8) | ((u8*)ptr)[0];
#else
    u32 temp;
    memcpy(&temp, ptr, 4);
    return temp;
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
    return (((u8*)ptr)[3] << 24) | (((u8*)ptr)[2] << 16) | (((u8*)ptr)[1] << 8) | ((u8*)ptr)[0];
#endif
}

FORCEINLINE s32 READ_BE_S32(void *ptr)
{
    return (s32)READ_LE_U32(ptr);
}
