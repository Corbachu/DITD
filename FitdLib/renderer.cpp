//----------------------------------------------------------------------------
//  Dream In The Dark renderer (Rendering)
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
#include "common.h"

#include <algorithm>

#include "dc_fastmath.h"

#ifdef DREAMCAST
#include "System/r_units.h"
#endif

#if defined(DREAMCAST)
// GCC vector extension: 2-wide float SIMD (8 bytes). Used for hot vertex rotation loops.
typedef float fitd_vec2f __attribute__((vector_size(8), may_alias));
#endif

#include "fitd_endian_read.h"


struct rendererPointStruct
{
    float X;
    float Y;
    float Z;
};

#define NUM_MAX_VERTEX_IN_PRIM 64

struct primEntryStruct
{
	u8 material;
	u8 color;
	u16 size;
	u16 numOfVertices;
	primTypeEnum type;
    float sortDepth;
	rendererPointStruct vertices[NUM_MAX_VERTEX_IN_PRIM];
};

#define NUM_MAX_PRIM_ENTRY 500

primEntryStruct primTable[NUM_MAX_PRIM_ENTRY];

u32 positionInPrimEntry = 0;

int BBox3D1=0;
int BBox3D2=0;
int BBox3D3=0;
int BBox3D4=0;

int renderVar1=0;

int numOfPrimitiveToRender=0;

#ifdef DREAMCAST
static uint64_t s_dc_rdbg_next_ms = 0;
static int s_dc_rdbg_aff_calls = 0;
static int s_dc_rdbg_prims_total = 0;
static int s_dc_rdbg_prims_kept = 0;
static int s_dc_rdbg_prims_culled_depth = 0;
#endif

char renderBuffer[3261];

char* renderVar2=NULL;

int modelFlags = 0;

int modelCosAlpha;
int modelSinAlpha;
int modelCosBeta;
int modelSinBeta;
int modelCosGamma;
int modelSinGamma;

bool noModelRotation;

int renderX;
int renderY;
int renderZ;

int numOfPoints;
int numOfBones;

std::array<point3dStruct, NUM_MAX_POINT_IN_POINT_BUFFER> pointBuffer;
s16 cameraSpaceBuffer[NUM_MAX_POINT_IN_POINT_BUFFER*3];
s16 bonesBuffer[NUM_MAX_BONES];

bool boneRotateX;
bool boneRotateY;
bool boneRotateZ;

int boneRotateXCos;
int boneRotateXSin;
int boneRotateYCos;
int boneRotateYSin;
int boneRotateZCos;
int boneRotateZSin;

char primBuffer[30000];

int renderVar3;

#ifndef AITD_UE4
void fillpoly(s16 * datas, int n, char c);
#endif

/*

 
 
*/

void transformPoint(float* ax, float* bx, float* cx)
{
    // Match C/C++ signed division semantics (truncate toward zero),
    // while still using a shift-based implementation.
    auto q16_trunc = [](int64_t v) -> int {
        if (v >= 0)
            return (int)(v >> 16);

        // Safe abs for INT64_MIN (avoid UB from -v).
        const uint64_t mag = (uint64_t)(-(v + 1)) + 1ULL;
        return -(int)(mag >> 16);
    };

    int X = (int)*ax;
    int Y = (int)*bx;
    int Z = (int)*cx;

    int x;
    int y;
    int z;

    if (transformUseY)
    {
        // cosTable is 16.16-ish fixed.
        const int64_t vx = (int64_t)X * (int64_t)transformYSin - (int64_t)Z * (int64_t)transformYCos;
        const int64_t vz = (int64_t)X * (int64_t)transformYCos + (int64_t)Z * (int64_t)transformYSin;
        x = q16_trunc(vx) << 1;
        z = q16_trunc(vz) << 1;
    }
    else
    {
        x = X;
        z = Z;
    }

    if (transformUseX)
    {
        const int tempY = Y;
        const int tempZ = z;
        const int64_t vy = (int64_t)tempY * (int64_t)transformXSin - (int64_t)tempZ * (int64_t)transformXCos;
        const int64_t vz = (int64_t)tempY * (int64_t)transformXCos + (int64_t)tempZ * (int64_t)transformXSin;
        y = q16_trunc(vy) << 1;
        z = q16_trunc(vz) << 1;
    }
    else
    {
        y = Y;
    }

    if (transformUseZ)
    {
        const int tempX = x;
        const int tempY = y;
        const int64_t vx = (int64_t)tempX * (int64_t)transformZSin - (int64_t)tempY * (int64_t)transformZCos;
        const int64_t vy = (int64_t)tempX * (int64_t)transformZCos + (int64_t)tempY * (int64_t)transformZSin;
        x = q16_trunc(vx) << 1;
        y = q16_trunc(vy) << 1;
    }

    *ax = (float)x;
    *bx = (float)y;
    *cx = (float)z;
}

