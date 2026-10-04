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

#include "rf5c68.h"
#include "state_serializer.h"

RF5C68::RF5C68()
{
    memset(m_wave_ram, 0xFF, sizeof(m_wave_ram));
    Reset();
}

RF5C68::~RF5C68()
{
}

void RF5C68::Init()
{
    memset(m_wave_ram, 0xFF, sizeof(m_wave_ram));
    Reset();
}

void RF5C68::Reset()
{
    memset(m_channels, 0, sizeof(m_channels));

    m_channel_bank = 0;
    m_wave_bank = 0;
    m_enabled = false;
    m_irq_mask = 0;
    m_irq_flags = 0;

    m_elapsed_cycles = 0;
    m_cycle_counter = 0;

    m_left_sample = 0;
    m_right_sample = 0;
    m_previous_left_sample = 0;
    m_previous_right_sample = 0;
}

void RF5C68::WriteRegister(u16 address, u8 value)
{
    Synchronize();

    RF5C68_Channel& channel = m_channels[m_channel_bank];

    switch (address)
    {
        case 0x00:
            channel.envelope = value;
            break;
        case 0x01:
            channel.pan = value;
            break;
        case 0x02:
            channel.step = (channel.step & 0xFF00) | value;
            break;
        case 0x03:
            channel.step = (channel.step & 0x00FF) | ((u16)value << 8);
            break;
        case 0x04:
            channel.loop_start = (channel.loop_start & 0xFF00) | value;
            break;
        case 0x05:
            channel.loop_start = (channel.loop_start & 0x00FF) | ((u16)value << 8);
            break;
        case 0x06:
            channel.start = value;

            if (!m_enabled || !channel.enabled)
                ResetChannelAddress(m_channel_bank);

            break;
        case 0x07:
        {
            bool enabled = (value & 0x80) != 0;

            if (!enabled)
                ResetChannelAddresses();

            m_enabled = enabled;

            if ((value & 0x40) != 0)
                m_channel_bank = value & 0x07;
            else
                m_wave_bank = value & 0x0F;

            break;
        }

        case 0x08:
            for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
            {
                m_channels[i].enabled = ((value & (1 << i)) == 0) ? 1 : 0;

                if (!m_enabled || !m_channels[i].enabled)
                    ResetChannelAddress(i);
            }

            break;
        default:
            break;
    }
}

void RF5C68::WriteIRQMask(u8 value)
{
    Synchronize();
    m_irq_mask = value;
}

u8 RF5C68::ReadIRQFlags()
{
    Synchronize();

    u8 flags = m_irq_flags;
    // Reading the cause register clears every cause bit
    m_irq_flags = 0;
    return flags;
}

void RF5C68::Synchronize()
{
    u64 cycles = m_elapsed_cycles;
    m_elapsed_cycles = 0;
    RunCycles(cycles);
}

void RF5C68::Sample(s16& left, s16& right)
{
    Synchronize();

    s32 phase = (s32)m_cycle_counter;
    s32 left_delta = m_left_sample - m_previous_left_sample;
    s32 right_delta = m_right_sample - m_previous_right_sample;
    left = (s16)(m_previous_left_sample + (left_delta * phase) / k_rf5c68_cycles_per_sample);
    right = (s16)(m_previous_right_sample + (right_delta * phase) / k_rf5c68_cycles_per_sample);
}

void RF5C68::RunCycles(u64 cycles)
{
    while (cycles >= k_rf5c68_cycles_per_sample - m_cycle_counter)
    {
        cycles -= k_rf5c68_cycles_per_sample - m_cycle_counter;
        m_cycle_counter = 0;
        GenerateSample();
    }

    m_cycle_counter += (u32)cycles;
}

