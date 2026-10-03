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

#include <string.h>
#include "ym3438.h"
#include "state_serializer.h"

// Register order is slot 1, 3, 2, 4.
static const u8 k_ym3438_slot_map[4] = { 0, 2, 1, 3 };
static const u8 k_ym3438_note[16] = { 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3 };
static const u8 k_ym3438_lfo_cycles[8] = { 108, 77, 71, 67, 62, 44, 8, 5 };
static const u8 k_ym3438_am_shift[4] = { 7, 3, 1, 0 };
static const u8 k_ym3438_detune[8] = { 16, 17, 19, 20, 22, 24, 27, 29 };

static const u8 k_ym3438_lfo_shift_1[8][8] =
{
    { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 1, 1 },
    { 7, 7, 7, 7, 1, 1, 1, 1 },
    { 7, 7, 7, 1, 1, 1, 1, 0 },
    { 7, 7, 1, 1, 0, 0, 0, 0 },
    { 7, 7, 1, 1, 0, 0, 0, 0 },
    { 7, 7, 1, 1, 0, 0, 0, 0 }
};

static const u8 k_ym3438_lfo_shift_2[8][8] =
{
    { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 2, 2, 2, 2 },
    { 7, 7, 7, 2, 2, 2, 7, 7 },
    { 7, 7, 2, 2, 7, 7, 2, 2 },
    { 7, 7, 2, 7, 7, 7, 2, 7 },
    { 7, 7, 7, 2, 7, 7, 2, 1 },
    { 7, 7, 7, 2, 7, 7, 2, 1 },
    { 7, 7, 7, 2, 7, 7, 2, 1 }
};

// Die ROM contents: round(-log2(sin((i + 0.5) * pi / 512)) * 256)
static const u16 k_ym3438_log_sine[256] =
{
/* 0x00 */ 0x859, 0x6C3, 0x607, 0x58B, 0x52E, 0x4E4, 0x4A6, 0x471, 0x443, 0x41A, 0x3F5, 0x3D3, 0x3B5, 0x398, 0x37E, 0x365,
/* 0x10 */ 0x34E, 0x339, 0x324, 0x311, 0x2FF, 0x2ED, 0x2DC, 0x2CD, 0x2BD, 0x2AF, 0x2A0, 0x293, 0x286, 0x279, 0x26D, 0x261,
/* 0x20 */ 0x256, 0x24B, 0x240, 0x236, 0x22C, 0x222, 0x218, 0x20F, 0x206, 0x1FD, 0x1F5, 0x1EC, 0x1E4, 0x1DC, 0x1D4, 0x1CD,
/* 0x30 */ 0x1C5, 0x1BE, 0x1B7, 0x1B0, 0x1A9, 0x1A2, 0x19B, 0x195, 0x18F, 0x188, 0x182, 0x17C, 0x177, 0x171, 0x16B, 0x166,
/* 0x40 */ 0x160, 0x15B, 0x155, 0x150, 0x14B, 0x146, 0x141, 0x13C, 0x137, 0x133, 0x12E, 0x129, 0x125, 0x121, 0x11C, 0x118,
/* 0x50 */ 0x114, 0x10F, 0x10B, 0x107, 0x103, 0x0FF, 0x0FB, 0x0F8, 0x0F4, 0x0F0, 0x0EC, 0x0E9, 0x0E5, 0x0E2, 0x0DE, 0x0DB,
/* 0x60 */ 0x0D7, 0x0D4, 0x0D1, 0x0CD, 0x0CA, 0x0C7, 0x0C4, 0x0C1, 0x0BE, 0x0BB, 0x0B8, 0x0B5, 0x0B2, 0x0AF, 0x0AC, 0x0A9,
/* 0x70 */ 0x0A7, 0x0A4, 0x0A1, 0x09F, 0x09C, 0x099, 0x097, 0x094, 0x092, 0x08F, 0x08D, 0x08A, 0x088, 0x086, 0x083, 0x081,
/* 0x80 */ 0x07F, 0x07D, 0x07A, 0x078, 0x076, 0x074, 0x072, 0x070, 0x06E, 0x06C, 0x06A, 0x068, 0x066, 0x064, 0x062, 0x060,
/* 0x90 */ 0x05E, 0x05C, 0x05B, 0x059, 0x057, 0x055, 0x053, 0x052, 0x050, 0x04E, 0x04D, 0x04B, 0x04A, 0x048, 0x046, 0x045,
/* 0xA0 */ 0x043, 0x042, 0x040, 0x03F, 0x03E, 0x03C, 0x03B, 0x039, 0x038, 0x037, 0x035, 0x034, 0x033, 0x031, 0x030, 0x02F,
/* 0xB0 */ 0x02E, 0x02D, 0x02B, 0x02A, 0x029, 0x028, 0x027, 0x026, 0x025, 0x024, 0x023, 0x022, 0x021, 0x020, 0x01F, 0x01E,
/* 0xC0 */ 0x01D, 0x01C, 0x01B, 0x01A, 0x019, 0x018, 0x017, 0x017, 0x016, 0x015, 0x014, 0x014, 0x013, 0x012, 0x011, 0x011,
/* 0xD0 */ 0x010, 0x00F, 0x00F, 0x00E, 0x00D, 0x00D, 0x00C, 0x00C, 0x00B, 0x00A, 0x00A, 0x009, 0x009, 0x008, 0x008, 0x007,
/* 0xE0 */ 0x007, 0x007, 0x006, 0x006, 0x005, 0x005, 0x005, 0x004, 0x004, 0x004, 0x003, 0x003, 0x003, 0x002, 0x002, 0x002,
/* 0xF0 */ 0x002, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000
};

