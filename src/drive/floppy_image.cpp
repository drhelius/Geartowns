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

#include <new>
#include <time.h>
#include "floppy_image.h"
#include "../media/crc.h"

struct FloppyImage_Geometry
{
    u32 size;
    u8 cylinders;
    u8 sectors;
    u8 n;
    u8 media;
};

struct FloppyImage_Format
{
    FloppyImage_Geometry geometry;
    u8 sectors_per_cluster;
    u16 root_entries;
    u8 media_descriptor;
    u16 fat_sectors;
    bool ipl;
    u8 loader_sector;
    u8 loader_head;
    u16 empty_root_sector;
};

static const int k_floppy_raw_geometries = 5;
static const u32 k_floppy_raw_2dd_max_size = 737280;

static const FloppyImage_Geometry k_floppy_raw_geometry[k_floppy_raw_geometries] =
{
    { 1261568, 77, 8, 3, k_floppy_media_2hd },
    { 1310720, 80, 8, 3, k_floppy_media_2hd },
    { 655360, 80, 8, 2, k_floppy_media_2dd },
    { 737280, 80, 9, 2, k_floppy_media_2dd },
    { 1474560, 80, 18, 2, k_floppy_media_2hd }
};

// Towns OS FORMAT's 1.44 MB entry reuses the 720 KB data position and writes the empty root at sector 35
static const FloppyImage_Format k_floppy_blank_format[FLOPPY_BLANK_COUNT] =
{
    { { 1261568, 77, 8, 3, k_floppy_media_2hd }, 1, 192, 0xFE, 2, true, 4, 1, 5 },
    { { 655360, 80, 8, 2, k_floppy_media_2dd }, 2, 112, 0xFB, 2, false, 5, 1, 5 },
    { { 737280, 80, 9, 2, k_floppy_media_2dd }, 2, 112, 0xF9, 3, false, 6, 1, 7 },
    { { 1474560, 80, 18, 2, k_floppy_media_2hd }, 1, 224, 0xF0, 9, false, 6, 1, 35 }
};

static const u8 k_floppy_boot_header[11] = { 'I', 'P', 'L', '4', 0xEB, 0x4D, 0x06, ' ', ' ', ' ', ' ' };
static const char k_floppy_volume_label[] = "USERDISK";
static const u32 k_floppy_boot_loader_offset = 0x35;
static const u8 k_floppy_boot_loader[459] =
{
    0x40, 0x00, 0x03, 0xAE, 0x01, 0x24, 0x01, 0xC7, 0x00, 0xAE, 0x01, 0x0A, 0x01, 0x88, 0x00, 0xBB,
    0x00, 0xBB, 0x00, 0xBB, 0x00, 0xC3, 0x00, 0x46, 0x42, 0x49, 0x4F, 0x53, 0x00, 0x00, 0x0E, 0x1F,
    0x50, 0x53, 0x80, 0x3E, 0x06, 0x00, 0x04, 0x75, 0x0E, 0xB7, 0x02, 0xB3, 0x04, 0xC6, 0x06, 0x20,
    0x00, 0x06, 0xC6, 0x06, 0x21, 0x00, 0x00, 0x80, 0xE3, 0x0F, 0x8B, 0xC3, 0x32, 0xFF, 0xD0, 0xE3,
    0xA3, 0x51, 0x00, 0xFF, 0x57, 0x38, 0x8A, 0xDC, 0x32, 0xFF, 0xD0, 0xE3, 0x8B, 0xF3, 0x5B, 0x58,
    0xFF, 0x64, 0x42, 0xBF, 0x40, 0x00, 0x8E, 0xC7, 0xBF, 0x00, 0x00, 0x0E, 0x1F, 0xBE, 0x4C, 0x00,
    0xB9, 0x05, 0x00, 0xFC, 0xF3, 0xA6, 0x75, 0x22, 0x8B, 0x0E, 0x51, 0x00, 0x80, 0x3E, 0x06, 0x00,
    0x06, 0x75, 0x03, 0xB9, 0x80, 0x02, 0xBA, 0x50, 0x00, 0x8E, 0xC2, 0xBF, 0x00, 0x00, 0x26, 0x8B,
    0x15, 0x52, 0x33, 0xD2, 0x52, 0xCB, 0xB4, 0x01, 0xEB, 0x06, 0xB4, 0x02, 0xEB, 0x02, 0xB4, 0x80,
    0xF9, 0xCB, 0x80, 0xCC, 0x20, 0x88, 0x26, 0x30, 0x00, 0xE8, 0x1B, 0x01, 0x80, 0xFC, 0x00, 0x75,
    0x33, 0xC7, 0x06, 0x31, 0x00, 0x01, 0x00, 0xB9, 0x78, 0x00, 0xE8, 0xCF, 0x00, 0x80, 0xFC, 0x00,
    0x75, 0x22, 0xFE, 0x06, 0x20, 0x00, 0x80, 0x3E, 0x20, 0x00, 0x08, 0x76, 0x10, 0xC6, 0x06, 0x20,
    0x00, 0x01, 0x80, 0x36, 0x21, 0x00, 0x01, 0x75, 0x04, 0xFF, 0x06, 0x22, 0x00, 0x83, 0x06, 0x35,
    0x00, 0x40, 0xE2, 0xD6, 0xC3, 0x80, 0xE4, 0x03, 0x80, 0xFC, 0x02, 0x74, 0x03, 0xE9, 0x99, 0x00,
    0xC7, 0x06, 0x31, 0x00, 0x78, 0x00, 0x80, 0xCC, 0x40, 0x88, 0x26, 0x30, 0x00, 0xEB, 0xAA, 0x80,
    0xCC, 0xB0, 0x88, 0x26, 0x30, 0x00, 0x8C, 0xDE, 0x81, 0xC6, 0x00, 0x01, 0x8E, 0xC6, 0x33, 0xF6,
    0xB8, 0x00, 0x04, 0x33, 0xD2, 0x26, 0xF7, 0x74, 0x0E, 0x8B, 0xE8, 0x83, 0xC6, 0x20, 0xB9, 0x0A,
    0x00, 0x26, 0x80, 0x3C, 0xFF, 0x74, 0x08, 0x83, 0xC6, 0x30, 0xE2, 0xF5, 0xE9, 0x57, 0x00, 0x26,
    0x8B, 0x44, 0x02, 0x26, 0x8B, 0x54, 0x04, 0x01, 0x06, 0x20, 0x00, 0x11, 0x16, 0x22, 0x00, 0xE8,
    0x85, 0x00, 0x80, 0xFC, 0x00, 0x75, 0x41, 0xC7, 0x06, 0x31, 0x00, 0x3F, 0x00, 0x83, 0xFD, 0x01,
    0x74, 0x04, 0xD1, 0x26, 0x31, 0x00, 0xE8, 0x33, 0x00, 0x80, 0xFC, 0x00, 0x75, 0x2A, 0xC7, 0x06,
    0x35, 0x00, 0x00, 0x10, 0xB8, 0x3F, 0x00, 0xC7, 0x06, 0x31, 0x00, 0x39, 0x00, 0x83, 0xFD, 0x01,
    0x74, 0x06, 0xD1, 0x26, 0x31, 0x00, 0xD1, 0xE0, 0x01, 0x06, 0x20, 0x00, 0x83, 0x16, 0x22, 0x00,
    0x00, 0xE8, 0x08, 0x00, 0xEB, 0x02, 0xB4, 0x04, 0xC3, 0xB4, 0x01, 0xC3, 0x51, 0x55, 0xC6, 0x06,
    0x37, 0x00, 0x03, 0x1E, 0xA0, 0x30, 0x00, 0x8B, 0x0E, 0x22, 0x00, 0x8B, 0x16, 0x20, 0x00, 0x8B,
    0x1E, 0x31, 0x00, 0xC5, 0x3E, 0x33, 0x00, 0xB4, 0x05, 0x9A, 0x14, 0x00, 0xFB, 0xFF, 0x1F, 0x80,
    0xFC, 0x00, 0x74, 0x10, 0xE8, 0x10, 0x00, 0x80, 0xFC, 0x00, 0x75, 0x08, 0xFE, 0x0E, 0x37, 0x00,
    0x75, 0xD1, 0xB4, 0x03, 0x5D, 0x59, 0xC3, 0x1E, 0xA0, 0x30, 0x00, 0xB4, 0x03, 0x9A, 0x14, 0x00,
    0xFB, 0xFF, 0x80, 0xFC, 0x00, 0x74, 0x02, 0xB4, 0x02, 0x1F, 0xC3
};

