#include "common.h"

#ifdef DREAMCAST

#include "System/r_clipping.h"

// GLdc
#include <GL/gl.h>
#include <GL/glkos.h>

#include <dc/video.h>

void dc_gl_ensure_fullscreen_viewport()
{
    // GLdc's glViewport math depends on GetVideoMode()->height (backed by KOS
    // vid_mode->height). Always match the current KOS video mode to avoid
    // incorrect scaling/offset.
    if (vid_mode)
        glViewport(0, 0, vid_mode->width, vid_mode->height);
    else
        glViewport(0, 0, 640, 480);
}

void dc_gl_force_fullscreen_scissor()
{
    // GLdc's scissor implementation emits a PVR tile-clip command only when
    // GL_SCISSOR_TEST is enabled. If any earlier code enabled scissor and set
    // a small clip, then later disabled scissor, the last tile-clip can
    // effectively "stick" on hardware/emulators. Force a full-screen clip.
    int vw = 640;
    int vh = 480;
    if (vid_mode)
    {
        vw = vid_mode->width;
        vh = vid_mode->height;
    }

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, vw, vh);
}

void dc_gl_ensure_game_ortho()
{
    dc_gl_ensure_fullscreen_viewport();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, 320.0f, 200.0f, 0.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    dc_gl_force_fullscreen_scissor();
}

#endif // DREAMCAST
