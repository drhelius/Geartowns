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

#ifndef AUDIO_INLINE_H
#define AUDIO_INLINE_H

#include "audio.h"
#include "ym3438.h"
#include "rf5c68.h"

// FM and PCM both run from the 8 MHz sound clock, half the CPU clock
static const u32 k_audio_cpu_clocks_per_sound_clock = GT_CPU_CLOCK_RATE / GT_SOUND_CLOCK_RATE;

INLINE void Audio::Mute(bool mute)
{
    m_mute = mute;
}

INLINE void Audio::SetMasterVolume(float volume)
{
    m_master_volume = volume;
}

INLINE void Audio::SetFMVolume(float volume)
{
    m_fm_volume = volume;
}

INLINE void Audio::SetPCMVolume(float volume)
{
    m_pcm_volume = volume;
}

INLINE YM3438* Audio::GetYM3438()
{
    return m_ym3438;
}

INLINE RF5C68* Audio::GetRF5C68()
{
    return m_rf5c68;
}

INLINE void Audio::Clock(u32 clocks)
{
    if (clocks == 0)
        return;

    u64 sample_clock_counter = m_sample_clock_counter + (u64)clocks * GT_AUDIO_SAMPLE_RATE;

    if (sample_clock_counter < GT_CPU_CLOCK_RATE)
    {
        ClockSources(clocks);
        m_sample_clock_counter = sample_clock_counter;
        return;
    }

    while (clocks > 0)
    {
        u32 step = clocks;
        u64 remaining = GT_CPU_CLOCK_RATE - m_sample_clock_counter;
        u64 clocks_to_sample = (remaining + GT_AUDIO_SAMPLE_RATE - 1) / GT_AUDIO_SAMPLE_RATE;

        if ((clocks_to_sample > 0) && (clocks_to_sample < step))
            step = (u32)clocks_to_sample;

        ClockSources(step);

        m_sample_clock_counter += (u64)step * GT_AUDIO_SAMPLE_RATE;
        clocks -= step;

        while (m_sample_clock_counter >= GT_CPU_CLOCK_RATE)
        {
            m_sample_clock_counter -= GT_CPU_CLOCK_RATE;
            SampleSources();
        }
    }
}

INLINE void Audio::ClockSources(u32 clocks)
{
    u32 total_clocks = m_sound_clock_remainder + clocks;
    u32 sound_clocks = total_clocks / k_audio_cpu_clocks_per_sound_clock;
    m_sound_clock_remainder = total_clocks % k_audio_cpu_clocks_per_sound_clock;

    m_ym3438->Clock(sound_clocks);
    m_rf5c68->Clock(sound_clocks);
}

INLINE void Audio::SampleSources()
{
    s16 fm_left = 0;
    s16 fm_right = 0;
    s16 pcm_left = 0;
    s16 pcm_right = 0;

    m_ym3438->Sample(fm_left, fm_right);
    m_rf5c68->Sample(pcm_left, pcm_right);

    // The PCM DAC output passes a smoothing filter of about 4 kHz before the mixer
    //FM feeds it directly
    m_pcm_lowpass_left += ((s32)m_pcm_lowpass_alpha_q15 * (pcm_left - m_pcm_lowpass_left)) >> 15;
    m_pcm_lowpass_right += ((s32)m_pcm_lowpass_alpha_q15 * (pcm_right - m_pcm_lowpass_right)) >> 15;

    // Keep the samples already buffered for this frame and drop the excess
    if (m_buffer_index + 1 >= GT_AUDIO_BUFFER_SIZE)
    {
        if (!m_buffer_overflow)
            Error("Audio buffer overflow");

        m_buffer_overflow = true;
        return;
    }

    m_fm_buffer[m_buffer_index + 0] = fm_left;
    m_fm_buffer[m_buffer_index + 1] = fm_right;
    m_pcm_buffer[m_buffer_index + 0] = (s16)m_pcm_lowpass_left;
    m_pcm_buffer[m_buffer_index + 1] = (s16)m_pcm_lowpass_right;
    m_buffer_index += 2;
}

#endif /* AUDIO_INLINE_H */
