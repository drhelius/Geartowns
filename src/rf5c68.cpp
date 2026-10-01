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
    memset(m_buffer, 0, sizeof(m_buffer));

    m_channel_bank = 0;
    m_wave_bank = 0;
    m_enabled = false;

    m_elapsed_cycles = 0;
    m_cycle_counter = 0;

    m_left_sample = 0;
    m_right_sample = 0;

    m_buffer_index = 0;
    m_frame_samples = 0;
}

u8 RF5C68::Read(u16 address)
{
    Synchronize();
    address &= 0x1FFF;

    // CPU Wave RAM reads are only available while global playback is stopped
    if (address >= 0x1000 && !m_enabled)
        return m_wave_ram[((u16)m_wave_bank << 12) | (address & 0x0FFF)];

    return 0xFF;
}

void RF5C68::Write(u16 address, u8 value)
{
    Synchronize();
    address &= 0x1FFF;

    if (address >= 0x1000)
    {
        m_wave_ram[((u16)m_wave_bank << 12) | (address & 0x0FFF)] = value;
        return;
    }

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
            for (int i = 0; i < CHANNEL_COUNT; i++)
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

void RF5C68::Synchronize()
{
    u64 cycles = m_elapsed_cycles + m_cycle_counter;

    m_elapsed_cycles = 0;

    if (cycles < CYCLES_PER_SAMPLE)
    {
        m_cycle_counter = (u32)cycles;
        return;
    }

    u64 samples = cycles / CYCLES_PER_SAMPLE;
    m_cycle_counter = (u32)(cycles % CYCLES_PER_SAMPLE);

    if (!m_enabled)
    {
        ResetChannelAddresses();
        m_left_sample = 0;
        m_right_sample = 0;
        return;
    }

    while (samples > 0)
    {
        GenerateSample();
        samples--;
    }
}

void RF5C68::GenerateSample()
{
    s32 left = 0;
    s32 right = 0;

    for (int i = 0; i < CHANNEL_COUNT; i++)
    {
        RF5C68_Channel& channel = m_channels[i];

        if (!channel.enabled)
        {
            ResetChannelAddress(i);
            continue;
        }

        u32 address = channel.address;
        u8 sample = m_wave_ram[(address >> ADDRESS_FRACTION_BITS) & 0xFFFF];

        if (sample == 0xFF)
        {
            address = (u32)channel.loop_start << ADDRESS_FRACTION_BITS;
            sample = m_wave_ram[channel.loop_start];

            if (sample == 0xFF)
            {
                channel.address = address;
                continue;
            }
        }

        channel.address = (address + channel.step) & ADDRESS_MASK;

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
    }

    m_left_sample = QuantizeSample(left);
    m_right_sample = QuantizeSample(right);
}

void RF5C68::ResetChannelAddress(int channel)
{
    m_channels[channel].address = (u32)m_channels[channel].start << (8 + ADDRESS_FRACTION_BITS);
}

void RF5C68::ResetChannelAddresses()
{
    for (int i = 0; i < CHANNEL_COUNT; i++)
        ResetChannelAddress(i);
}

s16 RF5C68::QuantizeSample(s32 sample) const
{
    sample = CLAMP(sample, -32768, 32767);
    // The DAC output uses the upper 10 bits of the saturated 16-bit accumulator
    sample = ((sample + 32768) & ~OUTPUT_QUANTIZATION_MASK) - 32768;
    return (s16)sample;
}

void RF5C68::Sample()
{
    Synchronize();

    if (m_buffer_index > (GT_AUDIO_BUFFER_SIZE - 2))
    {
        Error("RF5C68 audio buffer overflow");
        return;
    }

    m_buffer[m_buffer_index++] = m_left_sample;
    m_buffer[m_buffer_index++] = m_right_sample;
}

int RF5C68::EndFrame(s16* sample_buffer)
{
    Synchronize();

    int samples = 0;
    m_frame_samples = m_buffer_index;

    if (IsValidPointer(sample_buffer))
    {
        samples = m_buffer_index;
        memcpy(sample_buffer, m_buffer, samples * sizeof(s16));
    }

    m_buffer_index = 0;
    return samples;
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

    m_channel_bank &= 0x07;
    m_wave_bank &= 0x0F;
    m_cycle_counter %= CYCLES_PER_SAMPLE;
    m_buffer_index = CLAMP(m_buffer_index, 0, GT_AUDIO_BUFFER_SIZE);
    m_buffer_index &= ~1;
    m_frame_samples = CLAMP(m_frame_samples, 0, GT_AUDIO_BUFFER_SIZE);
    m_frame_samples &= ~1;

    for (int i = 0; i < CHANNEL_COUNT; i++)
    {
        m_channels[i].enabled = m_channels[i].enabled ? 1 : 0;
        m_channels[i].address &= ADDRESS_MASK;
    }

    if (!m_enabled)
        ResetChannelAddresses();
}

void RF5C68::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE_ARRAY(serializer, m_wave_ram, WAVE_RAM_SIZE);

    for (int i = 0; i < CHANNEL_COUNT; i++)
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
    G_SERIALIZE(serializer, m_elapsed_cycles);
    G_SERIALIZE(serializer, m_cycle_counter);
    G_SERIALIZE(serializer, m_left_sample);
    G_SERIALIZE(serializer, m_right_sample);
    G_SERIALIZE_ARRAY(serializer, m_buffer, GT_AUDIO_BUFFER_SIZE);
    G_SERIALIZE(serializer, m_buffer_index);
    G_SERIALIZE(serializer, m_frame_samples);
}
