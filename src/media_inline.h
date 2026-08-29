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

INLINE bool Media::IsReady() const
{
    return m_media_info.ready;
}

INLINE const char* Media::GetTempPath() const
{
    return m_temp_path;
}

INLINE const char* Media::GetFilePath() const
{
    return m_media_info.path;
}

INLINE const char* Media::GetFileDirectory() const
{
    return m_media_info.directory;
}

INLINE const char* Media::GetFileName() const
{
    return m_media_info.name;
}

INLINE const char* Media::GetFileExtension() const
{
    return m_media_info.extension;
}

INLINE const u8* Media::GetData() const
{
    return m_media_data;
}

INLINE int Media::GetSize() const
{
    return m_media_info.size;
}

INLINE u32 Media::GetCRC() const
{
    return m_media_info.crc;
}

INLINE const Media::MediaFileInfo& Media::GetMediaInfo() const
{
    return m_media_info;
}