static const u32 k_floppy_mfm_header_size = 19;
static const u32 k_floppy_mfm_entry_size = 11;
static const u16 k_floppy_mfm_bit_rate_2hd = 400;
static const u16 k_floppy_mfm_sync = 0x4489;
static const u16 k_floppy_mfm_weak_cells = 0x8888;
static const u32 k_floppy_mfm_no_flux_cells = 5;
static const int k_floppy_max_zip_images = 64;
static const u32 k_floppy_mfm_data_window = 80;
static const u16 k_floppy_fm_gap_cells = 0x8888;
static const u16 k_floppy_fm_mark_cells = 0xAA22;
static const u16 k_floppy_fm_mark_tails[5] = { 0x2AA8, 0x28AA, 0x28A8, 0x288A, 0x2888 };

static u16 mfm_word(const u8* cells, u32 position)
{
    u16 value = 0;

    for (int bit = 0; bit < 16; bit++)
        value = (u16)((value << 1) | cells[position + bit]);

    return value;
}

static u8 mfm_byte(const u8* cells, u32 position, u32 phase = 1)
{
    u8 value = 0;

    for (int bit = 0; bit < 8; bit++)
        value = (u8)((value << 1) | cells[position + bit * 2 + phase]);

    return value;
}

static u8 fm_byte(const u8* cells, u32 position)
{
    u8 value = 0;

    for (int bit = 0; bit < 8; bit++)
        value = (u8)((value << 1) | cells[position + bit * 4 + 2]);

    return value;
}

static u8 field_byte(const u8* cells, u32 position, bool fm)
{
    return fm ? fm_byte(cells, position) : mfm_byte(cells, position);
}

static bool fm_mark(const u8* cells, u32 last_cell, u32 shift)
{
    // FM runs at half the MFM rate, so each FM cell takes two cells: the marks (clock C7 over FE, FB, FA, F9 or F8)
    // must follow at least two gap bytes of 00 with FF clocks
    if ((shift >> 16) != k_floppy_fm_mark_cells || last_cell < 95)
        return false;

    for (u32 k = 0; k < 4; k++)
    {
        if (mfm_word(cells, last_cell - 95 + k * 16) != k_floppy_fm_gap_cells)
            return false;
    }

    for (int t = 0; t < 5; t++)
    {
        if ((u16)shift == k_floppy_fm_mark_tails[t])
            return true;
    }

    return false;
}

static int compare_names(const char* a, const char* b)
{
    for (;; a++, b++)
    {
        int x = std::tolower((unsigned char)*a);
        int y = std::tolower((unsigned char)*b);

        if (x != y || x == 0)
            return x - y;
    }
}

bool FloppyImage::IsD77(const u8* data, u32 size)
{
    if (!IsValidPointer(data) || size < 0x20)
        return false;

    u8 media = data[0x1B];

    if ((data[0x1A] & ~0x10) != 0 || (media != 0x00 && media != 0x10 && media != 0x20 && media != 0x30 &&
        media != 0x40))
        return false;

    u32 disk_size = read_u32_le(data + 0x1C);

    if (disk_size < 0x20 || disk_size > size)
        return false;

    u32 header_size = GetHeaderSize(data, disk_size);

    if (header_size == 0)
        return false;

    for (int track = 0; track < k_floppy_tracks; track++)
    {
        u32 run_size = 0;

        if (GetTrackRun(data, disk_size, header_size, track, &run_size) == 0xFFFFFFFF)
            return false;
    }

    return true;
}

