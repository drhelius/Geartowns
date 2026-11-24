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
#ifndef MIDI_CARD_H
#define MIDI_CARD_H

#include <iostream>
#include "../common/common.h"
#include "../system/i8251.h"
#include "../system/i8253.h"

#define MIDI_CARD_OUTPUTS 2

class MidiOut;
class PIC;
class Scheduler;
class StateSerializer;
class TraceLogger;

class MidiCard
{
public:
    struct MidiCard_State
    {
        u8 output_mask;
        u8 input_mask;
        u8 timer_mask;
        u8 fifo_control;
        u32 bytes[MIDI_CARD_OUTPUTS];
    };

public:
    MidiCard();
    ~MidiCard();
    void Init(Scheduler* scheduler, PIC* pic, MidiOut* midi_out);
    void SetTraceLogger(TraceLogger* trace_logger);
    void SetFitted(bool fitted);
    void Reset();
    void Synchronize(u64 clocks);
    void HandleEvent(u64 clocks);
    u8 Read(u16 port, u64 clocks);
    u8 Peek(u16 port, u64 clocks) const;
    void Write(u16 port, u8 value, u64 clocks);
    bool IsFitted() const;
    u8 GetOutputRequests() const;
    u8 GetTimerRequests(u64 clocks) const;
    I8251* GetUART(int output);
    I8253* GetTimer();
    MidiCard_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void UpdateIRQ(u64 clocks);
    void UpdateNextEvent(u64 clocks);
    void TraceRegister(u16 port, u8 value);
    void TraceByte(int output, u8 value);
    void TraceIRQ(int line, u8 causes);
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    MidiCard_State m_state;
    I8251 m_uart[MIDI_CARD_OUTPUTS];
    I8253 m_timer;
    Scheduler* m_scheduler;
    PIC* m_pic;
    MidiOut* m_midi_out;
    TraceLogger* m_trace_logger;
    bool m_fitted;
    bool m_output_irq;
    bool m_timer_irq;
};

static const int k_midi_card_output_irq = 4;
static const int k_midi_card_timer_irq = 5;

// Both the timer and the x16 baud rate run from 500 kHz
static const u32 k_midi_card_clock_divisor = GT_MACHINE_CLOCK_RATE / 500000;
static const u8 k_midi_card_fifo_enable = 0x01;
static const u8 k_midi_card_fifo_empty = 0x02;
static const u8 k_midi_card_timers = 0x03;

#include "midi_card_inline.h"

#endif /* MIDI_CARD_H */