// Die ROM contents: round((2^(i / 256) - 1) * 1024)
static const u16 k_ym3438_exp[256] =
{
/* 0x00 */ 0x000, 0x003, 0x006, 0x008, 0x00B, 0x00E, 0x011, 0x014, 0x016, 0x019, 0x01C, 0x01F, 0x022, 0x025, 0x028, 0x02A,
/* 0x10 */ 0x02D, 0x030, 0x033, 0x036, 0x039, 0x03C, 0x03F, 0x042, 0x045, 0x048, 0x04B, 0x04E, 0x051, 0x054, 0x057, 0x05A,
/* 0x20 */ 0x05D, 0x060, 0x063, 0x066, 0x069, 0x06C, 0x06F, 0x072, 0x075, 0x078, 0x07B, 0x07E, 0x082, 0x085, 0x088, 0x08B,
/* 0x30 */ 0x08E, 0x091, 0x094, 0x098, 0x09B, 0x09E, 0x0A1, 0x0A4, 0x0A8, 0x0AB, 0x0AE, 0x0B1, 0x0B5, 0x0B8, 0x0BB, 0x0BE,
/* 0x40 */ 0x0C2, 0x0C5, 0x0C8, 0x0CC, 0x0CF, 0x0D2, 0x0D6, 0x0D9, 0x0DC, 0x0E0, 0x0E3, 0x0E7, 0x0EA, 0x0ED, 0x0F1, 0x0F4,
/* 0x50 */ 0x0F8, 0x0FB, 0x0FF, 0x102, 0x106, 0x109, 0x10C, 0x110, 0x114, 0x117, 0x11B, 0x11E, 0x122, 0x125, 0x129, 0x12C,
/* 0x60 */ 0x130, 0x134, 0x137, 0x13B, 0x13E, 0x142, 0x146, 0x149, 0x14D, 0x151, 0x154, 0x158, 0x15C, 0x160, 0x163, 0x167,
/* 0x70 */ 0x16B, 0x16F, 0x172, 0x176, 0x17A, 0x17E, 0x181, 0x185, 0x189, 0x18D, 0x191, 0x195, 0x199, 0x19C, 0x1A0, 0x1A4,
/* 0x80 */ 0x1A8, 0x1AC, 0x1B0, 0x1B4, 0x1B8, 0x1BC, 0x1C0, 0x1C4, 0x1C8, 0x1CC, 0x1D0, 0x1D4, 0x1D8, 0x1DC, 0x1E0, 0x1E4,
/* 0x90 */ 0x1E8, 0x1EC, 0x1F0, 0x1F5, 0x1F9, 0x1FD, 0x201, 0x205, 0x209, 0x20E, 0x212, 0x216, 0x21A, 0x21E, 0x223, 0x227,
/* 0xA0 */ 0x22B, 0x230, 0x234, 0x238, 0x23C, 0x241, 0x245, 0x249, 0x24E, 0x252, 0x257, 0x25B, 0x25F, 0x264, 0x268, 0x26D,
/* 0xB0 */ 0x271, 0x276, 0x27A, 0x27F, 0x283, 0x288, 0x28C, 0x291, 0x295, 0x29A, 0x29E, 0x2A3, 0x2A8, 0x2AC, 0x2B1, 0x2B5,
/* 0xC0 */ 0x2BA, 0x2BF, 0x2C4, 0x2C8, 0x2CD, 0x2D2, 0x2D6, 0x2DB, 0x2E0, 0x2E5, 0x2E9, 0x2EE, 0x2F3, 0x2F8, 0x2FD, 0x302,
/* 0xD0 */ 0x306, 0x30B, 0x310, 0x315, 0x31A, 0x31F, 0x324, 0x329, 0x32E, 0x333, 0x338, 0x33D, 0x342, 0x347, 0x34C, 0x351,
/* 0xE0 */ 0x356, 0x35B, 0x360, 0x365, 0x36A, 0x370, 0x375, 0x37A, 0x37F, 0x384, 0x38A, 0x38F, 0x394, 0x399, 0x39F, 0x3A4,
/* 0xF0 */ 0x3A9, 0x3AE, 0x3B4, 0x3B9, 0x3BF, 0x3C4, 0x3C9, 0x3CF, 0x3D4, 0x3DA, 0x3DF, 0x3E4, 0x3EA, 0x3EF, 0x3F5, 0x3FA
};

static const u8 k_ym3438_envelope_increment[17][8] =
{
    { 0, 1, 0, 1, 0, 1, 0, 1 },
    { 0, 1, 0, 1, 1, 1, 0, 1 },
    { 0, 1, 1, 1, 0, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 2, 1, 1, 1, 2 },
    { 1, 2, 1, 2, 1, 2, 1, 2 },
    { 1, 2, 2, 2, 1, 2, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2 },
    { 2, 2, 2, 4, 2, 2, 2, 4 },
    { 2, 4, 2, 4, 2, 4, 2, 4 },
    { 2, 4, 4, 4, 2, 4, 4, 4 },
    { 4, 4, 4, 4, 4, 4, 4, 4 },
    { 4, 4, 4, 8, 4, 4, 4, 8 },
    { 4, 8, 4, 8, 4, 8, 4, 8 },
    { 4, 8, 8, 8, 4, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 }
};

YM3438::YM3438()
{
    Reset();
}

YM3438::~YM3438()
{
}

void YM3438::Init()
{
    Reset();
}

void YM3438::ResetOperator(YM3438_Operator& op)
{
    memset(&op, 0, sizeof(op));
    op.envelope = YM3438_ENVELOPE_MAX;
    op.state = YM3438_ENVELOPE_RELEASE;
}

void YM3438::ResetChannel(YM3438_Channel& channel)
{
    memset(&channel, 0, sizeof(channel));

    for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
        ResetOperator(channel.operators[i]);

    channel.pan_left = 1;
    channel.pan_right = 1;
    channel.phase_dirty = 1;
}

void YM3438::Reset()
{
    memset(m_registers, 0, sizeof(m_registers));

    for (int i = 0; i < YM3438_CHANNEL_COUNT; i++)
        ResetChannel(m_channels[i]);

    m_address = 0;
    m_f_number_high = 0;
    m_special_f_number_high = 0;
    m_channel_3_mode = 0;

    m_dac_enabled = 0;
    m_dac_data = 0;

    m_lfo_enabled = 0;
    m_lfo_frequency = 0;
    m_lfo_counter = 0;
    m_lfo_quotient = 0;
    m_lfo_phase_changed = 0;

    m_timer_a_register = 0;
    m_timer_a_counter = 0;
    m_timer_b_register = 0;
    m_timer_b_counter = 0;
    m_timer_b_prescaler = 0;
    m_timer_a_load = 0;
    m_timer_b_load = 0;
    m_timer_a_enable = 0;
    m_timer_b_enable = 0;
    m_timer_a_flag = 0;
    m_timer_b_flag = 0;

    m_csm_key_pending = 0;
    m_csm_key_active = 0;

    m_envelope_counter = 0;
    m_envelope_divider = 0;

    m_native_cycle = 0;
    m_elapsed_cycles = 0;
    m_busy_cycles = 0;
    m_status = 0;

    m_left_sample = 0;
    m_right_sample = 0;
    m_previous_left_sample = 0;
    m_previous_right_sample = 0;
}

