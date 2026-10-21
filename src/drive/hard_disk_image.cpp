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

#include <time.h>
#include "hard_disk_image.h"
#include "../media/media_file.h"

static u64 s_sessions = 0;

HardDiskImage::HardDiskImage()
{
    InitPointer(m_file);
    m_blocks = 0;
    m_write_protected = false;
    m_overlay_blocks = 0;
    m_write_count = 0;
    m_flush_failed = false;
    m_session = 0;
    m_next_write = 1;
    m_last_write = 0;
    m_journal_base = 0;
    m_journal_start = 0;
    m_journal_count = 0;
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
    m_session = ((u64)time(NULL) << 20) + ++s_sessions;

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
    m_session = 0;
    m_next_write = 1;
    m_last_write = 0;
    m_journal_base = 0;
    ClearJournal();
    std::vector<HardDiskJournalEntry>().swap(m_redo);
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

    u8 old[k_hard_disk_block_size];

    if (!ReadBlock(lba, old))
    {
        ClearJournal();
        m_journal_base = m_next_write;
    }
    else if (memcmp(old, buffer, k_hard_disk_block_size) == 0)
        return true;
    else
        Journal(lba, old, m_next_write);

    m_redo.clear();
    m_last_write = m_next_write++;
    StoreBlock(lba, buffer);
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

u32 HardDiskImage::GetWriteCount() const
{
    return m_write_count;
}

u64 HardDiskImage::GetSession() const
{
    return m_session;
}

u64 HardDiskImage::GetLastWrite() const
{
    return m_last_write;
}

bool HardDiskImage::Restore(u64 session, u64 write)
{
    if (!IsValidPointer(m_file) || (session != m_session))
        return false;

    if (write == m_last_write)
        return true;

    return (write < m_last_write) ? Undo(write) : Redo(write);
}

void HardDiskImage::StoreBlock(u32 lba, const u8* buffer)
{
    if (m_overlay_index.empty())
        m_overlay_index.assign(m_blocks, 0);

    if (m_overlay_index[lba] == 0)
    {
        m_overlay.resize(m_overlay.size() + k_hard_disk_block_size);
        m_overlay_index[lba] = ++m_overlay_blocks;
    }

    memcpy(&m_overlay[(size_t)(m_overlay_index[lba] - 1) * k_hard_disk_block_size], buffer, k_hard_disk_block_size);
    m_write_count++;
}

bool HardDiskImage::Undo(u64 write)
{
    u32 newer = 0;

    while ((newer < m_journal_count) &&
        (m_journal[(m_journal_start + m_journal_count - 1 - newer) % k_hard_disk_journal_entries].write > write))
        newer++;

    if (newer == m_journal_count ? write != m_journal_base :
        m_journal[(m_journal_start + m_journal_count - 1 - newer) % k_hard_disk_journal_entries].write != write)
        return false;

    HardDiskJournalEntry redo;
    bool keep = true;

    for (u32 i = 0; i < newer; i++)
    {
        m_journal_count--;
        const HardDiskJournalEntry& entry = m_journal[(m_journal_start + m_journal_count) % k_hard_disk_journal_entries];
        redo.write = entry.write;
        redo.lba = entry.lba;
        keep = keep && ReadBlock(entry.lba, redo.data);

        if (keep)
            m_redo.push_back(redo);
        else
            m_redo.clear();

        StoreBlock(entry.lba, entry.data);
    }

    m_last_write = write;
    return true;
}

bool HardDiskImage::Redo(u64 write)
{
    size_t count = m_redo.size();
    size_t older = 0;

    while ((older < count) && (m_redo[count - 1 - older].write < write))
        older++;

    if ((older == count) || (m_redo[count - 1 - older].write != write))
        return false;

    u8 current[k_hard_disk_block_size];

    for (size_t i = 0; i <= older; i++)
    {
        HardDiskJournalEntry entry = m_redo.back();
        m_redo.pop_back();

        if (ReadBlock(entry.lba, current))
            Journal(entry.lba, current, entry.write);
        else
        {
            ClearJournal();
            m_journal_base = entry.write;
        }

        StoreBlock(entry.lba, entry.data);
    }

    m_last_write = write;
    return true;
}

void HardDiskImage::Journal(u32 lba, const u8* data, u64 write)
{
    if (m_journal_count == k_hard_disk_journal_entries)
    {
        m_journal_base = m_journal[m_journal_start].write;
        m_journal_start = (m_journal_start + 1) % k_hard_disk_journal_entries;
        m_journal_count--;
    }

    u32 index = (m_journal_start + m_journal_count) % k_hard_disk_journal_entries;

    if (index == m_journal.size())
        m_journal.push_back(HardDiskJournalEntry());

    HardDiskJournalEntry& entry = m_journal[index];
    entry.write = write;
    entry.lba = lba;
    memcpy(entry.data, data, k_hard_disk_block_size);
    m_journal_count++;
}

void HardDiskImage::ClearJournal()
{
    std::vector<HardDiskJournalEntry>().swap(m_journal);
    m_journal_start = 0;
    m_journal_count = 0;
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
