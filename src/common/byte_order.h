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

#ifndef BYTE_ORDER_H
#define BYTE_ORDER_H

#include "defines.h"
#include "types.h"

INLINE u16 read_u16_le(const u8* p)
{
    return (u16)p[0] | ((u16)p[1] << 8);
}

INLINE u32 read_u32_le(const u8* p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

INLINE u64 read_u64_le(const u8* p)
{
    return (u64)read_u32_le(p) | ((u64)read_u32_le(p + 4) << 32);
}

INLINE void write_u16_le(u8* p, u16 value)
{
    p[0] = (u8)(value >> 0);
    p[1] = (u8)(value >> 8);
}

INLINE void write_u32_le(u8* p, u32 value)
{
    p[0] = (u8)(value >> 0);
    p[1] = (u8)(value >> 8);
    p[2] = (u8)(value >> 16);
    p[3] = (u8)(value >> 24);
}

INLINE void write_u64_le(u8* p, u64 value)
{
    write_u32_le(p, (u32)value);
    write_u32_le(p + 4, (u32)(value >> 32));
}

INLINE u16 read_u16_be(const u8* p)
{
    return (u16)p[1] | ((u16)p[0] << 8);
}

INLINE u32 read_u32_be(const u8* p)
{
    return (u32)p[3] | ((u32)p[2] << 8) | ((u32)p[1] << 16) | ((u32)p[0] << 24);
}

INLINE void write_u16_be(u8* p, u16 value)
{
    p[0] = (u8)(value >> 8);
    p[1] = (u8)value;
}

INLINE void write_u32_be(u8* p, u32 value)
{
    p[0] = (u8)(value >> 24);
    p[1] = (u8)(value >> 16);
    p[2] = (u8)(value >> 8);
    p[3] = (u8)value;
}

#endif /* BYTE_ORDER_H */
