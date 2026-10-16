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

#include <algorithm>
#include <new>
#include <string>
#include "firmware.h"
#include "media_file.h"
#include "game_db.h"
#include "crc.h"

static const char* const k_firmware_file_names[GT_FIRMWARE_COUNT][FIRMWARE_FILE_NAMES] =
{
    { "FMT_SYS.ROM", "FMT_SYS_A.ROM", NULL },
    { "FMT_DOS.ROM", "FMT_DOS_A.ROM", NULL },
    { "FMT_FNT.ROM", NULL, NULL },
    { "FMT_DIC.ROM", NULL, NULL },
    { "FMT_F20.ROM", "FMT_F20_A.ROM", NULL },
    { "MYTOWNS.ROM", "MYTOWNSUX.ROM", "MYTOWNSM2.ROM" }
};

Firmware::Firmware()
{
    Init();
}

Firmware::~Firmware()
{
}

void Firmware::Init()
{
    Unload();
}

void Firmware::Unload()
{
    memset(&m_source, 0, sizeof(m_source));
    m_source.set = -1;
    memset(m_info, 0, sizeof(m_info));
    memset(m_system_rom, 0xFF, sizeof(m_system_rom));
    memset(m_os_rom, 0xFF, sizeof(m_os_rom));
    memset(m_font_rom, 0xFF, sizeof(m_font_rom));
    memset(m_dictionary_rom, 0xFF, sizeof(m_dictionary_rom));
    memset(m_font20_rom, 0xFF, sizeof(m_font20_rom));
    memset(m_serial_rom, 0xFF, sizeof(m_serial_rom));
    m_ready = false;
}

bool Firmware::LoadSource(const GT_Firmware_Source& source, bool quiet)
{
    if (source.path[0] == '\0')
    {
        Error("Invalid firmware folder");
        return false;
    }

    u8* pending_data[GT_FIRMWARE_COUNT] = {};
    GT_Firmware_Info pending_info[GT_FIRMWARE_COUNT];
    memset(pending_info, 0, sizeof(pending_info));
    MediaFile* zip_file = NULL;
    mz_zip_archive zip;
    bool success = !source.zip || OpenZip(source.path, &zip_file, zip);

    if (!success)
        Error("Unable to open firmware zip %s", source.path);

    for (int i = 0; success && (i < GT_FIRMWARE_COUNT); i++)
    {
        GT_Firmware_Type type = (GT_Firmware_Type)i;
        bool ok = source.zip ? LoadZipComponent(zip, source.path, type, &pending_data[i], pending_info[i]) :
            LoadComponent(GetFilePath(source.path, type).c_str(), type, &pending_data[i], pending_info[i]);

        if (!ok)
        {
            if (IsRequired(type))
            {
                Error("Required firmware is missing or invalid: %s in %s", GetFileName(type), source.path);
                success = false;
                break;
            }

            pending_info[i].size = GetExpectedSize(type);
            pending_info[i].synthetic = true;
        }
    }

    if (IsValidPointer(zip_file))
    {
        mz_zip_reader_end(&zip);
        SafeDelete(zip_file);
    }

    if (!success)
    {
        for (int i = 0; i < GT_FIRMWARE_COUNT; i++)
            SafeDeleteArray(pending_data[i]);

        return false;
    }

    for (int i = 0; i < GT_FIRMWARE_COUNT; i++)
    {
        GT_Firmware_Type type = (GT_Firmware_Type)i;
        u8* destination = GetWritableData(type);
        int expected_size = GetExpectedSize(type);

        if (IsValidPointer(pending_data[i]))
            memcpy(destination, pending_data[i], expected_size);
        else
            memset(destination, 0xFF, expected_size);

        m_info[i] = pending_info[i];

        if (m_info[i].synthetic)
            m_info[i].crc = CalculateCRC32(0, destination, expected_size);

        GatherDatabaseInfo(type, m_info[i]);
        SafeDeleteArray(pending_data[i]);

        if (quiet)
            continue;

        if (m_info[i].synthetic)
        {
            Log("Optional firmware not found: %s. Using %s.", GetFileName(type),
                (type == GT_FIRMWARE_SERIAL) ? "a generated serial ROM" : "blank ROM");
        }
        else if (m_info[i].recognized)
        {
            Log("Firmware loaded: %s. CRC: %08X", m_info[i].database_name, m_info[i].crc);
        }
        else
        {
            Log("Unknown firmware loaded: %s. CRC: %08X", GetFileName(type), m_info[i].crc);
        }
    }

    m_source = source;
    m_ready = true;
    return true;
}

