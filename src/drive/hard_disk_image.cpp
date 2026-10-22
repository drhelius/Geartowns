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
#include <libchdr/chd.h>
#include "hard_disk_image.h"
#include "floppy_image.h"
#include "../media/media_file.h"
#include "../media/chd_file_adapter.h"

static u64 s_sessions = 0;
static const u32 k_no_hunk = 0xFFFFFFFF;

static chd_file* OpenHardDiskChd(const char* path, ChdFileAdapter* adapter, u32* blocks, chd_error* error)
{
    chd_file* chd = NULL;
    *error = adapter->Open(path) ? chd_open_core_file(adapter->GetCoreFile(), CHD_OPEN_READ, NULL, &chd) :
        CHDERR_FILE_NOT_FOUND;

    if (*error != CHDERR_NONE)
        return NULL;

    const chd_header* header = chd_get_header(chd);
    char metadata[256] = { };
    int cylinders = 0;
    int heads = 0;
    int sectors = 0;
    int bytes = 0;
    *error = chd_get_metadata(chd, HARD_DISK_METADATA_TAG, 0, metadata, sizeof(metadata) - 1, NULL, NULL, NULL);

    if ((*error == CHDERR_NONE) && (sscanf(metadata, HARD_DISK_METADATA_FORMAT, &cylinders, &heads, &sectors, &bytes) == 4) &&
        (bytes == (int)k_hard_disk_block_size) && (cylinders > 0) && (heads > 0) && (sectors > 0) &&
        (header->hunkbytes != 0) && ((header->hunkbytes % k_hard_disk_block_size) == 0))
    {
        u64 total = MIN((u64)cylinders * heads * sectors, header->logicalbytes / k_hard_disk_block_size);

        if ((total != 0) && (total <= 0xFFFFFFFF))
        {
            *blocks = (u32)total;
            return chd;
        }
    }

    *error = CHDERR_INVALID_DATA;
    chd_close(chd);
    return NULL;
}