void YM3438::Synchronize()
{
    u64 cycles = m_elapsed_cycles;
    m_elapsed_cycles = 0;
    RunCycles(cycles);
}

void YM3438::Sample(s16& left, s16& right)
{
    Synchronize();

    s32 phase = (s32)m_native_cycle;
    s32 left_delta = m_left_sample - m_previous_left_sample;
    s32 right_delta = m_right_sample - m_previous_right_sample;
    left = (s16)(m_previous_left_sample + (left_delta * phase) / YM3438_NATIVE_SAMPLE_CYCLES);
    right = (s16)(m_previous_right_sample + (right_delta * phase) / YM3438_NATIVE_SAMPLE_CYCLES);
}

void YM3438::RunCycles(u64 cycles)
{
    if (m_busy_cycles > cycles)
        m_busy_cycles -= (u32)cycles;
    else
        m_busy_cycles = 0;

    while (cycles >= YM3438_NATIVE_SAMPLE_CYCLES - m_native_cycle)
    {
        cycles -= YM3438_NATIVE_SAMPLE_CYCLES - m_native_cycle;
        m_native_cycle = 0;
        m_previous_left_sample = m_left_sample;
        m_previous_right_sample = m_right_sample;
        GenerateNativeSample();
    }

    m_native_cycle += (u32)cycles;
}

void YM3438::ClockTimers()
{
    // Both timers count once per native sample, like the chip's 24-slot frame
    if (m_timer_a_load)
    {
        m_timer_a_counter++;

        if (m_timer_a_counter >= 1024)
        {
            m_timer_a_counter = m_timer_a_register;
            TimerAOverflow();
        }
    }

    // Timer B's divide-by-16 prescaler keeps running while its counter is stopped
    m_timer_b_prescaler = (m_timer_b_prescaler + 1) & 0x0F;

    if (m_timer_b_load && m_timer_b_prescaler == 0)
    {
        m_timer_b_counter++;

        if (m_timer_b_counter >= 256)
        {
            m_timer_b_counter = m_timer_b_register;

            if (m_timer_b_enable)
                m_timer_b_flag = 1;
        }
    }
}

void YM3438::TimerAOverflow()
{
    if (m_timer_a_enable)
        m_timer_a_flag = 1;

    if (m_channel_3_mode == 2)
        m_csm_key_pending = 1;
}

void YM3438::GenerateNativeSample()
{
    ClockTimers();
    UpdateKeyStates();
    UpdateEnvelopes();

    s32 left = 0;
    s32 right = 0;

    for (int i = 0; i < YM3438_CHANNEL_COUNT; i++)
    {
        s16 output = CalculateChannel(i);

        if (m_channels[i].pan_left)
            left += output;

        if (m_channels[i].pan_right)
            right += output;
    }

    m_lfo_phase_changed = 0;

    left = CLAMP(left * 16, -32768, 32767);
    right = CLAMP(right * 16, -32768, 32767);
    m_left_sample = (s16)left;
    m_right_sample = (s16)right;

    UpdateLFO();
}

void YM3438::UpdateKeyStates()
{
    // A Timer A overflow in CSM mode keys on channel 3 within the same sample, and only for that sample
    m_csm_key_active = m_csm_key_pending;
    m_csm_key_pending = 0;

    for (int channel_index = 0; channel_index < YM3438_CHANNEL_COUNT; channel_index++)
    {
        YM3438_Channel& channel = m_channels[channel_index];
        bool csm_key_on = channel_index == 2 && m_csm_key_active;

        // S1 samples the key register just before a 0x28 write lands, so it follows S2-S4 one sample later
        channel.operators[0].manual_key_on = channel.s1_key_register;
        channel.s1_key_register = channel.s1_key_written;

        int operator_count = channel_index == 2 ? YM3438_OPERATOR_COUNT : 1;

        for (int i = 0; i < operator_count; i++)
        {
            YM3438_Operator& op = channel.operators[i];
            SetKeyState(op, op.manual_key_on || csm_key_on, channel_index, i);
        }
    }
}

void YM3438::UpdateLFO()
{
    u8 lfo_phase = GetLFOPhase();

    if (!m_lfo_enabled)
        m_lfo_counter = 0;

    m_lfo_quotient++;

    if ((m_lfo_quotient & k_ym3438_lfo_cycles[m_lfo_frequency]) == k_ym3438_lfo_cycles[m_lfo_frequency])
    {
        m_lfo_quotient = 0;

        if (m_lfo_enabled)
            m_lfo_counter = (m_lfo_counter + 1) & 0x7F;
    }

    if (GetLFOPhase() != lfo_phase)
        m_lfo_phase_changed = 1;
}

void YM3438::UpdateEnvelopes()
{
    // Rate increments advance every three samples; key/SSG control runs each sample
    m_envelope_divider++;

    if (m_envelope_divider == 3)
    {
        m_envelope_divider = 0;
        m_envelope_counter++;

        // The 12-bit timer adds its overflow carry back in, skipping zero
        if (m_envelope_counter >= 0x1000)
            m_envelope_counter = 1;
    }

    for (int channel = 0; channel < YM3438_CHANNEL_COUNT; channel++)
    {
        for (int operator_index = 0; operator_index < YM3438_OPERATOR_COUNT; operator_index++)
        {
            UpdateEnvelope(m_channels[channel].operators[operator_index], channel, operator_index);
        }
    }
}

