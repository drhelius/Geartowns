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

#ifndef FIRMWARE_H
#define FIRMWARE_H

#include <string>
#include <vector>
#include "../common/common.h"

class MediaFile;

#define FIRMWARE_SET_COUNT 5
#define FIRMWARE_FILE_NAMES 3

class Firmware
{
public:
    Firmware();
    ~Firmware();
    void Init();
    void Unload();
    bool LoadSource(const GT_Firmware_Source& source, bool quiet = false);
    bool IsReady() const;
    bool IsComplete() const;
    const char* GetDirectory() const;
    const GT_Firmware_Source& GetSource() const;
    const GT_Firmware_Info& GetInfo(GT_Firmware_Type type) const;
    const u8* GetSystemRom() const;
    const u8* GetOsRom() const;
    const u8* GetFontRom() const;
    const u8* GetDictionaryRom() const;
    const u8* GetFont20Rom() const;
    const u8* GetSerialRom() const;

    static void FindSources(const char* default_folder, const std::string* model_folders,
        GT_Firmware_Source* sources);
    static int FindSet(u32 system_crc);
    static GT_Firmware_Source_Type GetSourceType(int set, GT_Machine_Model model);
    static const char* GetComponentName(GT_Firmware_Type type);
    static const char* GetFileName(GT_Firmware_Type type);
    static std::string GetFilePath(const char* directory_path, GT_Firmware_Type type);
    static int GetExpectedSize(GT_Firmware_Type type);
    static bool IsRequired(GT_Firmware_Type type);
    static u16 GetDatabaseFlag(GT_Firmware_Type type);

private:
    bool LoadComponent(const char* file_path, GT_Firmware_Type type, u8** data, GT_Firmware_Info& info);
    bool LoadZipComponent(mz_zip_archive& zip, const char* zip_path, GT_Firmware_Type type, u8** data,
        GT_Firmware_Info& info);
    void GatherDatabaseInfo(GT_Firmware_Type type, GT_Firmware_Info& info);
    u8* GetWritableData(GT_Firmware_Type type);
    static bool IdentifySource(GT_Firmware_Source& source);
    static void AddCandidate(std::vector<GT_Firmware_Source>& candidates, const char* path, bool zip);
    static bool OpenZip(const char* path, MediaFile** file, mz_zip_archive& zip);
    static int FindZipComponent(mz_zip_archive& zip, GT_Firmware_Type type);
    static size_t ReadZipCallback(void* opaque, mz_uint64 offset, void* buffer, size_t size);

private:
    GT_Firmware_Source m_source;
    GT_Firmware_Info m_info[GT_FIRMWARE_COUNT];
    bool m_ready;
    u8 m_system_rom[GT_FIRMWARE_SYSTEM_SIZE];
    u8 m_os_rom[GT_FIRMWARE_OS_SIZE];
    u8 m_font_rom[GT_FIRMWARE_FONT_SIZE];
    u8 m_dictionary_rom[GT_FIRMWARE_DICTIONARY_SIZE];
    u8 m_font20_rom[GT_FIRMWARE_FONT20_SIZE];
    u8 m_serial_rom[GT_FIRMWARE_SERIAL_SIZE];
};

static const GT_Firmware_Set k_firmware_sets[FIRMWARE_SET_COUNT] =
{
    { "fmtowns", "Model 1/2 set", 0x53319E23, 1 << GT_MACHINE_MODEL1_2, 0 },
    { "freetowns", "Free FM Towns set", 0x65DC3B1F, 0, 1 << GT_MACHINE_MODEL1_2 },
    { "fmtownsux", "UX set", 0x92F3FA67, 1 << GT_MACHINE_UX,
        (1 << GT_MACHINE_1F_2F) | (1 << GT_MACHINE_10F_20F) | (1 << GT_MACHINE_CX) },
    { "fmtmarty2", "Marty 2 set", 0x937311F6, 1 << GT_MACHINE_MARTY, (1 << GT_MACHINE_UG) | (1 << GT_MACHINE_HG) },
    { "fmtownsftv", "V10 set", 0xAFE4EBCF, 1 << GT_MACHINE_MF_FRESH, (1 << GT_MACHINE_UG) | (1 << GT_MACHINE_HG) }
};

#include "firmware_inline.h"

#endif /* FIRMWARE_H */