HardDiskImage::HardDiskImage()
{
    InitPointer(m_file);
    InitPointer(m_chd_adapter);
    InitPointer(m_chd);
    m_hunk_index = k_no_hunk;
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

bool HardDiskImage::Open(const char* path, bool write_protected, const char* working_path)
{
    Close();

    if (!IsValidPointer(path) || (path[0] == 0))
        return false;

    bool copy_on_write = IsValidPointer(working_path) && (working_path[0] != 0);

    if (copy_on_write)
    {
        m_file = MediaFile::OpenFile(working_path, !write_protected);

        if (IsValidPointer(m_file))
            Log("Hard disk working copy %s", working_path);
        else if (!write_protected)
            m_working_path = working_path;
    }

    if (!IsValidPointer(m_file) && ends_with_no_case(path, ".chd"))
    {
        if (!OpenChd(path))
            return false;

        write_protected = write_protected || m_working_path.empty();
    }
    else
    {
        if (!IsValidPointer(m_file))
            m_file = (write_protected || copy_on_write) ? NULL : MediaFile::OpenFile(path, true);

        if (!IsValidPointer(m_file))
        {
            m_file = MediaFile::OpenFile(path, false);
            write_protected = write_protected || !copy_on_write;
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
    }

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
    if (IsOpen() && (m_overlay_blocks != 0))
        Flush();

    SafeDelete(m_file);
    CloseChd();
    m_path.clear();
    m_file_name.clear();
    m_working_path.clear();
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
    return IsValidPointer(m_file) || IsValidPointer(m_chd);
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
    if (!IsOpen() || (lba >= m_blocks))
        return false;

    if (!m_overlay_index.empty() && (m_overlay_index[lba] != 0))
    {
        memcpy(buffer, &m_overlay[(size_t)(m_overlay_index[lba] - 1) * k_hard_disk_block_size],
            k_hard_disk_block_size);
        return true;
    }

    return ReadImage(lba, buffer, 1);
}

bool HardDiskImage::WriteBlock(u32 lba, const u8* buffer)
{
    if (!IsOpen() || m_write_protected || (lba >= m_blocks))
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
    if (!IsOpen() || (m_overlay_blocks == 0))
        return true;

    bool copied = m_working_path.empty() ? IsValidPointer(m_file) : CreateWorkingCopy();
    u32 lba = 0;

    for (; copied && (lba < m_blocks); lba++)
    {
        u32 slot = m_overlay_index[lba];

        if (slot == 0)
            continue;

        if (!m_file->WriteAt((u64)lba * k_hard_disk_block_size, &m_overlay[(size_t)(slot - 1) * k_hard_disk_block_size],
            k_hard_disk_block_size))
            break;
    }

    if (!copied || !m_file->Flush() || (lba < m_blocks))
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
    if (!IsOpen() || (session != m_session))
        return false;

    if (write == m_last_write)
        return true;

    return (write < m_last_write) ? Undo(write) : Redo(write);
}

bool HardDiskImage::CreateWorkingCopy()
{
    MediaFile* copy = MediaFile::OpenNewFile(m_working_path.c_str());

    if (!IsValidPointer(copy))
        return false;

    std::vector<u8> chunk(1024 * 1024);
    u32 chunk_blocks = (u32)chunk.size() / k_hard_disk_block_size;
    bool ok = true;

    for (u64 lba = 0; ok && (lba < m_blocks); lba += chunk_blocks)
    {
        u32 count = (u32)MIN((u64)chunk_blocks, m_blocks - lba);
        ok = ReadImage((u32)lba, chunk.data(), count) &&
            copy->WriteAt(lba * k_hard_disk_block_size, chunk.data(), (u64)count * k_hard_disk_block_size);
    }

    ok = copy->Flush() && ok;
    SafeDelete(copy);
    MediaFile* file = ok ? MediaFile::OpenFile(m_working_path.c_str(), true) : NULL;

    if (!IsValidPointer(file))
        return false;

    SafeDelete(m_file);
    CloseChd();
    m_file = file;
    Log("Hard disk working copy created: %s", m_working_path.c_str());
    m_working_path.clear();
    return true;
}

bool HardDiskImage::OpenChd(const char* path)
{
    chd_error error = CHDERR_NONE;
    m_chd_adapter = new ChdFileAdapter;
    m_chd = OpenHardDiskChd(path, m_chd_adapter, &m_blocks, &error);

    if (!IsValidPointer(m_chd))
    {
        Error("Unable to open hard disk CHD %s: %s", path, chd_error_string(error));
        SafeDelete(m_chd_adapter);
        m_blocks = 0;
        return false;
    }

    m_hunk.resize(chd_get_header(m_chd)->hunkbytes);
    m_hunk_index = k_no_hunk;
    return true;
}

void HardDiskImage::CloseChd()
{
    if (IsValidPointer(m_chd))
        chd_close(m_chd);

    InitPointer(m_chd);
    SafeDelete(m_chd_adapter);
    std::vector<u8>().swap(m_hunk);
    m_hunk_index = k_no_hunk;
}

bool HardDiskImage::ReadImage(u32 lba, u8* buffer, u32 count)
{
    if (IsValidPointer(m_file))
        return m_file->ReadAt((u64)lba * k_hard_disk_block_size, buffer, (u64)count * k_hard_disk_block_size);

    u32 hunk_blocks = (u32)m_hunk.size() / k_hard_disk_block_size;

    for (u32 i = 0; i < count; i++)
    {
        u32 hunk = (lba + i) / hunk_blocks;

        if (hunk != m_hunk_index)
        {
            m_hunk_index = (chd_read(m_chd, hunk, m_hunk.data()) == CHDERR_NONE) ? hunk : k_no_hunk;

            if (m_hunk_index == k_no_hunk)
                return false;
        }

        memcpy(buffer + ((size_t)i * k_hard_disk_block_size),
            &m_hunk[(size_t)((lba + i) % hunk_blocks) * k_hard_disk_block_size], k_hard_disk_block_size);
    }

    return true;
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

bool HardDiskImage::IsImage(const char* path)
{
    if (!IsValidPointer(path))
        return false;

    if (ends_with_no_case(path, ".hdd") || (GetImageId(path) >= 0))
        return true;

    if (ends_with_no_case(path, ".chd"))
        return GetChdHash(path, NULL);

    if (!ends_with_no_case(path, ".hdm"))
        return false;

    MediaFile* file = MediaFile::OpenFile(path);
    bool hard_disk = IsValidPointer(file) && (file->GetSize() > (s64)k_floppy_max_image_size);
    SafeDelete(file);
    return hard_disk;
}

int HardDiskImage::GetImageId(const char* path)
{
    size_t length = IsValidPointer(path) ? strlen(path) : 0;

    if ((length > 3) && (path[length - 3] == '.') && ((path[length - 2] | 0x20) == 'h') &&
        (path[length - 1] >= '0') && (path[length - 1] < '0' + GT_MAX_HARD_DISKS))
        return path[length - 1] - '0';

    return -1;
}

bool HardDiskImage::GetChdHash(const char* path, u32* hash)
{
    ChdFileAdapter adapter;
    chd_error error = CHDERR_NONE;
    u32 blocks = 0;
    chd_file* chd = IsValidPointer(path) ? OpenHardDiskChd(path, &adapter, &blocks, &error) : NULL;

    if (!IsValidPointer(chd))
        return false;

    if (IsValidPointer(hash))
    {
        const u8* sha1 = chd_get_header(chd)->sha1;
        *hash = ((u32)sha1[0] << 24) | ((u32)sha1[1] << 16) | ((u32)sha1[2] << 8) | sha1[3];
    }

    chd_close(chd);
    return true;
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
