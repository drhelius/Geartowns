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
    return m_ready;
}

INLINE bool Media::IsBiosReady() const
{
    return m_bios_ready;
}

INLINE bool Media::IsCdRomReady() const
{
    return m_cdrom_ready;
}

INLINE const char* Media::GetFilePath() const
{
    return m_file_path;
}

INLINE const char* Media::GetFileDirectory() const
{
    return m_file_directory;
}

INLINE const char* Media::GetFileName() const
{
    return m_file_name;
}

INLINE const char* Media::GetFileExtension() const
{
    return m_file_extension;
}