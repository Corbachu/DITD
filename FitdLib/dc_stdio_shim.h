//----------------------------------------------------------------------------
//  Dream In The Dark dc stdio shim (Dreamcast / KOS)
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

// Dreamcast/KOS stdio shim:
// Some KOS toolchains/newlib builds can abort inside fopen() on ISO9660.
// Redirect the common stdio file operations used by the game to KOS fs_*.
//
// NOTE: This is intentionally minimal: enough for rb/wb-style usage.

#ifdef DREAMCAST

#include <cstddef>
#include <cstdio>

#ifdef fopen
#undef fopen
#endif
#ifdef fclose
#undef fclose
#endif
#ifdef fread
#undef fread
#endif
#ifdef fwrite
#undef fwrite
#endif
#ifdef fseek
#undef fseek
#endif
#ifdef ftell
#undef ftell
#endif
#ifdef fflush
#undef fflush
#endif
#ifdef feof
#undef feof
#endif
#ifdef ferror
#undef ferror
#endif

extern "C" {
    FILE* dc_fopen(const char* path, const char* mode);
    int dc_fclose(FILE* fp);
    size_t dc_fread(void* ptr, size_t size, size_t nmemb, FILE* fp);
    size_t dc_fwrite(const void* ptr, size_t size, size_t nmemb, FILE* fp);
    int dc_fseek(FILE* fp, long offset, int whence);
    long dc_ftell(FILE* fp);
    int dc_fflush(FILE* fp);
    int dc_feof(FILE* fp);
    int dc_ferror(FILE* fp);
}

#define fopen  dc_fopen
#define fclose dc_fclose
#define fread  dc_fread
#define fwrite dc_fwrite
#define fseek  dc_fseek
#define ftell  dc_ftell
#define fflush dc_fflush
#define feof   dc_feof
#define ferror dc_ferror

#endif // DREAMCAST
