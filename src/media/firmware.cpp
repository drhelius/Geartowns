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
    { "MYTOWNS.ROM", "MYTOWNSUX.ROM", "MYTOWNSM2.ROM" },
    { "MAR_EX0-3.ROM", NULL, NULL }
};

static const char* const k_firmware_extension_names[FIRMWARE_EXTENSION_PARTS] =
{
    "MAR_EX0.ROM", "MAR_EX1.ROM", "MAR_EX2.ROM", "MAR_EX3.ROM"
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
    memset(m_extension_rom, 0xFF, sizeof(m_extension_rom));
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
    Container container;
    bool success = OpenContainer(source.path, source.zip, container);

    if (!success)
        Error("Unable to open firmware zip %s", source.path);

    for (int i = 0; success && (i < GT_FIRMWARE_COUNT); i++)
    {
        GT_Firmware_Type type = (GT_Firmware_Type)i;
        pending_data[i] = new (std::nothrow) u8[GetExpectedSize(type)];
        bool ok = IsValidPointer(pending_data[i]) &&
            ReadComponent(container, type, pending_data[i], pending_info[i], true);

        if (!ok)
        {
            SafeDeleteArray(pending_data[i]);

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

    CloseContainer(container);

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
        case GT_FIRMWARE_EXTENSION:
            return m_extension_rom;
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
        case GT_FIRMWARE_EXTENSION:
            return "Extension ROM";
        default:
            return "Unknown ROM";
    }
}

const char* Firmware::GetFileName(GT_Firmware_Type type)
{
    return ((int)type >= 0) && ((int)type < GT_FIRMWARE_COUNT) ? k_firmware_file_names[type][0] : "";
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
        case GT_FIRMWARE_EXTENSION:
            return GT_FIRMWARE_EXTENSION_SIZE;
        default:
            return 0;
    }
}

