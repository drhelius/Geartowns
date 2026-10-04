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

#include <math.h>
#include "audio.h"
#include "ym3438.h"
#include "rf5c68.h"
#include "../common/state_serializer.h"

// Data Book 5.1 puts an approximately 4 kHz reconstruction filter after the PCM DACs
static const float k_audio_pcm_lowpass_cutoff = 4000.0f;

Audio::Audio()
{
    InitPointer(m_ym3438);
    InitPointer(m_rf5c68);
    m_mute = false;
    m_master_volume = 1.0f;
    m_fm_volume = 1.0f;
    m_pcm_volume = 1.0f;
    SetPCMLowpassCutoff(k_audio_pcm_lowpass_cutoff);
}

Audio::~Audio()
{
    SafeDelete(m_rf5c68);
    SafeDelete(m_ym3438);
}

void Audio::Init()
{
    if (!IsValidPointer(m_ym3438))
        m_ym3438 = new YM3438();

    if (!IsValidPointer(m_rf5c68))
        m_rf5c68 = new RF5C68();

    m_ym3438->Init();
    m_rf5c68->Init();
    Reset();
}

void Audio::Reset()
{
    m_ym3438->Reset();
    m_rf5c68->Reset();

    m_sound_clock_remainder = 0;
    m_sample_clock_counter = 0;
    m_pcm_lowpass_left = 0;
    m_pcm_lowpass_right = 0;
    m_buffer_index = 0;
    m_buffer_overflow = false;
}

void Audio::SetPCMLowpassCutoff(float cutoff)
{
    // One pole: alpha = 1 - exp(-2 * pi * fc / fs), stored as Q1.15
    float alpha = 1.0f - expf(-2.0f * 3.14159265358979323846f * cutoff / (float)GT_AUDIO_SAMPLE_RATE);
    alpha = CLAMP(alpha, 0.0f, 0.9999f);
    m_pcm_lowpass_alpha_q15 = (u16)(alpha * 32768.0f + 0.5f);
}

void Audio::EndFrame(s16* sample_buffer, int* sample_count)
{
    int samples = m_buffer_index;

    m_buffer_index = 0;
    m_buffer_overflow = false;

    if (!IsValidPointer(sample_buffer) || !IsValidPointer(sample_count))
    {
        if (IsValidPointer(sample_count))
            *sample_count = 0;

        return;
    }

    *sample_count = samples;

    if (m_mute || (m_master_volume <= 0.0f))
    {
        memset(sample_buffer, 0, sizeof(s16) * samples);
        return;
    }

    // Relative FM/PCM levels are unmeasured: 
    // FM enters at its full DAC sum and PCM at half scale
    if ((m_master_volume == 1.0f) && (m_fm_volume == 1.0f) && (m_pcm_volume == 1.0f))
    {
        for (int i = 0; i < samples; i++)
        {
            s32 mix = (s32)m_fm_buffer[i] + (m_pcm_buffer[i] >> 1);
            sample_buffer[i] = (s16)CLAMP(mix, -32768, 32767);
        }
    }
    else
    {
        for (int i = 0; i < samples; i++)
        {
            float mix = (float)m_fm_buffer[i] * m_fm_volume + (float)(m_pcm_buffer[i] >> 1) * m_pcm_volume;
            s32 out = (s32)(mix * m_master_volume);
            sample_buffer[i] = (s16)CLAMP(out, -32768, 32767);
        }
    }
}

void Audio::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    m_ym3438->SaveState(stream);
    m_rf5c68->SaveState(stream);
}

void Audio::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);

    m_sound_clock_remainder %= k_audio_cpu_clocks_per_sound_clock;
    m_sample_clock_counter %= GT_CPU_CLOCK_RATE;
    m_pcm_lowpass_left = CLAMP(m_pcm_lowpass_left, -32768, 32767);
    m_pcm_lowpass_right = CLAMP(m_pcm_lowpass_right, -32768, 32767);

    m_ym3438->LoadState(stream);
    m_rf5c68->LoadState(stream);
}

void Audio::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_sound_clock_remainder);
    G_SERIALIZE(serializer, m_sample_clock_counter);
    G_SERIALIZE(serializer, m_pcm_lowpass_left);
    G_SERIALIZE(serializer, m_pcm_lowpass_right);
}