bool FloppyImage::IsCanonical(const u8* data, u32 size)
{
    return IsD77(data, size) && read_u32_le(data + 0x1C) == size && GetHeaderSize(data, size) == k_floppy_header_size;
}

bool FloppyImage::IsRaw(const u8* data, u32 size)
{
    return GetRawGeometry(data, size) >= 0;
}

bool FloppyImage::IsMFM(const u8* data, u32 size)
{
    if (!IsValidPointer(data) || size < k_floppy_mfm_header_size || memcmp(data, "HXCMFM", 6) != 0)
        return false;

    u32 entries = (u32)read_u16_le(data + 7) * data[9];
    u32 list = read_u32_le(data + 15);

    return list <= size && entries <= (size - list) / k_floppy_mfm_entry_size;
}

bool FloppyImage::IsImageName(const char* name)
{
    if (!IsValidPointer(name))
        return false;

    return ends_with_no_case(name, ".d77") || ends_with_no_case(name, ".d88") || ends_with_no_case(name, ".hdm") ||
        ends_with_no_case(name, ".xdf") || ends_with_no_case(name, ".img") || ends_with_no_case(name, ".mfm");
}

int FloppyImage::GetDiskCount(const u8* data, u32 size)
{
    if (IsMFM(data, size))
        return 1;

    if (!IsD77(data, size))
        return IsRaw(data, size) ? 1 : 0;

    int count = 0;
    u32 offset = 0;

    while (offset + 0x20 <= size && IsD77(data + offset, size - offset))
    {
        offset += read_u32_le(data + offset + 0x1C);
        count++;
    }

    return count;
}

bool FloppyImage::GetDisk(const u8* data, u32 size, int index, u32* offset, u32* disk_size)
{
    bool mfm = IsMFM(data, size);

    if (mfm || !IsD77(data, size))
    {
        if (index != 0 || (!mfm && !IsRaw(data, size)))
            return false;

        *offset = 0;
        *disk_size = size;
        return true;
    }

    u32 position = 0;

    for (int i = 0; position + 0x20 <= size && IsD77(data + position, size - position); i++)
    {
        u32 length = read_u32_le(data + position + 0x1C);

        if (i == index)
        {
            *offset = position;
            *disk_size = length;
            return true;
        }

        position += length;
    }

    return false;
}

bool FloppyImage::HasEarlierCopy(const u8* data, u32 size, int index)
{
    u32 offset = 0;
    u32 disk_size = 0;

    if (!GetDisk(data, size, index, &offset, &disk_size))
        return false;

    for (int i = 0; i < index; i++)
    {
        u32 earlier_offset = 0;
        u32 earlier_size = 0;

        if (GetDisk(data, size, i, &earlier_offset, &earlier_size) && (earlier_size == disk_size) &&
            (memcmp(data + earlier_offset, data + offset, disk_size) == 0))
            return true;
    }

    return false;
}

bool FloppyImage::IsBootDisk(const u8* data, u32 size, int index)
{
    u32 offset = 0;
    u32 disk_size = 0;
    u32 image_size = 0;

    if (!GetDisk(data, size, index, &offset, &disk_size))
        return false;

    u8* image = Load(data + offset, disk_size, &image_size);
    bool boot = false;

    for (int i = 0; IsValidPointer(image) && !boot; i++)
    {
        u32 header = GetSectorHeader(image, image_size, 0, i);

        if (header == 0)
            break;

        boot = (image[header + 2] == 1) && ((image[header + 6] & k_floppy_density_fm) == 0) &&
            (read_u16_le(image + header + 14) >= 4) &&
            (memcmp(image + header + k_floppy_sector_header_size, "IPL4", 4) == 0);
    }

    SafeDeleteArray(image);
    return boot;
}

void FloppyImage::GetDiskName(const u8* disk, u32 disk_size, char* name, size_t name_size)
{
    if (name_size == 0)
        return;

    name[0] = '\0';

    if (!IsD77(disk, disk_size))
        return;

    size_t length = 0;

    for (int i = 0; i < 17 && disk[i] != 0 && length + 1 < name_size; i++)
    {
        if (disk[i] < 0x20 || disk[i] > 0x7E)
        {
            name[0] = '\0';
            return;
        }

        name[length++] = (char)disk[i];
    }

    while (length > 0 && name[length - 1] == ' ')
        length--;

    name[length] = '\0';
}

void FloppyImage::SetDiskName(u8* disk, u32 disk_size, const char* name)
{
    if (!IsValidPointer(name) || !IsD77(disk, disk_size))
        return;

    memset(disk, 0, 17);

    for (int i = 0; i < 16 && name[i] != 0; i++)
        disk[i] = (name[i] < 0x20 || name[i] > 0x7E) ? '_' : (u8)name[i];
}

u8* FloppyImage::Load(const u8* data, u32 size, u32* image_size)
{
    if (!IsValidPointer(data) || size == 0)
        return NULL;

    if (IsMFM(data, size))
        return CreateFromMFM(data, size, image_size);

    if (IsD77(data, size))
    {
        u32 disk_size = read_u32_le(data + 0x1C);
        return Build(data, data, disk_size, -1, NULL, 0, image_size);
    }

    if (IsRaw(data, size))
        return CreateFromRaw(data, size, image_size);

    return NULL;
}

