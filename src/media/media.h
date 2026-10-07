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

#include <vector>
#include "../common/common.h"

class CdRomMedia;

class Media
{
public:
    struct MediaFileInfo
    {
        char path[GT_MAX_PATH];
        char directory[GT_MAX_PATH];
        char name[GT_MAX_PATH];
        char extension[64];
        int size;
        u32 crc;
        bool ready;
        bool cdrom;
        bool physical_cdrom;
        char physical_cdrom_device_id[256];
    };

public:
    Media(CdRomMedia* cdrom_media);
    ~Media();
    void Init();
    void Reset();
    static bool ParsePlaylist(const char* playlist_path, const char* text, size_t size, std::vector<std::string>& entries);
    bool LoadMedia(const char* file_path);
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    bool LoadPhysicalCdRom(const char* device_id);
#endif
    bool IsReady() const;
    bool IsCDROM() const;
    bool IsPhysicalCdRom() const;
    const char* GetPhysicalCdRomDeviceId() const;
    bool HasPhysicalCdRomError();
    void PreloadCdRom(bool enable);
    bool IsPreloadCdRomEnabled() const;
    void SetTempPath(const char* path);
    const char* GetTempPath() const;
    const char* GetFilePath() const;
    const char* GetFileDirectory() const;
    const char* GetFileName() const;
    const char* GetFileExtension() const;
    const u8* GetData() const;
    int GetSize() const;
    u32 GetCRC() const;
    const MediaFileInfo& GetMediaInfo() const;

private:
    bool LoadFile(const char* file_path);
    bool LoadCdRomFromZipFile(const char* file_path);
    void ResetMediaInfo();
    void GatherDataFromPath(const char* path);

private:
    CdRomMedia* m_cdrom_media;
    MediaFileInfo m_media_info;
    char m_temp_path[GT_MAX_PATH];
    u8* m_media_data;
    bool m_preload_cdrom;
};

#include "media_inline.h"

#endif /* MEDIA_H */
