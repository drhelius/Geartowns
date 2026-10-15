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
#include "../cdrom/cdrom_audio.h"
#include "../system/pit.h"
#include "../system/scheduler.h"

// FM and PCM both run from the 8 MHz sound clock, half the machine clock
static const u32 k_audio_clocks_per_sound_clock = GT_MACHINE_CLOCK_RATE / GT_SOUND_CLOCK_RATE;

// 307.2 kHz PIT clocks in one 44.1 kHz output sample, rounded
static const int k_audio_buzzer_ticks = 7;
static const s32 k_audio_buzzer_level = 4096;

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

INLINE void Audio::SetCDDAVolume(float volume)
{
    m_cdda_volume = volume;
}

INLINE void Audio::SetBuzzerVolume(float volume)
{
    m_buzzer_volume = volume;
}

INLINE YM3438* Audio::GetYM3438()
{
    return m_ym3438;
}

INLINE RF5C68* Audio::GetRF5C68()
{
    return m_rf5c68;
}

INLINE Audio::Audio_State* Audio::GetState()
{
    return &m_state;
}

INLINE void Audio::SetSourceMute(int source, bool mute)
{
    m_source_mute[source] = mute;
}

INLINE bool Audio::IsSourceMuted(int source) const
{
    return m_source_mute[source];
}

INLINE void Audio::EnableChannelScopes(bool enable)
{
    m_channel_scopes = enable;
}

// Stereo values in the last completed frame
INLINE int Audio::GetFrameSamples() const
{
    return m_frame_samples;
}

INLINE const s16* Audio::GetSourceBuffer(int source) const
{
    switch (source)
    {
        case AUDIO_SOURCE_FM:
            return m_fm_buffer;
        case AUDIO_SOURCE_PCM:
            return m_pcm_buffer;
        case AUDIO_SOURCE_CDDA:
            return m_cdda_buffer;
        default:
            return m_buzzer_buffer;
    }
}

INLINE const s16* Audio::GetFMChannelBuffer(int channel) const
{
    return m_fm_channel_buffer[channel];
}

INLINE const s16* Audio::GetPCMChannelBuffer(int channel) const
{
    return m_pcm_channel_buffer[channel];
}

INLINE void Audio::Synchronize(u64 clocks)
{
    while (clocks > m_state.clocks)
        Clock((u32)MIN(clocks - m_state.clocks, 0x10000000ULL));
}

INLINE void Audio::HandleEvent(u64 clocks)
{
    Synchronize(clocks);
    UpdatePCMIRQ();
}

INLINE void Audio::Clock(u32 clocks)
{
    if (clocks == 0)
        return;

    u64 sample_clock_counter = m_state.sample_clock_counter + (u64)clocks * GT_AUDIO_SAMPLE_RATE;

    if (sample_clock_counter < GT_MACHINE_CLOCK_RATE)
    {
        ClockSources(clocks);
        m_state.sample_clock_counter = sample_clock_counter;
        m_state.clocks += clocks;
        return;
    }

    while (clocks > 0)
    {
        u32 step = clocks;
        u64 remaining = GT_MACHINE_CLOCK_RATE - m_state.sample_clock_counter;
        u64 clocks_to_sample = (remaining + GT_AUDIO_SAMPLE_RATE - 1) / GT_AUDIO_SAMPLE_RATE;

        if ((clocks_to_sample > 0) && (clocks_to_sample < step))
            step = (u32)clocks_to_sample;

        ClockSources(step);

        m_state.sample_clock_counter += (u64)step * GT_AUDIO_SAMPLE_RATE;
        m_state.clocks += step;
        clocks -= step;

        while (m_state.sample_clock_counter >= GT_MACHINE_CLOCK_RATE)
        {
            m_state.sample_clock_counter -= GT_MACHINE_CLOCK_RATE;
            SampleSources();
        }
    }
}

INLINE void Audio::ClockSources(u32 clocks)
{
    u32 total_clocks = m_state.sound_clock_remainder + clocks;
    u32 sound_clocks = total_clocks / k_audio_clocks_per_sound_clock;
    m_state.sound_clock_remainder = total_clocks % k_audio_clocks_per_sound_clock;

    m_ym3438->Clock(sound_clocks);
    m_rf5c68->Clock(sound_clocks);
}

INLINE void Audio::SampleSources()
{
    s16 fm_left = 0;
    s16 fm_right = 0;
    s16 pcm_left = 0;
    s16 pcm_right = 0;
    s16 cdda_left = 0;
    s16 cdda_right = 0;

    m_ym3438->Sample(fm_left, fm_right);
    m_rf5c68->Sample(pcm_left, pcm_right);

    // CD-DA advances one disc sample per output sample since both run at 44.1 kHz
    m_cdrom_audio->Sample(cdda_left, cdda_right);

    // The PCM DAC output passes a smoothing filter of about 4 kHz before the mixer
    //FM feeds it directly
    m_state.pcm_lowpass_left += ((s32)m_pcm_lowpass_alpha_q15 * (pcm_left - m_state.pcm_lowpass_left)) >> 15;
    m_state.pcm_lowpass_right += ((s32)m_pcm_lowpass_alpha_q15 * (pcm_right - m_state.pcm_lowpass_right)) >> 15;

#ifndef GT_DISABLE_VGMRECORDER
    if (m_vgm_recording_enabled)
        m_vgm_recorder.UpdateTiming();
#endif

    // Keep the samples already buffered for this frame and drop the excess
    if (m_buffer_index + 1 >= GT_AUDIO_BUFFER_SIZE)
    {
        if (!m_buffer_overflow)
            Error("Audio buffer overflow");

        m_buffer_overflow = true;
        return;
    }

    if (m_channel_scopes)
        CaptureChannels(m_buffer_index >> 1);

    s16 buzzer = SampleBuzzer();

    // Muted sources and a closed output reach the mixer as silence
    m_fm_buffer[m_buffer_index + 0] = m_fm_enabled ? fm_left : 0;
    m_fm_buffer[m_buffer_index + 1] = m_fm_enabled ? fm_right : 0;
    m_pcm_buffer[m_buffer_index + 0] = m_pcm_enabled ? (s16)m_state.pcm_lowpass_left : 0;
    m_pcm_buffer[m_buffer_index + 1] = m_pcm_enabled ? (s16)m_state.pcm_lowpass_right : 0;
    m_cdda_buffer[m_buffer_index + 0] = m_cdda_enabled ? (s16)((cdda_left * m_cdda_gain_left) >> 15) : 0;
    m_cdda_buffer[m_buffer_index + 1] = m_cdda_enabled ? (s16)((cdda_right * m_cdda_gain_right) >> 15) : 0;
    m_buzzer_buffer[m_buffer_index + 0] = buzzer;
    m_buzzer_buffer[m_buffer_index + 1] = buzzer;
    m_buffer_index += 2;
}

// The buzzer plays channel 2's OUT, averaged over the PIT clocks before the sample so high tones don't alias
INLINE s16 Audio::SampleBuzzer()
{
    if (!m_pit->IsBuzzerEnabled())
        return 0;

    u64 tick = m_pit->GetTick(k_pit_buzzer_channel, m_state.clocks) - k_audio_buzzer_ticks;
    int high = 0;

    for (int i = 1; i <= k_audio_buzzer_ticks; i++)
        high += m_pit->GetBuzzerOutput(tick + i) ? 1 : 0;

    return (s16)(k_audio_buzzer_level * (2 * high - k_audio_buzzer_ticks) / k_audio_buzzer_ticks);
}

#endif /* AUDIO_INLINE_H */
