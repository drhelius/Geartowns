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

#ifndef MEDIA_H
#define MEDIA_H

#include "common.h"

class Media
{
public:
    Media();
    ~Media();
    void Init();
    void Reset();
    bool LoadBios(const char* directory_path);
    bool LoadBiosFromBuffer(const u8* buffer, int size);
    void UnloadBios();
    bool LoadMedia(const char* file_path);
    bool LoadFloppy(int drive, const char* file_path, bool write_protect);
    bool LoadCdRom(const char* file_path);
    bool LoadHardDisk(int scsi_id, const char* file_path);
    void EjectFloppy(int drive);
    void EjectCdRom();
    bool IsReady() const;
    bool IsBiosReady() const;
    bool IsCdRomReady() const;
    const char* GetFilePath() const;
    const char* GetFileDirectory() const;
    const char* GetFileName() const;
    const char* GetFileExtension() const;
    void SetTempPath(const char* path);

private:
    bool LoadMediaFromZipFile(const char* path);
    void GatherDataFromPath(const char* path);
    bool IsValidFile(const char* path);

private:
    bool m_ready;
    bool m_bios_ready;
    bool m_cdrom_ready;
    char m_file_path[512];
    char m_file_directory[512];
    char m_file_name[512];
    char m_file_extension[512];
    char m_temp_path[512];
};

#include "media_inline.h"

#endif /* MEDIA_H */