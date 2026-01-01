//----------------------------------------------------------------------------
//  Dream In The Dark fitd (Game Code)
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

extern "C" {
    int FitdMain(int argc, char* argv[]);
}

int FitdInit(int argc, char* argv[]);

int main(int argc, char* argv[])
{
    FitdInit(argc, argv);
    FitdMain(argc, argv);
    return 0;
}
