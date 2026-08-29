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

INLINE bool Firmware::IsReady() const
{
    return m_ready;
}

INLINE bool Firmware::IsComplete() const
{
    for (int i = 0; i < GT_FIRMWARE_COUNT; i++)
    {
        if (!m_info[i].loaded)
            return false;
    }

    return true;
}

INLINE const char* Firmware::GetDirectory() const
{
    return m_directory;
}

INLINE const GT_Firmware_Info& Firmware::GetInfo(GT_Firmware_Type type) const
{
    return m_info[type];
}

INLINE const u8* Firmware::GetSystemRom() const
{
    return m_system_rom;
}

INLINE const u8* Firmware::GetOsRom() const
{
    return m_os_rom;
}

INLINE const u8* Firmware::GetFontRom() const
{
    return m_font_rom;
}

INLINE const u8* Firmware::GetDictionaryRom() const
{
    return m_dictionary_rom;
}

INLINE const u8* Firmware::GetFont20Rom() const
{
    return m_font20_rom;
}
