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

#ifndef SPRITE_INLINE_H
#define SPRITE_INLINE_H

#include "sprite.h"

INLINE void Sprite::Synchronize(u64 clocks)
{
    if (!m_state.busy)
        return;

    ApplyContention();

    if (clocks > m_state.start_clocks)
        Run(clocks);
}

// The CPU's cycles to VRAM and sprite RAM since the last look hold the engine back, a write longer than a read
INLINE void Sprite::ApplyContention()
{
    if (!IsValidPointer(m_bus_cycle_counts))
        return;

    u32 reads = GetBusCycles(0);
    u32 writes = GetBusCycles(1);
    u64 delay = (u64)(writes - m_bus_cycles_seen[1]) * m_timing->sprite_write_contention +
        (u64)(reads - m_bus_cycles_seen[0]) * m_timing->sprite_read_contention + m_state.contention_fraction;

    m_bus_cycles_seen[0] = reads;
    m_bus_cycles_seen[1] = writes;
    m_state.start_clocks += delay >> 8;
    m_state.contention_fraction = (u8)(delay & 0xFF);
}

// In 1/256 machine clocks
INLINE void Sprite::Delay(u32 amount)
{
    if (!m_state.busy)
        return;

    u32 total = amount + m_state.contention_fraction;
    m_state.start_clocks += total >> 8;
    m_state.contention_fraction = (u8)(total & 0xFF);
}

INLINE void Sprite::ResetContention()
{
    m_state.contention_fraction = 0;

    if (!IsValidPointer(m_bus_cycle_counts))
        return;

    m_bus_cycles_seen[0] = GetBusCycles(0);
    m_bus_cycles_seen[1] = GetBusCycles(1);
}

INLINE u32 Sprite::GetBusCycles(int write) const
{
    return m_bus_cycle_counts[GT_BUS_VRAM * 2 + write] + m_bus_cycle_counts[GT_BUS_SPRITE * 2 + write];
}

INLINE bool Sprite::IsEnabled() const
{
    return (m_state.registers[k_sprite_control1] & 0x80) != 0;
}

INLINE bool Sprite::IsBusy() const
{
    return m_state.busy;
}

// The first clock at which Run clears busy
INLINE u64 Sprite::GetFinishClocks() const
{
    u64 half_clocks = m_state.clear_half_clocks +
        (u64)(k_sprite_entries - m_state.first_index) * m_state.entry_half_clocks;
    return m_state.start_clocks + (half_clocks + 1) / 2;
}

INLINE bool Sprite::GetPage() const
{
    return m_state.page;
}

// Layer 1 shows the half not being drawn, or the half picked by DP1 while SPEN is clear
INLINE u32 Sprite::GetDisplayOffset() const
{
    bool second = IsEnabled() ? !m_state.page : (m_state.registers[k_sprite_display_page] & 0x80) != 0;
    return second ? k_sprite_half_size : 0;
}

INLINE Sprite::Sprite_State* Sprite::GetState()
{
    return &m_state;
}

INLINE u16 Sprite::GetFirstIndex() const
{
    return (u16)(((m_state.registers[k_sprite_control1] & 0x03) << 8) | m_state.registers[k_sprite_control0]);
}

INLINE u32 Sprite::GetOffsetX() const
{
    return ((m_state.registers[k_sprite_offset_x + 1] & 0x01) << 8) | m_state.registers[k_sprite_offset_x];
}

INLINE u32 Sprite::GetOffsetY() const
{
    return ((m_state.registers[k_sprite_offset_y + 1] & 0x01) << 8) | m_state.registers[k_sprite_offset_y];
}

INLINE u32 Sprite::GetWorkOffset() const
{
    return k_sprite_work_base + (m_state.page ? k_sprite_half_size : 0);
}

INLINE u8* Sprite::GetWorkHalf() const
{
    return m_vram + GetWorkOffset();
}

#endif /* SPRITE_INLINE_H */
