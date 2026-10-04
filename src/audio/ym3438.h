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

#ifndef YM3438_H
#define YM3438_H

#include <iostream>
#include "../common/common.h"

#define YM3438_CHANNEL_COUNT 6
#define YM3438_OPERATOR_COUNT 4

class StateSerializer;

class YM3438
{
public:
    YM3438();
    ~YM3438();
    void Init();
    void Reset();
    void Clock(u32 cycles);
    void Synchronize();
    void Sample(s16& left, s16& right);
    void Write(u8 port, u8 value);
    u8 Read(u8 port);
    bool IsIRQAsserted();

    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

    u16 GetSelectedAddress() const;
    u8 GetRegister(u16 address) const;

private:
    enum YM3438_Envelope_State
    {
        YM3438_ENVELOPE_ATTACK = 0,
        YM3438_ENVELOPE_DECAY,
        YM3438_ENVELOPE_SUSTAIN,
        YM3438_ENVELOPE_RELEASE
    };

    struct YM3438_Operator
    {
        u32 phase;
        u32 phase_increment;
        u16 envelope;
        u8 state;

        u8 detune;
        u8 multiple;
        u8 total_level;
        u8 key_scale;

        u8 attack_rate;
        u8 decay_rate;
        u8 amplitude_modulation_enabled;

        u8 sustain_rate;
        u8 sustain_level;
        u8 release_rate;

        u8 ssg_envelope;

        u8 key_on;
        u8 manual_key_on;
        u8 key_on_pending;

        u8 ssg_direction;
    };

    struct YM3438_Channel
    {
        YM3438_Operator operators[YM3438_OPERATOR_COUNT];

        u16 f_number;
        u8 block;
        u8 key_code;

        u16 special_f_number[3];
        u8 special_block[3];
        u8 special_key_code[3];

        u8 algorithm;
        u8 feedback;
        u8 amplitude_modulation;
        u8 phase_modulation;
        u8 pan_left;
        u8 pan_right;

        u8 s1_key_written;
        u8 s1_key_register;
        u8 phase_dirty;

        s16 feedback_output[2];
        s32 memory_output;
        s16 output;
    };

private:
    void ResetOperator(YM3438_Operator& op);
    void ResetChannel(YM3438_Channel& channel);
    void RunCycles(u64 cycles);
    u64 GetCyclesToTimerFlag() const;

    void ClockTimers();
    void TimerAOverflow();

    void GenerateNativeSample();
    void UpdateKeyStates();
    void UpdateLFO();
    void UpdateEnvelopes();
    void UpdateEnvelope(YM3438_Operator& op, int channel, int operator_index);
    u8 GetEnvelopeIncrement(u8 rate) const;
    u8 GetEnvelopeRate(const YM3438_Operator& op, int channel, int operator_index) const;
    u16 GetEnvelopeOutput(const YM3438_Operator& op) const;
    bool HandleSSGEnvelope(YM3438_Operator& op);

    void SetKeyState(YM3438_Operator& op, bool key_on, int channel, int operator_index);
    void KeyOnChannel(int channel, u8 slots);
    void KeyOffCSM();

    s16 CalculateChannel(int channel);
    s16 CalculateOperator(int channel, int operator_index, s32 modulation);
    u32 CalculatePhaseIncrement(int channel, int operator_index) const;
    void GetOperatorFrequency(int channel, int operator_index, u16& f_number, u8& block, u8& key_code) const;
    u8 GetOperatorKeyCode(int channel, int operator_index) const;
    u8 CalculateKeyCode(u16 f_number, u8 block) const;
    u8 GetLFOAmplitude() const;
    u8 GetLFOPhase() const;

    void WriteRegister(u16 address, u8 value);
    void WriteModeRegister(u8 address, u8 value);
    void WriteOperatorRegister(int bank, u8 address, u8 value);
    void WriteChannelRegister(int bank, u8 address, u8 value);
    void WriteTimerControl(u8 value);

    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    YM3438_Channel m_channels[YM3438_CHANNEL_COUNT];
    u8 m_registers[2][256];
    u16 m_address;
    u8 m_f_number_high;
    u8 m_special_f_number_high;
    u8 m_channel_3_mode;

    u8 m_dac_enabled;
    s16 m_dac_data;

    u8 m_lfo_enabled;
    u8 m_lfo_frequency;
    u8 m_lfo_counter;
    u8 m_lfo_quotient;
    u8 m_lfo_phase_changed;

    u16 m_timer_a_register;
    u16 m_timer_a_counter;
    u8 m_timer_b_register;
    u16 m_timer_b_counter;
    u8 m_timer_b_prescaler;
    u8 m_timer_a_load;
    u8 m_timer_b_load;
    u8 m_timer_a_enable;
    u8 m_timer_b_enable;
    u8 m_timer_a_flag;
    u8 m_timer_b_flag;

    u8 m_csm_key_pending;
    u8 m_csm_key_active;

    u32 m_envelope_counter;
    u8 m_envelope_divider;

    u32 m_native_cycle;
    u64 m_elapsed_cycles;
    u32 m_busy_cycles;
    u8 m_status;

    s16 m_left_sample;
    s16 m_right_sample;
    s16 m_previous_left_sample;
    s16 m_previous_right_sample;
};

static const int k_ym3438_native_sample_cycles = 144;
static const int k_ym3438_busy_cycles = 192;
static const u32 k_ym3438_phase_mask = 0xFFFFF;
static const int k_ym3438_envelope_max = 0x3FF;
static const int k_ym3438_ssg_envelope_max = 0x200;

#include "ym3438_inline.h"

#endif /* YM3438_H */