u8* FloppyImage::CreateBlank(FloppyImage_Blank type, bool formatted, u32* image_size)
{
    const FloppyImage_Format& format = k_floppy_blank_format[CLAMP((int)type, 0, FLOPPY_BLANK_COUNT - 1)];
    const FloppyImage_Geometry& geometry = format.geometry;

    if (!formatted)
    {
        u8 header[k_floppy_header_size];
        memset(header, 0, sizeof(header));
        header[0x1B] = geometry.media;
        write_u32_le(header + 0x1C, k_floppy_header_size);
        return Build(header, header, k_floppy_header_size, -1, NULL, 0, image_size);
    }

    u8* raw = new (std::nothrow) u8[geometry.size];

    if (!IsValidPointer(raw))
        return NULL;

    u32 sector_size = 128U << geometry.n;
    u32 total_sectors = geometry.size / sector_size;
    u32 root_sector = 1 + (2 * format.fat_sectors);
    u32 data_sector = root_sector + ((format.root_entries * 32) / sector_size);
    memset(raw, 0xE5, geometry.size);
    memset(raw, 0, sector_size);
    memcpy(raw, k_floppy_boot_header, sizeof(k_floppy_boot_header));

    if (!format.ipl)
    {
        raw[0] = 0xEB;
        raw[1] = 0xFE;
        raw[2] = 0x90;
    }

    write_u16_le(raw + 11, (u16)sector_size);
    raw[13] = format.sectors_per_cluster;
    write_u16_le(raw + 14, 1);
    raw[16] = 2;
    write_u16_le(raw + 17, format.root_entries);
    write_u16_le(raw + 19, (u16)total_sectors);
    raw[21] = format.media_descriptor;
    write_u16_le(raw + 22, format.fat_sectors);
    write_u16_le(raw + 24, geometry.sectors);
    write_u16_le(raw + 26, 2);
    write_u16_le(raw + 30, (u16)data_sector);
    raw[32] = format.loader_sector;
    raw[33] = format.loader_head;
    raw[36] = geometry.sectors;
    raw[37] = 2;
    write_u16_le(raw + 38, geometry.cylinders);
    CopyBootLoader(raw);

    for (int fat = 0; fat < 2; fat++)
    {
        u8* table = raw + (1 + fat * format.fat_sectors) * sector_size;
        memset(table, 0, (size_t)format.fat_sectors * sector_size);
        table[0] = format.media_descriptor;
        table[1] = 0xFF;
        table[2] = 0xFF;
    }

    u8* empty_root = raw + (format.empty_root_sector * sector_size);

    for (u32 i = 0; i < format.root_entries; i++)
    {
        empty_root[i * 32] = 0;
        memset(empty_root + (i * 32) + 1, 0xF6, 31);
    }

    WriteVolumeLabel(raw + (root_sector * sector_size));

    u8* image = CreateFromRaw(raw, geometry.size, image_size);
    SafeDeleteArray(raw);
    return image;
}

void FloppyImage::CopyBootLoader(u8* boot_sector)
{
    memcpy(boot_sector + k_floppy_boot_loader_offset, k_floppy_boot_loader, sizeof(k_floppy_boot_loader));
}

void FloppyImage::WriteVolumeLabel(u8* entry)
{
    struct tm local;
    bool valid = get_host_time(time(NULL), false, &local);
    memset(entry, 0, 32);
    memset(entry, ' ', 11);
    memcpy(entry, k_floppy_volume_label, strlen(k_floppy_volume_label));
    entry[11] = 0x28;

    if (valid && (local.tm_year >= 80))
    {
        write_u16_le(entry + 22, (u16)((local.tm_hour << 11) | (local.tm_min << 5) | (local.tm_sec / 2)));
        write_u16_le(entry + 24, (u16)(((local.tm_year - 80) << 9) | ((local.tm_mon + 1) << 5) | local.tm_mday));
    }
}

u8* FloppyImage::ReplaceTrack(const u8* image, u32 size, int track, const u8* sectors, u32 sectors_size, u32* image_size)
{
    return Build(image, image, size, track, sectors, sectors_size, image_size);
}

u32 FloppyImage::GetRawSize(const u8* image, u32 size)
{
    u32 header_size = GetHeaderSize(image, size);
    u32 run_size = 0;
    u32 first = GetTrackRun(image, size, header_size, 0, &run_size);

    if (first == 0 || first == 0xFFFFFFFF)
        return 0;

    int count = read_u16_le(image + first + 4);
    u8 n = image[first + 3];

    for (int i = 0; i < k_floppy_raw_geometries; i++)
    {
        const FloppyImage_Geometry& geometry = k_floppy_raw_geometry[i];

        if (geometry.sectors == count && geometry.n == n &&
            IsRawLayout(image, size, header_size, geometry.cylinders, geometry.sectors, geometry.n))
            return geometry.size;
    }

    return 0;
}

bool FloppyImage::ExportRaw(const u8* image, u32 size, u8* raw, u32 raw_size)
{
    if (GetRawSize(image, size) != raw_size)
        return false;

    u32 header_size = GetHeaderSize(image, size);
    u32 run_size = 0;
    u32 first = GetTrackRun(image, size, header_size, 0, &run_size);
    int count = read_u16_le(image + first + 4);
    u32 sector_size = read_u16_le(image + first + 14);
    u32 tracks = raw_size / (count * sector_size);

    for (u32 track = 0; track < tracks; track++)
    {
        u32 offset = GetTrackRun(image, size, header_size, track, &run_size);

        for (int i = 0; i < count; i++)
        {
            u8 r = image[offset + 2];
            memcpy(raw + (track * count + r - 1) * sector_size, image + offset + k_floppy_sector_header_size,
                sector_size);
            offset += k_floppy_sector_header_size + sector_size;
        }
    }

    return true;
}

int FloppyImage::GetZipImageCount(const u8* archive, size_t archive_size)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!IsValidPointer(archive) || archive_size == 0 || !mz_zip_reader_init_mem(&zip, archive, archive_size, 0))
        return 0;

    mz_uint members[k_floppy_max_zip_images];
    int count = ListZipImages(&zip, members);
    mz_zip_reader_end(&zip);
    return count;
}

bool FloppyImage::ExtractFromZip(const u8* archive, size_t archive_size, int image, u8** data, u32* size, char* name,
    size_t name_size)
{
    *data = NULL;
    *size = 0;

    if (IsValidPointer(name) && name_size > 0)
        name[0] = '\0';

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!IsValidPointer(archive) || archive_size == 0 || !mz_zip_reader_init_mem(&zip, archive, archive_size, 0))
        return false;

    mz_uint members[k_floppy_max_zip_images];
    int count = ListZipImages(&zip, members);
    bool ok = image >= 0 && image < count && ExtractZipMember(&zip, members[image], data, size);

    if (ok && IsValidPointer(name) && name_size > 0)
        mz_zip_reader_get_filename(&zip, members[image], name, (mz_uint)name_size);

    mz_zip_reader_end(&zip);
    return ok;
}