void InitGroupeRot(int transX,int transY,int transZ)
{
    if(transX)
    {
        boneRotateXCos = cosTable[transX&0x3FF];
        boneRotateXSin = cosTable[(transX+0x100)&0x3FF];

        boneRotateX = true;
    }
    else
    {
        boneRotateX = false;
    }

    if(transY)
    {
        boneRotateYCos = cosTable[transY&0x3FF];
        boneRotateYSin = cosTable[(transY+0x100)&0x3FF];

        boneRotateY = true;
    }
    else
    {
        boneRotateY = false;
    }

    if(transZ)
    {
        boneRotateZCos = cosTable[transZ&0x3FF];
        boneRotateZSin = cosTable[(transZ+0x100)&0x3FF];

        boneRotateZ = true;
    }
    else
    {
        boneRotateZ = false;
    }
}

void RotateList(point3dStruct* pointPtr, int numOfPoint)
{
    for(int i=0;i<numOfPoint;i++)
    {
        point3dStruct& point = pointPtr[i];
        int x = point.x;
        int y = point.y;
        int z = point.z;

        if(boneRotateY)
        {
            int tempX = x;
            int tempZ = z;
            x = ((((tempX * boneRotateYSin) - (tempZ * boneRotateYCos)))>>16)<<1;
            z = ((((tempX * boneRotateYCos) + (tempZ * boneRotateYSin)))>>16)<<1;
        }

        if(boneRotateX)
        {
            int tempY = y;
            int tempZ = z;
            y = ((((tempY * boneRotateXSin ) - (tempZ * boneRotateXCos)))>>16)<<1;
            z = ((((tempY * boneRotateXCos ) + (tempZ * boneRotateXSin)))>>16)<<1;
        }

		if(boneRotateZ)
		{
			int tempX = x;
			int tempY = y;
			x = ((((tempX * boneRotateZSin) - ( tempY * boneRotateZCos)))>>16)<<1;
			y = ((((tempX * boneRotateZCos) + ( tempY * boneRotateZSin)))>>16)<<1;
		}

        point.x = x;
        point.y = y;
        point.z = z;
    }
}

void RotateGroupeOptimise(sGroup* ptr)
{
    if (ptr->m_numGroup) // if group number is 0
    {
        int baseBone = ptr->m_start;
        int numPoints = ptr->m_numVertices;

        RotateList(pointBuffer.data() + baseBone, numPoints);
    }
}

void RotateGroupe(sGroup* ptr)
{
    int baseBone = ptr->m_start;
    int numPoints = ptr->m_numVertices;
    int temp;
    int temp2;

    RotateList(pointBuffer.data() + baseBone, numPoints);

    temp = ptr->m_numGroup; // group number

    temp2 = numOfBones - temp;

    do
    {
        if (ptr->m_orgGroup == temp) // is it on of this group child
        {
            RotateGroupe(ptr); // yes, so apply the transformation to him
        }

        ptr++;
    } while (--temp2);
}

void TranslateGroupe(int transX, int transY, int transZ, sGroup* ptr)
{
    for (int i = 0; i < ptr->m_numVertices; i++)
    {
        point3dStruct& point = pointBuffer[ptr->m_start + i];
        point.x += transX;
        point.y += transY;
        point.z += transZ;
    }
}

void ZoomGroupe(int zoomX, int zoomY, int zoomZ, sGroup* ptr)
{
    for (int i = 0; i < ptr->m_numVertices; i++)
    {
        point3dStruct& point = pointBuffer[ptr->m_start + i];
        point.x = (point.x * (zoomX + 256)) / 256;
        point.y = (point.y * (zoomY + 256)) / 256;
        point.z = (point.z * (zoomZ + 256)) / 256;
    }
}

