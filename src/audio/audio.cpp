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
#include "../cdrom/cdrom_audio.h"
#include "../common/state_serializer.h"

// Data Book 5.1 puts an approximately 4 kHz reconstruction filter after the PCM DACs
static const float k_audio_pcm_lowpass_cutoff = 4000.0f;

// MB87078 attenuation in Q15: round(32768 * 10^((data - 63) * 0.5 / 20))
static const s32 k_audio_volume_gain[64] =
{
/* 0x00 */   872,   924,   978,  1036,  1098,  1163,  1232,  1305,
/* 0x08 */  1382,  1464,  1550,  1642,  1740,  1843,  1952,  2068,
/* 0x10 */  2190,  2320,  2457,  2603,  2757,  2920,  3093,  3277,
/* 0x18 */  3471,  3677,  3894,  4125,  4370,  4629,  4903,  5193,
/* 0x20 */  5501,  5827,  6172,  6538,  6925,  7336,  7771,  8231,
/* 0x28 */  8719,  9235,  9783, 10362, 10976, 11627, 12315, 13045,
/* 0x30 */ 13818, 14637, 15504, 16423, 17396, 18427, 19519, 20675,
/* 0x38 */ 21900, 23198, 24573, 26029, 27571, 29205, 30935, 32768
};

static const s32 k_audio_volume_gain_minus_32db = 823;
static const u8 k_audio_volume_enable = 0x04;
static const u8 k_audio_volume_0db = 0x08;
static const u8 k_audio_volume_minus_32db = 0x10;
static const int k_audio_volume_cdda = 1;
static const int k_audio_volume_cdda_left = 0;
static const int k_audio_volume_cdda_right = 1;

Audio::Audio()
{
    InitPointer(m_ym3438);
    InitPointer(m_rf5c68);
    InitPointer(m_cdrom_audio);
    InitPointer(m_scheduler);
    m_mute = false;
    m_master_volume = 1.0f;
    m_fm_volume = 1.0f;
    m_pcm_volume = 1.0f;
    m_cdda_volume = 1.0f;
    m_cdda_gain_left = 0;
    m_cdda_gain_right = 0;
    SetPCMLowpassCutoff(k_audio_pcm_lowpass_cutoff);
}

Audio::~Audio()
{
    SafeDelete(m_rf5c68);
    SafeDelete(m_ym3438);
}