u16 FloppyImage::CRC16(u16 crc, const u8* data, u32 size)
{
    for (u32 i = 0; i < size; i++)
    {
        crc ^= (u16)(data[i] << 8);

        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x8000) != 0 ? (u16)((crc << 1) ^ 0x1021) : (u16)(crc << 1);
    }

    return crc;
}

u32 FloppyImage::GetDataCRC(const u8* image, u32 size)
{
    u32 crc = 0;

    for (int track = 0; IsValidPointer(image) && (size >= k_floppy_header_size) && (track < k_floppy_tracks); track++)
    {
        u32 offset = read_u32_le(image + 0x20 + track * 4);

        if ((offset == 0) || (offset + k_floppy_sector_header_size > size))
            continue;

        int count = MIN((int)read_u16_le(image + offset + 4), k_floppy_max_sectors);
        u32 headers[k_floppy_max_sectors];
        int used = 0;

        for (int i = 0; i < count; i++)
        {
            if (offset + k_floppy_sector_header_size > size)
                break;

            u32 data_size = read_u16_le(image + offset + 14);
            const u8* sector = image + offset + k_floppy_sector_header_size;

            if (offset + k_floppy_sector_header_size + data_size > size)
                break;

            bool uniform = true;

            for (u32 b = 1; uniform && (b < data_size); b++)
                uniform = sector[b] == sector[0];

            if (!uniform)
            {
                int j = used++;

                while ((j > 0) && (memcmp(image + headers[j - 1], image + offset, 4) > 0))
                {
                    headers[j] = headers[j - 1];
                    j--;
                }

                headers[j] = offset;
            }

            offset += k_floppy_sector_header_size + data_size;
        }

        for (int i = 0; i < used; i++)
            crc = CalculateCRC32(crc, image + headers[i] + k_floppy_sector_header_size,
                (int)read_u16_le(image + headers[i] + 14));
    }

    return crc;
}

u32 FloppyImage::CalculateDataCRC(const u8* data, u32 size)
{
    u32 image_size = 0;
    u8* image = Load(data, size, &image_size);
    u32 crc = GetDataCRC(image, image_size);
    SafeDeleteArray(image);
    return crc;
}

FloppyImage_Weak* FloppyImage::GetWeakBytes(const u8* source, u32 source_size, const u8* image, u32 image_size, u32* count)
{
    *count = 0;

    if (!IsMFM(source, source_size) || !IsCanonical(image, image_size))
        return NULL;

    FloppyImage_Weak* found = new (std::nothrow) FloppyImage_Weak[k_floppy_max_weak_bytes];
    u32 found_count = 0;
    u32 decoded_size = 0;
    u8* decoded = IsValidPointer(found) ? CreateFromMFM(source, source_size, &decoded_size, found, &found_count) : NULL;
    FloppyImage_Weak* weak = NULL;

    if (IsValidPointer(decoded) && found_count > 0)
        weak = new (std::nothrow) FloppyImage_Weak[found_count];

    // A weak byte stays only while its sector still holds the data of the source
    for (u32 i = 0; IsValidPointer(weak) && i < found_count; i++)
    {
        int track = 0;
        int index = 0;
        u32 from = FindSector(decoded, decoded_size, found[i].offset, &track, &index);
        u32 to = from != 0 ? GetSectorHeader(image, image_size, track, index) : 0;
        u32 sector_size = to != 0 ? read_u16_le(image + to + 14) : 0;

        const u8* source_data = decoded + from + k_floppy_sector_header_size;
        bool same = to != 0 && memcmp(decoded + from, image + to, 4) == 0 &&
            read_u16_le(decoded + from + 14) == sector_size &&
            memcmp(source_data, image + to + k_floppy_sector_header_size, sector_size) == 0;

        if (!same)
            continue;

        u32 offset = to + (found[i].offset - from);
        u32 at = *count;

        while (at > 0 && weak[at - 1].offset > offset)
        {
            weak[at] = weak[at - 1];
            at--;
        }

        weak[at].offset = offset;
        weak[at].value = found[i].value;
        (*count)++;
    }

    SafeDeleteArray(decoded);
    SafeDeleteArray(found);

    if (*count == 0)
        SafeDeleteArray(weak);

    return weak;
}

u32 FloppyImage::FindSector(const u8* image, u32 size, u32 offset, int* track, int* index)
{
    u32 header_size = GetHeaderSize(image, size);

    for (int t = 0; t < k_floppy_tracks; t++)
    {
        u32 run_size = 0;
        u32 header = GetTrackRun(image, size, header_size, t, &run_size);

        if (header == 0 || header == 0xFFFFFFFF || offset < header || offset >= header + run_size)
            continue;

        int count = MIN((int)read_u16_le(image + header + 4), k_floppy_max_sectors);

        for (int i = 0; i < count; i++)
        {
            u32 data = header + k_floppy_sector_header_size;
            u32 data_size = read_u16_le(image + header + 14);

            if (offset >= data && offset < data + data_size)
            {
                *track = t;
                *index = i;
                return header;
            }

            header = data + data_size;
        }
    }

    return 0;
}

