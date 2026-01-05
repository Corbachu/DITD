/*
 * Dream in the Dark Fast Batch Vertex Buffer Definitions
 * Filename: d:\Dev\Dreamcast\UB_SHARE\dreamedge_slave\src\system\r_batch_dreamcast.h
 * Path: d:\Dev\Dreamcast\UB_SHARE\dreamedge_slave\src\system
 * Created Date: Wednesday, November 6th 2019, 9:16:26 am
 * Author: Hayden Kowalchuk
 * 
 * Copyright (c) 2019 Hayden Kowalchuk
 * Copyright (c) 2025 Corbin Annis
 * Copyright (c) 2019-2025 The EDGE Team
 */

#ifndef R_BATCH_DREAMCAST_H
#define R_BATCH_DREAMCAST_H

#include <stdint.h>

// these could go in a vertexbuffer/indexbuffer pair
#define MAX_BATCHED_SURFVERTEXES 256

#define VERTEX_EOL 0xf0000000
#define VERTEX 0xe0000000

typedef struct __attribute__((packed, aligned(4))) vec3f_gl
{
    float x, y, z;
} vec3f_gl;

typedef struct __attribute__((packed, aligned(4))) uv_float
{
    float u, v;
} uv_float;

typedef struct __attribute__((packed, aligned(1))) color_uc
{
    unsigned char b,g,r,a;
} color_uc;

typedef struct __attribute__((packed, aligned(4))) glvert_fast_t
{
    uint32_t flags;
    struct vec3f_gl vert;
    uv_float texture;
	color_uc color; //bgra
    union {
        float pad;
        unsigned int vertindex;
    } pad0;
} glvert_fast_t;


extern glvert_fast_t gVertexFastBuffer[];


#endif /* R_BATCH_DREAMCAST_H */