void YM3438::UpdateEnvelope(YM3438_Operator& op, int channel, int operator_index)
{
    bool ssg_enabled = (op.ssg_envelope & 0x08) != 0;
    u8 old_state = op.state;
    bool key_event = op.key_on_pending != 0;
    op.key_on_pending = 0;
    bool repeat = HandleSSGEnvelope(op);
    bool envelope_off = ssg_enabled ? op.envelope >= YM3438_SSG_ENVELOPE_MAX : op.envelope >= 0x3F0;

    // Key and SSG control above still run when the attenuation is stationary
    if (!repeat &&
        !key_event &&
        !(channel == 2 && m_csm_key_active) &&
        ((op.state == YM3438_ENVELOPE_SUSTAIN && op.key_on && op.sustain_rate == 0 && !envelope_off) ||
         (op.state == YM3438_ENVELOPE_RELEASE && op.envelope == YM3438_ENVELOPE_MAX)))
    {
        return;
    }

    u8 rate = m_envelope_divider == 0 || repeat ? GetEnvelopeRate(op, channel, operator_index) : 0;
    u8 increment = m_envelope_divider == 0 ? GetEnvelopeIncrement(rate) : 0;
    s32 change = 0;

    // A new key-on precedes attack/decay progression, including CSM TL
    if (repeat && !key_event)
    {
        if (rate >= 62)
            op.envelope = 0;
        else if (old_state == YM3438_ENVELOPE_ATTACK && op.envelope != 0 && increment)
            change = ((~(s32)op.envelope) * increment) >> 4;
    }
    else if (!key_event)
    {
        switch (op.state)
        {
            case YM3438_ENVELOPE_ATTACK:
            {
                if (op.envelope == 0)
                    op.state = YM3438_ENVELOPE_DECAY;
                else if (op.key_on && rate < 62 && increment > 0)
                    change = ((~(s32)op.envelope) * increment) >> 4;

                break;
            }

            case YM3438_ENVELOPE_DECAY:
            {
                u16 sustain = op.sustain_level == 15 ? 992 : (u16)(op.sustain_level << 5);

                if ((op.envelope >> 4) == (sustain >> 4))
                    op.state = YM3438_ENVELOPE_SUSTAIN;
                else if (!envelope_off)
                    change = increment * (ssg_enabled ? 4 : 1);

                break;
            }

            case YM3438_ENVELOPE_SUSTAIN:
            case YM3438_ENVELOPE_RELEASE:
            {
                if (!envelope_off)
                    change = increment * (ssg_enabled ? 4 : 1);

                break;
            }

            default:
            {
                op.state = YM3438_ENVELOPE_RELEASE;
                break;
            }
        }

        // A key-off still steps with the previous phase's rate, then enters release
        if (!op.key_on)
            op.state = YM3438_ENVELOPE_RELEASE;
    }

    // CSM incorporates TL into the envelope, rather than adding it at the output
    if (channel == 2 && m_csm_key_active)
        op.envelope |= op.total_level << 3;

    bool hold_up = op.key_on && ssg_enabled && ((op.ssg_envelope & 0x07) == 3 || (op.ssg_envelope & 0x07) == 5);

    if (!key_event && !repeat && !hold_up && old_state != YM3438_ENVELOPE_ATTACK && envelope_off)
    {
        op.envelope = YM3438_ENVELOPE_MAX;
        op.state = YM3438_ENVELOPE_RELEASE;
    }

    op.envelope = (u16)(((s32)op.envelope + change) & YM3438_ENVELOPE_MAX);
}

u8 YM3438::GetEnvelopeIncrement(u8 rate) const
{
    if (rate < 2)
        return 0;

    int shift = 0;
    int pattern = 0;

    if (rate < 48)
    {
        shift = 11 - (rate >> 2);
        pattern = rate & 0x03;
    }
    else if (rate < 60)
    {
        pattern = 4 + (((rate - 48) >> 2) << 2) + (rate & 0x03);
    }
    else
    {
        pattern = 16;
    }

    if (shift > 0 && (m_envelope_counter & ((1U << shift) - 1)) != 0)
        return 0;

    // Fast rates walk their step pattern backwards through the counter's low bits
    int cycle = rate < 48 ? (m_envelope_counter >> shift) & 0x07 : (~m_envelope_counter) & 0x07;

    return k_ym3438_envelope_increment[pattern][cycle];
}

u8 YM3438::GetEnvelopeRate(const YM3438_Operator& op, int channel, int operator_index) const
{
    u8 parameter = 0;

    switch (op.state)
    {
        case YM3438_ENVELOPE_ATTACK:
            parameter = op.attack_rate;
            break;
        case YM3438_ENVELOPE_DECAY:
            parameter = op.decay_rate;
            break;
        case YM3438_ENVELOPE_SUSTAIN:
            parameter = op.sustain_rate;
            break;
        case YM3438_ENVELOPE_RELEASE:
            parameter = (op.release_rate << 1) | 0x01;
            break;
        default:
            return 0;
    }

    if (parameter == 0)
        return 0;

    u8 key_scale = GetOperatorKeyCode(channel, operator_index) >> (op.key_scale ^ 0x03);
    int rate = (parameter << 1) + key_scale;
    return (u8)MIN(rate, 63);
}

u16 YM3438::GetEnvelopeOutput(const YM3438_Operator& op) const
{
    u16 envelope = op.envelope;

    if ((op.ssg_envelope & 0x08) && op.key_on && (op.ssg_direction ^ ((op.ssg_envelope >> 2) & 0x01)))
    {
        envelope = (YM3438_SSG_ENVELOPE_MAX - envelope) & YM3438_ENVELOPE_MAX;
    }

    return envelope;
}

bool YM3438::HandleSSGEnvelope(YM3438_Operator& op)
{
    if ((op.ssg_envelope & 0x08) == 0)
    {
        op.ssg_direction = 0;
        return false;
    }

    if (!op.key_on || op.envelope < YM3438_SSG_ENVELOPE_MAX)
        return false;

    u8 shape = op.ssg_envelope & 0x03;

    if (shape == 0)
        op.phase = 0;
    else if (shape == 2)
        op.ssg_direction ^= 1;
    else if (shape == 3)
        op.ssg_direction = 1;

    if ((shape & 0x01) == 0)
    {
        // Repeat attacks start from the current level, just like a normal key-on
        op.state = YM3438_ENVELOPE_ATTACK;
        return true;
    }

    return false;
}

void YM3438::SetKeyState(YM3438_Operator& op, bool key_on, int channel, int operator_index)
{
    if (key_on)
    {
        if (!op.key_on)
        {
            op.key_on = 1;
            op.key_on_pending = 1;
            op.state = YM3438_ENVELOPE_ATTACK;
            op.phase = 0;
            op.ssg_direction = 0;

            if (GetEnvelopeRate(op, channel, operator_index) >= 62)
                op.envelope = 0;
        }

        return;
    }

    if (op.key_on)
    {
        if ((op.ssg_envelope & 0x08) && (op.ssg_direction ^ ((op.ssg_envelope >> 2) & 0x01)))
        {
            op.envelope = (YM3438_SSG_ENVELOPE_MAX - op.envelope) & YM3438_ENVELOPE_MAX;
        }

        op.key_on = 0;
        op.key_on_pending = 0;
        op.ssg_direction = 0;
    }
}

