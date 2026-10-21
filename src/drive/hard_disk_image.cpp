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

#include "hard_disk_image.h"
#include "../media/media_file.h"

HardDiskImage::HardDiskImage()
{
    InitPointer(m_file);
    m_blocks = 0;
    m_write_protected = false;
    m_overlay_blocks = 0;
    m_flush_failed = false;
}

HardDiskImage::~HardDiskImage()
{
    Close();
}

bool HardDiskImage::Open(const char* path, bool write_protected)
{
    Close();

    if (!IsValidPointer(path) || (path[0] == 0))
        return false;

    m_file = write_protected ? NULL : MediaFile::OpenFile(path, true);

    if (!IsValidPointer(m_file))
    {
        m_file = MediaFile::OpenFile(path, false);
        write_protected = true;
    }

    if (!IsValidPointer(m_file))
    {
        Error("Unable to open hard disk image %s", path);
        return false;
    }

    s64 size = m_file->GetSize();

    if ((size < (s64)k_hard_disk_block_size) || !m_file->CanSeek())
    {
        Error("Invalid hard disk image %s", path);
        SafeDelete(m_file);
        return false;
    }

    if ((size % k_hard_disk_block_size) != 0)
        Log("Hard disk image %s is not a whole number of 512 byte sectors, the last %d bytes are ignored", path,
            (int)(size % k_hard_disk_block_size));

    u64 blocks = (u64)size / k_hard_disk_block_size;
    m_blocks = (u32)MIN(blocks, (u64)0xFFFFFFFF);
    m_write_protected = write_protected;
    m_path = path;

    size_t separator = m_path.find_last_of("/\\");
    m_file_name = separator == std::string::npos ? m_path : m_path.substr(separator + 1);

    Log("Hard disk image %s: %u sectors%s", path, m_blocks, m_write_protected ? ", write protected" : "");
    return true;
}

void HardDiskImage::Close()
{
    if (IsValidPointer(m_file) && (m_overlay_blocks != 0))
        Flush();

    SafeDelete(m_file);
    m_path.clear();
    m_file_name.clear();
    m_blocks = 0;
    m_write_protected = false;
    m_overlay_index.clear();
    m_overlay.clear();
    m_overlay_blocks = 0;
    m_flush_failed = false;
}

bool HardDiskImage::IsOpen() const
{
    return IsValidPointer(m_file);
}

const char* HardDiskImage::GetPath() const
{
    return m_path.c_str();
}

const char* HardDiskImage::GetFileName() const
{
    return m_file_name.c_str();
}

u32 HardDiskImage::GetBlocks() const
{
    return m_blocks;
}

bool HardDiskImage::IsWriteProtected() const
{
    return m_write_protected;
}

bool HardDiskImage::ReadBlock(u32 lba, u8* buffer)
{
    if (!IsValidPointer(m_file) || (lba >= m_blocks))
        return false;

    if (!m_overlay_index.empty() && (m_overlay_index[lba] != 0))
    {
        memcpy(buffer, &m_overlay[(size_t)(m_overlay_index[lba] - 1) * k_hard_disk_block_size],
            k_hard_disk_block_size);
        return true;
    }

    return m_file->ReadAt((u64)lba * k_hard_disk_block_size, buffer, k_hard_disk_block_size);
}

bool HardDiskImage::WriteBlock(u32 lba, const u8* buffer)
{
    if (!IsValidPointer(m_file) || m_write_protected || (lba >= m_blocks))
        return false;

    if (m_overlay_index.empty())
        m_overlay_index.assign(m_blocks, 0);

    if (m_overlay_index[lba] == 0)
    {
        m_overlay.resize(m_overlay.size() + k_hard_disk_block_size);
        m_overlay_index[lba] = ++m_overlay_blocks;
    }

    memcpy(&m_overlay[(size_t)(m_overlay_index[lba] - 1) * k_hard_disk_block_size], buffer, k_hard_disk_block_size);
    return true;
}

bool HardDiskImage::Flush()
{
    if (!IsValidPointer(m_file) || (m_overlay_blocks == 0))
        return true;

    u32 lba = 0;

    for (; lba < m_blocks; lba++)
    {
        u32 slot = m_overlay_index[lba];

        if (slot == 0)
            continue;

        if (!m_file->WriteAt((u64)lba * k_hard_disk_block_size, &m_overlay[(size_t)(slot - 1) * k_hard_disk_block_size],
            k_hard_disk_block_size))
            break;
    }

    if (!m_file->Flush() || (lba < m_blocks))
    {
        if (!m_flush_failed)
            Error("Unable to write hard disk image %s, its changes stay in memory", m_path.c_str());

        m_flush_failed = true;
        return false;
    }

    m_overlay_index.clear();
    m_overlay.clear();
    m_overlay_blocks = 0;
    m_flush_failed = false;
    return true;
}

u32 HardDiskImage::GetUnflushedBlocks() const
{
    return m_overlay_blocks;
}

bool HardDiskImage::Create(const char* path, u64 size)
{
    if (!IsValidPointer(path) || (size < k_hard_disk_block_size) || (size > 0x7FFFFFFF))
        return false;

    FILE* file = fopen_utf8(path, "wb");

    if (!IsValidPointer(file))
    {
        Error("Unable to create hard disk image %s", path);
        return false;
    }

    u8 zero = 0;
    bool ok = (fseek(file, (long)(size - 1), SEEK_SET) == 0) && (fwrite(&zero, 1, 1, file) == 1);
    ok = (fclose(file) == 0) && ok;

    if (!ok)
        Error("Unable to create hard disk image %s", path);

    return ok;
}
