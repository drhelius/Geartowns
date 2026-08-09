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
    struct MediaFileInfo
    {
        char path[512];
        char directory[512];
        char name[512];
        char extension[512];
        u32 crc;
        bool ready;
    };

public:
    Media();
    ~Media();
    void Init();
    void Reset();
    bool LoadBios(const char* directory_path);
    bool LoadBiosFromBuffer(const u8* buffer, int size);
    bool LoadMedia(const char* file_path);
    bool IsReady() const;
    bool IsBiosReady() const;
    void SetTempPath(const char* path);
    const char* GetTempPath() const;
    const MediaFileInfo& GetMediaInfo() const;
    const MediaFileInfo& GetBiosInfo() const;

private:
    bool IsValidFile(const char* path);

private:
    MediaFileInfo m_media_info;
    MediaFileInfo m_bios_info;
    char m_temp_path[512];
    bool m_preload_cdrom;

};

#include "media_inline.h"

#endif /* MEDIA_H */