bool Firmware::IsRequired(GT_Firmware_Type type)
{
    return (type != GT_FIRMWARE_FONT20) && (type != GT_FIRMWARE_SERIAL) && (type != GT_FIRMWARE_EXTENSION);
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
        case GT_FIRMWARE_EXTENSION:
            return GT_GAMEDB_FIRMWARE_EXTENSION;
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
    Container container;

    if (!OpenContainer(source.path, source.zip, container))
        return false;

    bool complete = true;

    for (int i = 0; complete && (i < GT_FIRMWARE_COUNT); i++)
    {
        GT_Firmware_Type type = (GT_Firmware_Type)i;

        if (!IsRequired(type))
            continue;

        for (int part = 0; complete && (part < GetPartCount(type)); part++)
            complete = ReadPart(container, type, part, NULL, NULL, false);
    }

    if (complete)
    {
        u8* system_rom = new (std::nothrow) u8[GT_FIRMWARE_SYSTEM_SIZE];
        complete = IsValidPointer(system_rom) && ReadPart(container, GT_FIRMWARE_SYSTEM, 0, system_rom, NULL, false);

        if (complete)
            source.crc = CalculateCRC32(0, system_rom, GT_FIRMWARE_SYSTEM_SIZE);

        SafeDeleteArray(system_rom);
    }

    CloseContainer(container);

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

bool Firmware::OpenContainer(const char* path, bool zip, Container& container)
{
    container.zip = zip;
    container.path = path;
    InitPointer(container.zip_file);
    container.listed = false;
    container.packed_entry = -1;

    if (zip)
        return OpenZip(path, &container.zip_file, container.archive);

    std::vector<std::string> folders;
    container.listed = MediaFile::ListDirectory(path, folders, container.files);
    std::sort(container.files.begin(), container.files.end());
    return true;
}

void Firmware::CloseContainer(Container& container)
{
    if (!IsValidPointer(container.zip_file))
        return;

    mz_zip_reader_end(&container.archive);
    SafeDelete(container.zip_file);
}

bool Firmware::ReadComponent(Container& container, GT_Firmware_Type type, u8* destination, GT_Firmware_Info& info, bool report)
{
    int parts = GetPartCount(type);
    u32 part_size = (u32)(GetExpectedSize(type) / parts);
    std::string path;

    for (int part = 0; part < parts; part++)
    {
        if (!ReadPart(container, type, part, destination + part * part_size, part == 0 ? &path : NULL, report))
            return false;
    }

    strncpy_fit(info.path, path.c_str(), sizeof(info.path));
    info.size = GetExpectedSize(type);
    info.crc = CalculateCRC32(0, destination, info.size);
    info.loaded = true;
    return true;
}

bool Firmware::ReadPart(Container& container, GT_Firmware_Type type, int part, u8* destination, std::string* path, bool report)
{
    static const Slice k_packed[] =
    {
        { GT_FIRMWARE_OS, 0, "MROM.M36", 0x000000 },
        { GT_FIRMWARE_EXTENSION, 0, "MROM.M36", 0x080000 },
        { GT_FIRMWARE_EXTENSION, 1, "MROM.M36", 0x100000 },
        { GT_FIRMWARE_EXTENSION, 2, "MROM.M36", 0x180000 },
        { GT_FIRMWARE_EXTENSION, 3, "MROM.M37", 0x000000 },
        { GT_FIRMWARE_FONT20, 0, "MROM.M37", 0x080000 },
        { GT_FIRMWARE_DICTIONARY, 0, "MROM.M37", 0x100000 },
        { GT_FIRMWARE_FONT, 0, "MROM.M37", 0x180000 },
        { GT_FIRMWARE_SYSTEM, 0, "MROM.M37", 0x1C0000 }
    };

    u32 size = (u32)(GetExpectedSize(type) / GetPartCount(type));

    for (int i = 0; IsValidPointer(GetPartName(type, part, i)); i++)
    {
        if (ReadNamed(container, GetPartName(type, part, i), 0, size, size, destination, path, report))
            return true;
    }

    for (size_t i = 0; i < sizeof(k_packed) / sizeof(k_packed[0]); i++)
    {
        const Slice& slice = k_packed[i];

        if ((slice.type == type) && (slice.part == part) &&
            ReadNamed(container, slice.file, slice.offset, size, FIRMWARE_PACKED_SIZE, destination, path, report))
            return true;
    }

    return false;
}

bool Firmware::ReadNamed(Container& container, const char* name, u32 offset, u32 size, u32 file_size, u8* destination, std::string* path, bool report)
{
    if (container.zip)
    {
        int index = FindZipEntry(container.archive, name);
        mz_zip_archive_file_stat stat;

        if ((index < 0) || !mz_zip_reader_file_stat(&container.archive, (mz_uint)index, &stat))
            return false;

        if (stat.m_uncomp_size != (mz_uint64)file_size)
        {
            if (report)
                Error("Incorrect firmware size for %s in %s: %llu bytes, expected %u", stat.m_filename, container.path,
                    (unsigned long long)stat.m_uncomp_size, file_size);
            return false;
        }

        if (IsValidPointer(destination) && (size == file_size))
        {
            if (!mz_zip_reader_extract_to_mem(&container.archive, (mz_uint)index, destination, size, 0))
            {
                Error("Unable to read firmware %s in %s", stat.m_filename, container.path);
                return false;
            }
        }
        else if (IsValidPointer(destination))
        {
            if (container.packed_entry != index)
            {
                container.packed.resize(file_size);

                if (!mz_zip_reader_extract_to_mem(&container.archive, (mz_uint)index, &container.packed[0], file_size,
                    0))
                {
                    Error("Unable to read firmware %s in %s", stat.m_filename, container.path);
                    container.packed_entry = -1;
                    return false;
                }

                container.packed_entry = index;
            }

            memcpy(destination, &container.packed[offset], size);
        }

        if (IsValidPointer(path))
        {
            *path = container.path;
            append_path_component(*path, stat.m_filename);
        }

        return true;
    }

    std::string file_path = FindFile(container, name);
    MediaFile* file = file_path.empty() ? NULL : MediaFile::OpenFile(file_path.c_str());

    if (!IsValidPointer(file))
        return false;

    s64 actual_size = file->GetSize();
    bool ok = actual_size == (s64)file_size;

    if (!ok && report)
        Error("Incorrect firmware size for %s: %lld bytes, expected %u", file_path.c_str(), (long long)actual_size,
            file_size);

    if (ok && IsValidPointer(destination))
    {
        ok = file->ReadAt(offset, destination, size);

        if (!ok)
            Error("Unable to read firmware file %s", file_path.c_str());
    }

    SafeDelete(file);

    if (ok && IsValidPointer(path))
        *path = file_path;

    return ok;
}

std::string Firmware::FindFile(const Container& container, const char* name)
{
    std::string upper(name);
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (container.listed)
    {
        for (size_t i = 0; i < container.files.size(); i++)
        {
            if (strings_equal_ignore_case(container.files[i], upper))
            {
                std::string path(container.path);
                append_path_component(path, container.files[i].c_str());
                return path;
            }
        }

        return "";
    }

    for (int i = 0; i < 2; i++)
    {
        std::string path(container.path);
        append_path_component(path, (i == 0) ? upper.c_str() : lower.c_str());
        MediaFile* file = MediaFile::OpenFile(path.c_str());

        if (IsValidPointer(file))
        {
            SafeDelete(file);
            return path;
        }
    }

    return "";
}

int Firmware::FindZipEntry(mz_zip_archive& zip, const char* name)
{
    for (mz_uint file = 0; file < mz_zip_reader_get_num_files(&zip); file++)
    {
        char file_name[GT_MAX_PATH];
        mz_zip_reader_get_filename(&zip, file, file_name, sizeof(file_name));

        if ((strlen(file_name) == strlen(name)) && ends_with_no_case(file_name, name))
            return (int)file;
    }

    return -1;
}

int Firmware::GetPartCount(GT_Firmware_Type type)
{
    return type == GT_FIRMWARE_EXTENSION ? FIRMWARE_EXTENSION_PARTS : 1;
}

const char* Firmware::GetPartName(GT_Firmware_Type type, int part, int index)
{
    if (type == GT_FIRMWARE_EXTENSION)
        return index == 0 ? k_firmware_extension_names[part] : NULL;

    return index < FIRMWARE_FILE_NAMES ? k_firmware_file_names[type][index] : NULL;
}

size_t Firmware::ReadZipCallback(void* opaque, mz_uint64 offset, void* buffer, size_t size)
{
    MediaFile* file = (MediaFile*)opaque;
    return file->ReadAt(offset, buffer, size) ? size : 0;
}
