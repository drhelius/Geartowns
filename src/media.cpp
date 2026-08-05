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
    m_temp_path[0] = 0;
    m_bios_ready = false;
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
    m_ready = false;
    m_cdrom_ready = false;
    m_file_path[0] = 0;
    m_file_directory[0] = 0;
    m_file_name[0] = 0;
    m_file_extension[0] = 0;
}

bool Media::LoadBios(const char* directory_path)
{
    m_bios_ready = IsValidPointer(directory_path) && directory_path[0] != 0;
    return m_bios_ready;
}

bool Media::LoadBiosFromBuffer(const u8* buffer, int size)
{
    m_bios_ready = IsValidPointer(buffer) && size > 0;
    return m_bios_ready;
}

void Media::UnloadBios()
{
    m_bios_ready = false;
}

bool Media::LoadMedia(const char* file_path)
{
    Reset();

    if (!IsValidFile(file_path))
        return false;

    Log("Loading %s...", file_path);
    GatherDataFromPath(file_path);

    if (strcmp(m_file_extension, "zip") == 0)
        m_ready = LoadMediaFromZipFile(file_path);
    else
        m_ready = true;

    m_cdrom_ready = (strcmp(m_file_extension, "cue") == 0) ||
                    (strcmp(m_file_extension, "chd") == 0) ||
                    (strcmp(m_file_extension, "iso") == 0);

    if (!m_ready)
        Reset();

    return m_ready;
}

bool Media::LoadFloppy(int drive, const char* file_path, bool write_protect)
{
    UNUSED(drive);
    UNUSED(file_path);
    UNUSED(write_protect);
    return false;
}

bool Media::LoadCdRom(const char* file_path)
{
    UNUSED(file_path);
    return false;
}

bool Media::LoadHardDisk(int scsi_id, const char* file_path)
{
    UNUSED(scsi_id);
    UNUSED(file_path);
    return false;
}

void Media::EjectFloppy(int drive)
{
    UNUSED(drive);
}

void Media::EjectCdRom()
{
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
    Debug("Loading Media from ZIP file: %s", path);

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    mz_bool status = mz_zip_reader_init_file(&zip_archive, path, 0);

    if (!status)
    {
        Error("mz_zip_reader_init_file() failed!");
        return false;
    }

    bool supported_file = false;

    for (unsigned int i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++)
    {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
        {
            Error("mz_zip_reader_file_stat() failed!");
            mz_zip_reader_end(&zip_archive);
            return false;
        }

        Debug("ZIP Content - Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u", file_stat.m_filename, file_stat.m_comment, (unsigned int)file_stat.m_uncomp_size, (unsigned int)file_stat.m_comp_size);

        if (!file_stat.m_is_directory)
        {
            std::string file_name = file_stat.m_filename;
            size_t dot = file_name.find_last_of('.');
            std::string extension = dot == std::string::npos ? "" : file_name.substr(dot + 1);
            std::transform(extension.begin(), extension.end(), extension.begin(), (int(*)(int))tolower);
            supported_file = (extension == "d77") || (extension == "rdd") ||
                             (extension == "cue") || (extension == "chd") ||
                             (extension == "iso") || (extension == "bin");
            if (supported_file)
                break;
        }
    }

    mz_zip_reader_end(&zip_archive);
    return supported_file;
}

void Media::GatherDataFromPath(const char* path)
{
    if (!IsValidPointer(path))
    {
        m_file_path[0] = 0;
        m_file_directory[0] = 0;
        m_file_name[0] = 0;
        m_file_extension[0] = 0;
        return;
    }

    using namespace std;

    string fullpath(path);
    string directory;
    string filename;
    string extension;

    size_t pos = fullpath.find_last_of("/\\");
    if (pos != string::npos)
    {
        filename = fullpath.substr(pos + 1);
        directory = fullpath.substr(0, pos);
    }
    else
    {
        filename = fullpath;
        directory = "";
    }

    extension = fullpath.substr(fullpath.find_last_of(".") + 1);
    transform(extension.begin(), extension.end(), extension.begin(), (int(*)(int)) tolower);

    snprintf(m_file_path, sizeof(m_file_path), "%s", path);
    snprintf(m_file_directory, sizeof(m_file_directory), "%s", directory.c_str());
    snprintf(m_file_name, sizeof(m_file_name), "%s", filename.c_str());
    snprintf(m_file_extension, sizeof(m_file_extension), "%s", extension.c_str());
}

bool Media::IsValidFile(const char* path)
{
    using namespace std;

    if (!IsValidPointer(path))
    {
        Error("Invalid path %s", path);
        return false;
    }

    ifstream file;
    open_ifstream_utf8(file, path, ios::in | ios::binary | ios::ate);

    if (file.is_open())
    {
        int size = static_cast<int>(file.tellg());

        if (size <= 0)
        {
            Error("Unable to open file %s. Size: %d", path, size);
            file.close();
            return false;
        }

        if (file.bad() || file.fail() || !file.good() || file.eof())
        {
            Error("Unable to open file %s. Bad file!", path);
            file.close();
            return false;
        }

        file.close();
        return true;
    }
    else
    {
        Error("Unable to open file %s", path);
        return false;
    }
}