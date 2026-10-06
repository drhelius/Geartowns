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

#ifndef FLOPPY_IMAGE_H
#define FLOPPY_IMAGE_H

#include "../common/common.h"

enum FloppyImage_Blank
{
    FLOPPY_BLANK_2HD_1232 = 0,
    FLOPPY_BLANK_2DD_640,
    FLOPPY_BLANK_2DD_720,
    FLOPPY_BLANK_2HD_1440,
    FLOPPY_BLANK_COUNT
};

class FloppyImage
{
public:
    static bool IsD77(const u8* data, u32 size);
    static bool IsCanonical(const u8* data, u32 size);
    static bool IsRawSize(u32 size);
    static bool IsImageName(const char* name);
    static int GetDiskCount(const u8* data, u32 size);
    static bool GetDisk(const u8* data, u32 size, int index, u32* offset, u32* disk_size);
    static void GetDiskName(const u8* disk, u32 disk_size, char* name, size_t name_size);
    static u8* Load(const u8* data, u32 size, u32* image_size);
    static u8* CreateBlank(FloppyImage_Blank type, bool formatted, u32* image_size);
    static u8* ReplaceTrack(const u8* image, u32 size, int track, const u8* sectors, u32 sectors_size,
        u32* image_size);
    static u32 GetRawSize(const u8* image, u32 size);
    static bool ExportRaw(const u8* image, u32 size, u8* raw, u32 raw_size);
    static bool ExtractFromZip(const u8* archive, size_t archive_size, u8** data, u32* size, char* name,
        size_t name_size);

private:
    static u32 GetHeaderSize(const u8* data, u32 size);
    static u32 GetTrackRun(const u8* data, u32 size, u32 header_size, int track, u32* run_size);
    static u8* Build(const u8* header, const u8* data, u32 size, int track, const u8* sectors, u32 sectors_size,
        u32* image_size);
    static u8* CreateFromRaw(const u8* data, u32 size, u32* image_size);
};

static const u32 k_floppy_header_size = 0x2B0;
static const u32 k_floppy_sector_header_size = 16;
static const int k_floppy_tracks = 164;
static const int k_floppy_max_sectors = 64;
static const u32 k_floppy_max_image_size = 8 * 1024 * 1024;

static const u8 k_floppy_media_2d = 0x00;
static const u8 k_floppy_media_2dd = 0x10;
static const u8 k_floppy_media_2hd = 0x20;

static const u8 k_floppy_density_fm = 0x40;
static const u8 k_floppy_deleted = 0x10;
static const u8 k_floppy_status_id_crc = 0xA0;
static const u8 k_floppy_status_data_crc = 0xB0;
static const u8 k_floppy_status_no_id = 0xE0;
static const u8 k_floppy_status_no_data = 0xF0;

#endif /* FLOPPY_IMAGE_H */