void RF5C68::GenerateSample()
{
    m_previous_left_sample = m_left_sample;
    m_previous_right_sample = m_right_sample;

    // A stopped IC holds every pointer at its start address and outputs silence
    if (!m_enabled)
    {
        m_left_sample = 0;
        m_right_sample = 0;
        return;
    }

    s32 left = 0;
    s32 right = 0;
    s32 left_limit = 0;
    s32 right_limit = 0;

    for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
    {
        RF5C68_Channel& channel = m_channels[i];

        if (!channel.enabled)
            continue;

        u32 address = channel.address;
        u32 offset = address >> k_rf5c68_address_fraction_bits;
        u8 sample = m_wave_ram[offset];

        if (sample == 0xFF)
        {
            // A loop marker on the last byte of a 4 KiB block also ends that block
            if ((offset & 0x0FFF) == 0x0FFF)
                SetBlockIRQ(offset >> 12);

            address = (u32)channel.loop_start << k_rf5c68_address_fraction_bits;
            sample = m_wave_ram[channel.loop_start];
        }

        u32 next_address = (address + channel.step) & k_rf5c68_address_mask;

        // Advancing into another 4 KiB block raises the IRQ of the block left behind
        if (((address ^ next_address) >> k_rf5c68_irq_block_shift) != 0)
            SetBlockIRQ(address >> k_rf5c68_irq_block_shift);

        channel.address = next_address;

        // 0xFF is never waveform data, but the pointer keeps advancing from the loop address
        if (sample == 0xFF)
            continue;

        s32 magnitude = sample & 0x7F;
        // The DCA feeds product bits 18..5 to the channel accumulator
        s32 left_output = (magnitude * channel.envelope * (channel.pan & 0x0F)) >> 5;
        s32 right_output = (magnitude * channel.envelope * (channel.pan >> 4)) >> 5;

        // RF5C68 samples are sign-magnitude with bit 7 set for positive values
        if ((sample & 0x80) != 0)
        {
            left += left_output;
            right += right_output;
        }
        else
        {
            left -= left_output;
            right -= right_output;
        }

        // An overflow while totaling channels 1 to 8 forces the limiter output to FFFFh or 0000h
        if (left > k_rf5c68_output_max)
            left_limit = k_rf5c68_output_max;
        else if (left < k_rf5c68_output_min)
            left_limit = k_rf5c68_output_min;

        if (right > k_rf5c68_output_max)
            right_limit = k_rf5c68_output_max;
        else if (right < k_rf5c68_output_min)
            right_limit = k_rf5c68_output_min;
    }

    m_left_sample = QuantizeSample((left_limit != 0) ? left_limit : left);
    m_right_sample = QuantizeSample((right_limit != 0) ? right_limit : right);
}

void RF5C68::SetBlockIRQ(u32 block)
{
    // PCM IRQ mask and cause bits each cover an 8 KiB region made of two 4 KiB blocks
    u8 region = (u8)(1 << ((block >> 1) & 0x07));

    if ((m_irq_mask & region) != 0)
        m_irq_flags |= region;
}

void RF5C68::ResetChannelAddress(int channel)
{
    m_channels[channel].address = (u32)m_channels[channel].start << (8 + k_rf5c68_address_fraction_bits);
}

void RF5C68::ResetChannelAddresses()
{
    for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
        ResetChannelAddress(i);
}

s16 RF5C68::QuantizeSample(s32 sample) const
{
    // The DAC output uses the upper 10 bits of the limited 16-bit accumulator
    return (s16)(sample & ~k_rf5c68_output_quantization_mask);
}

void RF5C68::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void RF5C68::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void RF5C68::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE_ARRAY(serializer, m_wave_ram, RF5C68_WAVE_RAM_SIZE);

    for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
    {
        G_SERIALIZE(serializer, m_channels[i].envelope);
        G_SERIALIZE(serializer, m_channels[i].pan);
        G_SERIALIZE(serializer, m_channels[i].start);
        G_SERIALIZE(serializer, m_channels[i].enabled);
        G_SERIALIZE(serializer, m_channels[i].step);
        G_SERIALIZE(serializer, m_channels[i].loop_start);
        G_SERIALIZE(serializer, m_channels[i].address);
    }

    G_SERIALIZE(serializer, m_channel_bank);
    G_SERIALIZE(serializer, m_wave_bank);
    G_SERIALIZE(serializer, m_enabled);
    G_SERIALIZE(serializer, m_irq_mask);
    G_SERIALIZE(serializer, m_irq_flags);
    G_SERIALIZE(serializer, m_elapsed_cycles);
    G_SERIALIZE(serializer, m_cycle_counter);
    G_SERIALIZE(serializer, m_left_sample);
    G_SERIALIZE(serializer, m_right_sample);
    G_SERIALIZE(serializer, m_previous_left_sample);
    G_SERIALIZE(serializer, m_previous_right_sample);
}

void RF5C68::SanitizeState()
{
    m_channel_bank &= 0x07;
    m_wave_bank &= 0x0F;

    // Pending work never spans a whole second, so this bounds corrupt states without touching valid ones
    m_elapsed_cycles = MIN(m_elapsed_cycles, (u64)GT_SOUND_CLOCK_RATE);
    m_cycle_counter %= k_rf5c68_cycles_per_sample;

    for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
    {
        m_channels[i].enabled = m_channels[i].enabled ? 1 : 0;
        m_channels[i].address &= k_rf5c68_address_mask;

        // Channels that are not sounding hold their pointer at the start address
        if (!m_enabled || !m_channels[i].enabled)
            ResetChannelAddress(i);
    }
}
