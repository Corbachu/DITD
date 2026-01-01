//----------------------------------------------------------------------------
//  Dream In The Dark HQR resource loader (Resources)
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
#pragma once

template <typename T>
struct hqrSubEntryStruct;

template <typename T>
struct hqrEntryStruct;

template <typename T>
T* HQR_Get(hqrEntryStruct<T>* hqrPtr, int index);

hqrEntryStruct<char>* HQR_Init(int size, int numEntry);
int HQ_Malloc(hqrEntryStruct<char>* hqrPtr,int size);
char* HQ_PtrMalloc(hqrEntryStruct<char>* hqrPtr, int index);
void HQ_Free_Malloc(hqrEntryStruct<char>* hqrPtr, int index);

template <typename T>
void HQR_Free(hqrEntryStruct<T>* hqrPtr);

template <typename T>
hqrEntryStruct<T>* HQR_InitRessource(const char* name, int size, int numEntries);

template <typename T>
void HQR_Reset(hqrEntryStruct<T>* hqrPtr);

template <typename T>
void configureHqrHero(hqrEntryStruct<T>* hqrPtr, const char* name);

struct sBody* createBodyFromPtr(void* ptr);


