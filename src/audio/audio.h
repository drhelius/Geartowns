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

#ifndef AUDIO_H
#define AUDIO_H

#include <iostream>
#include "../common/common.h"
#include "ym3438.h"
#include "rf5c68.h"

#define AUDIO_VOLUME_CHIPS 2
#define AUDIO_VOLUME_CHANNELS 4

class CdRomAudio;
class Scheduler;
class StateSerializer;

class Audio
{
public:
    enum Audio_Source
    {
        AUDIO_SOURCE_FM = 0,
        AUDIO_SOURCE_PCM,
        AUDIO_SOURCE_CDDA,
        AUDIO_SOURCE_COUNT
    };

    struct Audio_State
    {
        u32 sound_clock_remainder;
        u64 sample_clock_counter;
        s32 pcm_lowpass_left;
        s32 pcm_lowpass_right;
        u64 clocks;
        u8 volume_channel[AUDIO_VOLUME_CHIPS];
        u8 volume_data[AUDIO_VOLUME_CHIPS][AUDIO_VOLUME_CHANNELS];
        u8 volume_control[AUDIO_VOLUME_CHIPS][AUDIO_VOLUME_CHANNELS];
    };

public:
    Audio();
    ~Audio();
    void Init(Scheduler* scheduler, CdRomAudio* cdrom_audio);
    void Reset();
    void Mute(bool mute);
    void SetMasterVolume(float volume);
    void SetFMVolume(float volume);
    void SetPCMVolume(float volume);
    void SetCDDAVolume(float volume);
    void SetPCMLowpassCutoff(float cutoff);
    void Synchronize(u64 clocks);
    u8 ReadVolume(u16 port) const;
    void WriteVolume(u16 port, u8 value);
    void EndFrame(s16* sample_buffer, int* sample_count);
    YM3438* GetYM3438();
    RF5C68* GetRF5C68();
    Audio_State* GetState();
    void SetSourceMute(int source, bool mute);
    bool IsSourceMuted(int source) const;
    void EnableChannelScopes(bool enable);
    int GetFrameSamples() const;
    const s16* GetSourceBuffer(int source) const;
    const s16* GetFMChannelBuffer(int channel) const;
    const s16* GetPCMChannelBuffer(int channel) const;
    s32 GetVolumeGain(int chip, int channel) const;
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

    static u8 ReadWaveWindowCallback(void* device, u32 offset);
    static u8 PeekWaveWindowCallback(void* device, u32 offset);
    static void WriteWaveWindowCallback(void* device, u32 offset, u8 value);

private:
    void Clock(u32 clocks);
    void ClockSources(u32 clocks);
    void SampleSources();
    void CaptureChannels(int index);
    void UpdateCDDAGain();
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    YM3438* m_ym3438;
    RF5C68* m_rf5c68;
    CdRomAudio* m_cdrom_audio;
    Scheduler* m_scheduler;
    bool m_mute;
    float m_master_volume;
    float m_fm_volume;
    float m_pcm_volume;
    float m_cdda_volume;
    Audio_State m_state;
    u16 m_pcm_lowpass_alpha_q15;
    s32 m_cdda_gain_left;
    s32 m_cdda_gain_right;
    s16 m_fm_buffer[GT_AUDIO_BUFFER_SIZE];
    s16 m_pcm_buffer[GT_AUDIO_BUFFER_SIZE];
    s16 m_cdda_buffer[GT_AUDIO_BUFFER_SIZE];
    int m_buffer_index;
    bool m_buffer_overflow;
    int m_frame_samples;
    bool m_source_mute[AUDIO_SOURCE_COUNT];
    bool m_channel_scopes;
    s16 m_fm_channel_buffer[YM3438_CHANNEL_COUNT][GT_AUDIO_BUFFER_SIZE / 2];
    s16 m_pcm_channel_buffer[RF5C68_CHANNEL_COUNT][GT_AUDIO_BUFFER_SIZE / 2];
};

#include "audio_inline.h"

#endif /* AUDIO_H */
