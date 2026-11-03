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

#include "cdrom_media.h"
#include "cdrom_cuebin_image.h"
#include "cdrom_chd_image.h"
#if defined(GT_ENABLE_PHYSICAL_CDROM)
#include "cdrom_physical_image.h"
#endif

CdRomMedia::CdRomMedia()
{
    InitPointer(m_current_image);
    m_media_generation = 0;
    InitPointer(m_cue_bin_image);
    InitPointer(m_chd_image);
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    InitPointer(m_physical_image);
#endif
}

CdRomMedia::~CdRomMedia()
{
    SafeDelete(m_cue_bin_image);
    SafeDelete(m_chd_image);
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    SafeDelete(m_physical_image);
#endif
}

void CdRomMedia::Init()
{
    m_cue_bin_image = new CdRomCueBinImage();
    m_cue_bin_image->Init();

    m_chd_image = new CdRomChdImage();
    m_chd_image->Init();

#if defined(GT_ENABLE_PHYSICAL_CDROM)
    m_physical_image = new CdRomPhysicalImage();
    m_physical_image->Init();
#endif

    Reset();
}

void CdRomMedia::Reset()
{
    InitPointer(m_current_image);
    m_media_generation++;

    m_cue_bin_image->Reset();
    m_chd_image->Reset();
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    m_physical_image->Reset();
#endif
}

bool CdRomMedia::LoadCueFromFile(const char* path, bool preload)
{
    bool cdrom_uri_path = IsCdRomUriPath(path);
    GT_CdRomCueBinLoadOptions options = cdrom_uri_path ?
        GT_CdRomCueBinStreamingLoadOptions() : GT_CdRomCueBinDefaultLoadOptions();

    m_cue_bin_image->SetLoadOptions(options);

    if (m_cue_bin_image->LoadFromFile(path, preload))
    {
        m_current_image = m_cue_bin_image;
        m_media_generation++;
        return true;
    }
    else
    {
        Error("Failed to load CUE file from %s", path);
        Reset();
        return false;
    }
}

bool CdRomMedia::LoadChdFromFile(const char* path, bool preload)
{
    if (m_chd_image->LoadFromFile(path, preload))
    {
        m_current_image = m_chd_image;
        m_media_generation++;
        return true;
    }
    else
    {
        Error("Failed to load CHD file from %s", path);
        Reset();
        return false;
    }
}

bool CdRomMedia::LoadIsoFromFile(const char* path, bool preload)
{
    m_cue_bin_image->SetLoadOptions(GT_CdRomCueBinDefaultLoadOptions());

    if (m_cue_bin_image->LoadFromIsoFile(path, preload))
    {
        m_current_image = m_cue_bin_image;
        m_media_generation++;
        return true;
    }
    else
    {
        Error("Failed to load ISO file from %s", path);
        Reset();
        return false;
    }
}

#if defined(GT_ENABLE_PHYSICAL_CDROM)
bool CdRomMedia::LoadPhysicalDrive(const char* device_id, bool preload)
{
    if (m_physical_image->LoadFromDevice(device_id, preload))
    {
        m_current_image = m_physical_image;
        m_media_generation++;
        return true;
    }
    else
    {
        Error("Failed to load physical CD-ROM from %s", device_id);
        Reset();
        return false;
    }
}

bool CdRomMedia::HasPhysicalDriveError()
{
    return IsValidPointer(m_physical_image) && m_physical_image->HasDiscError();
}
#endif

bool CdRomMedia::ReadSector(u32 lba, u8* buffer)
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->ReadSector(lba, buffer);
    }
    else
    {
        Error("ReadSector failed - Current image is NULL");
        return false;
    }
}

bool CdRomMedia::ReadRawSector2352(u32 lba, u8* buffer)
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->ReadRawSector2352(lba, buffer);
    }
    else
    {
        Error("ReadRawSector2352 failed - Current image is NULL");
        return false;
    }
}

bool CdRomMedia::ReadSamples(u32 lba, u32 offset, s16* buffer, u32 count)
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->ReadSamples(lba, offset, buffer, count);
    }
    else
    {
        Error("ReadBytes failed - Current image is NULL");
        return false;
    }
}

bool CdRomMedia::PreloadTrack(u32 track_number, u32 lba)
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->PreloadTrack(track_number, lba);
    }
    else
    {
        Error("PreloadTrack failed - Current image is NULL");
        return false;
    }
}

s32 CdRomMedia::FindTrackFromLBA(u32 lba, bool include_lead_in)
{
    if (IsValidPointer(m_current_image))
        return m_current_image->FindTrackFromLBA(lba, include_lead_in);

    return -1;
}

bool CdRomMedia::IsCdRomUriPath(const char* path)
{
    return IsValidPointer(path) && (strncmp(path, "cdrom://", 8) == 0);
}
