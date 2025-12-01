//----------------------------------------------------------------------------
//  EDGE stb_image interface
//----------------------------------------------------------------------------
//
//  Copyright (c) 2018 The EDGE Team
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
//----------------------------------

#include "epi.h"

#include "file.h"
#include "filesystem.h"
#include "image_data.h"

#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"

#ifdef _arch_dreamcast
// Access DC-only image loader cvar without pulling all renderer headers
extern "C" { struct cvar_c { int d; const char *str; }; extern cvar_c r_image_loader_dc_only; }
#endif

// Lightweight buffered reader to reduce small read syscalls from stb_image
struct epi_buf_reader_t {
    epi::file_c *F;
    unsigned char buf[2048];
    int buf_pos;
    int buf_len;
};

static int eread(void *user, char *data, int size)
{
    epi_buf_reader_t *R = (epi_buf_reader_t *)user;
    int copied = 0;
    while (copied < size)
    {
        if (R->buf_pos >= R->buf_len)
        {
            R->buf_len = R->F->Read((char*)R->buf, (int)sizeof(R->buf));
            R->buf_pos = 0;
            if (R->buf_len <= 0)
                break;
        }
        int chunk = R->buf_len - R->buf_pos;
        if (chunk > (size - copied)) chunk = (size - copied);
        memcpy(data + copied, R->buf + R->buf_pos, chunk);
        R->buf_pos += chunk;
        copied += chunk;
    }
    return copied;
}

static void eskip(void *user, int n)
{
    epi_buf_reader_t *R = (epi_buf_reader_t *)user;
    // If skipping within buffer, advance pointers; otherwise seek
    if (n > 0)
    {
        int within = R->buf_len - R->buf_pos;
        if (n <= within)
        {
            R->buf_pos += n;
            return;
        }
        int rem = n - within;
        R->F->Seek(rem, epi::file_c::SEEKPOINT_CURRENT);
        R->buf_pos = R->buf_len; // invalidate buffer
    }
}

static int eeof(void *user)
{
    epi_buf_reader_t *R = (epi_buf_reader_t *)user;
    if (R->buf_pos < R->buf_len)
        return 0;
    return R->F->GetPosition() >= R->F->GetLength() ? 1 : 0;
}

epi::image_data_c *epimg_load(epi::file_c *F, int read_flags)
{
#ifdef _arch_dreamcast
    if (r_image_loader_dc_only.d)
    {
        fprintf(stderr, "epimg_load - DC-only image loader enabled; skipping stb_image.\n");
        return NULL;
    }
#endif
    stbi_io_callbacks callbacks;
    int w = 0, h = 0, c = 0;

    callbacks.read = &eread;
    callbacks.skip = &eskip;
    callbacks.eof = &eeof;

    epi_buf_reader_t Rinfo; Rinfo.F = F; Rinfo.buf_pos = 0; Rinfo.buf_len = 0;
    F->Seek(0,  epi::file_c::SEEKPOINT_START);
    if (!stbi_info_from_callbacks(&callbacks, (void *)&Rinfo, &w, &h, &c))
    {
        fprintf(stderr, "epimg_load - stb_image cannot handle file!\n");
        return NULL;
    }
    bool solid = (c == 1) || (c == 3);

    epi_buf_reader_t Rload; Rload.F = F; Rload.buf_pos = 0; Rload.buf_len = 0;
    F->Seek(0,  epi::file_c::SEEKPOINT_START);
    stbi_uc *p = stbi_load_from_callbacks(&callbacks, (void *)&Rload, &w, &h, &c, solid ? 3 : 4);
    if (p == NULL)
    {
        fprintf(stderr, "epimg_load - stb_image cannot decode image!\n");
        return NULL;
    }

    int tot_W = w;
    int tot_H = h;
	
    if (read_flags == epi::IRF_Round_POW2)
    {
        tot_W = 1; while (tot_W < w)  tot_W <<= 1;
        tot_H = 1; while (tot_H < h) tot_H <<= 1;
    }

    epi::image_data_c *img = new epi::image_data_c(tot_W, tot_H, solid ? 3 : 4);
    if (img)
    {
		img->used_w = w;
		img->used_h = h;

		if (img->used_w != tot_W || img->used_h != tot_H)
			img->Clear();

		for (int y = 0; y < img->used_h; y++)
            memcpy(img->PixelAt(0, img->used_h - 1 - y), &p[y*w*(solid ? 3 : 4)], w*(solid ? 3 : 4));
    }

    stbi_image_free(p);
    return img;
}

bool epimg_info(epi::file_c *F, int *width, int *height, bool *solid)
{
    stbi_io_callbacks callbacks;
    int w = 0, h = 0, c = 0;

    callbacks.read = &eread;
    callbacks.skip = &eskip;
    callbacks.eof = &eeof;

    epi_buf_reader_t R; R.F = F; R.buf_pos = 0; R.buf_len = 0;
    F->Seek(0,  epi::file_c::SEEKPOINT_START);
    if (stbi_info_from_callbacks(&callbacks, (void *)&R, &w, &h, &c))
    {
        *width = w;
        *height = h;
        *solid = (c != 4); // not rgba
        return true;
    }

    return false;
}