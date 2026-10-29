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

#ifndef PCM16_INLINE_H
#define PCM16_INLINE_H

#include "pcm16.h"

INLINE void PCM16::Clock(u32 clocks, u64 time)
{
    if (!IsRunning())
        return;

    m_state.phase += (u64)clocks * k_pcm16_rates[m_state.clock];

    while (m_state.phase >= k_pcm16_period)
    {
        m_state.phase -= k_pcm16_period;
        Tick(time);
    }
}

INLINE void PCM16::Sample(s16& left, s16& right) const
{
    s64 phase = (s64)m_state.phase;
    s64 left_delta = m_state.left_sample - m_state.previous_left_sample;
    s64 right_delta = m_state.right_sample - m_state.previous_right_sample;
    left = (s16)(m_state.previous_left_sample + (left_delta * phase) / (s64)k_pcm16_period);
    right = (s16)(m_state.previous_right_sample + (right_delta * phase) / (s64)k_pcm16_period);
}

INLINE bool PCM16::IsFitted() const
{
    return m_fitted;
}

INLINE bool PCM16::IsRunning() const
{
    return m_fitted && ((m_state.transfer_control & k_pcm16_transfer_buffer_mask) == 0) &&
        (k_pcm16_rates[m_state.clock] != 0);
}

INLINE bool PCM16::IsRecording() const
{
    return (m_state.system_control & k_pcm16_system_record) != 0;
}

INLINE bool PCM16::IsSoftwareTransfer() const
{
    return (m_state.system_control & k_pcm16_system_data_port) != 0;
}

INLINE bool PCM16::IsDMAEnabled() const
{
    return (m_state.transfer_control & (k_pcm16_transfer_dreq_mask | k_pcm16_transfer_buffer_mask)) == 0;
}

INLINE bool PCM16::IsInputIRQArmed() const
{
    return m_fitted && !m_state.input_flag && ((m_state.transfer_control & k_pcm16_transfer_input_irq) != 0) &&
        (GetIRQLine() >= 0);
}

INLINE PCM16::PCM16_Format PCM16::GetFormat() const
{
    if ((m_state.mode & k_pcm16_mode_snd) != 0)
        return PCM16_FORMAT_SND;

    return (PCM16_Format)(((m_state.mode & k_pcm16_mode_16bit) != 0 ? 2 : 0) +
        ((m_state.mode & k_pcm16_mode_stereo) != 0 ? 1 : 0));
}

INLINE u32 PCM16::GetSampleRate() const
{
    return k_pcm16_rates[m_state.clock] / 2;
}

INLINE u8 PCM16::GetIRQCauses() const
{
    u8 causes = 0;

    if (m_state.input_flag && (m_state.transfer_control & k_pcm16_transfer_input_irq) != 0)
        causes |= k_pcm16_cause_input;

    if ((m_state.overrun || m_state.underrun) && (m_state.buffer_control & k_pcm16_buffer_irq) != 0)
        causes |= k_pcm16_cause_buffer;

    if (m_state.terminal_count && (m_state.dma_control & k_pcm16_dma_irq) != 0)
        causes |= k_pcm16_cause_dma;

    return causes;
}

INLINE int PCM16::GetIRQLine() const
{
    int route = m_state.system_control & k_pcm16_system_irq;
    return route < PCM16_IRQ_ROUTES ? k_pcm16_irq_lines[route] : -1;
}

INLINE PCM16::PCM16_State* PCM16::GetState()
{
    return &m_state;
}

#endif /* PCM16_INLINE_H */
