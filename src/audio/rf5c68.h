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
#include "../common/common.h"

#define RF5C68_CHANNEL_COUNT 8
#define RF5C68_WAVE_RAM_SIZE 0x10000

class StateSerializer;

class RF5C68
{
public:
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
    void Sample(s16& left, s16& right);
    u8 Read(u16 address);
    void Write(u16 address, u8 value);
    void WriteIRQMask(u8 value);
    u8 ReadIRQFlags();
    bool IsIRQAsserted();

    const RF5C68_Channel* GetChannels() const;
    const u8* GetWaveRAM() const;

    u8 GetChannelBank() const;
    u8 GetWaveBank() const;
    bool IsEnabled() const;
    u8 GetIRQMask() const;
    u8 GetIRQFlags() const;
    s16 GetLeftSample() const;
    s16 GetRightSample() const;

    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void WriteRegister(u16 address, u8 value);
    void RunCycles(u64 cycles);
    void GenerateSample();
    void SetBlockIRQ(u32 block);
    void ResetChannelAddress(int channel);
    void ResetChannelAddresses();
    s16 QuantizeSample(s32 sample) const;
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    RF5C68_Channel m_channels[RF5C68_CHANNEL_COUNT];
    u8 m_wave_ram[RF5C68_WAVE_RAM_SIZE];
    u8 m_channel_bank;
    u8 m_wave_bank;
    bool m_enabled;
    u8 m_irq_mask;
    u8 m_irq_flags;

    u64 m_elapsed_cycles;
    u32 m_cycle_counter;

    s16 m_left_sample;
    s16 m_right_sample;
    s16 m_previous_left_sample;
    s16 m_previous_right_sample;
};

static const int k_rf5c68_cycles_per_sample = 384;
static const int k_rf5c68_address_fraction_bits = 11;
static const u32 k_rf5c68_address_mask = 0x07FFFFFF;
static const int k_rf5c68_irq_block_shift = k_rf5c68_address_fraction_bits + 12;
static const s32 k_rf5c68_output_max = 32767;
static const s32 k_rf5c68_output_min = -32768;
static const s32 k_rf5c68_output_quantization_mask = 0x3F;

#include "rf5c68_inline.h"

#endif /* RF5C68_H */
