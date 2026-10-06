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
#include "floppy_disk.h"
#include "floppy_image.h"
#include "../media/crc.h"
#include "../common/state_serializer.h"

FloppyDisk::FloppyDisk()
{
    InitPointer(m_image);
    memset(&m_state, 0, sizeof(m_state));
    m_state.rpm = 300;
}

FloppyDisk::~FloppyDisk()
{
    SafeDeleteArray(m_image);
}

bool FloppyDisk::Insert(const u8* data, u32 size, bool write_protected, u32 base_crc)
{
    u32 image_size = 0;
    u8* image = FloppyImage::Load(data, size, &image_size);

    if (!IsValidPointer(image))
        return false;

    SafeDeleteArray(m_image);
    m_image = image;
    m_state.inserted = true;
    m_state.write_protected = write_protected || (image[0x1A] & 0x10) != 0;
    m_state.dirty = false;
    m_state.base_crc = base_crc != 0 ? base_crc : CalculateCRC32(0, data, (int)size);
    m_state.size = image_size;
    Identify();
    return true;
}

void FloppyDisk::Eject()
{
    SafeDeleteArray(m_image);
    memset(&m_state, 0, sizeof(m_state));
    m_state.rpm = 300;
}

void FloppyDisk::Swap(FloppyDisk& other)
{
    FloppyDisk_State state = m_state;
    u8* image = m_image;
    m_state = other.m_state;
    m_image = other.m_image;
    other.m_state = state;
    other.m_image = image;
}

int FloppyDisk::GetSectors(int track, FloppyDisk_Sector* sectors) const
{
    if (!m_state.inserted || track < 0 || track >= k_floppy_tracks)
        return 0;

    u32 offset = read_u32_le(m_image + 0x20 + track * 4);

    if (offset == 0)
        return 0;

    int count = read_u16_le(m_image + offset + 4);

    for (int i = 0; i < count; i++)
    {
        const u8* header = m_image + offset;
        FloppyDisk_Sector& sector = sectors[i];
        sector.header = offset;
        memcpy(sector.id, header, 4);
        sector.size = read_u16_le(header + 14);
        sector.fm = (header[6] & k_floppy_density_fm) != 0;
        sector.deleted = (header[7] & k_floppy_deleted) != 0;
        sector.status = header[8];
        offset += k_floppy_sector_header_size + sector.size;
    }

    return count;
}

void FloppyDisk::SetSectorStatus(u32 header, bool deleted, u8 status)
{
    if (header + k_floppy_sector_header_size > m_state.size)
        return;

    m_image[header + 7] = deleted ? k_floppy_deleted : 0x00;
    m_image[header + 8] = status;
    m_state.dirty = true;
}

bool FloppyDisk::ReplaceTrack(int track, const u8* sectors, u32 size, u32 rpm)
{
    u32 image_size = 0;
    u8* image = FloppyImage::ReplaceTrack(m_image, m_state.size, track, sectors, size, &image_size);

    if (!IsValidPointer(image))
        return false;

    SafeDeleteArray(m_image);
    m_image = image;
    m_state.size = image_size;
    m_state.dirty = true;

    if (m_state.media == FLOPPY_MEDIA_2HD)
        m_state.rpm = (u16)rpm;

    return true;
}

void FloppyDisk::Identify()
{
    u32 largest = 0;
    u32 longest = 0;
    int last_track = -1;
    FloppyDisk_Sector sectors[k_floppy_max_sectors];

    for (int track = 0; track < k_floppy_tracks; track++)
    {
        int count = GetSectors(track, sectors);
        u32 bytes = 0;
        u32 cells = 146;

        for (int i = 0; i < count; i++)
        {
            bytes += 128U << (sectors[i].id[3] & 3);
            cells += 62 + (128U << (sectors[i].id[3] & 3));
        }

        if (count > 0)
            last_track = track;

        largest = MAX(largest, bytes);
        longest = MAX(longest, cells);
    }

    u8 media = m_image[0x1B];

    if (largest > 4608 || (last_track < 0 && media == k_floppy_media_2hd))
    {
        m_state.media = FLOPPY_MEDIA_2HD;
        m_state.rpm = longest > 10416 ? 300 : 360;
    }
    else
    {
        bool narrow = last_track < 0 ? media == k_floppy_media_2d : (media == k_floppy_media_2d && last_track < 84);
        m_state.media = narrow ? FLOPPY_MEDIA_2D : FLOPPY_MEDIA_2DD;
        m_state.rpm = 300;
    }
}

void FloppyDisk::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void FloppyDisk::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void FloppyDisk::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.inserted);
    G_SERIALIZE(serializer, m_state.write_protected);
    G_SERIALIZE(serializer, m_state.dirty);
    G_SERIALIZE(serializer, m_state.media);
    G_SERIALIZE(serializer, m_state.rpm);
    G_SERIALIZE(serializer, m_state.base_crc);
    G_SERIALIZE(serializer, m_state.size);

    if (serializer.IsLoading())
    {
        SafeDeleteArray(m_image);
        m_state.size = MIN(m_state.size, k_floppy_max_image_size);

        if (m_state.size > 0)
            m_image = new (std::nothrow) u8[m_state.size];

        if (!IsValidPointer(m_image))
            m_state.size = 0;
    }

    if (m_state.size > 0)
        G_SERIALIZE_ARRAY(serializer, m_image, m_state.size);
}

void FloppyDisk::SanitizeState()
{
    if (!m_state.inserted || !FloppyImage::IsCanonical(m_image, m_state.size))
    {
        Eject();
        return;
    }

    m_state.media = (u8)MIN((int)m_state.media, (int)FLOPPY_MEDIA_2HD);
    m_state.rpm = m_state.rpm == 360 ? 360 : 300;
}