int FloppyImage::ListZipImages(mz_zip_archive* zip, mz_uint* members)
{
    int count = 0;
    mz_uint files = mz_zip_reader_get_num_files(zip);

    for (mz_uint i = 0; i < files; i++)
    {
        mz_zip_archive_file_stat stat;

        if (!mz_zip_reader_file_stat(zip, i, &stat) || stat.m_is_directory)
            continue;

        // The BIN tracks of a zipped CD-ROM image are not floppies
        if (ends_with_no_case(stat.m_filename, ".cue"))
            return 0;

        bool bin = ends_with_no_case(stat.m_filename, ".bin");

        if (!stat.m_is_supported || stat.m_uncomp_size == 0 || stat.m_uncomp_size > k_floppy_max_image_size ||
            (!bin && !IsImageName(stat.m_filename)) || count == k_floppy_max_zip_images)
            continue;

        if (bin)
        {
            u8* data = NULL;
            u32 size = 0;
            bool raw = ExtractZipMember(zip, i, &data, &size) && IsRaw(data, size);
            SafeDeleteArray(data);

            if (!raw)
                continue;
        }

        int at = count++;

        while (at > 0)
        {
            mz_zip_archive_file_stat previous;

            if (!mz_zip_reader_file_stat(zip, members[at - 1], &previous) ||
                compare_names(previous.m_filename, stat.m_filename) <= 0)
                break;

            members[at] = members[at - 1];
            at--;
        }

        members[at] = i;
    }

    return count;
}

bool FloppyImage::ExtractZipMember(mz_zip_archive* zip, mz_uint member, u8** data, u32* size)
{
    mz_zip_archive_file_stat stat;

    if (!mz_zip_reader_file_stat(zip, member, &stat) || stat.m_uncomp_size > k_floppy_max_image_size)
        return false;

    u32 length = (u32)stat.m_uncomp_size;
    u8* extracted = new (std::nothrow) u8[length > 0 ? length : 1];

    if (!IsValidPointer(extracted) || !mz_zip_reader_extract_to_mem(zip, member, extracted, length, 0))
    {
        SafeDeleteArray(extracted);
        return false;
    }

    *data = extracted;
    *size = length;
    return true;
}

u32 FloppyImage::GetSectorHeader(const u8* image, u32 size, int track, int index)
{
    u32 run_size = 0;
    u32 header = GetTrackRun(image, size, GetHeaderSize(image, size), track, &run_size);

    if (header == 0 || header == 0xFFFFFFFF || index >= MIN((int)read_u16_le(image + header + 4), k_floppy_max_sectors))
        return 0;

    for (int i = 0; i < index; i++)
        header += k_floppy_sector_header_size + read_u16_le(image + header + 14);

    return header;
}

int FloppyImage::GetRawGeometry(const u8* data, u32 size)
{
    if (!IsValidPointer(data))
        return -1;

    int padded = -1;

    for (int i = 0; i < k_floppy_raw_geometries; i++)
    {
        u32 raw_size = k_floppy_raw_geometry[i].size;

        if (raw_size == size)
            return i;

        if (raw_size > size || (padded >= 0 && k_floppy_raw_geometry[padded].size > raw_size))
            continue;

        u32 filler = raw_size;

        while (filler < size && data[filler] == 0xE5)
            filler++;

        if (filler == size)
            padded = i;
    }

    if (padded >= 0)
        return padded;

    const FloppyImage_Geometry& hd = k_floppy_raw_geometry[0];
    u32 sector_size = 128U << hd.n;

    if (size > k_floppy_raw_2dd_max_size && size < hd.size && (size % (hd.sectors * sector_size)) == 0 &&
        read_u16_le(data + 11) == sector_size)
        return 0;

    return -1;
}

u32 FloppyImage::GetHeaderSize(const u8* data, u32 size)
{
    u32 header_size = MIN(k_floppy_header_size, size);

    for (int track = 0; track < k_floppy_tracks; track++)
    {
        u32 entry = 0x20 + track * 4;

        if (entry + 4 > header_size)
            break;

        u32 offset = read_u32_le(data + entry);

        if (offset != 0 && offset < header_size)
            header_size = offset;
    }

    return header_size >= 0x20 ? header_size : 0;
}

u32 FloppyImage::GetTrackRun(const u8* data, u32 size, u32 header_size, int track, u32* run_size)
{
    *run_size = 0;
    u32 entry = 0x20 + track * 4;

    if (entry + 4 > header_size)
        return 0;

    u32 offset = read_u32_le(data + entry);

    if (offset == 0)
        return 0;

    if (offset < header_size || offset > size - k_floppy_sector_header_size)
        return 0xFFFFFFFF;

    int count = MIN((int)read_u16_le(data + offset + 4), k_floppy_max_sectors);

    if (count == 0)
        return 0;

    u32 position = offset;

    for (int i = 0; i < count; i++)
    {
        if (position + k_floppy_sector_header_size > size)
            return 0xFFFFFFFF;

        u32 data_size = read_u16_le(data + position + 14);

        if (position + k_floppy_sector_header_size + data_size > size)
            return 0xFFFFFFFF;

        position += k_floppy_sector_header_size + data_size;
    }

    *run_size = position - offset;
    return offset;
}

bool FloppyImage::IsRawLayout(const u8* image, u32 size, u32 header_size, int cylinders, int sectors, u8 n)
{
    u32 sector_size = 128U << n;
    u32 run_size = 0;

    for (int track = 0; track < k_floppy_tracks; track++)
    {
        u32 offset = GetTrackRun(image, size, header_size, track, &run_size);

        if (track >= cylinders * 2)
        {
            if (offset != 0)
                return false;

            continue;
        }

        if (offset == 0 || offset == 0xFFFFFFFF || read_u16_le(image + offset + 4) != sectors)
            return false;

        u32 found = 0;

        for (int i = 0; i < sectors; i++)
        {
            const u8* header = image + offset;
            u8 r = header[2];

            if (header[0] != track / 2 || header[1] != (track & 1) || header[3] != n || r < 1 || r > sectors ||
                read_u16_le(header + 14) != sector_size || (header[6] & k_floppy_density_fm) != 0)
                return false;

            found |= 1U << (r - 1);
            offset += k_floppy_sector_header_size + sector_size;
        }

        if (found != (1U << sectors) - 1)
            return false;
    }

    return true;
}

