#pragma once

#ifdef DREAMCAST

// Shared Dreamcast GLdc clipping/viewport helpers.
void dc_gl_ensure_fullscreen_viewport();
void dc_gl_force_fullscreen_scissor();

// Sets up 320x200 ortho projection and full-screen scissor.
void dc_gl_ensure_game_ortho();

#endif // DREAMCAST
