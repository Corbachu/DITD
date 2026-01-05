#include "common.h"

#include "r_occlude.h"

#include "fitd_endian_read.h"

#include <algorithm>
#include <array>
#include <vector>

struct sMaskStruct
{
    u16 x1;
    u16 y1;
    u16 x2;
    u16 y2;
    u16 deltaX;
    u16 deltaY;

    std::array<u8, 320 * 200> mask;
};

static std::vector<std::vector<sMaskStruct>> g_maskBuffers;

void loadMask(int cameraIdx)
{
    if (g_gameId == TIMEGATE)
        return;

    char name[16];

    sprintf(name, "MASK%02d", g_currentFloor);

    if (g_MaskPtr)
    {
        free(g_MaskPtr);
    }

    g_MaskPtr = (unsigned char*)loadPak(name, cameraIdx);

    g_maskBuffers.clear();
    g_maskBuffers.resize(cameraDataTable[NumCamera]->numViewedRooms);
    for (int i = 0; i < cameraDataTable[NumCamera]->numViewedRooms; i++)
    {
        cameraViewedRoomStruct* pRoomView = &cameraDataTable[NumCamera]->viewedRoomTable[i];
        unsigned char* pViewedRoomMask = g_MaskPtr + READ_LE_U32(g_MaskPtr + i * 4);

        g_maskBuffers[i].reserve(pRoomView->masks.size());
        for (int j = 0; j < pRoomView->masks.size(); j++)
        {
            g_maskBuffers[i].emplace_back();
            sMaskStruct* pDestMask = &g_maskBuffers[i].back();
            unsigned char* pMaskData = pViewedRoomMask + READ_LE_U32(pViewedRoomMask + j * 4);

            pDestMask->mask.fill(0);

            pDestMask->x1 = READ_LE_U16(pMaskData);
            pMaskData += 2;
            pDestMask->y1 = READ_LE_U16(pMaskData);
            pMaskData += 2;
            pDestMask->x2 = READ_LE_U16(pMaskData);
            pMaskData += 2;
            pDestMask->y2 = READ_LE_U16(pMaskData);
            pMaskData += 2;
            pDestMask->deltaX = READ_LE_U16(pMaskData);
            pMaskData += 2;
            pDestMask->deltaY = READ_LE_U16(pMaskData);
            pMaskData += 2;

            assert(pDestMask->deltaX == pDestMask->x2 - pDestMask->x1 + 1);
            assert(pDestMask->deltaY == pDestMask->y2 - pDestMask->y1 + 1);

            for (int k = 0; k < pDestMask->deltaY; k++)
            {
                u16 uNumEntryForLine = READ_LE_U16(pMaskData);
                pMaskData += 2;

                unsigned char* pSourceBuffer = (unsigned char*)aux;
                (void)pSourceBuffer;

                int offset = pDestMask->x1 + pDestMask->y1 * 320 + k * 320;

                for (int l = 0; l < uNumEntryForLine; l++)
                {
                    unsigned char uNumSkip = *(pMaskData++);
                    unsigned char uNumCopy = *(pMaskData++);

                    offset += uNumSkip;

                    for (int m = 0; m < uNumCopy; m++)
                    {
                        pDestMask->mask[offset] = 0xFF;
                        offset++;
                    }
                }
            }

            osystem_createMask(pDestMask->mask, i, j, pDestMask->x1, pDestMask->y1, pDestMask->x2, pDestMask->y2);
        }
    }
}

void fillpoly(s16* datas, int n, unsigned char c);
extern unsigned char* polyBackBuffer;

void createAITD1Mask()
{
    for (int viewedRoomIdx = 0; viewedRoomIdx < cameraDataTable[NumCamera]->numViewedRooms; viewedRoomIdx++)
    {
        cameraViewedRoomStruct* pcameraViewedRoomData = &cameraDataTable[NumCamera]->viewedRoomTable[viewedRoomIdx];

        char* data2 = room_PtrCamera[NumCamera] + pcameraViewedRoomData->offsetToMask;
        char* data = data2;
        data += 2;

        int numMask = *(s16*)(data2);

        for (int maskIdx = 0; maskIdx < numMask; maskIdx++)
        {
            sMaskStruct theMaskStruct;
            sMaskStruct* pDestMask = &theMaskStruct;
            pDestMask->mask.fill(0);
            polyBackBuffer = pDestMask->mask.data();

            int numMaskZone = READ_LE_U16(data);
            (void)numMaskZone;
            char* src = data2 + READ_LE_U16(data + 2);

            int minX = 319;
            int maxX = 0;
            int minY = 199;
            int maxY = 0;

            {
                int numMaskPoly = READ_LE_U16(src);
                src += 2;

                for (int maskPolyIdx = 0; maskPolyIdx < numMaskPoly; maskPolyIdx++)
                {
                    int numPoints = READ_LE_U16(src);
                    src += 2;

                    memcpy(cameraBuffer, src, numPoints * 4);

                    fillpoly((short*)src, numPoints, 0xFF);

                    for (int verticeId = 0; verticeId < numPoints; verticeId++)
                    {
                        short verticeX = *(short*)(src + verticeId * 4 + 0);
                        short verticeY = *(short*)(src + verticeId * 4 + 2);

                        minX = std::min<int>(minX, verticeX);
                        minY = std::min<int>(minY, verticeY);
                        maxX = std::max<int>(maxX, verticeX);
                        maxY = std::max<int>(maxY, verticeY);
                    }

                    src += numPoints * 4;
                }

                polyBackBuffer = nullptr;
            }

            osystem_createMask(pDestMask->mask, viewedRoomIdx, maskIdx, minX - 1, minY - 1, maxX + 1, maxY + 1);

            int numOverlay = READ_LE_U16(data);
            data += 2;
            data += ((numOverlay * 4) + 1) * 2;
        }
    }
}

