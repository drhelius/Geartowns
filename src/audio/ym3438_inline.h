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
    m_state.elapsed_cycles += cycles;
}

INLINE bool YM3438::IsIRQAsserted()
{
    // Flags are only cleared by register writes, which synchronize first
    if (m_state.timer_a_flag || m_state.timer_b_flag)
        return true;

    // No catch-up is needed until the next enabled timer can overflow
    if (m_state.elapsed_cycles < GetCyclesToTimerFlag())
        return false;

    Synchronize();
    return m_state.timer_a_flag || m_state.timer_b_flag;
}

INLINE YM3438::YM3438_State* YM3438::GetState()
{
    return &m_state;
}

INLINE u64 YM3438::GetCyclesToTimerFlag() const
{
    u32 samples = 0xFFFFFFFF;

    if (m_state.timer_a_load && m_state.timer_a_enable)
        samples = 1024 - m_state.timer_a_counter;

    if (m_state.timer_b_load && m_state.timer_b_enable)
    {
        u32 timer_b_samples = (16 - m_state.timer_b_prescaler) + ((255 - m_state.timer_b_counter) << 4);
        samples = MIN(samples, timer_b_samples);
    }

    if (samples == 0xFFFFFFFF)
        return 0xFFFFFFFFFFFFFFFFULL;

    return ((u64)(samples - 1) * k_ym3438_native_sample_cycles) +
        (k_ym3438_native_sample_cycles - m_state.native_cycle);
}

INLINE u16 YM3438::GetSelectedAddress() const
{
    return m_state.address;
}

INLINE u8 YM3438::GetRegister(u16 address) const
{
    return m_state.registers[(address >> 8) & 0x01][address & 0xFF];
}

INLINE bool YM3438::IsChannelMuted(int channel) const
{
    return m_channel_mute[channel];
}

#endif /* YM3438_INLINE_H */
