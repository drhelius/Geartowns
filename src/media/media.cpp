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
#include "game_db.h"
#include "../cdrom/cdrom_media.h"

Media::Media(CdRomMedia* cdrom_media)
{
    m_cdrom_media = cdrom_media;
    InitPointer(m_media_data);
    m_temp_path[0] = '\0';
    m_preload_cdrom = false;
    ResetMediaInfo();
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
    m_cdrom_media->Reset();
}

bool Media::ParsePlaylist(const char* playlist_path, const char* text, size_t size, std::vector<PlaylistEntry>& entries)
{
    entries.clear();

    if (!IsValidPointer(playlist_path) || !IsValidPointer(text))
        return false;

    std::string directory(playlist_path);
    size_t separator = directory.find_last_of("/\\");
    directory = separator == std::string::npos ? "" : directory.substr(0, separator + 1);
    std::string content(text, size);
    size_t position = content.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;

    while (position < content.size())
    {
        size_t end = content.find('\n', position);

        if (end == std::string::npos)
            end = content.size();

        std::string line = content.substr(position, end - position);
        position = end + 1;
        size_t first = line.find_first_not_of(" \t\r");
        size_t last = line.find_last_not_of(" \t\r");

        if (first == std::string::npos)
            continue;

        line = line.substr(first, last - first + 1);
        PlaylistEntry entry;
        entry.has_label = false;
        entry.save_disk = false;

        if (line.compare(0, 10, "#SAVEDISK:") == 0)
        {
            entry.label = line.substr(10);
            entry.save_disk = true;
            entries.push_back(entry);
            continue;
        }

        if (line[0] == '#')
            continue;

        size_t bar = line.find('|');

        if (bar != std::string::npos)
        {
            entry.label = line.substr(bar + 1);
            entry.has_label = true;
            line.erase(bar);
            size_t path_end = line.find_last_not_of(" \t");

            if (path_end == std::string::npos)
                continue;

            line.erase(path_end + 1);
        }

        bool absolute = line[0] == '/' || line[0] == '\\' || (line.size() > 1 && line[1] == ':') ||
            line.compare(0, 8, "cdrom://") == 0;
        entry.path = absolute ? line : directory + line;
        entries.push_back(entry);
    }

    return !entries.empty();
}

bool Media::LoadMedia(const char* file_path)
{
    if (!IsValidPointer(file_path) || file_path[0] == '\0')
    {
        Error("Invalid media path");
        return false;
    }

    Reset();
    GatherDataFromPath(file_path);

    const char* extension = m_media_info.extension;
    bool ok = false;

    if (extension[0] == '\0')
    {
        Error("Media file has no extension: %s", file_path);
        Reset();
        return false;
    }

    if (strcmp(extension, "cue") == 0)
    {
        m_media_info.cdrom = true;
        ok = m_cdrom_media->LoadCueFromFile(file_path, m_preload_cdrom);
    }
    else if (strcmp(extension, "chd") == 0)
    {
        m_media_info.cdrom = true;
        ok = m_cdrom_media->LoadChdFromFile(file_path, m_preload_cdrom);
    }
    else if (strcmp(extension, "iso") == 0)
    {
        m_media_info.cdrom = true;
        ok = m_cdrom_media->LoadIsoFromFile(file_path, m_preload_cdrom);
    }
    else if (strcmp(extension, "zip") == 0)
        ok = LoadCdRomFromZipFile(file_path) || (!m_media_info.cdrom && LoadFile(file_path));
    else
        ok = LoadFile(file_path);

    if (!ok)
    {
        Error("Unable to load media %s", file_path);
        Reset();
        return false;
    }

    if (m_media_info.cdrom)
        m_media_info.crc = m_cdrom_media->GetCRC();

    m_media_info.ready = true;

    Log("Media selected: %s (CRC %08X)", file_path, m_media_info.crc);
    GatherDataFromDB();
    return true;
}

#if defined(GT_ENABLE_PHYSICAL_CDROM)
bool Media::LoadPhysicalCdRom(const char* device_id)
{
    if (!IsValidPointer(device_id) || (device_id[0] == 0))
    {
        Error("Invalid physical CD-ROM device id");
        return false;
    }

    Log("Loading physical CD-ROM %s...", device_id);

    Reset();

    if (!m_cdrom_media->LoadPhysicalDrive(device_id, m_preload_cdrom))
    {
        Reset();
        return false;
    }

    m_media_info.cdrom = true;
    m_media_info.physical_cdrom = true;
    m_media_info.crc = m_cdrom_media->GetCRC();
    strncpy_fit(m_media_info.physical_cdrom_device_id, device_id, sizeof(m_media_info.physical_cdrom_device_id));

    // The disc gets a file name from its CRC or the drive because save states key on it
    if (m_media_info.crc != 0)
        snprintf(m_media_info.name, sizeof(m_media_info.name), "physical_cdrom_%08X.physicalcd", m_media_info.crc);
    else
    {
        char sanitized[128] = {};
        int pos = 0;

        for (int i = 0; (device_id[i] != 0) && (pos < ((int)sizeof(sanitized) - 1)); i++)
        {
            char c = device_id[i];
            bool valid = ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9'));
            sanitized[pos++] = valid ? c : '_';
        }

        if (pos == 0)
            strncpy_fit(sanitized, "unknown", sizeof(sanitized));

        snprintf(m_media_info.name, sizeof(m_media_info.name), "physical_cdrom_%s.physicalcd", sanitized);
    }

    strncpy_fit(m_media_info.path, m_media_info.name, sizeof(m_media_info.path));
    m_media_info.directory[0] = '\0';
    strncpy_fit(m_media_info.extension, "physicalcd", sizeof(m_media_info.extension));
    m_media_info.ready = true;

    Log("Physical CD-ROM selected: %s (CRC %08X)", device_id, m_media_info.crc);
    GatherDataFromDB();
    return true;
}
#endif

