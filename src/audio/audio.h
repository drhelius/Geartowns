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
#include "common.h"

class YM3438;
class RF5C68;
class StateSerializer;

class Audio
{
public:
    Audio();
    ~Audio();
    void Init();
    void Reset();
    void Mute(bool mute);
    void SetMasterVolume(float volume);
    void SetFMVolume(float volume);
    void SetPCMVolume(float volume);
    void SetPCMLowpassCutoff(float cutoff);
    void Clock(u32 clocks);
    void EndFrame(s16* sample_buffer, int* sample_count);
    YM3438* GetYM3438();
    RF5C68* GetRF5C68();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void ClockSources(u32 clocks);
    void SampleSources();
    void Serialize(StateSerializer& serializer);

private:
    YM3438* m_ym3438;
    RF5C68* m_rf5c68;
    bool m_mute;
    float m_master_volume;
    float m_fm_volume;
    float m_pcm_volume;
    u32 m_sound_clock_remainder;
    u64 m_sample_clock_counter;
    u16 m_pcm_lowpass_alpha_q15;
    s32 m_pcm_lowpass_left;
    s32 m_pcm_lowpass_right;
    s16 m_fm_buffer[GT_AUDIO_BUFFER_SIZE];
    s16 m_pcm_buffer[GT_AUDIO_BUFFER_SIZE];
    int m_buffer_index;
    bool m_buffer_overflow;
};

#include "audio_inline.h"

#endif /* AUDIO_H */
