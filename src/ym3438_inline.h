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

#ifndef YM3438_INLINE_H
#define YM3438_INLINE_H

INLINE void YM3438::Clock(u32 cycles)
{
    m_elapsed_cycles += cycles;
}

INLINE u32 YM3438::GetClockRate() const
{
    return m_clock_rate;
}

INLINE u32 YM3438::GetSampleRate() const
{
    return m_sample_rate;
}

INLINE u16 YM3438::GetSelectedAddress() const
{
    return m_address;
}

INLINE u8 YM3438::GetRegister(u16 address) const
{
    return m_registers[(address >> 8) & 0x01][address & 0xFF];
}

INLINE int YM3438::GetBufferedSamples() const
{
    return m_buffer_index;
}

#endif /* YM3438_INLINE_H */