void YM3438::KeyOnChannel(int channel, u8 slots)
{
    if (channel < 0 || channel >= YM3438_CHANNEL_COUNT)
        return;

    // S1 picks this up one sample later, in UpdateKeyStates
    m_channels[channel].s1_key_written = slots & 0x01;

    for (int i = 1; i < YM3438_OPERATOR_COUNT; i++)
    {
        YM3438_Operator& op = m_channels[channel].operators[i];
        op.manual_key_on = (slots >> i) & 0x01;
        bool key_on = op.manual_key_on || (channel == 2 && m_csm_key_active);
        SetKeyState(op, key_on, channel, i);
    }
}

void YM3438::KeyOffCSM()
{
    m_csm_key_active = 0;

    m_csm_key_pending = 0;

    for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
    {
        YM3438_Operator& op = m_channels[2].operators[i];
        SetKeyState(op, op.manual_key_on != 0, 2, i);
    }
}

s16 YM3438::CalculateChannel(int channel_index)
{
    YM3438_Channel& channel = m_channels[channel_index];

    // Increments only change on frequency writes, or on LFO PM steps for channels using PM
    if (channel.phase_modulation && m_lfo_phase_changed)
        channel.phase_dirty = 1;

    // Keyed-off operators at full attenuation output nothing, and key-on restarts their phase
    bool silent = true;

    for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
    {
        if (channel.operators[i].key_on || channel.operators[i].envelope != YM3438_ENVELOPE_MAX)
        {
            silent = false;
            break;
        }
    }

    if (silent)
    {
        channel.memory_output = 0;
        channel.feedback_output[1] = channel.feedback_output[0];
        channel.feedback_output[0] = 0;
        channel.output = (channel_index == 5 && m_dac_enabled) ? m_dac_data : 0;
        return channel.output;
    }

    s32 feedback = 0;

    if (channel.feedback > 0)
    {
        feedback = channel.feedback_output[0] + channel.feedback_output[1];
        feedback >>= 10 - channel.feedback;
    }

    s16 op1 = CalculateOperator(channel_index, 0, feedback);
    s16 op2 = 0;
    s16 op3 = 0;
    s16 op4 = 0;
    s32 output = 0;
    s32 memory = 0;

    // Algorithms 0, 1, 2, 3, and 5 use the OPN2's one-sample MEM delay
    // Carriers enter the 9-bit saturating accumulator in slot order 1, 3, 2, 4.
    switch (channel.algorithm)
    {
        case 0:
            op3 = CalculateOperator(channel_index, 2, channel.memory_output >> 1);
            op2 = CalculateOperator(channel_index, 1, op1 >> 1);
            op4 = CalculateOperator(channel_index, 3, op3 >> 1);
            memory = op2;
            output = op4 >> 5;
            break;
        case 1:
            op3 = CalculateOperator(channel_index, 2, channel.memory_output >> 1);
            op2 = CalculateOperator(channel_index, 1, 0);
            op4 = CalculateOperator(channel_index, 3, op3 >> 1);
            memory = op1 + op2;
            output = op4 >> 5;
            break;
        case 2:
            op3 = CalculateOperator(channel_index, 2, channel.memory_output >> 1);
            op2 = CalculateOperator(channel_index, 1, 0);
            op4 = CalculateOperator(channel_index, 3, (op1 + op3) >> 1);
            memory = op2;
            output = op4 >> 5;
            break;
        case 3:
            op2 = CalculateOperator(channel_index, 1, op1 >> 1);
            op3 = CalculateOperator(channel_index, 2, 0);
            op4 = CalculateOperator(channel_index, 3, (channel.memory_output + op3) >> 1);
            memory = op2;
            output = op4 >> 5;
            break;
        case 4:
            op2 = CalculateOperator(channel_index, 1, op1 >> 1);
            op3 = CalculateOperator(channel_index, 2, 0);
            op4 = CalculateOperator(channel_index, 3, op3 >> 1);
            output = (op2 >> 5) + (op4 >> 5);
            break;
        case 5:
            op2 = CalculateOperator(channel_index, 1, op1 >> 1);
            op3 = CalculateOperator(channel_index, 2, channel.memory_output >> 1);
            op4 = CalculateOperator(channel_index, 3, op1 >> 1);
            memory = op1;
            output = CLAMP((op3 >> 5) + (op2 >> 5), -256, 255) + (op4 >> 5);
            break;
        case 6:
            op2 = CalculateOperator(channel_index, 1, op1 >> 1);
            op3 = CalculateOperator(channel_index, 2, 0);
            op4 = CalculateOperator(channel_index, 3, 0);
            output = CLAMP((op3 >> 5) + (op2 >> 5), -256, 255) + (op4 >> 5);
            break;
        case 7:
        default:
            op2 = CalculateOperator(channel_index, 1, 0);
            op3 = CalculateOperator(channel_index, 2, 0);
            op4 = CalculateOperator(channel_index, 3, 0);
            output = CLAMP((op1 >> 5) + (op3 >> 5), -256, 255);
            output = CLAMP(output + (op2 >> 5), -256, 255) + (op4 >> 5);
            break;
    }

    channel.memory_output = memory;

    channel.feedback_output[1] = channel.feedback_output[0];
    channel.feedback_output[0] = op1;

    if (channel.phase_dirty)
    {
        channel.phase_dirty = 0;

        for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
            channel.operators[i].phase_increment = CalculatePhaseIncrement(channel_index, i);
    }

    for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
    {
        YM3438_Operator& op = channel.operators[i];
        op.phase = (op.phase + op.phase_increment) & YM3438_PHASE_MASK;
    }

    output = CLAMP(output, -256, 255);

    if (channel_index == 5 && m_dac_enabled)
        output = m_dac_data;

    channel.output = (s16)output;
    return channel.output;
}

