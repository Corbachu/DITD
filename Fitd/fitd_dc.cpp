//----------------------------------------------------------------------------
//  Dream In The Dark fitd dc (Game Code)
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
// KOS
extern "C" {
#include <kos.h>
}

#include <unistd.h>
#include <cstring>

// Ensure KOS core subsystems are initialized.
KOS_INIT_FLAGS(INIT_DEFAULT);

extern "C" {
    int FitdMain(int argc, char* argv[]);
}

extern "C" {
    // Defined in osystemSDL.cpp for other platforms; provide here for DC
    char homePath[512] = "/cd/"; // default to CD-ROM root
}

int main(int argc, char* argv[])
{
    // Prefer serial output for early boot debugging.
    // If you're running via dcload, you can switch to "dcload".
    dbgio_dev_select("scif");
    dbgio_printf("[FITD] boot: argc=%d\n", argc);

    // INIT_DEFAULT (via INIT_DEFAULT_ARCH) already initializes CD-ROM + ISO9660.
    // Calling fs_iso9660_init() again can leave the VFS/driver in a bad state.
    dbgio_printf("[FITD] __kos_init_flags=0x%08lx (INIT_CDROM=%s)\n",
                (unsigned long)__kos_init_flags,
                (__kos_init_flags & INIT_CDROM) ? "yes" : "no");

    // Best-effort: reset the ISO cache in case the environment was reused.
    int iso_rc = iso_reset();
    dbgio_printf("[FITD] iso_reset() -> %d\n", iso_rc);

    // Make relative fopen() calls resolve from the CD root too.
    int chdir_rc = chdir("/cd");
    dbgio_printf("[FITD] chdir(\"/cd\") -> %d\n", chdir_rc);

    // Prefer relative paths from /cd to avoid any absolute-path quirks.
    homePath[0] = '\0';
    dbgio_printf("[FITD] homePath=<cwd> (/cd)\n");

    // Enter the main game loop.
    dbgio_printf("[FITD] entering FitdMain()\n");
    return FitdMain(argc, argv);
}
#endif // DREAMCAST