bool Firmware::LoadComponent(const char* file_path, GT_Firmware_Type type, u8** data, GT_Firmware_Info& info)
{
    MediaFile* file = MediaFile::OpenFile(file_path);

    if (!IsValidPointer(file))
        return false;

    int expected_size = GetExpectedSize(type);
    s64 file_size = file->GetSize();

    if (file_size != expected_size)
    {
        Error("Incorrect firmware size for %s: %lld bytes, expected %d", file_path, (long long)file_size, expected_size);
        SafeDelete(file);
        return false;
    }

    u8* buffer = new (std::nothrow) u8[expected_size];

    if (!IsValidPointer(buffer))
    {
        Error("Unable to allocate %d bytes for %s", expected_size, file_path);
        SafeDelete(file);
        return false;
    }

    bool ok = file->ReadExact(buffer, (u64)expected_size);
    SafeDelete(file);

    if (!ok)
    {
        Error("Unable to read firmware file %s", file_path);
        SafeDeleteArray(buffer);
        return false;
    }

    *data = buffer;
    strncpy_fit(info.path, file_path, sizeof(info.path));
    info.size = expected_size;
    info.crc = CalculateCRC32(0, buffer, expected_size);
    info.loaded = true;
    return true;
}

bool Firmware::LoadZipComponent(mz_zip_archive& zip, const char* zip_path, GT_Firmware_Type type, u8** data,
    GT_Firmware_Info& info)
{
    int index = FindZipComponent(zip, type);
    mz_zip_archive_file_stat stat;

    if ((index < 0) || !mz_zip_reader_file_stat(&zip, (mz_uint)index, &stat))
        return false;

    int expected_size = GetExpectedSize(type);

    if (stat.m_uncomp_size != (mz_uint64)expected_size)
    {
        Error("Incorrect firmware size for %s in %s: %llu bytes, expected %d", stat.m_filename, zip_path,
            (unsigned long long)stat.m_uncomp_size, expected_size);
        return false;
    }

    u8* buffer = new (std::nothrow) u8[expected_size];

    if (!IsValidPointer(buffer) || !mz_zip_reader_extract_to_mem(&zip, (mz_uint)index, buffer, expected_size, 0))
    {
        Error("Unable to read firmware %s in %s", stat.m_filename, zip_path);
        SafeDeleteArray(buffer);
        return false;
    }

    std::string path(zip_path);
    append_path_component(path, stat.m_filename);
    *data = buffer;
    strncpy_fit(info.path, path.c_str(), sizeof(info.path));
    info.size = expected_size;
    info.crc = CalculateCRC32(0, buffer, expected_size);
    info.loaded = true;
    return true;
}

void Firmware::GatherDatabaseInfo(GT_Firmware_Type type, GT_Firmware_Info& info)
{
    info.recognized = false;
    info.database_name[0] = '\0';
    u16 flag = GetDatabaseFlag(type);

    for (int i = 0; k_game_database[i].title != 0; i++)
    {
        const GT_DB_Entry& entry = k_game_database[i];

        if (entry.crc == info.crc && (entry.flags & flag))
        {
            info.recognized = true;
            strncpy_fit(info.database_name, entry.title, sizeof(info.database_name));
            return;
        }
    }
}

u8* Firmware::GetWritableData(GT_Firmware_Type type)
{
    switch (type)
    {
        case GT_FIRMWARE_SYSTEM:
            return m_system_rom;
        case GT_FIRMWARE_OS:
            return m_os_rom;
        case GT_FIRMWARE_FONT:
            return m_font_rom;
        case GT_FIRMWARE_DICTIONARY:
            return m_dictionary_rom;
        case GT_FIRMWARE_FONT20:
            return m_font20_rom;
        case GT_FIRMWARE_SERIAL:
            return m_serial_rom;
        default:
            return NULL;
    }
}

const char* Firmware::GetComponentName(GT_Firmware_Type type)
{
    switch (type)
    {
        case GT_FIRMWARE_SYSTEM:
            return "System ROM";
        case GT_FIRMWARE_OS:
            return "OS ROM";
        case GT_FIRMWARE_FONT:
            return "Font ROM";
        case GT_FIRMWARE_DICTIONARY:
            return "Dictionary ROM";
        case GT_FIRMWARE_FONT20:
            return "20-dot Font ROM";
        case GT_FIRMWARE_SERIAL:
            return "Serial ROM";
        default:
            return "Unknown ROM";
    }
}

const char* Firmware::GetFileName(GT_Firmware_Type type)
{
    return ((int)type >= 0) && ((int)type < GT_FIRMWARE_COUNT) ? k_firmware_file_names[type][0] : "";
}