s16 YM3438::CalculateOperator(int channel_index, int operator_index, s32 modulation)
{
    YM3438_Channel& channel = m_channels[channel_index];
    YM3438_Operator& op = channel.operators[operator_index];
    u32 attenuation = GetEnvelopeOutput(op);

    if (!(channel_index == 2 && m_channel_3_mode == 2))
        attenuation += op.total_level << 3;

    if (op.amplitude_modulation_enabled)
        attenuation += GetLFOAmplitude() >> k_ym3438_am_shift[channel.amplitude_modulation];

    attenuation = MIN(attenuation, (u32)YM3438_ENVELOPE_MAX);

    // At this level even the largest exponential-table value shifts to zero
    if (attenuation >= 832)
        return 0;

    u32 phase = ((op.phase >> 10) + modulation) & 0x3FF;
    u32 quarter = phase & 0x100 ? (phase ^ 0xFF) & 0xFF : phase & 0xFF;
    u32 level = k_ym3438_log_sine[quarter] + (attenuation << 2);
    level = MIN(level, 0x1FFFU);

    u32 output = ((k_ym3438_exp[(level & 0xFF) ^ 0xFF] | 0x400) << 2) >> (level >> 8);
    s32 signed_output = (s32)output;

    if (phase & 0x200)
        signed_output = -(s32)output;

    return (s16)signed_output;
}

u32 YM3438::CalculatePhaseIncrement(int channel_index, int operator_index) const
{
    const YM3438_Channel& channel = m_channels[channel_index];
    const YM3438_Operator& op = channel.operators[operator_index];
    u16 f_number = 0;
    u8 block = 0;
    u8 key_code = 0;
    GetOperatorFrequency(channel_index, operator_index, f_number, block, key_code);

    u32 adjusted_f_number = f_number << 1;

    if (m_lfo_enabled && channel.phase_modulation)
    {
        u32 f_number_high = f_number >> 4;
        u8 lfo = GetLFOPhase();
        u8 lfo_index = lfo & 0x0F;

        if (lfo_index & 0x08)
            lfo_index ^= 0x0F;

        u8 sensitivity = channel.phase_modulation;
        u32 modulation = (f_number_high >> k_ym3438_lfo_shift_1[sensitivity][lfo_index]) +
            (f_number_high >> k_ym3438_lfo_shift_2[sensitivity][lfo_index]);

        if (sensitivity > 5)
            modulation <<= sensitivity - 5;

        modulation >>= 2;

        if (lfo & 0x10)
            adjusted_f_number -= modulation;
        else
            adjusted_f_number += modulation;
    }

    adjusted_f_number &= 0x0FFF;

    u32 base_frequency = (adjusted_f_number << block) >> 2;
    u8 detune = op.detune & 0x03;

    if (detune)
    {
        key_code = MIN(key_code, (u8)0x1C);
        u8 sum = (key_code >> 2) + 9 + ((detune == 3) | (detune & 0x02));
        u8 detune_value = k_ym3438_detune[((sum & 0x01) << 2) | (key_code & 0x03)] >> (9 - (sum >> 1));

        if (op.detune & 0x04)
            base_frequency -= detune_value;
        else
            base_frequency += detune_value;

        base_frequency &= 0x1FFFF;
    }

    if (op.multiple == 0)
        base_frequency >>= 1;
    else
        base_frequency *= op.multiple;

    return base_frequency & YM3438_PHASE_MASK;
}

void YM3438::GetOperatorFrequency(int channel_index, int operator_index, u16& f_number, u8& block, u8& key_code) const
{
    const YM3438_Channel& channel = m_channels[channel_index];

    if (channel_index == 2 && m_channel_3_mode != 0 && operator_index < 3)
    {
        f_number = channel.special_f_number[operator_index];
        block = channel.special_block[operator_index];
        key_code = channel.special_key_code[operator_index];
    }
    else
    {
        f_number = channel.f_number;
        block = channel.block;
        key_code = channel.key_code;
    }
}

u8 YM3438::GetOperatorKeyCode(int channel_index, int operator_index) const
{
    const YM3438_Channel& channel = m_channels[channel_index];

    if (channel_index == 2 && m_channel_3_mode != 0 && operator_index < 3)
        return channel.special_key_code[operator_index];

    return channel.key_code;
}

u8 YM3438::CalculateKeyCode(u16 f_number, u8 block) const
{
    return (block << 2) | k_ym3438_note[(f_number >> 7) & 0x0F];
}

u8 YM3438::GetLFOAmplitude() const
{
    u8 amplitude = m_lfo_counter & 0x40 ? m_lfo_counter & 0x3F : m_lfo_counter ^ 0x3F;
    return amplitude << 1;
}

u8 YM3438::GetLFOPhase() const
{
    return m_lfo_enabled ? m_lfo_counter >> 2 : 0;
}

void YM3438::Write(u8 port, u8 value)
{
    Synchronize();
    port &= 0x03;

    if ((port & 0x01) == 0)
    {
        m_address = ((port & 0x02) << 7) | value;
        return;
    }

    m_busy_cycles = YM3438_BUSY_CYCLES;

    // FM data uses the address latch's bank; mode data must be written through bank 0
    if ((port & 0x02) && m_address < 0x30)
        return;

    WriteRegister(m_address, value);
}

u8 YM3438::Read(u8 port)
{
    // Address ports return live status, data ports return the last status latched
    if ((port & 0x01) == 0)
    {
        Synchronize();
        m_status = (m_busy_cycles ? 0x80 : 0) | (m_timer_b_flag << 1) | m_timer_a_flag;
    }

    return m_status;
}

void YM3438::WriteRegister(u16 address, u8 value)
{
    int bank = (address >> 8) & 0x01;
    u8 reg = address & 0xFF;

    if (bank == 0 && reg >= 0x21 && reg <= 0x2C)
    {
        m_registers[0][reg] = value;
        WriteModeRegister(reg, value);
        return;
    }

    if (reg >= 0x30 && reg <= 0x9F)
    {
        if ((reg & 0x03) == 0x03)
            return;

        m_registers[bank][reg] = value;
        WriteOperatorRegister(bank, reg, value);
        return;
    }

    if (reg >= 0xA0 && reg <= 0xB6)
    {
        if ((reg & 0x03) == 0x03)
            return;

        m_registers[bank][reg] = value;
        WriteChannelRegister(bank, reg, value);
    }
}

