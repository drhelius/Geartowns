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

#include <string>
#include <fstream>
#include <algorithm>
#include "media.h"

Media::Media()
{
    Reset();
}

Media::~Media()
{
}

void Media::Init()
{
    Reset();
}

void Media::Reset()
{

}

bool Media::LoadBios(const char* directory_path)
{

    return m_bios_ready;
}

bool Media::LoadBiosFromBuffer(const u8* buffer, int size)
{
    return m_bios_ready;
}

bool Media::LoadMedia(const char* file_path)
{
    Reset();

   
    return m_ready;
}

void Media::SetTempPath(const char* path)
{
    if (IsValidPointer(path))
    {
        strncpy_fit(m_temp_path, path, sizeof(m_temp_path));
    }
    else
    {
        Error("Invalid temp path %s", path);
    }
}

bool Media::LoadMediaFromZipFile(const char* path)
{
    
    return true;
}

void Media::GatherDataFromPath(const char* path)
{
   
}

bool Media::IsValidFile(const char* path)
{
    if (!IsValidPointer(path))
    {
        Error("Invalid path %s", path);
        return false;
    }

    MediaFile* file = MediaFile::OpenFile(path);

    if (file)
    {
        s64 size = file->GetSize();

        if (size <= 0)
        {
            Error("Unable to open file %s. Size: %lld", path, (long long)size);
            SafeDelete(file);
            return false;
        }

        if (!file->IsValid())
        {
            Error("Unable to open file %s. Bad file!", path);
            SafeDelete(file);
            return false;
        }

        SafeDelete(file);
        return true;
    }
    else
    {
        Error("Unable to open file %s", path);
        return false;
    }
}