bool Media::HasPhysicalCdRomError()
{
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    return m_media_info.physical_cdrom && m_cdrom_media->HasPhysicalDriveError();
#else
    return false;
#endif
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

bool Media::LoadFile(const char* file_path)
{
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

    bool ok = file->ReadExact(data, (u64)data_size);
    SafeDelete(file);

    if (!ok)
    {
        Error("Unable to read media file %s", file_path);
        SafeDeleteArray(data);
        return false;
    }

    SafeDeleteArray(m_media_data);
    m_media_data = data;
    m_media_info.size = data_size;
    m_media_info.crc = CalculateCRC32(0, data, data_size);
    return true;
}

// A ZIP holding a CUE sheet is extracted to the temp path and its disc loaded from there
bool Media::LoadCdRomFromZipFile(const char* file_path)
{
    using namespace std;

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_reader_init_file(&zip_archive, file_path, 0))
    {
        Error("Unable to open ZIP file %s", file_path);
        return false;
    }

    string cue_name;

    for (unsigned int i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++)
    {
        mz_zip_archive_file_stat file_stat;

        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
            break;

        string name(file_stat.m_filename);
        size_t dot = name.find_last_of('.');

        if ((dot != string::npos) && strings_equal_ignore_case(name.substr(dot + 1), "cue"))
        {
            cue_name = name;
            break;
        }
    }

    mz_zip_reader_end(&zip_archive);

    if (cue_name.empty())
        return false;

    m_media_info.cdrom = true;

#if defined(__LIBRETRO__)
    Error("CD-ROM images in ZIP files are not supported: %s", file_path);
    return false;
#else
    if (m_temp_path[0] == '\0')
    {
        Error("No temp path to extract ZIP file %s", file_path);
        return false;
    }

    string temp_path(m_temp_path);
    temp_path += "/";
    temp_path += m_media_info.name;
    temp_path += "_tmp";

    Debug("Extracting %s to %s", file_path, temp_path.c_str());

    if (!remove_directory_and_contents(temp_path.c_str()))
    {
        Error("Unable to clear the extraction folder %s", temp_path.c_str());
        return false;
    }

    if (!extract_zip_to_folder(file_path, temp_path.c_str()))
    {
        Error("Failed to extract ZIP file %s to %s", file_path, temp_path.c_str());
        return false;
    }

    string cue_path = temp_path + "/" + cue_name;
    return m_cdrom_media->LoadCueFromFile(cue_path.c_str(), m_preload_cdrom);
#endif
}

void Media::ResetMediaInfo()
{
    SafeDeleteArray(m_media_data);
    memset(&m_media_info, 0, sizeof(m_media_info));
    InitPointer(m_db_entry);
}

void Media::GatherDataFromPath(const char* path)
{
    strncpy_fit(m_media_info.path, path, sizeof(m_media_info.path));

    std::string full_path(path);
    size_t separator = full_path.find_last_of("/\\");
    std::string filename = separator == std::string::npos ? full_path : full_path.substr(separator + 1);

    if (separator == std::string::npos)
        m_media_info.directory[0] = '\0';
    else
    {
        std::string directory = full_path.substr(0, separator);
        strncpy_fit(m_media_info.directory, directory.c_str(), sizeof(m_media_info.directory));
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

    strncpy_fit(m_media_info.extension, extension.c_str(), sizeof(m_media_info.extension));
}

void Media::GatherDataFromDB()
{
    for (int i = 0; k_game_database[i].title != 0; i++)
    {
        const GT_DB_Entry& entry = k_game_database[i];

        if ((entry.crc == m_media_info.crc) && ((entry.flags & GT_GAMEDB_FIRMWARE_MASK) == 0))
        {
            m_db_entry = &entry;
            Log("Media found in database: %s. CRC: %08X", entry.title, m_media_info.crc);
            return;
        }
    }

    Debug("Media not found in database. CRC: %08X", m_media_info.crc);
}

const GT_DB_Entry* Media::FindFloppyEntry(u32 crc)
{
    for (int i = 0; k_floppy_database[i].title != 0; i++)
    {
        const GT_DB_Entry& entry = k_floppy_database[i];

        if (entry.crc == crc)
        {
            Log("Floppy disk found in database: %s. CRC: %08X", entry.title, crc);
            return &entry;
        }
    }

    return NULL;
}
