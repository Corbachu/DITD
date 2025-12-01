#ifdef DREAMCAST
extern "C" {
    int FitdMain(int argc, char* argv[]);
}

extern "C" {
    // Defined in osystemSDL.cpp for other platforms; provide here for DC
    char homePath[256] = "/cd/"; // default to CD-ROM; adjust as needed
}

int main(int argc, char* argv[])
{
    // On Dreamcast, just enter the main game loop
    return FitdMain(argc, argv);
}
#endif // DREAMCAST
