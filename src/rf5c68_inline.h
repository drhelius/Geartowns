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

#ifndef RF5C68_INLINE_H
#define RF5C68_INLINE_H

#include "rf5c68.h"

INLINE void RF5C68::Clock(u32 cycles)
{
    m_elapsed_cycles += cycles;
}

INLINE const RF5C68::RF5C68_Channel* RF5C68::GetChannels() const
{
    return m_channels;
}

INLINE const u8* RF5C68::GetWaveRAM() const
{
    return m_wave_ram;
}

INLINE u8 RF5C68::GetChannelBank() const
{
    return m_channel_bank;
}

INLINE u8 RF5C68::GetWaveBank() const
{
    return m_wave_bank;
}

INLINE bool RF5C68::IsEnabled() const
{
    return m_enabled;
}

INLINE s16 RF5C68::GetLeftSample() const
{
    return m_left_sample;
}

INLINE s16 RF5C68::GetRightSample() const
{
    return m_right_sample;
}

INLINE int RF5C68::GetFrameSamples() const
{
    return m_frame_samples;
}

#endif /* RF5C68_INLINE_H */