std::string Firmware::GetFilePath(const char* directory_path, GT_Firmware_Type type)
{
    std::vector<std::string> folders;
    std::vector<std::string> files;
    bool listed = MediaFile::ListDirectory(directory_path, folders, files);
    std::sort(files.begin(), files.end());

    for (int i = 0; i < FIRMWARE_FILE_NAMES; i++)
    {
        const char* name = k_firmware_file_names[type][i];

        if (!IsValidPointer(name))
            continue;

        std::string upper(name);
        std::string lower(name);
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (listed)
        {
            for (size_t j = 0; j < files.size(); j++)
            {
                if (strings_equal_ignore_case(files[j], upper))
                {
                    std::string path(directory_path);
                    append_path_component(path, files[j].c_str());
                    return path;
                }
            }

            continue;
        }

        for (int j = 0; j < 2; j++)
        {
            std::string path(directory_path);
            append_path_component(path, (j == 0) ? upper.c_str() : lower.c_str());
            MediaFile* file = MediaFile::OpenFile(path.c_str());

            if (IsValidPointer(file))
            {
                SafeDelete(file);
                return path;
            }
        }
    }

    std::string path(directory_path);
    append_path_component(path, GetFileName(type));
    return path;
}

int Firmware::GetExpectedSize(GT_Firmware_Type type)
{
    switch (type)
    {
        case GT_FIRMWARE_SYSTEM:
            return GT_FIRMWARE_SYSTEM_SIZE;
        case GT_FIRMWARE_OS:
            return GT_FIRMWARE_OS_SIZE;
        case GT_FIRMWARE_FONT:
            return GT_FIRMWARE_FONT_SIZE;
        case GT_FIRMWARE_DICTIONARY:
            return GT_FIRMWARE_DICTIONARY_SIZE;
        case GT_FIRMWARE_FONT20:
            return GT_FIRMWARE_FONT20_SIZE;
        case GT_FIRMWARE_SERIAL:
            return GT_FIRMWARE_SERIAL_SIZE;
        default:
            return 0;
    }
}

bool Firmware::IsRequired(GT_Firmware_Type type)
{
    return (type != GT_FIRMWARE_FONT20) && (type != GT_FIRMWARE_SERIAL);
}

u16 Firmware::GetDatabaseFlag(GT_Firmware_Type type)
{
    switch (type)
    {
        case GT_FIRMWARE_SYSTEM:
            return GT_GAMEDB_FIRMWARE_SYSTEM;
        case GT_FIRMWARE_OS:
            return GT_GAMEDB_FIRMWARE_OS;
        case GT_FIRMWARE_FONT:
            return GT_GAMEDB_FIRMWARE_FONT;
        case GT_FIRMWARE_DICTIONARY:
            return GT_GAMEDB_FIRMWARE_DICTIONARY;
        case GT_FIRMWARE_FONT20:
            return GT_GAMEDB_FIRMWARE_FONT20;
        case GT_FIRMWARE_SERIAL:
            return GT_GAMEDB_FIRMWARE_SERIAL;
        default:
            return GT_GAMEDB_FLAG_NONE;
    }
}

void Firmware::FindSources(const char* default_folder, const std::string* model_folders, GT_Firmware_Source* sources)
{
    std::vector<GT_Firmware_Source> candidates;

    if (IsValidPointer(default_folder) && (default_folder[0] != '\0'))
    {
        std::vector<std::string> folders;
        std::vector<std::string> files;
        MediaFile::ListDirectory(default_folder, folders, files);
        std::sort(folders.begin(), folders.end());
        std::sort(files.begin(), files.end());

        AddCandidate(candidates, default_folder, false);

        for (size_t i = 0; i < folders.size(); i++)
        {
            std::string path(default_folder);
            append_path_component(path, folders[i].c_str());
            AddCandidate(candidates, path.c_str(), false);
        }

        for (size_t i = 0; i < files.size(); i++)
        {
            if (!ends_with_no_case(files[i].c_str(), ".zip"))
                continue;

            std::string path(default_folder);
            append_path_component(path, files[i].c_str());
            AddCandidate(candidates, path.c_str(), true);
        }
    }

    for (int model = 0; model < GT_MACHINE_COUNT; model++)
    {
        GT_Firmware_Source& source = sources[model];
        memset(&source, 0, sizeof(source));
        source.set = -1;

        if (!model_folders[model].empty())
        {
            strncpy_fit(source.path, model_folders[model].c_str(), sizeof(source.path));
            IdentifySource(source);
            source.type = GT_FIRMWARE_SOURCE_MODEL_FOLDER;
            continue;
        }

        for (size_t i = 0; i < candidates.size(); i++)
        {
            GT_Firmware_Source_Type type = GetSourceType(candidates[i].set, (GT_Machine_Model)model);

            if ((source.type == GT_FIRMWARE_SOURCE_NONE) || (type < source.type))
            {
                source = candidates[i];
                source.type = type;
            }
        }
    }
}