static int AnimateCloud(int x,int y,int z,int alpha,int beta,int gamma, sBody* pBody)
{
    renderX = x - translateX;
    renderY = y;
    renderZ = z - translateZ;

    ASSERT(pBody->m_vertices.size()<NUM_MAX_POINT_IN_POINT_BUFFER);

    for (int i = 0; i < pBody->m_vertices.size(); i++)
    {
        pointBuffer[i] = pBody->m_vertices[i];
    }

    numOfPoints = pBody->m_vertices.size();
    numOfBones = pBody->m_groupOrder.size();
    ASSERT(numOfBones<NUM_MAX_BONES);

    if(pBody->m_flags & INFO_OPTIMISE)
    {
        for(int i=0;i<pBody->m_groupOrder.size();i++)
        {
            int boneDataOffset = pBody->m_groupOrder[i];
            sGroup* pGroup = &pBody->m_groups[pBody->m_groupOrder[i]];

            switch(pGroup->m_state.m_type)
            {
            case 1:
                if(pGroup->m_state.m_delta.x || pGroup->m_state.m_delta.y || pGroup->m_state.m_delta.z)
                {
                    TranslateGroupe(pGroup->m_state.m_delta.x, pGroup->m_state.m_delta.y, pGroup->m_state.m_delta.z, pGroup);
                }
                break;
            case 2:
                if (pGroup->m_state.m_delta.x || pGroup->m_state.m_delta.y || pGroup->m_state.m_delta.z)
                {
                    ZoomGroupe(pGroup->m_state.m_delta.x, pGroup->m_state.m_delta.y, pGroup->m_state.m_delta.z, pGroup);
                }
                break;
            }

            if (pGroup->m_state.m_hasRotateDelta)
                InitGroupeRot(pGroup->m_state.m_rotateDelta.x,
                             pGroup->m_state.m_rotateDelta.y,
                             pGroup->m_state.m_rotateDelta.z);
            else
                InitGroupeRot(0, 0, 0);
            RotateGroupeOptimise(pGroup);
        }
    }
    else
    {
        pBody->m_groups[0].m_state.m_delta.x = alpha;
        pBody->m_groups[0].m_state.m_delta.y = beta;
        pBody->m_groups[0].m_state.m_delta.z = gamma;

        for(int i=0;i<pBody->m_groups.size();i++)
        {
            int boneDataOffset = pBody->m_groupOrder[i];
            sGroup* pGroup = &pBody->m_groups[pBody->m_groupOrder[i]];

            int transX = pGroup->m_state.m_delta.x;
            int transY = pGroup->m_state.m_delta.y;
            int transZ = pGroup->m_state.m_delta.z;

            if(transX || transY || transZ)
            {
                switch(pGroup->m_state.m_type)
                {
                case 0:
                    { 
                        InitGroupeRot(transX,transY,transZ);
                        RotateGroupe(pGroup);
                        break;
                    }
                case 1:
                    {
                        TranslateGroupe(transX,transY,transZ, pGroup);
                        break;
                    }
                case 2:
                    {
                        ZoomGroupe(transX,transY,transZ, pGroup);
                        break;
                    }
                }
            }
        }
    }

    for(int i=0;i<pBody->m_groups.size();i++)
    {
        sGroup* pGroup = &pBody->m_groups[i];
        for(int j=0;j< pGroup->m_numVertices;j++)
        {
            pointBuffer[pGroup->m_start + j].x += pointBuffer[pGroup->m_baseVertices].x;
            pointBuffer[pGroup->m_start + j].y += pointBuffer[pGroup->m_baseVertices].y;
            pointBuffer[pGroup->m_start + j].z += pointBuffer[pGroup->m_baseVertices].z;
        }
    }

    if(modelFlags & INFO_OPTIMISE)
    {
        InitGroupeRot(alpha,beta,gamma);
        RotateList(pointBuffer.data(),numOfPoints);
    }

    {
        s16* outPtr = cameraSpaceBuffer;
        for(int i=0;i<numOfPoints;i++)
        {
            float X = pointBuffer[i].x;
            float Y = pointBuffer[i].y;
            float Z = pointBuffer[i].z;


            X += renderX;
            Y += renderY;
            Z += renderZ;

#if defined(AITD_UE4)
            *(outPtr++) = (s16)X;
            *(outPtr++) = (s16)Y;
            *(outPtr++) = (s16)Z;
#else
            if(Y>10000) // height clamp
            {
                *(outPtr++) = -10000;
                *(outPtr++) = -10000;
                *(outPtr++) = -10000;
            }
            else
            {
                Y -= translateY;

                transformPoint(&X,&Y,&Z);

                *(outPtr++) = (s16)X;
                *(outPtr++) = (s16)Y;
                *(outPtr++) = (s16)Z;
            }
#endif
        }

        s16* ptr = cameraSpaceBuffer;
        float* outPtr2 = renderPointList;

    #ifdef DREAMCAST
        // Make sure SH-4 fmath ops stay in single precision for consistency/speed.
        uint32_t old_fpscr = fitd_fpscr_force_single();
    #endif

        int k = numOfPoints;
        do
        {
            float X;
            float Y;
            float Z;

            X = ptr[0];
            Y = ptr[1];
            Z = ptr[2];
            ptr+=3;

#if defined(AITD_UE4)
            *(outPtr2++) = X;
            *(outPtr2++) = Y;
            *(outPtr2++) = Z;
#else
            Z += cameraPerspective;

            if( Z<=50 ) // clipping
            {
                *(outPtr2++) = -10000;
                *(outPtr2++) = -10000;
                *(outPtr2++) = -10000;
            }
            else
            {
                const float invZ = fitd_rcpf_fast(Z);
                float transformedX = (X * cameraFovX * invZ) + cameraCenterX;
                float transformedY;

                *(outPtr2++) = transformedX;

                if(transformedX < BBox3D1)
                    BBox3D1 = (int)transformedX;

                if(transformedX > BBox3D3)
                    BBox3D3 = (int)transformedX;

                transformedY = (Y * cameraFovY * invZ) + cameraCenterY;

                *(outPtr2++) = transformedY;

                if(transformedY < BBox3D2)
                    BBox3D2 = (int)transformedY;

                if(transformedY > BBox3D4)
                    BBox3D4 = (int)transformedY;

                *(outPtr2++) = Z; 
            }
#endif

            k--;
            if(k==0)
            {
#ifdef DREAMCAST
                fitd_fpscr_restore(old_fpscr);
#endif
                return(1);
            }

        }while(renderVar1 == 0);

    #ifdef DREAMCAST
        fitd_fpscr_restore(old_fpscr);
    #endif
    }

    return(0);
}

