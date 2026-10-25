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

INLINE const u8* Memory::GetCPUPageBus() const
{
    return m_cpu_page_bus;
}

INLINE u32 Memory::GetMapGeneration() const
{
    return m_map_generation;
}

INLINE u32 Memory::TranslateDMAAddress(u32 address) const
{
    return m_dma_map[address >> 20] + (address & 0xFFFFF);
}

INLINE void Memory::InvalidateDebugSnapshot()
{
    m_debug_snapshot_id++;
}

INLINE u8* Memory::GetCMOS()
{
    return m_state.cmos;
}

INLINE u8 Memory::ReadCMOS(u32 index) const
{
    index &= GT_CMOS_SIZE - 1;

    if (m_cmos_high_speed && (index == k_memory_cmos_speed))
        return 0x01;

    return m_state.cmos[index];
}

INLINE void Memory::WriteCMOS(u32 index, u8 value)
{
    index &= GT_CMOS_SIZE - 1;

    if (index == k_memory_cmos_speed)
        m_cmos_high_speed = false;

    m_state.cmos[index] = value;
}

INLINE void Memory::SetCMOSHighSpeed(bool enable)
{
    m_cmos_high_speed = enable;
}

INLINE Memory::Memory_State* Memory::GetState()
{
    return &m_state;
}

#endif /* MEMORY_INLINE_H */
