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
#include "floppy_image.h"

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
};

static const int k_floppy_raw_geometries = 4;

static const FloppyImage_Geometry k_floppy_raw_geometry[k_floppy_raw_geometries] =
{
    { 1261568, 77, 8, 3, k_floppy_media_2hd },
    { 655360, 80, 8, 2, k_floppy_media_2dd },
    { 737280, 80, 9, 2, k_floppy_media_2dd },
    { 1474560, 80, 18, 2, k_floppy_media_2hd }
};

static const FloppyImage_Format k_floppy_blank_format[FLOPPY_BLANK_COUNT] =
{
    { { 1261568, 77, 8, 3, k_floppy_media_2hd }, 1, 192, 0xFE, 2 },
    { { 655360, 80, 8, 2, k_floppy_media_2dd }, 2, 112, 0xFB, 2 },
    { { 737280, 80, 9, 2, k_floppy_media_2dd }, 2, 112, 0xF9, 3 },
    { { 1474560, 80, 18, 2, k_floppy_media_2hd }, 1, 224, 0xF0, 9 }
};

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

bool FloppyImage::IsRawSize(u32 size)
{
    for (int i = 0; i < k_floppy_raw_geometries; i++)
    {
        if (k_floppy_raw_geometry[i].size == size)
            return true;
    }

    return false;
}

bool FloppyImage::IsImageName(const char* name)
{
    if (!IsValidPointer(name))
        return false;

    return ends_with_no_case(name, ".d77") || ends_with_no_case(name, ".d88") || ends_with_no_case(name, ".hdm") ||
        ends_with_no_case(name, ".xdf") || ends_with_no_case(name, ".img");
}

int FloppyImage::GetDiskCount(const u8* data, u32 size)
{
    if (!IsD77(data, size))
        return IsRawSize(size) ? 1 : 0;

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
    if (!IsD77(data, size))
    {
        if (index != 0 || !IsRawSize(size))
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

u8* FloppyImage::Load(const u8* data, u32 size, u32* image_size)
{
    if (!IsValidPointer(data) || size == 0)
        return NULL;

    if (IsD77(data, size))
    {
        u32 disk_size = read_u32_le(data + 0x1C);
        return Build(data, data, disk_size, -1, NULL, 0, image_size);
    }

    if (IsRawSize(size))
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
    memset(raw, 0xE5, geometry.size);
    memset(raw, 0, sector_size);

    raw[0] = 0xEB;
    raw[1] = 0xFE;
    raw[2] = 0x90;
    memcpy(raw + 3, "GEARTOWN", 8);
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

    for (int fat = 0; fat < 2; fat++)
    {
        u8* table = raw + (1 + fat * format.fat_sectors) * sector_size;
        memset(table, 0, format.fat_sectors * sector_size);
        table[0] = format.media_descriptor;
        table[1] = 0xFF;
        table[2] = 0xFF;
    }

    u32 root_offset = (1 + 2 * format.fat_sectors) * sector_size;
    u32 root_size = ((format.root_entries * 32 + sector_size - 1) / sector_size) * sector_size;
    memset(raw + root_offset, 0, root_size);

    u8* image = CreateFromRaw(raw, geometry.size, image_size);
    SafeDeleteArray(raw);
    return image;
}

u8* FloppyImage::ReplaceTrack(const u8* image, u32 size, int track, const u8* sectors, u32 sectors_size,
    u32* image_size)
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
    const FloppyImage_Geometry* geometry = NULL;

    for (int i = 0; i < k_floppy_raw_geometries; i++)
    {
        if (k_floppy_raw_geometry[i].sectors == count && k_floppy_raw_geometry[i].n == n)
        {
            geometry = &k_floppy_raw_geometry[i];
            break;
        }
    }

    if (!IsValidPointer(geometry))
        return 0;

    u32 sector_size = 128U << geometry->n;

    for (int track = 0; track < k_floppy_tracks; track++)
    {
        u32 offset = GetTrackRun(image, size, header_size, track, &run_size);

        if (track >= geometry->cylinders * 2)
        {
            if (offset != 0)
                return 0;

            continue;
        }

        if (offset == 0 || offset == 0xFFFFFFFF || read_u16_le(image + offset + 4) != geometry->sectors)
            return 0;

        u32 found = 0;

        for (int i = 0; i < geometry->sectors; i++)
        {
            const u8* header = image + offset;
            u8 r = header[2];

            if (header[0] != track / 2 || header[1] != (track & 1) || header[3] != geometry->n || r < 1 ||
                r > geometry->sectors || read_u16_le(header + 14) != sector_size || (header[6] & k_floppy_density_fm) != 0)
                return 0;

            found |= 1U << (r - 1);
            offset += k_floppy_sector_header_size + sector_size;
        }

        if (found != (1U << geometry->sectors) - 1)
            return 0;
    }

    return geometry->size;
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

bool FloppyImage::ExtractFromZip(const u8* archive, size_t archive_size, u8** data, u32* size, char* name, size_t name_size)
{
    *data = NULL;
    *size = 0;

    if (IsValidPointer(name) && name_size > 0)
        name[0] = '\0';

    if (!IsValidPointer(archive) || archive_size == 0)
        return false;

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_mem(&zip, archive, archive_size, 0))
        return false;

    int candidate = -1;
    u32 candidate_size = 0;
    mz_uint files = mz_zip_reader_get_num_files(&zip);

    for (mz_uint i = 0; i < files; i++)
    {
        mz_zip_archive_file_stat stat;

        if (!mz_zip_reader_file_stat(&zip, i, &stat) || stat.m_is_directory || !stat.m_is_supported ||
            stat.m_uncomp_size == 0 || stat.m_uncomp_size > k_floppy_max_image_size)
            continue;

        bool image = IsImageName(stat.m_filename) ||
            (ends_with_no_case(stat.m_filename, ".bin") && IsRawSize((u32)stat.m_uncomp_size));

        if (!image)
            continue;

        if (candidate >= 0)
        {
            mz_zip_reader_end(&zip);
            return false;
        }

        candidate = (int)i;
        candidate_size = (u32)stat.m_uncomp_size;

        if (IsValidPointer(name) && name_size > 0)
            strncpy_fit(name, stat.m_filename, name_size);
    }

    if (candidate < 0)
    {
        mz_zip_reader_end(&zip);
        return false;
    }

    u8* extracted = new (std::nothrow) u8[candidate_size];
    bool ok = IsValidPointer(extracted) &&
        mz_zip_reader_extract_to_mem(&zip, (mz_uint)candidate, extracted, candidate_size, 0);
    mz_zip_reader_end(&zip);

    if (!ok)
    {
        SafeDeleteArray(extracted);
        return false;
    }

    *data = extracted;
    *size = candidate_size;
    return true;
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

    if (offset < header_size || offset + k_floppy_sector_header_size > size)
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
    const FloppyImage_Geometry* geometry = NULL;

    for (int i = 0; i < k_floppy_raw_geometries; i++)
    {
        if (k_floppy_raw_geometry[i].size == size)
            geometry = &k_floppy_raw_geometry[i];
    }

    if (!IsValidPointer(geometry))
        return NULL;

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
            memcpy(header + k_floppy_sector_header_size, data + (track * geometry->sectors + r - 1) * sector_size,
                sector_size);
            position += k_floppy_sector_header_size + sector_size;
        }
    }

    *image_size = total;
    return image;
}