// Compatibility wrapper (old French name).
int AnimNuage(int x,int y,int z,int alpha,int beta,int gamma, sBody* pBody)
{
    return AnimateCloud(x,y,z,alpha,beta,gamma, pBody);
}

/*
x - cameraX
y
z - cameraZ
 
*/

static int RotateAndProjectBody(int x,int y,int z,int alpha,int beta,int gamma, sBody* pBody)
{
    float* outPtr;

#if !defined(AITD_UE4) && defined(DREAMCAST)
    uint32_t old_fpscr = fitd_fpscr_force_single();
#endif

    renderX = x - translateX;
    renderY = y;
    renderZ = z - translateZ;

    if(!alpha && !beta && !gamma)
    {
        noModelRotation = true;
    }
    else
    {
        noModelRotation = false;

        modelCosAlpha = cosTable[alpha&0x3FF];
        modelSinAlpha = cosTable[(alpha+0x100)&0x3FF];

        modelCosBeta = cosTable[beta&0x3FF];
        modelSinBeta = cosTable[(beta+0x100)&0x3FF];

        modelCosGamma = cosTable[gamma&0x3FF];
        modelSinGamma = cosTable[(gamma+0x100)&0x3FF];
    }

    outPtr = renderPointList;

    // Cache frequently-used globals into locals for the hot loop.
    const float fovX = cameraFovX;
    const float fovY = cameraFovY;
    const float ctrX = cameraCenterX;
    const float ctrY = cameraCenterY;
    const float camPersp = cameraPerspective;

#if !defined(AITD_UE4)
    const bool camUseX = transformUseX;
    const bool camUseY = transformUseY;
    const bool camUseZ = transformUseZ;
    const int camXSin = transformXSin;
    const int camXCos = transformXCos;
    const int camYSin = transformYSin;
    const int camYCos = transformYCos;
    const int camZSin = transformZSin;
    const int camZCos = transformZCos;

    auto q16_trunc = [](int64_t v) -> int {
        if (v >= 0)
            return (int)(v >> 16);
        const uint64_t mag = (uint64_t)(-(v + 1)) + 1ULL;
        return -(int)(mag >> 16);
    };
#endif

    auto emitProjectedVertex = [&](float X, float Y, float Z) {
        X += renderX;
        Y += renderY;
        Z += renderZ;

#if defined(AITD_UE4)
        *(outPtr++) = X;
        *(outPtr++) = Y;
        *(outPtr++) = Z;
#else
        if (Y > 10000) // height clamp
        {
            *(outPtr++) = -10000;
            *(outPtr++) = -10000;
            *(outPtr++) = -10000;
        }
        else
        {
            float transformedX;
            float transformedY;

            Y -= translateY;

            // Inline transformPoint() for performance (same math/rounding).
            {
                int Xi = (int)X;
                int Yi = (int)Y;
                int Zi = (int)Z;

                int x;
                int y;
                int z;

                if (camUseY)
                {
                    const int64_t vx = (int64_t)Xi * (int64_t)camYSin - (int64_t)Zi * (int64_t)camYCos;
                    const int64_t vz = (int64_t)Xi * (int64_t)camYCos + (int64_t)Zi * (int64_t)camYSin;
                    x = q16_trunc(vx) << 1;
                    z = q16_trunc(vz) << 1;
                }
                else
                {
                    x = Xi;
                    z = Zi;
                }

                if (camUseX)
                {
                    const int tempY = Yi;
                    const int tempZ = z;
                    const int64_t vy = (int64_t)tempY * (int64_t)camXSin - (int64_t)tempZ * (int64_t)camXCos;
                    const int64_t vz = (int64_t)tempY * (int64_t)camXCos + (int64_t)tempZ * (int64_t)camXSin;
                    y = q16_trunc(vy) << 1;
                    z = q16_trunc(vz) << 1;
                }
                else
                {
                    y = Yi;
                }

                if (camUseZ)
                {
                    const int tempX = x;
                    const int tempY = y;
                    const int64_t vx = (int64_t)tempX * (int64_t)camZSin - (int64_t)tempY * (int64_t)camZCos;
                    const int64_t vy = (int64_t)tempX * (int64_t)camZCos + (int64_t)tempY * (int64_t)camZSin;
                    x = q16_trunc(vx) << 1;
                    y = q16_trunc(vy) << 1;
                }

                X = (float)x;
                Y = (float)y;
                Z = (float)z;
            }

            Z += camPersp;

            const float invZ = fitd_rcpf_fast(Z);
            transformedX = (X * fovX * invZ) + ctrX;

            *(outPtr++) = transformedX;

            if (transformedX < BBox3D1)
                BBox3D1 = (int)transformedX;

            if (transformedX > BBox3D3)
                BBox3D3 = (int)transformedX;

            transformedY = (Y * fovY * invZ) + ctrY;

            *(outPtr++) = transformedY;

            if (transformedY < BBox3D2)
                BBox3D2 = (int)transformedY;

            if (transformedY > BBox3D4)
                BBox3D4 = (int)transformedY;

            *(outPtr++) = Z;
        }
#endif
    };

#if defined(DREAMCAST) && !defined(AITD_UE4)
    if (!noModelRotation)
    {
        using vec2 = fitd_vec2f;

        // Pre-scale once: int cosTable values are fixed-ish; original code used k=2/65536.
        const float k = 1.0f / 32768.0f;
        const float sB0 = (float)modelSinBeta  * k;
        const float cB0 = (float)modelCosBeta  * k;
        const float sG0 = (float)modelSinGamma * k;
        const float cG0 = (float)modelCosGamma * k;
        const float sA0 = (float)modelSinAlpha * k;
        const float cA0 = (float)modelCosAlpha * k;

        const vec2 sB = { sB0, sB0 };
        const vec2 cB = { cB0, cB0 };
        const vec2 sG = { sG0, sG0 };
        const vec2 cG = { cG0, cG0 };
        const vec2 sA = { sA0, sA0 };
        const vec2 cA = { cA0, cA0 };

        const int n = (int)pBody->m_vertices.size();
        int i = 0;
        for (; i + 1 < n; i += 2)
        {
            vec2 X = { (float)pBody->m_vertices[i + 0].x, (float)pBody->m_vertices[i + 1].x };
            vec2 Y = { (float)pBody->m_vertices[i + 0].y, (float)pBody->m_vertices[i + 1].y };
            vec2 Z = { (float)pBody->m_vertices[i + 0].z, (float)pBody->m_vertices[i + 1].z };

            // Y rotation
            {
                const vec2 tempX = X;
                const vec2 tempZ = Z;
                X = (tempX * sB) - (tempZ * cB);
                Z = (tempX * cB) + (tempZ * sB);
            }

            // Z rotation
            {
                const vec2 tempX = X;
                const vec2 tempY = Y;
                X = (tempX * sG) - (tempY * cG);
                Y = (tempX * cG) + (tempY * sG);
            }

            // X rotation
            {
                const vec2 tempY = Y;
                const vec2 tempZ = Z;
                Y = (tempY * sA) - (tempZ * cA);
                Z = (tempY * cA) + (tempZ * sA);
            }

            const float* x = (const float*)&X;
            const float* y = (const float*)&Y;
            const float* z = (const float*)&Z;

            emitProjectedVertex(x[0], y[0], z[0]);
            emitProjectedVertex(x[1], y[1], z[1]);
        }

        // Tail
        for (; i < n; i++)
        {
            float X = pBody->m_vertices[i].x;
            float Y = pBody->m_vertices[i].y;
            float Z = pBody->m_vertices[i].z;

            // Scalar fallback for last vertex
            {
                float tempX = X;
                float tempZ = Z;
                X = (((modelSinBeta * tempX) - (modelCosBeta * tempZ)) * k);
                Z = (((modelCosBeta * tempX) + (modelSinBeta * tempZ)) * k);
            }
            {
                float tempX = X;
                float tempY = Y;
                X = (((modelSinGamma * tempX) - (modelCosGamma * tempY)) * k);
                Y = (((modelCosGamma * tempX) + (modelSinGamma * tempY)) * k);
            }
            {
                float tempY = Y;
                float tempZ = Z;
                Y = (((modelSinAlpha * tempY) - (modelCosAlpha * tempZ)) * k);
                Z = (((modelCosAlpha * tempY) + (modelSinAlpha * tempZ)) * k);
            }

            emitProjectedVertex(X, Y, Z);
        }
    }
    else
#endif
    {
        for(int i=0; i<pBody->m_vertices.size(); i++)
        {
            float X = pBody->m_vertices[i].x;
            float Y = pBody->m_vertices[i].y;
            float Z = pBody->m_vertices[i].z;
            emitProjectedVertex(X, Y, Z);
        }
    }

#if !defined(AITD_UE4) && defined(DREAMCAST)
    fitd_fpscr_restore(old_fpscr);
#endif
    return(1);
}

