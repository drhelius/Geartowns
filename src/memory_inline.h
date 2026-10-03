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

#ifndef MEMORY_INLINE_H
#define MEMORY_INLINE_H

#include "memory.h"

INLINE const u8* const* Memory::GetCPUReadPages() const
{
    return m_cpu_read_pages;
}

INLINE u8* const* Memory::GetCPUWritePages() const
{
    return m_cpu_write_pages;
}

INLINE u32 Memory::GetMapGeneration() const
{
    return m_map_generation;
}

INLINE void Memory::InvalidateDebugSnapshot()
{
    m_debug_snapshot_id++;
}

#endif /* MEMORY_INLINE_H */
