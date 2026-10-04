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

#ifndef UPD71071_INLINE_H
#define UPD71071_INLINE_H

#include "upd71071.h"

// Only I/O to memory and memory to I/O in demand or single service exist on the Towns board
INLINE bool UPD71071::IsSupportedMode(u8 mode) const
{
    u8 direction = (mode >> 2) & 0x03;
    u8 service = (mode >> 6) & 0x03;

    return (direction == k_upd71071_io_to_memory || direction == k_upd71071_memory_to_io) &&
        (service == k_upd71071_demand || service == k_upd71071_single);
}

INLINE UPD71071::UPD71071_State* UPD71071::GetState()
{
    return &m_state;
}

#endif /* UPD71071_INLINE_H */
