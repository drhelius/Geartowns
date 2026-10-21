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

#include "media_file_native.h"
#include "../common/common.h"
#include <limits>

MediaFileNative::MediaFileNative()
{

}

MediaFileNative::~MediaFileNative()
{
    Close();
}

bool MediaFileNative::ListDirectory(const char* path, std::vector<std::string>& folders, std::vector<std::string>& files)
{
#if defined(_WIN32)
    std::wstring search = utf8_to_wstring((std::string(path) + "\\*").c_str());
    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW(search.c_str(), &fd);

    if (find == INVALID_HANDLE_VALUE)
        return false;

    do
    {
        if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L".."))
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                folders.push_back(wstring_to_utf8(fd.cFileName));
            else
                files.push_back(wstring_to_utf8(fd.cFileName));
        }
    }
    while (FindNextFileW(find, &fd));

    FindClose(find);
    return true;
#elif !defined(__LIBRETRO__)
    DIR* dir = opendir(path);

    if (!IsValidPointer(dir))
        return false;

    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        std::string item(path);
        append_path_component(item, entry->d_name);
        struct stat info;

        if (stat(item.c_str(), &info) != 0)
            continue;

        if (S_ISDIR(info.st_mode))
            folders.push_back(entry->d_name);
        else if (S_ISREG(info.st_mode))
            files.push_back(entry->d_name);
    }

    closedir(dir);
    return true;
#else
    UNUSED(path);
    UNUSED(folders);
    UNUSED(files);
    return false;
#endif
}

bool MediaFileNative::Open(const char* path, bool writable)
{
    Close();

    if (!IsValidPointer(path))
        return false;

    std::ios::openmode mode = std::ios::in | std::ios::binary;

    if (writable)
        mode |= std::ios::out;

    open_ifstream_utf8(m_file, path, mode);
    return IsValid();
}

void MediaFileNative::Close()
{
    if (m_file.is_open())
        m_file.close();
}

bool MediaFileNative::IsOpen() const
{
    return m_file.is_open();
}

bool MediaFileNative::IsValid() const
{
    return m_file.is_open() && !m_file.bad() && !m_file.fail() && m_file.good() && !m_file.eof();
}

bool MediaFileNative::CanSeek() const
{
    return m_file.is_open();
}

s64 MediaFileNative::GetSize()
{
    if (!m_file.is_open())
        return -1;

    if (m_file.bad())
        return -1;

    std::streampos position = m_file.tellg();
    if (position < 0)
        position = 0;

    m_file.clear();
    m_file.seekg(0, std::ios::end);
    std::streampos size = m_file.tellg();
    m_file.clear();
    m_file.seekg(position, std::ios::beg);

    if ((size < 0) || m_file.bad() || m_file.fail())
        return -1;

    return (s64)size;
}

s64 MediaFileNative::Tell()
{
    if (!m_file.is_open())
        return -1;

    std::streampos position = m_file.tellg();
    if (position < 0)
        return -1;

    return (s64)position;
}

bool MediaFileNative::Seek(s64 offset)
{
    if (offset < 0)
        return false;

    if (!m_file.is_open())
        return false;

    m_file.clear();
    m_file.seekg(offset, std::ios::beg);
    return !m_file.fail();
}

s64 MediaFileNative::Read(void* buffer, u64 size)
{
    if (!IsValidPointer(buffer))
        return -1;

    if (!m_file.is_open())
        return -1;

    if (size > (u64)(std::numeric_limits<std::streamsize>::max)())
        return -1;

    m_file.read(reinterpret_cast<char*>(buffer), (std::streamsize)size);
    return (s64)m_file.gcount();
}

s64 MediaFileNative::Write(const void* buffer, u64 size)
{
    if (!IsValidPointer(buffer))
        return -1;

    if (!m_file.is_open())
        return -1;

    if (size > (u64)(std::numeric_limits<std::streamsize>::max)())
        return -1;

    std::streampos position = m_file.tellg();
    m_file.clear();
    m_file.seekp(position, std::ios::beg);
    m_file.write(reinterpret_cast<const char*>(buffer), (std::streamsize)size);

    if (m_file.fail())
    {
        m_file.clear();
        return -1;
    }

    return (s64)size;
}

bool MediaFileNative::Flush()
{
    if (!m_file.is_open())
        return false;

    m_file.flush();
    return !m_file.fail();
}
