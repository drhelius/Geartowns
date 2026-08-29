/*
 * Geartowns - FM Towns Emulator
 * Copyright (C) 2026  Ignacio Sanchez
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 */

#include <new>
#include <string>
#include "firmware.h"
#include "media_file.h"
#include "game_db.h"
#include "crc.h"

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
    m_directory[0] = '\0';
    memset(m_info, 0, sizeof(m_info));
    memset(m_system_rom, 0xFF, sizeof(m_system_rom));
    memset(m_os_rom, 0xFF, sizeof(m_os_rom));
    memset(m_font_rom, 0xFF, sizeof(m_font_rom));
    memset(m_dictionary_rom, 0xFF, sizeof(m_dictionary_rom));
    memset(m_font20_rom, 0xFF, sizeof(m_font20_rom));
    m_ready = false;
}

bool Firmware::LoadDirectory(const char* directory_path)
{
    if (!IsValidPointer(directory_path) || directory_path[0] == '\0')
    {
        Error("Invalid firmware directory");
        return false;
    }

    u8* pending_data[GT_FIRMWARE_COUNT] = {};
    GT_Firmware_Info pending_info[GT_FIRMWARE_COUNT];
    memset(pending_info, 0, sizeof(pending_info));
    bool success = true;

    for (int i = 0; i < GT_FIRMWARE_COUNT; i++)
    {
        GT_Firmware_Type type = (GT_Firmware_Type)i;
        std::string file_path(directory_path);
        append_path_component(file_path, GetFileName(type));

        if (!LoadComponent(file_path.c_str(), type, &pending_data[i], pending_info[i]))
        {
            if (IsRequired(type))
            {
                Error("Required firmware is missing or invalid: %s", file_path.c_str());
                success = false;
                break;
            }

            pending_info[i].size = GetExpectedSize(type);
            pending_info[i].synthetic = true;
        }
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

        if (m_info[i].synthetic)
        {
            Log("Optional firmware not found: %s. Using blank ROM.", GetFileName(type));
        }
        else if (m_info[i].recognized)
        {
            Log("Firmware loaded: %s. CRC: %08X", m_info[i].database_name,
                m_info[i].crc);
        }
        else
        {
            Log("Unknown firmware loaded: %s. CRC: %08X", GetFileName(type),
                m_info[i].crc);
        }
    }

    strncpy_fit(m_directory, directory_path, sizeof(m_directory));
    m_ready = true;
    return true;
}

bool Firmware::LoadComponent(const char* file_path, GT_Firmware_Type type, u8** data,
    GT_Firmware_Info& info)
{
    MediaFile* file = MediaFile::OpenFile(file_path);
    if (!IsValidPointer(file))
        return false;

    int expected_size = GetExpectedSize(type);
    s64 file_size = file->GetSize();
    if (file_size != expected_size)
    {
        Error("Incorrect firmware size for %s: %lld bytes, expected %d", file_path,
            (long long)file_size, expected_size);
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

    s64 read = file->Read(buffer, (u64)expected_size);
    SafeDelete(file);

    if (read != expected_size)
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
        default:
            return "Unknown ROM";
    }
}

const char* Firmware::GetFileName(GT_Firmware_Type type)
{
    switch (type)
    {
        case GT_FIRMWARE_SYSTEM:
            return "FMT_SYS.ROM";
        case GT_FIRMWARE_OS:
            return "FMT_DOS.ROM";
        case GT_FIRMWARE_FONT:
            return "FMT_FNT.ROM";
        case GT_FIRMWARE_DICTIONARY:
            return "FMT_DIC.ROM";
        case GT_FIRMWARE_FONT20:
            return "FMT_F20.ROM";
        default:
            return "";
    }
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
        default:
            return 0;
    }
}

bool Firmware::IsRequired(GT_Firmware_Type type)
{
    return type != GT_FIRMWARE_FONT20;
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
        default:
            return GT_GAMEDB_FLAG_NONE;
    }
}
