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

#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <cctype>
#include <string>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#if !defined(__LIBRETRO__)
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#endif
#include "defines.h"
#include "types.h"
#include "log.h"
#include "bit_ops.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>
#undef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "byte_order.h"
#include "string_utils.h"
#include "path_utils.h"
#include "file_utils.h"

inline bool checked_add_u32(u32 value, u32 addend, u32* result)
{
    if (value > 0xFFFFFFFFU - addend)
        return false;

    *result = value + addend;
    return true;
}

inline bool checked_add_u64(u64 value, u64 addend, u64* result)
{
    if (value > UINT64_MAX - addend)
        return false;

    *result = value + addend;
    return true;
}

inline bool checked_multiply_u64(u64 value, u64 multiplier, u64* result)
{
    if ((value != 0) && (multiplier > UINT64_MAX / value))
        return false;

    *result = value * multiplier;
    return true;
}

inline bool get_host_time(time_t timestamp, bool utc, struct tm* time_info)
{
#if defined(_WIN32)
    return (utc ? gmtime_s(time_info, &timestamp) : localtime_s(time_info, &timestamp)) == 0;
#else
    return IsValidPointer(utc ? gmtime_r(&timestamp, time_info) : localtime_r(&timestamp, time_info));
#endif
}

inline u8 hi(u16 a)
{
    return (u8)(a >> 8);
}

inline u8 lo(u16 a)
{
    return (u8)(a & 0xFF);
}

inline u32 pow_2_ceil(u32 n)
{
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    ++n;
    return n;
}

inline bool is_pow2(u32 x)
{
    return x && ((x & (x - 1)) == 0);
}

INLINE u8 dec_to_bcd(u8 value)
{
    return (u8)(((value / 10) << 4) | (value % 10));
}

INLINE u8 bcd_to_dec(u8 value)
{
    return (u8)((((value >> 4) & 0x0F) * 10) + (value & 0x0F));
}

INLINE u64 div_ceil_u64(u64 value, u64 divisor)
{
    return (value + divisor - 1) / divisor;
}

#endif /* COMMON_H */
