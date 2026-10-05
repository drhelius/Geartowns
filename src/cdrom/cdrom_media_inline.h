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

#ifndef CDROM_MEDIA_INLINE_H
#define CDROM_MEDIA_INLINE_H

#include "cdrom_media.h"

INLINE bool CdRomMedia::IsReady()
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->IsReady();
    }
    else
    {
        return false;
    }
}

INLINE u32 CdRomMedia::GetCRC()
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->GetCRC();
    }
    else
    {
        Error("CdRomMedia::GetCRC failed - Current image is NULL");
        return 0;
    }
}

INLINE const char* CdRomMedia::GetFilePath()
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->GetFilePath();
    }
    else
    {
        Error("CdRomMedia::GetFilePath failed - Current image is NULL");
        return "";
    }
}

INLINE const char* CdRomMedia::GetFileDirectory()
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->GetFileDirectory();
    }
    else
    {
        Error("CdRomMedia::GetFileDirectory failed - Current image is NULL");
        return "";
    }
}

INLINE const char* CdRomMedia::GetFileName()
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->GetFileName();
    }
    else
    {
        Error("CdRomMedia::GetFileName failed - Current image is NULL");
        return "";
    }
}

INLINE const char* CdRomMedia::GetFileExtension()
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->GetFileExtension();
    }
    else
    {
        return "";
    }
}

INLINE const std::vector<CdRomImage::Track>& CdRomMedia::GetTracks()
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->GetTOC()->tracks;
    }
    else
    {
        static std::vector<CdRomImage::Track> empty_tracks;
        return empty_tracks;
    }
}

INLINE u8 CdRomMedia::GetTrackCount()
{
    if (IsValidPointer(m_current_image))
    {
        return static_cast<u8>(m_current_image->GetTOC()->tracks.size());
    }
    else
    {
        return 0;
    }
}

INLINE GT_CdRomTrackType CdRomMedia::GetTrackType(u8 track_number)
{
    if (IsValidPointer(m_current_image))
    {
        if (track_number < m_current_image->GetTOC()->tracks.size())
            return m_current_image->GetTOC()->tracks[track_number].type;
        else
        {
            Error("CdRomMedia::GetTrackType failed - Track number %d out of bounds", track_number);
            return GT_CDROM_AUDIO_TRACK;
        }
    }
    else
    {
        Error("CdRomMedia::GetTrackType failed - Current image is NULL");
        return GT_CDROM_AUDIO_TRACK;
    }
}

INLINE bool CdRomMedia::IsAudioSector(u32 lba)
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->IsAudioSector(lba);
    }
    else
    {
        return false;
    }
}

INLINE GT_CdRomMSF CdRomMedia::GetCdRomLength()
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->GetTOC()->total_length;
    }
    else
    {
        return { 0, 0, 0 };
    }
}

INLINE u32 CdRomMedia::GetSectorCount()
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->GetTOC()->sector_count;
    }
    else
    {
        return 0;
    }
}

INLINE u32 CdRomMedia::GetCurrentSector()
{
    if (IsValidPointer(m_current_image))
    {
        return m_current_image->GetCurrentSector();
    }
    else
    {
        return 0;
    }
}

INLINE u32 CdRomMedia::GetMediaGeneration()
{
    return m_media_generation;
}

INLINE void CdRomMedia::SetCurrentSector(u32 sector)
{
    if (IsValidPointer(m_current_image))
    {
        m_current_image->SetCurrentSector(sector);
    }
}

#endif /* CDROM_MEDIA_INLINE_H */