// Compatibility wrapper (old French name).
int RotateNuage(int x,int y,int z,int alpha,int beta,int gamma, sBody* pBody)
{
    return RotateAndProjectBody(x,y,z,alpha,beta,gamma, pBody);
}

char* primVar1;
char* primVar2;

void primFunctionDefault(int primType,char** ptr,char** out)
{
    printf("UnHandled primType %d\n",primType);
    assert(0);
}

void processPrim_Line(int primType, sPrimitive* ptr, char** out)
{
    primEntryStruct* pCurrentPrimEntry = &primTable[positionInPrimEntry];

    ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);

    pCurrentPrimEntry->type = primTypeEnum_Line;
    pCurrentPrimEntry->numOfVertices = 2;
    pCurrentPrimEntry->color = ptr->m_color;
    pCurrentPrimEntry->material = ptr->m_material;

    float depth = 32000.f;
    ASSERT(pCurrentPrimEntry->numOfVertices < NUM_MAX_VERTEX_IN_PRIM);
    for (int i = 0; i < pCurrentPrimEntry->numOfVertices; i++)
    {
        u16 pointIndex;

        pointIndex = ptr->m_points[i] * 6;

        ASSERT((pointIndex % 2) == 0);

        pCurrentPrimEntry->vertices[i].X = renderPointList[pointIndex / 2];
        pCurrentPrimEntry->vertices[i].Y = renderPointList[(pointIndex / 2) + 1];
        pCurrentPrimEntry->vertices[i].Z = renderPointList[(pointIndex / 2) + 2];

        if (pCurrentPrimEntry->vertices[i].Z < depth)
            depth = pCurrentPrimEntry->vertices[i].Z;

    }

    // For painter's algorithm: larger Z is farther away.
    // Use min-Z as a conservative sort key.
    pCurrentPrimEntry->sortDepth = depth;