void YM3438::WriteModeRegister(u8 address, u8 value)
{
    switch (address)
    {
        case 0x22:
            m_lfo_enabled = (value >> 3) & 0x01;
            m_lfo_frequency = value & 0x07;

            if (!m_lfo_enabled)
                m_lfo_counter = 0;

            m_lfo_phase_changed = 1;
            break;
        case 0x24:
            m_timer_a_register = (m_timer_a_register & 0x0003) | ((u16)value << 2);
            break;
        case 0x25:
            m_timer_a_register = (m_timer_a_register & 0x03FC) | (value & 0x03);
            break;
        case 0x26:
            m_timer_b_register = value;
            break;
        case 0x27:
            WriteTimerControl(value);
            break;
        case 0x28:
        {
            int channel = value & 0x03;

            if (channel == 3)
                break;

            if (value & 0x04)
                channel += 3;

            KeyOnChannel(channel, value >> 4);
            break;
        }

        case 0x2A:
            m_dac_data = ((s16)value - 128) * 2;
            break;
        case 0x2B:
            m_dac_enabled = (value >> 7) & 0x01;
            break;
        default:
            break;
    }
}

void YM3438::WriteOperatorRegister(int bank, u8 address, u8 value)
{
    int channel = (address & 0x03) + bank * 3;
    int slot = k_ym3438_slot_map[(address >> 2) & 0x03];
    YM3438_Operator& op = m_channels[channel].operators[slot];

    switch (address & 0xF0)
    {
        case 0x30:
            op.detune = (value >> 4) & 0x07;
            op.multiple = value & 0x0F;
            m_channels[channel].phase_dirty = 1;
            break;
        case 0x40:
            op.total_level = value & 0x7F;
            break;
        case 0x50:
            op.key_scale = (value >> 6) & 0x03;
            op.attack_rate = value & 0x1F;
            break;
        case 0x60:
            op.decay_rate = value & 0x1F;
            op.amplitude_modulation_enabled = (value >> 7) & 0x01;
            break;
        case 0x70:
            op.sustain_rate = value & 0x1F;
            break;
        case 0x80:
            op.sustain_level = (value >> 4) & 0x0F;
            op.release_rate = value & 0x0F;
            break;
        case 0x90:
            op.ssg_envelope = value & 0x0F;
            break;
        default:
            break;
    }
}

void YM3438::WriteChannelRegister(int bank, u8 address, u8 value)
{
    int channel = (address & 0x03) + bank * 3;
    u8 group = address & 0xFC;

    switch (group)
    {
        case 0xA0:
            m_channels[channel].f_number = value | ((m_f_number_high & 0x07) << 8);
            m_channels[channel].block = (m_f_number_high >> 3) & 0x07;
            m_channels[channel].key_code = CalculateKeyCode(m_channels[channel].f_number, m_channels[channel].block);
            m_channels[channel].phase_dirty = 1;
            break;
        case 0xA4:
            m_f_number_high = value;
            break;
        case 0xA8:
        {
            if (bank != 0)
                break;

            int special_slot = address & 0x03;

            if (special_slot == 0)
                special_slot = 2;
            else if (special_slot == 1)
                special_slot = 0;
            else
                special_slot = 1;

            u16 f_number = value | ((m_special_f_number_high & 0x07) << 8);
            u8 block = (m_special_f_number_high >> 3) & 0x07;
            m_channels[2].special_f_number[special_slot] = f_number;
            m_channels[2].special_block[special_slot] = block;
            m_channels[2].special_key_code[special_slot] = CalculateKeyCode(f_number, block);
            m_channels[2].phase_dirty = 1;
            break;
        }

        case 0xAC:
            m_special_f_number_high = value;
            break;
        case 0xB0:
            m_channels[channel].algorithm = value & 0x07;
            m_channels[channel].feedback = (value >> 3) & 0x07;
            break;
        case 0xB4:
            m_channels[channel].phase_modulation = value & 0x07;
            m_channels[channel].amplitude_modulation = (value >> 4) & 0x03;
            m_channels[channel].pan_right = (value >> 6) & 0x01;
            m_channels[channel].pan_left = (value >> 7) & 0x01;
            m_channels[channel].phase_dirty = 1;
            break;
        default:
            break;
    }
}

void YM3438::WriteTimerControl(u8 value)
{
    u8 timer_a_load = value & 0x01;
    u8 timer_b_load = (value >> 1) & 0x01;

    if (value & 0x10)
        m_timer_a_flag = 0;

    if (value & 0x20)
        m_timer_b_flag = 0;

    m_timer_a_enable = (value >> 2) & 0x01;
    m_timer_b_enable = (value >> 3) & 0x01;

    if (timer_a_load && !m_timer_a_load)
    {
        m_timer_a_counter = m_timer_a_register;

        if ((value & 0xC0) == 0x80)
            m_csm_key_pending = 1;
    }

    if (timer_b_load && !m_timer_b_load)
    {
        m_timer_b_counter = m_timer_b_register;
    }

    m_timer_a_load = timer_a_load;
    m_timer_b_load = timer_b_load;

    u8 old_mode = m_channel_3_mode;
    m_channel_3_mode = (value >> 6) & 0x03;

    if (old_mode != m_channel_3_mode)
        m_channels[2].phase_dirty = 1;

    if (old_mode == 2 && m_channel_3_mode != 2)
        KeyOffCSM();
}

