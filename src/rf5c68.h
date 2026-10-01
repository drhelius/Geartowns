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

#ifndef RF5C68_H
#define RF5C68_H

#include <iostream>
#include "common.h"

class StateSerializer;

class RF5C68
{
public:
    enum
    {
        CHANNEL_COUNT = 8,
        WAVE_RAM_SIZE = 0x10000,
        CYCLES_PER_SAMPLE = 384,
        ADDRESS_FRACTION_BITS = 11,
        ADDRESS_MASK = 0x07FFFFFF,
        OUTPUT_QUANTIZATION_MASK = 0x3F
    };

    struct RF5C68_Channel
    {
        u8 envelope;
        u8 pan;
        u8 start;
        u8 enabled;
        u16 step;
        u16 loop_start;
        u32 address;
    };

public:
    RF5C68();
    ~RF5C68();
    void Init();
    void Reset();
    void Clock(u32 cycles);
    void Synchronize();
    u8 Read(u16 address);
    void Write(u16 address, u8 value);
    void Sample();
    int EndFrame(s16* sample_buffer);

    const RF5C68_Channel* GetChannels() const;
    const u8* GetWaveRAM() const;

    u8 GetChannelBank() const;
    u8 GetWaveBank() const;
    bool IsEnabled() const;
    s16 GetLeftSample() const;
    s16 GetRightSample() const;
    int GetFrameSamples() const;

    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void GenerateSample();
    void ResetChannelAddress(int channel);
    void ResetChannelAddresses();
    s16 QuantizeSample(s32 sample) const;
    void Serialize(StateSerializer& serializer);

private:
    RF5C68_Channel m_channels[CHANNEL_COUNT];
    u8 m_wave_ram[WAVE_RAM_SIZE];
    u8 m_channel_bank;
    u8 m_wave_bank;
    bool m_enabled;

    u64 m_elapsed_cycles;
    u32 m_cycle_counter;

    s16 m_left_sample;
    s16 m_right_sample;

    s16 m_buffer[GT_AUDIO_BUFFER_SIZE];
    int m_buffer_index;
    int m_frame_samples;
};

#include "rf5c68_inline.h"

#endif /* RF5C68_H */
