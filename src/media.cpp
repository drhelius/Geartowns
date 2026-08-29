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

#include <climits>
#include <cctype>
#include <new>
#include <string>
#include "media.h"
#include "media_file.h"
#include "crc.h"

Media::Media()
{
    InitPointer(m_media_data);
    m_temp_path[0] = '\0';
    Reset();
}

Media::~Media()
{
    SafeDeleteArray(m_media_data);
}

void Media::Init()
{
    Reset();
}

void Media::Reset()
{
    ResetMediaInfo();
}

bool Media::LoadMedia(const char* file_path)
{
    if (!IsValidPointer(file_path) || file_path[0] == '\0')
    {
        Error("Invalid media path");
        return false;
    }

    MediaFile* file = MediaFile::OpenFile(file_path);
    if (!IsValidPointer(file))
    {
        Error("Unable to open media file %s", file_path);
        return false;
    }

    s64 file_size = file->GetSize();
    if (file_size <= 0 || file_size > INT_MAX)
    {
        Error("Invalid media size for %s: %lld", file_path, (long long)file_size);
        SafeDelete(file);
        return false;
    }

    int data_size = (int)file_size;
    u8* data = new (std::nothrow) u8[data_size];
    if (!IsValidPointer(data))
    {
        Error("Unable to allocate %d bytes for %s", data_size, file_path);
        SafeDelete(file);
        return false;
    }

    s64 read = file->Read(data, (u64)data_size);
    SafeDelete(file);

    if (read != data_size)
    {
        Error("Unable to read media file %s", file_path);
        SafeDeleteArray(data);
        return false;
    }

    ResetMediaInfo();
    m_media_data = data;
    GatherDataFromPath(file_path);
    m_media_info.size = data_size;
    m_media_info.crc = CalculateCRC32(0, data, data_size);
    m_media_info.ready = true;

    Log("Media selected: %s (%d bytes, CRC %08X)", file_path, data_size,
        m_media_info.crc);
    return true;
}

void Media::SetTempPath(const char* path)
{
    if (!IsValidPointer(path))
    {
        Error("Invalid temp path");
        return;
    }

    strncpy_fit(m_temp_path, path, sizeof(m_temp_path));
}

void Media::ResetMediaInfo()
{
    SafeDeleteArray(m_media_data);
    memset(&m_media_info, 0, sizeof(m_media_info));
}

void Media::GatherDataFromPath(const char* path)
{
    strncpy_fit(m_media_info.path, path, sizeof(m_media_info.path));

    std::string full_path(path);
    size_t separator = full_path.find_last_of("/\\");
    std::string filename = separator == std::string::npos ? full_path :
        full_path.substr(separator + 1);

    if (separator == std::string::npos)
        m_media_info.directory[0] = '\0';
    else
    {
        std::string directory = full_path.substr(0, separator);
        strncpy_fit(m_media_info.directory, directory.c_str(),
            sizeof(m_media_info.directory));
    }

    strncpy_fit(m_media_info.name, filename.c_str(), sizeof(m_media_info.name));

    size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= filename.length())
    {
        m_media_info.extension[0] = '\0';
        return;
    }

    std::string extension = filename.substr(dot + 1);
    for (size_t i = 0; i < extension.length(); i++)
        extension[i] = (char)std::tolower((unsigned char)extension[i]);

    strncpy_fit(m_media_info.extension, extension.c_str(),
        sizeof(m_media_info.extension));
}
