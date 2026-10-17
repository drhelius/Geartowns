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
    InitPointer(m_weak);
    m_write_count = 0;
    m_weak_count = 0;
    memset(&m_state, 0, sizeof(m_state));
    m_state.rpm = 300;
}

FloppyDisk::~FloppyDisk()
{
    SafeDeleteArray(m_image);
    SafeDeleteArray(m_weak);
}

bool FloppyDisk::Insert(const u8* data, u32 size, bool write_protected, u32 base_crc, u32 data_crc, const u8* source,
    u32 source_size)
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
    m_state.data_crc = data_crc != 0 ? data_crc : FloppyImage::GetDataCRC(image, image_size);
    ClearWeakBytes();

    // A working copy takes its weak bytes from the source image it was made from
    if (IsValidPointer(source))
        m_weak = FloppyImage::GetWeakBytes(source, source_size, image, image_size, &m_weak_count);
    else
        m_weak = FloppyImage::GetWeakBytes(data, size, image, image_size, &m_weak_count);

    Identify();
    return true;
}

void FloppyDisk::Eject()
{
    SafeDeleteArray(m_image);
    ClearWeakBytes();
    memset(&m_state, 0, sizeof(m_state));
    m_state.rpm = 300;
}

void FloppyDisk::Swap(FloppyDisk& other)
{
    FloppyDisk_State state = m_state;
    u8* image = m_image;
    u32 write_count = m_write_count;
    FloppyImage_Weak* weak = m_weak;
    u32 weak_count = m_weak_count;
    m_state = other.m_state;
    m_image = other.m_image;
    m_write_count = other.m_write_count;
    m_weak = other.m_weak;
    m_weak_count = other.m_weak_count;
    other.m_state = state;
    other.m_image = image;
    other.m_write_count = write_count;
    other.m_weak = weak;
    other.m_weak_count = weak_count;
}

u8 FloppyDisk::ReadWeakByte(u32 offset, u8 fill) const
{
    u32 index = FindWeakByte(offset);
    return index < m_weak_count && m_weak[index].offset == offset ? m_weak[index].value : ReadByte(offset, fill);
}

bool FloppyDisk::HasWeakBytes(u32 offset, u32 size) const
{
    u32 index = FindWeakByte(offset);
    return index < m_weak_count && m_weak[index].offset < offset + size;
}

u32 FloppyDisk::FindWeakByte(u32 offset) const
{
    u32 low = 0;
    u32 high = m_weak_count;

    while (low < high)
    {
        u32 middle = (low + high) / 2;

        if (m_weak[middle].offset < offset)
            low = middle + 1;
        else
            high = middle;
    }

    return low;
}

void FloppyDisk::RemoveWeakByte(u32 offset)
{
    u32 index = FindWeakByte(offset);

    if (index >= m_weak_count || m_weak[index].offset != offset)
        return;

    memmove(m_weak + index, m_weak + index + 1, (m_weak_count - index - 1) * sizeof(FloppyImage_Weak));
    m_weak_count--;
}

void FloppyDisk::ClearWeakBytes()
{
    SafeDeleteArray(m_weak);
    m_weak_count = 0;
}

int FloppyDisk::GetSectors(int track, FloppyDisk_Sector* sectors) const
{
    if (!m_state.inserted || track < 0 || track >= k_floppy_tracks)
        return 0;

    u32 offset = read_u32_le(m_image + 0x20 + track * 4);

    if (offset == 0)
        return 0;

    int count = MIN((int)read_u16_le(m_image + offset + 4), k_floppy_max_sectors);

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
    m_write_count++;
}

