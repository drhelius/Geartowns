/*
 * Geartowns - FM Towns Emulator
 * Copyright (C) 2026  Ignacio Sanchez
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 */

#include <math.h>
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
    m_clock_rate = GT_YM3438_CLOCK_RATE;
    m_sample_rate = GT_AUDIO_SAMPLE_RATE;
    InitTables();
    Reset();
}

YM3438::~YM3438()
{
}

void YM3438::Init(u32 clock_rate, u32 sample_rate)
{
    m_clock_rate = clock_rate ? clock_rate : GT_YM3438_CLOCK_RATE;
    m_sample_rate = sample_rate ? sample_rate : GT_AUDIO_SAMPLE_RATE;
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

    m_timer_a_register = 0;
    m_timer_a_counter = 0;
    m_timer_b_register = 0;
    m_timer_b_counter = 0;
    m_timer_a_phase = 0;
    m_timer_b_phase = 0;
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
    m_sample_phase = 0;
    m_elapsed_cycles = 0;
    m_busy_cycles = 0;

    m_left_sample = 0;
    m_right_sample = 0;

    m_buffer_index = 0;
    memset(m_buffer, 0, sizeof(m_buffer));
}

void YM3438::InitTables()
{
    const double pi = 3.1415926535897932384626433832795;
    const double inverse_log_2 = 1.0 / log(2.0);

    for (int i = 0; i < 256; i++)
    {
        double angle = ((double)i + 0.5) * pi / 512.0;
        double logarithm = -log(sin(angle)) * inverse_log_2 * 256.0;
        double exponential = (pow(2.0, (double)i / 256.0) - 1.0) * 1024.0;
        m_log_sine_table[i] = (u16)(logarithm + 0.5);
        m_exp_table[i] = (u16)(exponential + 0.5);
    }
}

void YM3438::Synchronize()
{
    u64 cycles = m_elapsed_cycles;
    m_elapsed_cycles = 0;
    RunCycles(cycles);
}

void YM3438::RunCycles(u64 cycles)
{
    while (cycles > 0)
    {
        u64 step = YM3438_NATIVE_SAMPLE_CYCLES - m_native_cycle;

        if (m_sample_rate > 0)
        {
            u64 sample_cycles = (m_clock_rate - m_sample_phase + m_sample_rate - 1) / m_sample_rate;

            if (sample_cycles == 0)
                sample_cycles = 1;

            if (sample_cycles < step)
                step = sample_cycles;
        }

        if (step > cycles)
            step = cycles;

        AdvanceTimers((u32)step);

        if (m_busy_cycles > step)
            m_busy_cycles -= (u32)step;
        else
            m_busy_cycles = 0;

        m_native_cycle += (u32)step;
        m_sample_phase += step * m_sample_rate;
        cycles -= step;

        if (m_native_cycle >= YM3438_NATIVE_SAMPLE_CYCLES)
        {
            m_native_cycle -= YM3438_NATIVE_SAMPLE_CYCLES;
            GenerateNativeSample();
        }

        while (m_sample_phase >= m_clock_rate)
        {
            m_sample_phase -= m_clock_rate;
            WriteSample(m_left_sample, m_right_sample);
        }
    }
}

void YM3438::AdvanceTimers(u32 cycles)
{
    if (m_timer_a_load)
    {
        m_timer_a_phase += cycles;
        u32 ticks = m_timer_a_phase / 144;
        m_timer_a_phase %= 144;
        AdvanceTimerA(ticks);
    }

    // Timer B's divide-by-16 prescaler keeps running while its counter is stopped
    m_timer_b_phase += cycles;
    u32 ticks = m_timer_b_phase / 2304;
    m_timer_b_phase %= 2304;

    if (m_timer_b_load)
        AdvanceTimerB(ticks);
}

void YM3438::AdvanceTimerA(u32 ticks)
{
    while (ticks > 0)
    {
        u32 remaining = 1024 - m_timer_a_counter;

        if (ticks < remaining)
        {
            m_timer_a_counter = (u16)(m_timer_a_counter + ticks);
            return;
        }

        ticks -= remaining;
        m_timer_a_counter = m_timer_a_register;
        TimerAOverflow();
    }
}

