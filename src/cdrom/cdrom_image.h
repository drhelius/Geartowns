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

#ifndef CDROM_IMAGE_H
#define CDROM_IMAGE_H

#include "../common/common.h"
#include "cdrom_common.h"
#include <vector>

class CdRomImage
{
public:

    struct Track
    {
        GT_CdRomTrackType type;
        u32 sector_size;
        u32 sector_count;
        u32 start_lba;
        GT_CdRomMSF start_msf;
        u32 end_lba;
        GT_CdRomMSF end_msf;
        bool has_lead_in;
        u32 lead_in_lba;
        u64 file_offset;
        u8 control_flags;
    };

    struct TableOfContents
    {
        std::vector<Track> tracks;
        GT_CdRomMSF total_length;
        u32 sector_count;
    };

public:
    CdRomImage();
    virtual ~CdRomImage();
    virtual void Init();
    virtual void Reset();
    virtual bool LoadFromFile(const char* path, bool preload) = 0;
    virtual bool ReadSector(u32 lba, u8* buffer) = 0;
    virtual bool ReadRawSector2352(u32 lba, u8* buffer);
    virtual bool ReadSamples(u32 lba, u32 offset, s16* buffer, u32 count) = 0;
    virtual bool PreloadDisc() = 0;
    virtual bool PreloadTrack(u32 track_number) = 0;
    bool IsReady();
    s32 GetTrackFromLBA(u32 lba);
    const char* GetFilePath();
    const char* GetFileDirectory();
    const char* GetFileName();
    const char* GetFileExtension();
    TableOfContents* GetTOC();
    u32 GetCRC();
    u32 GetCurrentSector();
    void SetCurrentSector(u32 sector);
    bool IsAudioSector(u32 lba, bool include_lead_in = false);
    s32 FindTrackFromLBA(u32 lba, bool include_lead_in = false);

protected:
    void GatherPaths(const char* path);
    void InitTrack(Track& track);

protected:
    TableOfContents m_toc;
    bool m_ready;
    char m_file_path[512];
    char m_file_directory[512];
    char m_file_name[512];
    char m_file_extension[512];
    u32 m_current_sector;
    u32 m_crc;
};

#endif /* CDROM_IMAGE_H */