void Audio::Init(Scheduler* scheduler, CdRomAudio* cdrom_audio)
{
    m_scheduler = scheduler;
    m_cdrom_audio = cdrom_audio;

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

    m_state.sound_clock_remainder = 0;
    m_state.sample_clock_counter = 0;
    m_state.pcm_lowpass_left = 0;
    m_state.pcm_lowpass_right = 0;
    m_state.clocks = m_scheduler->GetClocks();

    // Both MB87078 start with every channel enabled at 0 dB
    for (int chip = 0; chip < AUDIO_VOLUME_CHIPS; chip++)
    {
        m_state.volume_channel[chip] = 0;

        for (int channel = 0; channel < AUDIO_VOLUME_CHANNELS; channel++)
        {
            m_state.volume_data[chip][channel] = 0x3F;
            m_state.volume_control[chip][channel] = k_audio_volume_enable;
        }
    }

    UpdateCDDAGain();
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

// Volume 1 at 04E0h attenuates line in
// Volume 2 at 04E2h attenuates the CD-DA left and right channels, the mic and the modem
// COM selects the channel that DATA writes
// EN, C0 and C32 are in bits 2-4 as on the chip pins
u8 Audio::ReadVolume(u16 port) const
{
    int chip = (port >> 1) & 0x01;
    int channel = m_state.volume_channel[chip];

    if ((port & 0x01) == 0)
        return m_state.volume_data[chip][channel];

    return (u8)(channel | m_state.volume_control[chip][channel]);
}

void Audio::WriteVolume(u16 port, u8 value)
{
    int chip = (port >> 1) & 0x01;

    if ((port & 0x01) == 0)
        m_state.volume_data[chip][m_state.volume_channel[chip]] = value & 0x3F;
    else
    {
        m_state.volume_channel[chip] = value & 0x03;
        m_state.volume_control[chip][value & 0x03] = value & 0x1C;
    }

    UpdateCDDAGain();
}

void Audio::UpdateCDDAGain()
{
    m_cdda_gain_left = GetVolumeGain(k_audio_volume_cdda, k_audio_volume_cdda_left);
    m_cdda_gain_right = GetVolumeGain(k_audio_volume_cdda, k_audio_volume_cdda_right);
}

// Per the MB87078 truth table EN = 0 mutes and C32 forces -32 dB even with C0 set
s32 Audio::GetVolumeGain(int chip, int channel) const
{
    u8 control = m_state.volume_control[chip][channel];

    if ((control & k_audio_volume_enable) == 0)
        return 0;

    if ((control & k_audio_volume_minus_32db) != 0)
        return k_audio_volume_gain_minus_32db;

    if ((control & k_audio_volume_0db) != 0)
        return k_audio_volume_gain[0x3F];

    return k_audio_volume_gain[m_state.volume_data[chip][channel]];
}

// The CPU window shows the 4 KiB wave RAM bank selected by the PCM control register
// Playback never writes wave RAM, so only writes need the chip caught up
u8 Audio::ReadWaveWindowCallback(void* device, u32 offset)
{
    Audio* audio = (Audio*)device;
    return audio->m_rf5c68->Read((u16)(0x1000 | (offset & 0x0FFF)));
}

void Audio::WriteWaveWindowCallback(void* device, u32 offset, u8 value)
{
    Audio* audio = (Audio*)device;
    audio->Synchronize(audio->m_scheduler->GetClocks());
    audio->m_rf5c68->Write((u16)(0x1000 | (offset & 0x0FFF)), value);
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

    // Relative FM/PCM/CD-DA levels are unmeasured: 
    // FM enters at its full DAC sum, PCM at half scale and CD-DA at full scale
    if ((m_master_volume == 1.0f) && (m_fm_volume == 1.0f) && (m_pcm_volume == 1.0f) && (m_cdda_volume == 1.0f))
    {
        for (int i = 0; i < samples; i++)
        {
            s32 mix = (s32)m_fm_buffer[i] + (m_pcm_buffer[i] >> 1) + m_cdda_buffer[i];
            sample_buffer[i] = (s16)CLAMP(mix, -32768, 32767);
        }
    }
    else
    {
        for (int i = 0; i < samples; i++)
        {
            float mix = (float)m_fm_buffer[i] * m_fm_volume + (float)(m_pcm_buffer[i] >> 1) * m_pcm_volume +
                (float)m_cdda_buffer[i] * m_cdda_volume;
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
    m_ym3438->LoadState(stream);
    m_rf5c68->LoadState(stream);
    SanitizeState();
}

void Audio::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.sound_clock_remainder);
    G_SERIALIZE(serializer, m_state.sample_clock_counter);
    G_SERIALIZE(serializer, m_state.pcm_lowpass_left);
    G_SERIALIZE(serializer, m_state.pcm_lowpass_right);
    G_SERIALIZE(serializer, m_state.clocks);
    G_SERIALIZE_ARRAY(serializer, m_state.volume_channel, AUDIO_VOLUME_CHIPS);
    G_SERIALIZE_ARRAY(serializer, &m_state.volume_data[0][0], AUDIO_VOLUME_CHIPS * AUDIO_VOLUME_CHANNELS);
    G_SERIALIZE_ARRAY(serializer, &m_state.volume_control[0][0], AUDIO_VOLUME_CHIPS * AUDIO_VOLUME_CHANNELS);
}

void Audio::SanitizeState()
{
    m_state.sound_clock_remainder %= k_audio_cpu_clocks_per_sound_clock;
    m_state.sample_clock_counter %= GT_CPU_CLOCK_RATE;
    m_state.pcm_lowpass_left = CLAMP(m_state.pcm_lowpass_left, -32768, 32767);
    m_state.pcm_lowpass_right = CLAMP(m_state.pcm_lowpass_right, -32768, 32767);

    for (int chip = 0; chip < AUDIO_VOLUME_CHIPS; chip++)
    {
        m_state.volume_channel[chip] &= 0x03;

        for (int channel = 0; channel < AUDIO_VOLUME_CHANNELS; channel++)
        {
            m_state.volume_data[chip][channel] &= 0x3F;
            m_state.volume_control[chip][channel] &= 0x1C;
        }
    }

    UpdateCDDAGain();
}
