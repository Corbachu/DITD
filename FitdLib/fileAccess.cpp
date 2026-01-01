//----------------------------------------------------------------------------
//  Dream In The Dark file Access (File Access)
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
// seg 20
void fatalError(int type, const char* name)
{
    //  freeScene();
    freeAll();
    printf("Error: %s\n", name);
    assert(0);
}

extern "C" {
    extern char homePath[512];
}

char* loadFromItd(const char* name)
{
    FILE* fHandle;
    char* ptr;

    char filePath[512];
    strcpy(filePath, homePath);
    strcat(filePath, name);

    fHandle = fopen(filePath,"rb");
    if(!fHandle)
    {
        fatalError(0,name);
        return NULL;
    }
    fseek(fHandle,0,SEEK_END);
    fileSize = ftell(fHandle);
    fseek(fHandle,0,SEEK_SET);
    ptr = (char*)malloc(fileSize);

    if(!ptr)
    {
        fatalError(1,name);
        return NULL;
    }
    fread(ptr,fileSize,1,fHandle);
    fclose(fHandle);
    return(ptr);
}

char* CheckLoadMallocPak(const char* name, int index)
{
    char* ptr;
    ptr = loadPak(name, index);
    if(!ptr)
    {
        fatalError(0,name);
    }
    return ptr;
}