u8* FloppyImage::Build(const u8* header, const u8* data, u32 size, int track, const u8* sectors, u32 sectors_size, u32* image_size)
{
    u32 header_size = GetHeaderSize(data, size);
    u32 offsets[k_floppy_tracks];
    u32 runs[k_floppy_tracks];
    u32 total = k_floppy_header_size;

    if (header_size == 0)
        return NULL;

    for (int i = 0; i < k_floppy_tracks; i++)
    {
        if (i == track)
        {
            offsets[i] = 0;
            runs[i] = sectors_size;
        }
        else
        {
            offsets[i] = GetTrackRun(data, size, header_size, i, &runs[i]);

            if (offsets[i] == 0xFFFFFFFF)
                return NULL;
        }

        total += runs[i];
    }

    if (total > k_floppy_max_image_size)
        return NULL;

    u8* image = new (std::nothrow) u8[total];

    if (!IsValidPointer(image))
        return NULL;

    memset(image, 0, k_floppy_header_size);
    memcpy(image, header, 17);
    image[0x1A] = header[0x1A];
    image[0x1B] = header[0x1B];
    write_u32_le(image + 0x1C, total);

    u32 position = k_floppy_header_size;

    for (int i = 0; i < k_floppy_tracks; i++)
    {
        if (runs[i] == 0)
            continue;

        write_u32_le(image + 0x20 + i * 4, position);
        memcpy(image + position, i == track ? sectors : data + offsets[i], runs[i]);

        // A run cut at the sector limit keeps a consistent count in every header
        u32 end = position + runs[i];
        u16 count = 0;

        for (u32 p = position; p < end; p += k_floppy_sector_header_size + read_u16_le(image + p + 14))
            count++;

        for (u32 p = position; p < end; p += k_floppy_sector_header_size + read_u16_le(image + p + 14))
            write_u16_le(image + p + 4, count);

        position = end;
    }

    *image_size = total;
    return image;
}

u8* FloppyImage::CreateFromRaw(const u8* data, u32 size, u32* image_size)
{
    int index = GetRawGeometry(data, size);

    if (index < 0)
        return NULL;

    const FloppyImage_Geometry* geometry = &k_floppy_raw_geometry[index];

    u32 sector_size = 128U << geometry->n;
    u32 tracks = geometry->cylinders * 2;
    u32 total = k_floppy_header_size + tracks * geometry->sectors * (k_floppy_sector_header_size + sector_size);
    u8* image = new (std::nothrow) u8[total];

    if (!IsValidPointer(image))
        return NULL;

    memset(image, 0, total);
    image[0x1B] = geometry->media;
    write_u32_le(image + 0x1C, total);

    u32 position = k_floppy_header_size;

    for (u32 track = 0; track < tracks; track++)
    {
        write_u32_le(image + 0x20 + track * 4, position);

        for (int r = 1; r <= geometry->sectors; r++)
        {
            u8* header = image + position;
            header[0] = (u8)(track / 2);
            header[1] = (u8)(track & 1);
            header[2] = (u8)r;
            header[3] = geometry->n;
            write_u16_le(header + 4, geometry->sectors);
            write_u16_le(header + 14, (u16)sector_size);
            u32 source = (track * geometry->sectors + r - 1) * sector_size;

            if (source + sector_size <= size)
                memcpy(header + k_floppy_sector_header_size, data + source, sector_size);
            else
                memset(header + k_floppy_sector_header_size, 0xE5, sector_size);
            position += k_floppy_sector_header_size + sector_size;
        }
    }

    *image_size = total;
    return image;
}

u8* FloppyImage::CreateFromMFM(const u8* data, u32 size, u32* image_size, FloppyImage_Weak* weak, u32* weak_count)
{
    u32 entries = (u32)read_u16_le(data + 7) * data[9];
    u32 list = read_u32_le(data + 15);
    u8* image = new (std::nothrow) u8[k_floppy_max_image_size];
    u8* cells = NULL;
    u32 cells_size = 0;
    u32 runs[k_floppy_tracks];
    memset(runs, 0, sizeof(runs));

    if (IsValidPointer(weak_count))
        *weak_count = 0;

    if (!IsValidPointer(image))
        return NULL;

    memset(image, 0, k_floppy_header_size);
    image[0x1B] = read_u16_le(data + 12) < k_floppy_mfm_bit_rate_2hd ? k_floppy_media_2dd : k_floppy_media_2hd;
    u32 position = k_floppy_header_size;

    for (u32 i = 0; i < entries; i++)
    {
        const u8* entry = data + list + i * k_floppy_mfm_entry_size;
        int track = read_u16_le(entry) * 2 + entry[2];
        u32 length = read_u32_le(entry + 3);
        u32 offset = read_u32_le(entry + 7);

        if (entry[2] > 1 || track >= k_floppy_tracks || length == 0 || offset > size || length > size - offset ||
            length > k_floppy_max_image_size)
            continue;

        // Each track is one turn from the index, decoded a tenth of a turn further so a sector across it reads whole
        u32 count = length * 8;
        u32 extended = count + count / 10;

        if (extended > cells_size)
        {
            SafeDeleteArray(cells);
            cells = new (std::nothrow) u8[extended];
            cells_size = IsValidPointer(cells) ? extended : 0;

            if (cells_size == 0)
                break;
        }

        for (u32 cell = 0; cell < extended; cell++)
        {
            u32 bit = cell < count ? cell : cell - count;
            cells[cell] = (data[offset + (bit >> 3)] >> (7 - (bit & 7))) & 0x01;
        }

        u32 first_weak = IsValidPointer(weak_count) ? *weak_count : 0;
        u32 run = DecodeMFMTrack(cells, count, image + position, k_floppy_max_image_size - position, weak, weak_count);

        if (run == 0)
            continue;

        for (u32 w = first_weak; IsValidPointer(weak_count) && w < *weak_count; w++)
            weak[w].offset += position;

        write_u32_le(image + 0x20 + track * 4, position);
        runs[track] = run;
        position += run;
    }

    write_u32_le(image + 0x1C, position);
    u8* result = Build(image, image, position, -1, NULL, 0, image_size);

    // Build packs the runs in track order, so each weak byte moves with its track
    u32 kept = 0;

    for (u32 w = 0; IsValidPointer(result) && IsValidPointer(weak_count) && w < *weak_count; w++)
    {
        for (int track = 0; track < k_floppy_tracks; track++)
        {
            u32 start = read_u32_le(image + 0x20 + track * 4);

            if (runs[track] == 0 || weak[w].offset < start || weak[w].offset >= start + runs[track])
                continue;

            weak[kept].offset = weak[w].offset - start + read_u32_le(result + 0x20 + track * 4);
            weak[kept].value = weak[w].value;
            kept++;
            break;
        }
    }

    if (IsValidPointer(weak_count))
        *weak_count = kept;

    SafeDeleteArray(cells);
    SafeDeleteArray(image);
    return result;
}