static int isBgOverlayRequired(int X1, int X2, int Z1, int Z2, char* data, int param)
{
    int i;
    for (i = 0; i < param; i++)
    {
        int zoneX1 = *(s16*)(data);
        int zoneZ1 = *(s16*)(data + 2);
        int zoneX2 = *(s16*)(data + 4);
        int zoneZ2 = *(s16*)(data + 6);

        if (X1 >= zoneX1 && Z1 >= zoneZ1 && X2 <= zoneX2 && Z2 <= zoneZ2)
        {
            return (1);
        }

        data += 0x8;
    }

    return (0);
}

void drawBgOverlay(tObject* actorPtr)
{
    char* data;
    char* data2;

    int numOverlayZone;

    actorPtr->screenXMin = BBox3D1;
    actorPtr->screenYMin = BBox3D2;
    actorPtr->screenXMax = BBox3D3;
    actorPtr->screenYMax = BBox3D4;

    SetClip(BBox3D1, BBox3D2, BBox3D3, BBox3D4);

    cameraDataStruct* pCamera = cameraDataTable[NumCamera];

    // look for the correct room data of that camera
    cameraViewedRoomStruct* pcameraViewedRoomData = NULL;
    int relativeCameraIndex = -1;
    for (int i = 0; i < pCamera->numViewedRooms; i++)
    {
        if (pCamera->viewedRoomTable[i].viewedRoomIdx == actorPtr->room)
        {
            pcameraViewedRoomData = &pCamera->viewedRoomTable[i];
            relativeCameraIndex = i;
            break;
        }
    }
    if (pcameraViewedRoomData == NULL)
        return;

    if (g_gameId == AITD1)
    {
        data2 = room_PtrCamera[NumCamera] + pcameraViewedRoomData->offsetToMask;
        data = data2;
        data += 2;

        numOverlayZone = *(s16*)(data2);

        for (int i = 0; i < numOverlayZone; i++)
        {
            int numOverlay;
            char* src = data2 + *(u16*)(data + 2);
            (void)src;

            if (isBgOverlayRequired(actorPtr->zv.ZVX1 / 10, actorPtr->zv.ZVX2 / 10,
                                    actorPtr->zv.ZVZ1 / 10, actorPtr->zv.ZVZ2 / 10,
                                    data + 4,
                                    *(s16*)(data)))
            {
                osystem_setClip(clipLeft, clipTop, clipRight, clipBottom);
                osystem_drawMask(relativeCameraIndex, i);
                osystem_clearClip();
            }

            numOverlay = *(s16*)(data);
            data += 2;
            data += ((numOverlay * 4) + 1) * 2;
        }
    }
    else
    {
        for (int i = 0; i < pcameraViewedRoomData->masks.size(); i++)
        {
            cameraMaskStruct* pMaskZones = &pcameraViewedRoomData->masks[i];

            for (int j = 0; j < pMaskZones->numTestRect; j++)
            {
                rectTestStruct* pRect = &pMaskZones->rectTests[j];

                int actorX1 = actorPtr->zv.ZVX1 / 10;
                int actorX2 = actorPtr->zv.ZVX2 / 10;
                int actorZ1 = actorPtr->zv.ZVZ1 / 10;
                int actorZ2 = actorPtr->zv.ZVZ2 / 10;

                if (actorX1 >= pRect->zoneX1 && actorZ1 >= pRect->zoneZ1 && actorX2 <= pRect->zoneX2 && actorZ2 <= pRect->zoneZ2)
                {
                    osystem_setClip(clipLeft, clipTop, clipRight, clipBottom);
                    osystem_drawMask(relativeCameraIndex, i);
                    osystem_clearClip();
                    break;
                }
            }
        }
    }

    SetClip(0, 0, 319, 199);
}
