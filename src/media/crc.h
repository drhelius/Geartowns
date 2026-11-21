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

#ifndef CRC_H
#define CRC_H

#include "../common/common.h"

inline u32 CalculateCRC32(u32 crc, const void* data, size_t size)
{
    return (u32)mz_crc32(crc, (const u8*)data, size);
}

inline std::string crc_key(const std::string& text)
{
    char key[9];
    snprintf(key, sizeof(key), "%08X", CalculateCRC32(0, text.data(), text.size()));
    return key;
}

#endif /* CRC_H */
