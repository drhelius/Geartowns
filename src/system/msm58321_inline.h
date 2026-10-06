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

#ifndef MSM58321_INLINE_H
#define MSM58321_INLINE_H

#include "msm58321.h"

// Nothing reads the clock between accesses, so it catches up one second at a time when the CPU looks
INLINE void MSM58321::Synchronize(u64 clocks)
{
    while (clocks >= m_state.update_clocks)
    {
        AdvanceSecond();
        m_state.update_clocks += k_msm58321_second_clocks;
    }
}

// Tens digits share their nibble with the 24 hour, PM and leap phase flags
INLINE int MSM58321::GetValue(int ones, u8 tens_mask) const
{
    return (m_state.registers[ones + 1] & tens_mask) * 10 + m_state.registers[ones];
}

INLINE void MSM58321::SetValue(int ones, u8 tens_mask, int value)
{
    m_state.registers[ones] = (u8)(value % 10);
    m_state.registers[ones + 1] = (u8)((m_state.registers[ones + 1] & ~tens_mask) | (value / 10));
}

INLINE MSM58321::MSM58321_State* MSM58321::GetState()
{
    return &m_state;
}

#endif /* MSM58321_INLINE_H */