GT_Firmware_Source_Type Firmware::GetSourceType(int set, GT_Machine_Model model)
{
    u32 bit = 1 << model;

    if (set < 0)
        return GT_FIRMWARE_SOURCE_UNKNOWN;

    if ((k_firmware_sets[set].models & bit) != 0)
        return GT_FIRMWARE_SOURCE_OWN_SET;

    if ((k_firmware_sets[set].compatible_models & bit) != 0)
        return GT_FIRMWARE_SOURCE_COMPATIBLE_SET;

    return GT_FIRMWARE_SOURCE_OTHER_SET;
}

int Firmware::FindSet(u32 system_crc)
{
    for (int i = 0; i < FIRMWARE_SET_COUNT; i++)
    {
        if (k_firmware_sets[i].system_crc == system_crc)
            return i;
    }

    return -1;
}

bool Firmware::IdentifySource(GT_Firmware_Source& source)
{
    source.crc = 0;
    source.set = -1;
    bool complete = true;

    if (source.zip)
    {
        MediaFile* file = NULL;
        mz_zip_archive zip;

        if (!OpenZip(source.path, &file, zip))
            return false;

        for (int i = 0; complete && (i < GT_FIRMWARE_COUNT); i++)
        {
            GT_Firmware_Type type = (GT_Firmware_Type)i;
            int index = FindZipComponent(zip, type);
            mz_zip_archive_file_stat stat;
            bool found = (index >= 0) && mz_zip_reader_file_stat(&zip, (mz_uint)index, &stat) &&
                (stat.m_uncomp_size == (mz_uint64)GetExpectedSize(type));

            if (found && (type == GT_FIRMWARE_SYSTEM))
                source.crc = stat.m_crc32;

            complete = found || !IsRequired(type);
        }

        mz_zip_reader_end(&zip);
        SafeDelete(file);
    }
    else
    {
        for (int i = 0; complete && (i < GT_FIRMWARE_COUNT); i++)
        {
            GT_Firmware_Type type = (GT_Firmware_Type)i;
            int size = GetExpectedSize(type);
            MediaFile* file = MediaFile::OpenFile(GetFilePath(source.path, type).c_str());
            bool found = IsValidPointer(file) && (file->GetSize() == size);

            if (found && (type == GT_FIRMWARE_SYSTEM))
            {
                u8* buffer = new (std::nothrow) u8[size];
                found = IsValidPointer(buffer) && file->ReadExact(buffer, (u64)size);

                if (found)
                    source.crc = CalculateCRC32(0, buffer, size);

                SafeDeleteArray(buffer);
            }

            SafeDelete(file);
            complete = found || !IsRequired(type);
        }
    }

    if (complete)
        source.set = FindSet(source.crc);

    return complete;
}

void Firmware::AddCandidate(std::vector<GT_Firmware_Source>& candidates, const char* path, bool zip)
{
    GT_Firmware_Source source;
    memset(&source, 0, sizeof(source));
    strncpy_fit(source.path, path, sizeof(source.path));
    source.zip = zip;

    if (IdentifySource(source))
        candidates.push_back(source);
}

bool Firmware::OpenZip(const char* path, MediaFile** file, mz_zip_archive& zip)
{
    memset(&zip, 0, sizeof(zip));
    *file = MediaFile::OpenFile(path);

    if (!IsValidPointer(*file) || !(*file)->CanSeek() || ((*file)->GetSize() <= 0))
    {
        SafeDelete(*file);
        return false;
    }

    zip.m_pRead = ReadZipCallback;
    zip.m_pIO_opaque = *file;

    if (!mz_zip_reader_init(&zip, (mz_uint64)(*file)->GetSize(), 0))
    {
        SafeDelete(*file);
        return false;
    }

    return true;
}

int Firmware::FindZipComponent(mz_zip_archive& zip, GT_Firmware_Type type)
{
    for (int i = 0; i < FIRMWARE_FILE_NAMES; i++)
    {
        const char* name = k_firmware_file_names[type][i];

        if (!IsValidPointer(name))
            continue;

        for (mz_uint file = 0; file < mz_zip_reader_get_num_files(&zip); file++)
        {
            char file_name[GT_MAX_PATH];
            mz_zip_reader_get_filename(&zip, file, file_name, sizeof(file_name));

            if ((strlen(file_name) == strlen(name)) && ends_with_no_case(file_name, name))
                return (int)file;
        }
    }

    return -1;
}

size_t Firmware::ReadZipCallback(void* opaque, mz_uint64 offset, void* buffer, size_t size)
{
    MediaFile* file = (MediaFile*)opaque;
    return file->ReadAt(offset, buffer, size) ? size : 0;
}