#if !defined(AITD_UE4)
    s_dc_rdbg_prims_total++;
    if (depth > 100)
#endif
    {
#ifdef DREAMCAST
        s_dc_rdbg_prims_kept++;
#endif
        positionInPrimEntry++;

        numOfPrimitiveToRender++;
        ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);
    }
#if !defined(AITD_UE4)
    else
    {
#ifdef DREAMCAST
        s_dc_rdbg_prims_culled_depth++;
#endif
    }
#endif
}

void processPrim_Poly(int primType, sPrimitive* ptr, char** out)
{
    primEntryStruct* pCurrentPrimEntry = &primTable[positionInPrimEntry];

    ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);

    pCurrentPrimEntry->type = primTypeEnum_Poly;
    pCurrentPrimEntry->numOfVertices = ptr->m_points.size();
    pCurrentPrimEntry->color = ptr->m_color;
    pCurrentPrimEntry->material = ptr->m_material;

    float depth = 32000.f;
    ASSERT(pCurrentPrimEntry->numOfVertices < NUM_MAX_VERTEX_IN_PRIM);
    for (int i = 0; i < pCurrentPrimEntry->numOfVertices; i++)
    {
        u16 pointIndex;

        pointIndex = ptr->m_points[i] * 6;

        ASSERT((pointIndex % 2) == 0);

        pCurrentPrimEntry->vertices[i].X = renderPointList[pointIndex / 2];
        pCurrentPrimEntry->vertices[i].Y = renderPointList[(pointIndex / 2) + 1];
        pCurrentPrimEntry->vertices[i].Z = renderPointList[(pointIndex / 2) + 2];

        if (pCurrentPrimEntry->vertices[i].Z < depth)
            depth = pCurrentPrimEntry->vertices[i].Z;

    }

    // Use min-Z as a conservative sort key.
    pCurrentPrimEntry->sortDepth = depth;

#if !defined(AITD_UE4)
    s_dc_rdbg_prims_total++;
    if (depth > 100)
#endif
    {
#ifdef DREAMCAST
        s_dc_rdbg_prims_kept++;
#endif
        positionInPrimEntry++;

        numOfPrimitiveToRender++;
        ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);
    }
#if !defined(AITD_UE4)
    else
    {
#ifdef DREAMCAST
        s_dc_rdbg_prims_culled_depth++;
#endif
    }
#endif
}

void processPrim_Point(primTypeEnum primType, sPrimitive* ptr, char** out)
{
    primEntryStruct* pCurrentPrimEntry = &primTable[positionInPrimEntry];

    ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);

    pCurrentPrimEntry->type = primType;
    pCurrentPrimEntry->numOfVertices = 1;
    pCurrentPrimEntry->color = ptr->m_color;
    pCurrentPrimEntry->material = ptr->m_material;

    float depth = 32000.f;
    {
        u16 pointIndex;
        pointIndex = ptr->m_points[0] * 6;
        ASSERT((pointIndex % 2) == 0);
        pCurrentPrimEntry->vertices[0].X = renderPointList[pointIndex / 2];
        pCurrentPrimEntry->vertices[0].Y = renderPointList[(pointIndex / 2) + 1];
        pCurrentPrimEntry->vertices[0].Z = renderPointList[(pointIndex / 2) + 2];

        depth = pCurrentPrimEntry->vertices[0].Z;
    }

    pCurrentPrimEntry->sortDepth = depth;

#if !defined(AITD_UE4)
    s_dc_rdbg_prims_total++;
    if (depth > 100)
#endif
    {
#ifdef DREAMCAST
        s_dc_rdbg_prims_kept++;
#endif
        positionInPrimEntry++;

        numOfPrimitiveToRender++;
        ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);
    }
#if !defined(AITD_UE4)
    else
    {
#ifdef DREAMCAST
        s_dc_rdbg_prims_culled_depth++;
#endif
    }
#endif
}

