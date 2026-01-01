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

#ifdef DREAMCAST

extern "C" {
#include <kos.h>
}

#include <cerrno>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>

static inline file_t file_from_FILE(FILE* fp) {
    // This project encodes file_t handles into FILE* by adding 1.
    return (file_t)((uintptr_t)fp - 1);
}

static inline FILE* FILE_from_file(file_t fh) {
    return (FILE*)((uintptr_t)fh + 1);
}

static int flags_from_mode(const char* mode) {
    // Minimal mode parsing.
    // "r"/"rb" => O_RDONLY
    // "w"/"wb" => O_WRONLY|O_TRUNC|O_CREAT
    // "a"/"ab" => O_WRONLY|O_APPEND|O_CREAT
    // '+' is treated as read/write when possible.
    if (!mode || !mode[0]) return O_RDONLY;

    bool plus = std::strchr(mode, '+') != nullptr;

    switch (mode[0]) {
    case 'r':
        return plus ? O_RDWR : O_RDONLY;
    case 'w':
        return (plus ? O_RDWR : O_WRONLY) | O_TRUNC | O_CREAT;
    case 'a':
        return (plus ? O_RDWR : O_WRONLY) | O_APPEND | O_CREAT;
    default:
        return O_RDONLY;
    }
}

extern "C" FILE* dc_fopen(const char* path, const char* mode) {
    if (!path) {
        errno = EINVAL;
        return nullptr;
    }

    int oflags = flags_from_mode(mode);

    // Trace file open attempts (keep it short; this runs a lot).
    //dbgio_printf("[dc_fopen] %s mode=%s oflags=0x%x\n", path, mode ? mode : "(null)", oflags);

    // On Dreamcast we primarily load data from /cd. Some environments have
    // brittle cwd/relative-path behavior, so for read-only opens we fall back
    // to an absolute /cd/ path when the caller passes a relative filename.
    const bool wants_write = (oflags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND)) != 0;
    file_t fh = -1;
    if (!wants_write && path[0] != '/') {
        char cd_path[1024];
        snprintf(cd_path, sizeof(cd_path), "/cd/%s", path);
        //dbgio_printf("[dc_fopen] -> try %s\n", cd_path);
        fh = fs_open(cd_path, oflags);
    }
    if (fh < 0) {
        fh = fs_open(path, oflags);
    }
    //dbgio_printf("[dc_fopen] fs_open -> %d\n", (int)fh);
    if (fh < 0) {
        // errno isn't reliably set by fs_open.
        errno = ENOENT;
        return nullptr;
    }

    return FILE_from_file(fh);
}

extern "C" int dc_fclose(FILE* fp) {
    if (!fp) return 0;
    file_t fh = file_from_FILE(fp);
    fs_close(fh);
    return 0;
}

extern "C" size_t dc_fread(void* ptr, size_t size, size_t nmemb, FILE* fp) {
    if (!fp || !ptr) return 0;
    size_t total = size * nmemb;
    if (total == 0) return 0;

    file_t fh = file_from_FILE(fp);
    int rc = fs_read(fh, ptr, total);
    if (rc <= 0) return 0;

    return (size_t)rc / size;
}

extern "C" size_t dc_fwrite(const void* ptr, size_t size, size_t nmemb, FILE* fp) {
    if (!fp || !ptr) return 0;
    size_t total = size * nmemb;
    if (total == 0) return 0;

    file_t fh = file_from_FILE(fp);
    int rc = fs_write(fh, ptr, total);
    if (rc <= 0) return 0;

    return (size_t)rc / size;
}

extern "C" int dc_fseek(FILE* fp, long offset, int whence) {
    if (!fp) return -1;
    file_t fh = file_from_FILE(fp);
    int rc = fs_seek(fh, offset, whence);
    return rc;
}

extern "C" long dc_ftell(FILE* fp) {
    if (!fp) return -1;
    file_t fh = file_from_FILE(fp);
    return (long)fs_tell(fh);
}

extern "C" int dc_fflush(FILE* /*fp*/) {
    // No buffered stdio state here.
    return 0;
}

extern "C" int dc_feof(FILE* /*fp*/) {
    // Not tracked; callers should use read result.
    return 0;
}

extern "C" int dc_ferror(FILE* /*fp*/) {
    // Not tracked.
    return 0;
}

#endif // DREAMCAST
