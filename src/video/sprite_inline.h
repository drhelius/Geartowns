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
    if (m_state.busy && clocks > m_state.start_clocks)
        Run(clocks);
}

INLINE bool Sprite::IsEnabled() const
{
    return (m_state.registers[k_sprite_control1] & 0x80) != 0;
}

INLINE bool Sprite::IsBusy() const
{
    return m_state.busy;
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

INLINE u16 Sprite::ReadWord(u32 offset) const
{
    return (u16)(m_sprite_ram[offset] | (m_sprite_ram[offset + 1] << 8));
}

INLINE u8* Sprite::GetWorkHalf() const
{
    return m_vram + k_sprite_work_base + (m_state.page ? k_sprite_half_size : 0);
}

#endif /* SPRITE_INLINE_H */