void processPrim_Sphere(int primType, sPrimitive* ptr, char** out)
{
    primEntryStruct* pCurrentPrimEntry = &primTable[positionInPrimEntry];

    ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);

    pCurrentPrimEntry->type = primTypeEnum_Sphere;
    pCurrentPrimEntry->numOfVertices = 1;
    pCurrentPrimEntry->color = ptr->m_color;
    pCurrentPrimEntry->material = ptr->m_material;
    pCurrentPrimEntry->size = ptr->m_size;

    float depth = 32000.f;
    {
        u16 pointIndex;
        pointIndex = ptr->m_points[0] * 6;
        ASSERT((pointIndex % 2) == 0);
        pCurrentPrimEntry->vertices[0].X = renderPointList[pointIndex / 2];
        pCurrentPrimEntry->vertices[0].Y = renderPointList[(pointIndex / 2) + 1];
        pCurrentPrimEntry->vertices[0].Z = renderPointList[(pointIndex / 2) + 2];

        depth = pCurrentPrimEntry->vertices[0].Z;
    }

    pCurrentPrimEntry->sortDepth = depth;

#if !defined(AITD_UE4)
    s_dc_rdbg_prims_total++;
    if (depth > 100)
#endif
    {
#ifdef DREAMCAST
        s_dc_rdbg_prims_kept++;
#endif
        positionInPrimEntry++;

        numOfPrimitiveToRender++;
        ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);
    }
#if !defined(AITD_UE4)
    else
    {
#ifdef DREAMCAST
        s_dc_rdbg_prims_culled_depth++;
#endif
    }
#endif
}

void primType5(int primType, char** ptr, char** out) // draw out of hardClip
{
	(void)primType;
	(void)ptr;
	(void)out;
	printf("ignoring prim type 5\n");
}

void line(int x1, int y1, int x2, int y2, char c);

void renderLine(primEntryStruct* pEntry) // line
{
    osystem_draw3dLine( pEntry->vertices[0].X,pEntry->vertices[0].Y,pEntry->vertices[0].Z,
						pEntry->vertices[1].X,pEntry->vertices[1].Y,pEntry->vertices[1].Z,
						pEntry->color);
}

void renderPoly(primEntryStruct* pEntry) // poly
{
    osystem_fillPoly((float*)pEntry->vertices,pEntry->numOfVertices, pEntry->color, pEntry->material);
}

void renderZixel(primEntryStruct* pEntry) // point
{
    static float pointSize = 20.f;
    const float invZp = fitd_rcpf_fast((float)(pEntry->vertices[0].Z + cameraPerspective));
    float transformedSize = (pointSize * (float)cameraFovX) * invZp;

    osystem_drawPoint(pEntry->vertices[0].X,pEntry->vertices[0].Y,pEntry->vertices[0].Z,pEntry->color,pEntry->material, transformedSize);
}

void renderPoint(primEntryStruct* pEntry) // point
{
    static float pointSize = 0.3f; // TODO: better way to compute that?
    osystem_drawPoint(pEntry->vertices[0].X,pEntry->vertices[0].Y,pEntry->vertices[0].Z,pEntry->color, pEntry->material, pointSize);
}

void renderBigPoint(primEntryStruct* pEntry) // point
{
    static float bigPointSize = 1.f; // TODO: better way to compute that?
    osystem_drawPoint(pEntry->vertices[0].X,pEntry->vertices[0].Y,pEntry->vertices[0].Z,pEntry->color, pEntry->material, bigPointSize);
}

void renderSphere(primEntryStruct* pEntry) // sphere
{
    float transformedSize;

    {
        const float invZp = fitd_rcpf_fast((float)(pEntry->vertices[0].Z + cameraPerspective));
        transformedSize = ((float)pEntry->size * (float)cameraFovX) * invZp;
    }

    osystem_drawSphere(pEntry->vertices[0].X,pEntry->vertices[0].Y,pEntry->vertices[0].Z,pEntry->color, pEntry->material, transformedSize);
}


void defaultRenderFunction(primEntryStruct* buffer)
{
    printf("Unsupported renderType\n");
}

typedef void (*renderFunction)(primEntryStruct* buffer);

renderFunction renderFunctions[]={
    renderLine, // line
    renderPoly, // poly
    renderPoint, // point
    renderSphere, // sphere
    nullptr,
    nullptr,
    renderBigPoint,
	renderZixel,
};

