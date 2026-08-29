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

#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H

#include "types.h"

#define GT_DEBUG_MEMORY_MAX_REGIONS 32
#define GT_DEBUG_MEMORY_REGION_NAME_SIZE 64
#define GT_DEBUG_MEMORY_REASON_SIZE 128

enum GT_Debug_Memory_Space
{
    GT_DEBUG_MEMORY_LOGICAL = 0,
    GT_DEBUG_MEMORY_LINEAR,
    GT_DEBUG_MEMORY_PHYSICAL,
    GT_DEBUG_MEMORY_BUS,
    GT_DEBUG_MEMORY_REGION,
    GT_DEBUG_MEMORY_IO,
    GT_DEBUG_MEMORY_SPACE_COUNT
};

enum GT_Debug_Memory_Status
{
    GT_DEBUG_MEMORY_VALID = 0,
    GT_DEBUG_MEMORY_READ_ONLY,
    GT_DEBUG_MEMORY_UNMAPPED,
    GT_DEBUG_MEMORY_UNAVAILABLE
};

enum GT_Debug_Memory_Region_Flags
{
    GT_DEBUG_REGION_READABLE   = 0x0001,
    GT_DEBUG_REGION_WRITABLE   = 0x0002,
    GT_DEBUG_REGION_EXECUTABLE = 0x0004,
    GT_DEBUG_REGION_ROM        = 0x0008,
    GT_DEBUG_REGION_MAPPED     = 0x0010,
    GT_DEBUG_REGION_MMIO       = 0x0020,
    GT_DEBUG_REGION_VIDEO      = 0x0040,
    GT_DEBUG_REGION_AUDIO      = 0x0080
};

enum GT_Debug_Memory_Region_Id
{
    GT_DEBUG_REGION_MAIN_RAM = 1,
    GT_DEBUG_REGION_SYSTEM_ROM,
    GT_DEBUG_REGION_OS_ROM,
    GT_DEBUG_REGION_FONT_ROM,
    GT_DEBUG_REGION_DICTIONARY_ROM,
    GT_DEBUG_REGION_FONT20_ROM,
    GT_DEBUG_REGION_CMOS,
    GT_DEBUG_REGION_VRAM,
    GT_DEBUG_REGION_SPRITE_RAM,
    GT_DEBUG_REGION_PCM_RAM,
    GT_DEBUG_REGION_MEDIA_IMAGE = 0x10000
};

struct GT_Debug_Memory_Address
{
    GT_Debug_Memory_Space space;
    u32 address;
    u16 segment;
    s8 segment_register;
    int region;
};

struct GT_Debug_Memory_Region
{
    int id;
    char name[GT_DEBUG_MEMORY_REGION_NAME_SIZE];
    u32 size;
    u32 physical_base;
    u32 flags;
};

struct GT_Debug_Memory_Block_Info
{
    u64 snapshot_id;
    u32 map_generation;
};

struct GT_Debug_Memory_Translation
{
    bool logical_valid;
    bool linear_valid;
    bool physical_valid;
    bool bus_valid;
    bool region_valid;
    u16 segment;
    u32 offset;
    u32 segment_base;
    u32 segment_limit;
    u32 linear;
    u32 page_directory_entry;
    u32 page_table_entry;
    u32 page_flags;
    u32 physical;
    u32 bus;
    int region;
    u32 region_offset;
    u32 map_generation;
    char region_name[GT_DEBUG_MEMORY_REGION_NAME_SIZE];
    char reason[GT_DEBUG_MEMORY_REASON_SIZE];
};

#endif /* DEBUG_MEMORY_H */
