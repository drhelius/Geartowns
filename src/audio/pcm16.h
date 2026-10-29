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

#ifndef PCM16_H
#define PCM16_H

#include <iostream>
#include "../common/common.h"

#define PCM16_BUFFER_SIZE 15
#define PCM16_RATE_COUNT 16
#define PCM16_IRQ_ROUTES 5

class Memory;
class PIC;
class StateSerializer;
class TraceLogger;

class PCM16
{
public:
    enum PCM16_Format
    {
        PCM16_FORMAT_8BIT_MONO = 0,
        PCM16_FORMAT_8BIT_STEREO,
        PCM16_FORMAT_16BIT_MONO,
        PCM16_FORMAT_16BIT_STEREO,
        PCM16_FORMAT_SND
    };

    struct PCM16_State
    {
        u32 base_address;
        u32 current_address;
        u16 base_count;
        u16 current_count;
        u16 buffer[PCM16_BUFFER_SIZE];
        u8 buffer_head;
        u8 buffer_count;
        u8 bank;
        u8 dma_control;
        bool terminal_count;
        u8 clock;
        u8 mode;
        u8 system_control;
        u8 buffer_control;
        bool overrun;
        bool underrun;
        u8 transfer_control;
        bool input_flag;
        u8 peak;
        u8 threshold;
        u8 level_left;
        u8 level_right;
        u8 data_low;
        u8 data_high;
        u16 word;
        bool high_byte;
        u64 phase;
        u64 reset_clocks;
        u64 ready_clocks;
        s16 input_left;
        s16 input_right;
        s16 left_sample;
        s16 right_sample;
        s16 previous_left_sample;
        s16 previous_right_sample;
    };

public:
    PCM16();
    ~PCM16();
    void Init(PIC* pic, Memory* memory);
    void SetTraceLogger(TraceLogger* trace_logger);
    void SetFitted(bool fitted);
    void Reset(u64 clocks);
    void Clock(u32 clocks, u64 time);
    void Sample(s16& left, s16& right) const;
    void Input(s32 left, s32 right);
    u8 Read(u16 port, u64 clocks);
    u8 Peek(u16 port, u64 clocks) const;
    void Write(u16 port, u8 value, u64 clocks);
    u64 GetClocksToEvent() const;
    bool IsFitted() const;
    bool IsRunning() const;
    bool IsRecording() const;
    bool IsSoftwareTransfer() const;
    bool IsDMAEnabled() const;
    bool IsInputIRQArmed() const;
    PCM16_Format GetFormat() const;
    u32 GetSampleRate() const;
    u8 GetIRQCauses() const;
    int GetIRQLine() const;
    PCM16_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void ResetRegisters(u64 clocks);
    void Tick(u64 time);
    void Play(u64 time);
    void Record(u64 time);
    bool PopWord(u16& word, u64 time);
    void PushWord(u16 word, u64 time);
    void ServiceDMA(u64 time);
    void AdvanceDMA();
    s16 ExpandByte(u8 value) const;
    u8 ConvertSample(s32 sample) const;
    u8 GetLevel(s32 sample) const;
    u64 GetTicksToWord(u32 words, bool high_byte_moves) const;
    u64 GetClocksToTick(u64 ticks) const;
    void SetUnderrun();
    void SetOverrun();
    void UpdateIRQ();
    void TraceRegister(u16 port, u8 value);
    void TraceData(u16 word, bool read);
    void TraceDMAEnd();
    void TraceBufferError();
    void TraceIRQ(u8 causes);
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    PCM16_State m_state;
    PIC* m_pic;
    Memory* m_memory;
    TraceLogger* m_trace_logger;
    bool m_fitted;
    int m_irq_line;
    bool m_irq_asserted;
};

// Sample clocks in half hertz, 5.5125 kHz has a fraction
static const u32 k_pcm16_rates[PCM16_RATE_COUNT] =
{
    96000, 88200, 64000, 44100, 38400, 32000, 22050, 19200, 16000, 11025, 48000, 24000, 0, 0, 0, 0
};

static const int k_pcm16_irq_lines[PCM16_IRQ_ROUTES] = { 4, 5, 10, 14, 15 };
static const u64 k_pcm16_period = 2ULL * GT_MACHINE_CLOCK_RATE;
static const u64 k_pcm16_reset_clocks = GT_MACHINE_CLOCK_RATE / 500000;
static const u64 k_pcm16_ready_clocks = GT_MACHINE_CLOCK_RATE / 100;
static const u8 k_pcm16_reset_clock = 0x04;
static const u8 k_pcm16_reset_mode = 0x07;
static const u8 k_pcm16_reset_system = 0x07;
static const u8 k_pcm16_reset_transfer = 0x03;
static const u8 k_pcm16_dma_autoinit = 0x10;
static const u8 k_pcm16_dma_irq = 0x02;
static const u8 k_pcm16_mode_snd = 0x20;
static const u8 k_pcm16_mode_stereo = 0x10;
static const u8 k_pcm16_mode_16bit = 0x08;
static const u8 k_pcm16_mode_level = 0x07;
static const u8 k_pcm16_system_reset = 0x80;
static const u8 k_pcm16_system_data_port = 0x20;
static const u8 k_pcm16_system_record = 0x10;
static const u8 k_pcm16_system_irq = 0x07;
static const u8 k_pcm16_buffer_irq = 0x80;
static const u8 k_pcm16_transfer_input_irq = 0x20;
static const u8 k_pcm16_transfer_input_flag = 0x10;
static const u8 k_pcm16_transfer_dreq_mask = 0x02;
static const u8 k_pcm16_transfer_buffer_mask = 0x01;
static const u8 k_pcm16_cause_input = 0x40;
static const u8 k_pcm16_cause_buffer = 0x20;
static const u8 k_pcm16_cause_dma = 0x10;

#include "pcm16_inline.h"

#endif /* PCM16_H */