int DisplayObject(int x,int y,int z,int alpha,int beta,int gamma, sBody* pBody)
{
    int numPrim;
    int i;
    char* out;

    // reinit the 2 static tables
    positionInPrimEntry = 0;
    //

#ifdef DREAMCAST
    s_dc_rdbg_aff_calls++;
#endif

    BBox3D1 = 0x7FFF;
    BBox3D2 = 0x7FFF;

    BBox3D3 = -0x7FFF;
    BBox3D4 = -0x7FFF;

    renderVar1 = 0;

    numOfPrimitiveToRender = 0;

    renderVar2 = renderBuffer;

    modelFlags = pBody->m_flags;

    if(modelFlags&INFO_ANIM)
    {
        if(!AnimNuage(x,y,z,alpha,beta,gamma, pBody))
        {
            BBox3D3 = -32000;
            BBox3D4 = -32000;
            BBox3D1 = 32000;
            BBox3D2 = 32000;
            return(2);
        }
    }
    else
        if(!(modelFlags&INFO_TORTUE))
        {
            if(!RotateAndProjectBody(x,y,z,alpha,beta,gamma, pBody))
            {
                BBox3D3 = -32000;
                BBox3D4 = -32000;
                BBox3D1 = 32000;
                BBox3D2 = 32000;
                return(2);
            }
        }
        else
        {
            printf("unsupported model type prerenderFlag4 in renderer !\n");

            BBox3D3 = -32000;
            BBox3D4 = -32000;
            BBox3D1 = 32000;
            BBox3D2 = 32000;
            return(2);
        }

        numPrim = pBody->m_primitives.size();

        if(!numPrim)
        {
            BBox3D3 = -32000;
            BBox3D4 = -32000;
            BBox3D1 = 32000;
            BBox3D2 = 32000;
            return(2);
        }

        out = primBuffer;

		// create the list of all primitives to render
        for(i=0;i<numPrim;i++)
        {
            sPrimitive* pPrimitive = &pBody->m_primitives[i];
            primTypeEnum primType = pPrimitive->m_type;

			switch(primType)
			{
			case primTypeEnum_Line:
				processPrim_Line(primType, pPrimitive,&out);
				break;
			case primTypeEnum_Poly:
				processPrim_Poly(primType, pPrimitive,&out);
				break;
			case primTypeEnum_Point:
			case primTypeEnum_BigPoint:
			case primTypeEnum_Zixel:
				processPrim_Point(primType, pPrimitive,&out);
				break;
			case primTypeEnum_Sphere:
				processPrim_Sphere(primType, pPrimitive,&out);
				break;
            case processPrim_PolyTexture9:
            case processPrim_PolyTexture10:
                processPrim_Poly(primType, pPrimitive, &out);
                break;
			default:
				return 0;
				assert(0);
			}

        }

        // Sort primitives by depth (painter's algorithm) to reduce poly overlap issues
        // on renderers without a Z-buffer. Draw far -> near.
        std::array<primEntryStruct*, NUM_MAX_PRIM_ENTRY> sorted;
        for (i = 0; i < numOfPrimitiveToRender; i++)
            sorted[i] = &primTable[i];

        std::sort(sorted.begin(), sorted.begin() + numOfPrimitiveToRender,
                  [](const primEntryStruct* a, const primEntryStruct* b) {
                      return a->sortDepth > b->sortDepth;
                  });

        if(!numOfPrimitiveToRender)
        {
            BBox3D3 = -32000;
            BBox3D4 = -32000;
            BBox3D1 = 32000;
            BBox3D2 = 32000;
            return(1); // model ok, but out of screen
        }

        for(i=0;i<numOfPrimitiveToRender;i++)
            renderFunctions[sorted[i]->type](sorted[i]);

        //DEBUG
        /*  for(i=0;i<numPointInPoly;i++)
        {
        int x;
        int y;

        x = renderPointList[i*3];
        y = renderPointList[i*3+1];

        if(x>=0 && x < 319 && y>=0 && y<199)
        {
        screen[y*320+x] = 15;
        }
        }*/
        //

        osystem_flushPendingPrimitives();

#ifdef DREAMCAST
        if (!DC_IsMenuActive())
        {
            const uint64_t now_ms = timer_ms_gettime64();
            if (now_ms >= s_dc_rdbg_next_ms)
            {
                s_dc_rdbg_next_ms = now_ms + 1000;
                const int rglTris = (int)(g_rglTriVtx.size() / 3);
                const int rglLines = (int)(g_rglLineVtx.size() / 2);

                I_Printf("[dc][render] DisplayObject:%d prims:%d kept:%d culled_depth:%d rgl_tris:%d rgl_lines:%d rgl_tri_vec:%p rgl_line_vec:%p\n",
                         s_dc_rdbg_aff_calls,
                         s_dc_rdbg_prims_total,
                         s_dc_rdbg_prims_kept,
                         s_dc_rdbg_prims_culled_depth,
                         rglTris,
                         rglLines,
                         (void*)&g_rglTriVtx,
                         (void*)&g_rglLineVtx);
                s_dc_rdbg_aff_calls = 0;
                s_dc_rdbg_prims_total = 0;
                s_dc_rdbg_prims_kept = 0;
                s_dc_rdbg_prims_culled_depth = 0;
            }
        }
#endif
        return(0);
}

// Compatibility wrapper (old French name).
int AffObjet(int x,int y,int z,int alpha,int beta,int gamma, sBody* pBody)
{
    return DisplayObject(x, y, z, alpha, beta, gamma, pBody);
}

void computeScreenBox(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr)
{
    BBox3D1 = 0x7FFF;
    BBox3D2 = 0x7FFF;

    BBox3D3 = -0x7FFF;
    BBox3D4 = -0x7FFF;

    renderVar1 = 0;

    numOfPrimitiveToRender = 0;

    renderVar2 = renderBuffer;

    modelFlags = bodyPtr->m_flags;

    if(modelFlags&INFO_ANIM)
    {
        AnimNuage(x,y,z,alpha,beta,gamma, bodyPtr);
    }
}
