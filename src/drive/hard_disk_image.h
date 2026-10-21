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

#ifndef HARD_DISK_IMAGE_H
#define HARD_DISK_IMAGE_H

#include <string>
#include <vector>
#include "../common/common.h"

class MediaFile;

struct HardDiskJournalEntry
{
    u64 write;
    u32 lba;
    u8 data[512];
};

class HardDiskImage
{
public:
    HardDiskImage();
    ~HardDiskImage();
    bool Open(const char* path, bool write_protected, const char* working_path = NULL);
    void Close();
    bool IsOpen() const;
    const char* GetPath() const;
    const char* GetFileName() const;
    u32 GetBlocks() const;
    bool IsWriteProtected() const;
    bool ReadBlock(u32 lba, u8* buffer);
    bool WriteBlock(u32 lba, const u8* buffer);
    bool Flush();
    u32 GetUnflushedBlocks() const;
    u32 GetWriteCount() const;
    u64 GetSession() const;
    u64 GetLastWrite() const;
    bool Restore(u64 session, u64 write);
    static bool Create(const char* path, u64 size);
    static bool IsImage(const char* path);
    static int GetImageId(const char* path);

private:
    void StoreBlock(u32 lba, const u8* buffer);
    bool CreateWorkingCopy();
    bool Undo(u64 write);
    bool Redo(u64 write);
    void Journal(u32 lba, const u8* data, u64 write);
    void ClearJournal();

private:
    MediaFile* m_file;
    std::string m_path;
    std::string m_file_name;
    std::string m_working_path;
    u32 m_blocks;
    bool m_write_protected;
    std::vector<u32> m_overlay_index;
    std::vector<u8> m_overlay;
    u32 m_overlay_blocks;
    u32 m_write_count;
    bool m_flush_failed;
    u64 m_session;
    u64 m_next_write;
    u64 m_last_write;
    u64 m_journal_base;
    std::vector<HardDiskJournalEntry> m_journal;
    u32 m_journal_start;
    u32 m_journal_count;
    std::vector<HardDiskJournalEntry> m_redo;
};

static const u32 k_hard_disk_block_size = 512;
static const u32 k_hard_disk_journal_entries = (32 * 1024 * 1024) / sizeof(HardDiskJournalEntry);

#endif /* HARD_DISK_IMAGE_H */