u32 FloppyImage::DecodeMFMTrack(const u8* cells, u32 count, u8* run, u32 run_capacity, FloppyImage_Weak* weak,
    u32* weak_count)
{
    u32 limit = count + count / 10;
    u32 position = 0;
    u8* last = NULL;
    u32 last_start = 0;
    int sectors = 0;
    u32 shift = 0;

    for (u32 i = 0; i < limit; i++)
    {
        shift = (shift << 1) | cells[i];
        bool fm = fm_mark(cells, i, shift) && i + 1 + 32 * 6 <= limit;
        u32 step = fm ? 32 : 16;
        u32 p = fm ? i - 31 : i + 1;
        u32 start = p;

        if (!fm)
        {
            if ((u16)shift != k_floppy_mfm_sync)
                continue;

            int syncs = 1;
            shift = 0;

            while (p + 16 <= limit && mfm_word(cells, p) == k_floppy_mfm_sync)
            {
                syncs++;
                p += 16;
            }

            if (syncs < 3 || p + 16 * 7 > limit)
            {
                i = p - 1;
                continue;
            }

            start = p - 48;
        }

        // The FM CRC starts at the mark, the MFM one at the three A1 syncs before it
        u8 mark = field_byte(cells, p, fm);
        u32 crc_start = fm ? 3 : 0;

        if (mark == 0xFE && start < count && sectors < k_floppy_max_sectors &&
            position + k_floppy_sector_header_size <= run_capacity)
        {
            u8 field[8] = { 0xA1, 0xA1, 0xA1, 0xFE };

            for (int k = 0; k < 4; k++)
                field[4 + k] = field_byte(cells, p + step + k * step, fm);

            u16 crc = (u16)((field_byte(cells, p + step * 5, fm) << 8) | field_byte(cells, p + step * 6, fm));
            bool crc_ok = CRC16(0xFFFF, field + crc_start, 8 - crc_start) == crc;

            // An FM ID with a bad CRC can't be told apart from MFM data that happens to look like one
            if (fm && !crc_ok)
                continue;

            last = run + position;
            last_start = start;
            memset(last, 0, k_floppy_sector_header_size);
            memcpy(last, field + 4, 4);
            last[6] = fm ? k_floppy_density_fm : 0x00;
            last[8] = crc_ok ? k_floppy_status_no_data : k_floppy_status_id_crc;
            position += k_floppy_sector_header_size;
            sectors++;
        }
        else if ((mark == 0xFB || mark == 0xF8 || (fm && (mark == 0xFA || mark == 0xF9))) && IsValidPointer(last) &&
            read_u16_le(last + 14) == 0 && (last[6] == k_floppy_density_fm) == fm &&
            p - last_start < k_floppy_mfm_data_window * step)
        {
            // The data is kept as the FDC reads it, sized by the low two bits of N, also when it runs over the next IDs
            u32 data_size = 128U << (last[3] & 0x03);

            if (p + step * (data_size + 3) <= limit && position + data_size <= run_capacity)
            {
                u8 field[4 + 1024 + 2] = { 0xA1, 0xA1, 0xA1, mark };

                for (u32 k = 0; k < data_size + 2; k++)
                    field[4 + k] = field_byte(cells, p + step + k * step, fm);

                memcpy(last + k_floppy_sector_header_size, field + 4, data_size);
                write_u16_le(last + 14, (u16)data_size);
                last[7] = mark != 0xFB ? k_floppy_deleted : 0x00;

                if (last[8] == k_floppy_status_no_data)
                {
                    last[8] = CRC16(0xFFFF, field + crc_start, data_size + 6 - crc_start) == 0 ? 0x00 :
                        k_floppy_status_data_crc;
                }

                // Weak bytes read differently from turn to turn: runs of AA written one cell late read as 00 or AA,
                // depending on where the data separator locks, and stretches without flux transitions read as noise
                bool weak_bytes[1024];
                u32 weak_run = 0;
                u32 zero_run = 0;
                memset(weak_bytes, 0, sizeof(weak_bytes));

                for (u32 k = 0; IsValidPointer(weak) && !fm && k < data_size; k++)
                {
                    u32 byte_cells = p + 16 + k * 16;
                    weak_run = mfm_word(cells, byte_cells) == k_floppy_mfm_weak_cells ? weak_run + 1 : 0;

                    if (weak_run >= 2)
                        weak_bytes[k] = weak_bytes[k - 1] = true;

                    for (u32 c = 0; c < 16; c++)
                    {
                        zero_run = cells[byte_cells + c] == 0 ? zero_run + 1 : 0;

                        if (zero_run < k_floppy_mfm_no_flux_cells)
                            continue;

                        for (u32 w = (k * 16 + c + 1 - zero_run) / 16; w <= k; w++)
                            weak_bytes[w] = true;
                    }
                }

                for (u32 k = 0; IsValidPointer(weak) && k < data_size && *weak_count < k_floppy_max_weak_bytes; k++)
                {
                    if (!weak_bytes[k])
                        continue;

                    weak[*weak_count].offset = (u32)(last - run) + k_floppy_sector_header_size + k;
                    weak[*weak_count].value = mfm_byte(cells, p + 16 + k * 16, 0);
                    (*weak_count)++;
                }

                position += data_size;
            }
        }

        i = p + step - 1;
    }

    for (u32 p = 0; p < position; p += k_floppy_sector_header_size + read_u16_le(run + p + 14))
        write_u16_le(run + p + 4, (u16)sectors);

    return position;
}
