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
    // Mount ISO9660 filesystem at /cd so fopen("/cd/...") works.
    // Safe to call multiple times.
    fs_iso9660_init();

    // Make relative fopen() calls resolve from the CD root too.
    chdir("/cd");

    // Keep homePath consistent with the mounted CD root.
    std::strncpy(homePath, "/cd/", sizeof(homePath) - 1);
    homePath[sizeof(homePath) - 1] = '\0';

    // Enter the main game loop.
    return FitdMain(argc, argv);
}
#endif // DREAMCAST