void YM3438::AdvanceTimerB(u32 ticks)
{
    while (ticks > 0)
    {
        u32 remaining = 256 - m_timer_b_counter;

        if (ticks < remaining)
        {
            m_timer_b_counter = (u16)(m_timer_b_counter + ticks);
            return;
        }

        ticks -= remaining;
        m_timer_b_counter = m_timer_b_register;

        if (m_timer_b_enable)
            m_timer_b_flag = 1;
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
    // Consecutive Timer A pulses keep the CSM key input asserted
    m_csm_key_active = m_csm_key_pending;

    m_csm_key_pending = 0;

    for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
    {
        YM3438_Operator& op = m_channels[2].operators[i];
        SetKeyState(op, op.manual_key_on || m_csm_key_active, 2, i);
    }

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

    left = CLAMP(left * 16, -32768, 32767);
    right = CLAMP(right * 16, -32768, 32767);
    m_left_sample = (s16)left;
    m_right_sample = (s16)right;

    UpdateLFO();
}

void YM3438::UpdateLFO()
{
    if (!m_lfo_enabled)
        m_lfo_counter = 0;

    m_lfo_quotient++;

    if ((m_lfo_quotient & k_ym3438_lfo_cycles[m_lfo_frequency]) == k_ym3438_lfo_cycles[m_lfo_frequency])
    {
        m_lfo_quotient = 0;

        if (m_lfo_enabled)
            m_lfo_counter = (m_lfo_counter + 1) & 0x7F;
    }
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
        ((op.state == YM3438_ENVELOPE_SUSTAIN && op.sustain_rate == 0 && !envelope_off) ||
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
                else if (rate < 62 && increment > 0)
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

    int cycle = (m_envelope_counter >> shift) & 0x07;

    if (rate >= 48)
        cycle = (m_envelope_counter - 1) & 0x07;

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

    u16 f_number = 0;
    u8 block = 0;
    GetOperatorFrequency(channel, operator_index, f_number, block);
    u8 key_code = CalculateKeyCode(f_number, block);
    u8 key_scale = key_code >> (op.key_scale ^ 0x03);
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

    return MIN(envelope, (u16)YM3438_ENVELOPE_MAX);
}

bool YM3438::HandleSSGEnvelope(YM3438_Operator& op)
{
    op.ssg_holding = 0;

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

    op.ssg_holding = op.state != YM3438_ENVELOPE_ATTACK;
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
            op.ssg_holding = 0;

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
        op.state = YM3438_ENVELOPE_RELEASE;
        op.ssg_direction = 0;
        op.ssg_holding = 0;
    }
}

void YM3438::KeyOnChannel(int channel, u8 slots)
{
    if (channel < 0 || channel >= YM3438_CHANNEL_COUNT)
        return;

    for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
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

    for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
    {
        YM3438_Operator& op = channel.operators[i];
        op.phase = (op.phase + CalculatePhaseIncrement(channel_index, i)) & YM3438_PHASE_MASK;
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
    u32 level = m_log_sine_table[quarter] + (attenuation << 2);
    level = MIN(level, 0x1FFFU);

    u32 output = ((m_exp_table[(level & 0xFF) ^ 0xFF] | 0x400) << 2) >> (level >> 8);
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
    GetOperatorFrequency(channel_index, operator_index, f_number, block);

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
        u8 key_code = CalculateKeyCode(f_number, block);
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

void YM3438::GetOperatorFrequency(int channel_index, int operator_index, u16& f_number, u8& block) const
{
    const YM3438_Channel& channel = m_channels[channel_index];

    if (channel_index == 2 && m_channel_3_mode != 0 && operator_index < 3)
    {
        f_number = channel.special_f_number[operator_index];
        block = channel.special_block[operator_index];
    }
    else
    {
        f_number = channel.f_number;
        block = channel.block;
    }
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

void YM3438::WriteSample(s16 left, s16 right)
{
    if (m_buffer_index < 0 || m_buffer_index + 1 >= GT_AUDIO_BUFFER_SIZE)
    {
        Error("YM3438 audio buffer overflow");
        m_buffer_index = 0;
    }

    m_buffer[m_buffer_index++] = left;
    m_buffer[m_buffer_index++] = right;
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
    UNUSED(port);
    Synchronize();
    return (m_busy_cycles ? 0x80 : 0) | (m_timer_b_flag << 1) | m_timer_a_flag;
}

bool YM3438::IsIRQAsserted()
{
    Synchronize();
    return m_timer_a_flag || m_timer_b_flag;
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

            m_channels[2].special_f_number[special_slot] = value | ((m_special_f_number_high & 0x07) << 8);
            m_channels[2].special_block[special_slot] = (m_special_f_number_high >> 3) & 0x07;
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
        m_timer_a_phase = 0;

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

    if (old_mode == 2 && m_channel_3_mode != 2)
        KeyOffCSM();
}

int YM3438::EndFrame(s16* sample_buffer)
{
    Synchronize();
    int samples = 0;

    if (IsValidPointer(sample_buffer))
    {
        samples = m_buffer_index;
        memcpy(sample_buffer, m_buffer, samples * sizeof(s16));
    }

    m_buffer_index = 0;
    return samples;
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
    G_SERIALIZE(serializer, m_timer_a_phase);
    G_SERIALIZE(serializer, m_timer_b_phase);
    G_SERIALIZE(serializer, m_timer_a_load);
    G_SERIALIZE(serializer, m_timer_b_load);
    G_SERIALIZE(serializer, m_timer_a_enable);
    G_SERIALIZE(serializer, m_timer_b_enable);
    G_SERIALIZE(serializer, m_timer_a_flag);
    G_SERIALIZE(serializer, m_timer_b_flag);
    G_SERIALIZE(serializer, m_csm_key_pending);
    G_SERIALIZE(serializer, m_envelope_counter);
    G_SERIALIZE(serializer, m_envelope_divider);
    G_SERIALIZE(serializer, m_native_cycle);
    G_SERIALIZE(serializer, m_sample_phase);
    G_SERIALIZE(serializer, m_elapsed_cycles);
    G_SERIALIZE(serializer, m_busy_cycles);
    G_SERIALIZE(serializer, m_clock_rate);
    G_SERIALIZE(serializer, m_sample_rate);
    G_SERIALIZE(serializer, m_left_sample);
    G_SERIALIZE(serializer, m_right_sample);
    G_SERIALIZE_ARRAY(serializer, m_buffer, GT_AUDIO_BUFFER_SIZE);
    G_SERIALIZE(serializer, m_buffer_index);

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
            G_SERIALIZE(serializer, op.ssg_direction);
            G_SERIALIZE(serializer, op.ssg_holding);
        }
    }

    G_SERIALIZE(serializer, m_csm_key_active);

    for (int channel = 0; channel < YM3438_CHANNEL_COUNT; channel++)
    {
        for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
        {
            G_SERIALIZE(serializer, m_channels[channel].operators[i].manual_key_on);
            G_SERIALIZE(serializer, m_channels[channel].operators[i].key_on_pending);
        }
    }
}

void YM3438::SanitizeState()
{
    if (m_clock_rate == 0)
        m_clock_rate = GT_YM3438_CLOCK_RATE;

    if (m_sample_rate == 0)
        m_sample_rate = GT_AUDIO_SAMPLE_RATE;

    m_address &= 0x01FF;
    m_channel_3_mode &= 0x03;
    m_dac_enabled &= 0x01;
    m_lfo_enabled &= 0x01;
    m_lfo_frequency &= 0x07;
    m_lfo_counter &= 0x7F;
    m_timer_a_register &= 0x03FF;
    m_timer_a_counter &= 0x03FF;
    m_timer_b_counter &= 0x00FF;
    m_timer_a_phase %= 144;
    m_timer_b_phase %= 2304;
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
    m_sample_phase %= m_clock_rate;
    m_busy_cycles = MIN(m_busy_cycles, (u32)YM3438_BUSY_CYCLES);
    m_buffer_index = CLAMP(m_buffer_index, 0, GT_AUDIO_BUFFER_SIZE);
    m_buffer_index &= ~1;

    for (int channel = 0; channel < YM3438_CHANNEL_COUNT; channel++)
    {
        YM3438_Channel& ch = m_channels[channel];
        ch.f_number &= 0x07FF;
        ch.block &= 0x07;
        ch.algorithm &= 0x07;
        ch.feedback &= 0x07;
        ch.amplitude_modulation &= 0x03;
        ch.phase_modulation &= 0x07;
        ch.pan_left &= 0x01;
        ch.pan_right &= 0x01;

        for (int i = 0; i < 3; i++)
        {
            ch.special_f_number[i] &= 0x07FF;
            ch.special_block[i] &= 0x07;
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
            op.ssg_holding &= 0x01;
        }
    }
}