void YM3438::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void YM3438::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void YM3438::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE_ARRAY(serializer, m_registers[0], sizeof(m_registers));
    G_SERIALIZE(serializer, m_address);
    G_SERIALIZE(serializer, m_f_number_high);
    G_SERIALIZE(serializer, m_special_f_number_high);
    G_SERIALIZE(serializer, m_channel_3_mode);
    G_SERIALIZE(serializer, m_dac_enabled);
    G_SERIALIZE(serializer, m_dac_data);
    G_SERIALIZE(serializer, m_lfo_enabled);
    G_SERIALIZE(serializer, m_lfo_frequency);
    G_SERIALIZE(serializer, m_lfo_counter);
    G_SERIALIZE(serializer, m_lfo_quotient);
    G_SERIALIZE(serializer, m_timer_a_register);
    G_SERIALIZE(serializer, m_timer_a_counter);
    G_SERIALIZE(serializer, m_timer_b_register);
    G_SERIALIZE(serializer, m_timer_b_counter);
    G_SERIALIZE(serializer, m_timer_b_prescaler);
    G_SERIALIZE(serializer, m_timer_a_load);
    G_SERIALIZE(serializer, m_timer_b_load);
    G_SERIALIZE(serializer, m_timer_a_enable);
    G_SERIALIZE(serializer, m_timer_b_enable);
    G_SERIALIZE(serializer, m_timer_a_flag);
    G_SERIALIZE(serializer, m_timer_b_flag);
    G_SERIALIZE(serializer, m_csm_key_pending);
    G_SERIALIZE(serializer, m_csm_key_active);
    G_SERIALIZE(serializer, m_envelope_counter);
    G_SERIALIZE(serializer, m_envelope_divider);
    G_SERIALIZE(serializer, m_native_cycle);
    G_SERIALIZE(serializer, m_elapsed_cycles);
    G_SERIALIZE(serializer, m_busy_cycles);
    G_SERIALIZE(serializer, m_status);
    G_SERIALIZE(serializer, m_left_sample);
    G_SERIALIZE(serializer, m_right_sample);
    G_SERIALIZE(serializer, m_previous_left_sample);
    G_SERIALIZE(serializer, m_previous_right_sample);

    for (int channel = 0; channel < YM3438_CHANNEL_COUNT; channel++)
    {
        YM3438_Channel& ch = m_channels[channel];
        G_SERIALIZE(serializer, ch.f_number);
        G_SERIALIZE(serializer, ch.block);
        G_SERIALIZE_ARRAY(serializer, ch.special_f_number, 3);
        G_SERIALIZE_ARRAY(serializer, ch.special_block, 3);
        G_SERIALIZE(serializer, ch.algorithm);
        G_SERIALIZE(serializer, ch.feedback);
        G_SERIALIZE(serializer, ch.amplitude_modulation);
        G_SERIALIZE(serializer, ch.phase_modulation);
        G_SERIALIZE(serializer, ch.pan_left);
        G_SERIALIZE(serializer, ch.pan_right);
        G_SERIALIZE(serializer, ch.s1_key_written);
        G_SERIALIZE(serializer, ch.s1_key_register);
        G_SERIALIZE_ARRAY(serializer, ch.feedback_output, 2);
        G_SERIALIZE(serializer, ch.memory_output);
        G_SERIALIZE(serializer, ch.output);

        for (int operator_index = 0; operator_index < YM3438_OPERATOR_COUNT; operator_index++)
        {
            YM3438_Operator& op = ch.operators[operator_index];
            G_SERIALIZE(serializer, op.phase);
            G_SERIALIZE(serializer, op.envelope);
            G_SERIALIZE(serializer, op.state);
            G_SERIALIZE(serializer, op.detune);
            G_SERIALIZE(serializer, op.multiple);
            G_SERIALIZE(serializer, op.total_level);
            G_SERIALIZE(serializer, op.key_scale);
            G_SERIALIZE(serializer, op.attack_rate);
            G_SERIALIZE(serializer, op.decay_rate);
            G_SERIALIZE(serializer, op.amplitude_modulation_enabled);
            G_SERIALIZE(serializer, op.sustain_rate);
            G_SERIALIZE(serializer, op.sustain_level);
            G_SERIALIZE(serializer, op.release_rate);
            G_SERIALIZE(serializer, op.ssg_envelope);
            G_SERIALIZE(serializer, op.key_on);
            G_SERIALIZE(serializer, op.manual_key_on);
            G_SERIALIZE(serializer, op.key_on_pending);
            G_SERIALIZE(serializer, op.ssg_direction);
        }
    }
}

void YM3438::SanitizeState()
{
    m_address &= 0x01FF;
    m_channel_3_mode &= 0x03;
    m_dac_enabled &= 0x01;
    m_lfo_enabled &= 0x01;
    m_lfo_frequency &= 0x07;
    m_lfo_counter &= 0x7F;
    m_lfo_phase_changed = 0;
    m_timer_a_register &= 0x03FF;
    m_timer_a_counter &= 0x03FF;
    m_timer_b_counter &= 0x00FF;
    m_timer_b_prescaler &= 0x0F;
    m_timer_a_load &= 0x01;
    m_timer_b_load &= 0x01;
    m_timer_a_enable &= 0x01;
    m_timer_b_enable &= 0x01;
    m_timer_a_flag &= 0x01;
    m_timer_b_flag &= 0x01;
    m_csm_key_pending &= 0x01;
    m_csm_key_active &= 0x01;
    m_envelope_counter &= 0x0FFF;
    m_envelope_divider %= 3;
    m_native_cycle %= YM3438_NATIVE_SAMPLE_CYCLES;
    m_busy_cycles = MIN(m_busy_cycles, (u32)YM3438_BUSY_CYCLES);
    m_status &= 0x83;

    for (int channel = 0; channel < YM3438_CHANNEL_COUNT; channel++)
    {
        YM3438_Channel& ch = m_channels[channel];
        ch.f_number &= 0x07FF;
        ch.block &= 0x07;
        ch.key_code = CalculateKeyCode(ch.f_number, ch.block);
        ch.algorithm &= 0x07;
        ch.feedback &= 0x07;
        ch.amplitude_modulation &= 0x03;
        ch.phase_modulation &= 0x07;
        ch.pan_left &= 0x01;
        ch.pan_right &= 0x01;
        ch.s1_key_written &= 0x01;
        ch.s1_key_register &= 0x01;
        ch.phase_dirty = 1;

        for (int i = 0; i < 3; i++)
        {
            ch.special_f_number[i] &= 0x07FF;
            ch.special_block[i] &= 0x07;
            ch.special_key_code[i] = CalculateKeyCode(ch.special_f_number[i], ch.special_block[i]);
        }

        for (int operator_index = 0; operator_index < YM3438_OPERATOR_COUNT; operator_index++)
        {
            YM3438_Operator& op = ch.operators[operator_index];
            op.phase &= YM3438_PHASE_MASK;
            op.envelope = MIN(op.envelope, (u16)YM3438_ENVELOPE_MAX);

            if (op.state > YM3438_ENVELOPE_RELEASE)
                op.state = YM3438_ENVELOPE_RELEASE;

            op.detune &= 0x07;
            op.multiple &= 0x0F;
            op.total_level &= 0x7F;
            op.key_scale &= 0x03;
            op.attack_rate &= 0x1F;
            op.decay_rate &= 0x1F;
            op.amplitude_modulation_enabled &= 0x01;
            op.sustain_rate &= 0x1F;
            op.sustain_level &= 0x0F;
            op.release_rate &= 0x0F;
            op.ssg_envelope &= 0x0F;
            op.key_on &= 0x01;
            op.manual_key_on &= 0x01;
            op.key_on_pending &= 0x01;
            op.ssg_direction &= 0x01;
        }
    }
}
