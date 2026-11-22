/*
 * Geartowns - FM Towns Emulator
 * Copyright (C) 2026  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#ifndef FLOPPY_SET_H
#define FLOPPY_SET_H

#include <string>
#include <vector>
#include "../common/common.h"
#include "floppy_image.h"

struct FloppySet_Disk
{
    int image;
    int images;
    int disk;
    int count;
    u32 crc;
    std::string member;
    std::string name;
};

class FloppySet
{
public:
    static bool LoadSource(const char* path, int image, std::vector<u8>& data, char* name = NULL, size_t name_size = 0);
    static bool ListDisks(const char* path, std::vector<FloppySet_Disk>& disks);
    static std::string GetDiskLabel(const FloppySet_Disk& disk);
    static bool IsBootDisk(const char* path, int image, int disk);
    static bool ReadDisk(const char* path, int image, int disk, std::vector<u8>& file, u32* offset, u32* size);
    static bool ReadWorkingCopy(const char* path, std::vector<u8>& copy);
};

static const u32 k_floppy_max_file_size = k_floppy_max_image_size * 4;

#endif /* FLOPPY_SET_H */
