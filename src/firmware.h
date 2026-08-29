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

#ifndef FIRMWARE_H
#define FIRMWARE_H

#include "common.h"

class Firmware
{
public:
    Firmware();
    ~Firmware();
    void Init();
    void Unload();
    bool LoadDirectory(const char* directory_path);
    bool IsReady() const;
    bool IsComplete() const;
    const char* GetDirectory() const;
    const GT_Firmware_Info& GetInfo(GT_Firmware_Type type) const;
    const u8* GetSystemRom() const;
    const u8* GetOsRom() const;
    const u8* GetFontRom() const;
    const u8* GetDictionaryRom() const;
    const u8* GetFont20Rom() const;

    static const char* GetComponentName(GT_Firmware_Type type);
    static const char* GetFileName(GT_Firmware_Type type);
    static int GetExpectedSize(GT_Firmware_Type type);
    static bool IsRequired(GT_Firmware_Type type);

private:
    bool LoadComponent(const char* file_path, GT_Firmware_Type type, u8** data,
        GT_Firmware_Info& info);
    void GatherDatabaseInfo(GT_Firmware_Type type, GT_Firmware_Info& info);
    u8* GetWritableData(GT_Firmware_Type type);
    static u16 GetDatabaseFlag(GT_Firmware_Type type);

private:
    char m_directory[GT_MAX_PATH];
    GT_Firmware_Info m_info[GT_FIRMWARE_COUNT];
    bool m_ready;
    u8 m_system_rom[GT_FIRMWARE_SYSTEM_SIZE];
    u8 m_os_rom[GT_FIRMWARE_OS_SIZE];
    u8 m_font_rom[GT_FIRMWARE_FONT_SIZE];
    u8 m_dictionary_rom[GT_FIRMWARE_DICTIONARY_SIZE];
    u8 m_font20_rom[GT_FIRMWARE_FONT20_SIZE];
};

#include "firmware_inline.h"

#endif /* FIRMWARE_H */
