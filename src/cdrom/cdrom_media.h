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

#ifndef CDROM_MEDIA_H
#define CDROM_MEDIA_H

#include <vector>
#include <string>
#include "../common/common.h"
#include "cdrom_common.h"
#include "cdrom_image.h"

class CdRomCueBinImage;
class CdRomChdImage;
#if defined(GT_ENABLE_PHYSICAL_CDROM)
class CdRomPhysicalImage;
#endif

class CdRomMedia
{
public:
    CdRomMedia();
    ~CdRomMedia();
    void Init();
    void Reset();
    bool IsReady();
    u32 GetCRC();
    u32 GetVolumeKey();
    const char* GetFilePath();
    const char* GetFileDirectory();
    const char* GetFileName();
    const char* GetFileExtension();
    const std::vector<CdRomImage::Track>& GetTracks();
    GT_CdRomTrackType GetTrackType(u8 track_number);
    u8 GetTrackCount();
    GT_CdRomMSF GetCdRomLength();
    u32 GetSectorCount();
    u32 GetCurrentSector();
    u32 GetMediaGeneration();
    bool IsAudioSector(u32 lba);
    bool IsPlayableAudioSector(u32 lba);
    void SetCurrentSector(u32 sector);
    bool LoadCueFromFile(const char* path, bool preload);
    bool LoadChdFromFile(const char* path, bool preload);
    bool LoadIsoFromFile(const char* path, bool preload);
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    bool LoadPhysicalDrive(const char* device_id, bool preload);
    bool HasPhysicalDriveError();
#endif
    bool ReadSector(u32 lba, u8* buffer);
    bool ReadRawSector2352(u32 lba, u8* buffer);
    bool ReadSamples(u32 lba, u32 offset, s16* buffer, u32 count);
    s32 FindTrackFromLBA(u32 lba, bool include_lead_in = false);
    bool PreloadTrack(u32 track_number, u32 lba);

private:
    bool IsCdRomUriPath(const char* path);

private:
    CdRomImage* m_current_image;
    u32 m_media_generation;
    CdRomCueBinImage* m_cue_bin_image;
    CdRomChdImage* m_chd_image;
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    CdRomPhysicalImage* m_physical_image;
#endif
};

#include "cdrom_media_inline.h"

#endif /* CDROM_MEDIA_H */
