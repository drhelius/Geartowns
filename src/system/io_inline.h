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

#ifndef IO_INLINE_H
#define IO_INLINE_H

#include "io.h"

INLINE u16 IO::Read16(u16 port, GT_Bus_Access_Context& context)
{
    u16 value = Read8(port, context);
    value |= (u16)Read8((u16)(port + 1), context) << 8;
    return value;
}

INLINE u32 IO::Read32(u16 port, GT_Bus_Access_Context& context)
{
    u32 value = Read16(port, context);
    value |= (u32)Read16((u16)(port + 2), context) << 16;
    return value;
}

INLINE void IO::Write16(u16 port, u16 value, GT_Bus_Access_Context& context)
{
    Write8(port, (u8)value, context);
    Write8((u16)(port + 1), (u8)(value >> 8), context);
}

INLINE void IO::Write32(u16 port, u32 value, GT_Bus_Access_Context& context)
{
    Write16(port, (u16)value, context);
    Write16((u16)(port + 2), (u16)(value >> 16), context);
}

#endif /* IO_INLINE_H */