bool FloppyDisk::ReplaceTrack(int track, const u8* sectors, u32 size, u32 rpm)
{
    FloppyDisk_Sector old[k_floppy_max_sectors];
    int count = GetSectors(track, old);
    u32 start = count > 0 ? old[0].header : m_state.size;
    u32 end = count > 0 ? old[count - 1].header + k_floppy_sector_header_size + old[count - 1].size : m_state.size;

    for (int next = track + 1; count == 0 && next < k_floppy_tracks && start == m_state.size; next++)
    {
        u32 offset = read_u32_le(m_image + 0x20 + next * 4);

        if (offset != 0)
            start = end = offset;
    }

    u32 image_size = 0;
    u8* image = FloppyImage::ReplaceTrack(m_image, m_state.size, track, sectors, size, &image_size);

    if (!IsValidPointer(image))
        return false;

    SafeDeleteArray(m_image);
    m_image = image;
    m_state.size = image_size;
    m_state.dirty = true;
    m_write_count++;

    // The new track loses its weak bytes and the runs after it move by the change in its size
    u32 kept = 0;

    for (u32 i = 0; i < m_weak_count; i++)
    {
        if (m_weak[i].offset >= start && m_weak[i].offset < end)
            continue;

        m_weak[kept].offset = m_weak[i].offset >= end ? m_weak[i].offset - (end - start) + size : m_weak[i].offset;
        m_weak[kept].value = m_weak[i].value;
        kept++;
    }

    m_weak_count = kept;

    if (m_state.media == FLOPPY_MEDIA_2HD)
        m_state.rpm = (u16)rpm;

    return true;
}

void FloppyDisk::Identify()
{
    u32 largest = 0;
    int formatted = 0;
    int overflowing = 0;
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
        {
            last_track = track;
            formatted++;

            if (cells > 10416)
                overflowing++;
        }

        largest = MAX(largest, bytes);
    }

    u8 media = m_image[0x1B];

    if (largest > 4608 || media == k_floppy_media_2hd)
    {
        m_state.media = FLOPPY_MEDIA_2HD;
        m_state.rpm = overflowing * 2 > formatted ? 300 : 360;
    }
    else
    {
        bool narrow = last_track < 0 ? media == k_floppy_media_2d : (media == k_floppy_media_2d && last_track < 84);
        m_state.media = narrow ? FLOPPY_MEDIA_2D : FLOPPY_MEDIA_2DD;
        m_state.rpm = 300;
    }
}

void FloppyDisk::SaveState(std::ostream& stream, bool media)
{
    StateSerializer serializer(stream);
    Serialize(serializer, media, GT_SAVESTATE_VERSION);
}

void FloppyDisk::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    bool media = false;
    Serialize(serializer, media, version);

    if (media)
        SanitizeState();
}

// A state without media keeps the disk in the drive and its writes
void FloppyDisk::Serialize(StateSerializer& serializer, bool& media, int version)
{
    G_SERIALIZE(serializer, media);

    if (!media)
        return;

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

    if (version < 7)
    {
        if (serializer.IsLoading())
            ClearWeakBytes();

        return;
    }

    u32 weak_count = m_weak_count;
    G_SERIALIZE(serializer, weak_count);

    if (serializer.IsLoading())
    {
        ClearWeakBytes();
        weak_count = MIN(weak_count, k_floppy_max_weak_bytes);

        if (weak_count > 0)
            m_weak = new (std::nothrow) FloppyImage_Weak[weak_count];

        m_weak_count = IsValidPointer(m_weak) ? weak_count : 0;
    }

    for (u32 i = 0; i < weak_count; i++)
    {
        u32 offset = i < m_weak_count ? m_weak[i].offset : 0;
        u8 value = i < m_weak_count ? m_weak[i].value : 0;
        G_SERIALIZE(serializer, offset);
        G_SERIALIZE(serializer, value);

        if (i < m_weak_count)
        {
            m_weak[i].offset = offset;
            m_weak[i].value = value;
        }
    }
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
    u32 kept = 0;

    for (u32 i = 0; i < m_weak_count; i++)
    {
        if (m_weak[i].offset < m_state.size && (kept == 0 || m_weak[i].offset > m_weak[kept - 1].offset))
            m_weak[kept++] = m_weak[i];
    }

    m_weak_count = kept;
}
