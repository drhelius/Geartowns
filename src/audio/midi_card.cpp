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
#include "midi_card.h"
#include "midi_out.h"
#include "../system/pic.h"
#include "../system/scheduler.h"
#include "../common/state_serializer.h"
#include "../common/trace_logger.h"

MidiCard::MidiCard()
{
    InitPointer(m_scheduler);
    InitPointer(m_pic);
    InitPointer(m_midi_out);
    InitPointer(m_trace_logger);
    m_fitted = false;
    m_output_irq = false;
    m_timer_irq = false;
    memset(&m_state, 0, sizeof(m_state));
}

MidiCard::~MidiCard()
{
}

void MidiCard::Init(Scheduler* scheduler, PIC* pic, MidiOut* midi_out)
{
    m_scheduler = scheduler;
    m_pic = pic;
    m_midi_out = midi_out;

    for (int i = 0; i < MIDI_CARD_OUTPUTS; i++)
        m_uart[i].Init(k_midi_card_clock_divisor);

    m_timer.Init();
    Reset();
}

void MidiCard::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void MidiCard::SetFitted(bool fitted)
{
    m_fitted = fitted;
    UpdateIRQ(m_scheduler->GetClocks());
    UpdateNextEvent(m_scheduler->GetClocks());
}

void MidiCard::Reset()
{
    memset(&m_state, 0, sizeof(m_state));

    for (int i = 0; i < MIDI_CARD_OUTPUTS; i++)
        m_uart[i].Reset();

    m_timer.Reset();
    UpdateIRQ(m_scheduler->GetClocks());
    UpdateNextEvent(m_scheduler->GetClocks());
}

// Output B plays into nothing
void MidiCard::Synchronize(u64 clocks)
{
    for (int i = 0; i < MIDI_CARD_OUTPUTS; i++)
    {
        u8 value = 0;

        while (m_uart[i].Shift(clocks, value))
        {
            m_state.bytes[i]++;

            if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEventEnabled(TRACE_MIDI, TRACE_MIDI_BYTE)))
                TraceByte(i, value);

            if (i == 0)
                m_midi_out->Write(value);
        }
    }
}

void MidiCard::HandleEvent(u64 clocks)
{
    Synchronize(clocks);
    UpdateIRQ(clocks);
    UpdateNextEvent(clocks);
}

u8 MidiCard::Read(u16 port, u64 clocks)
{
    Synchronize(clocks);

    if ((port >= 0x0E74) && (port <= 0x0E76))
        return m_timer.ReadCounter(port & 0x03, clocks / k_midi_card_clock_divisor);

    return Peek(port, clocks);
}

// The requests read inverted, and the ones with no output or input behind them never come
u8 MidiCard::Peek(u16 port, u64 clocks) const
{
    switch (port)
    {
        case 0x0E50:
        case 0x0E54:
            return 0x00;
        case 0x0E51:
        case 0x0E55:
            return m_uart[(port >> 2) & 0x01].ReadStatus();
        case 0x0E53:
            return k_midi_card_fifo_empty;
        case 0x0E70:
            return (u8)~GetOutputRequests();
        case 0x0E73:
            return GetTimerRequests(clocks);
        case 0x0E74:
        case 0x0E75:
        case 0x0E76:
            return m_timer.PeekCounter(port & 0x03, clocks / k_midi_card_clock_divisor);
        default:
            return 0xFF;
    }
}

void MidiCard::Write(u16 port, u8 value, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x0E50:
        case 0x0E54:
            m_uart[(port >> 2) & 0x01].WriteData(value, clocks);
            break;
        case 0x0E51:
        case 0x0E55:
            m_uart[(port >> 2) & 0x01].WriteControl(value, clocks);
            break;
        case 0x0E53:
            m_state.fifo_control = value;
            break;
        case 0x0E70:
            m_state.output_mask = value;
            break;
        case 0x0E71:
            m_state.input_mask = value;
            break;
        case 0x0E73:
            m_state.timer_mask = value & k_midi_card_timers;
            break;
        case 0x0E74:
        case 0x0E75:
        case 0x0E76:
            m_timer.WriteCounter(port & 0x03, value, clocks / k_midi_card_clock_divisor);
            break;
        case 0x0E77:
            m_timer.WriteControl(value, clocks / k_midi_card_clock_divisor);
            break;
        default:
            break;
    }

    if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEventEnabled(TRACE_MIDI, TRACE_MIDI_REGISTER)))
        TraceRegister(port, value);

    UpdateIRQ(clocks);
    UpdateNextEvent(clocks);
}

u8 MidiCard::GetOutputRequests() const
{
    u8 requests = 0;

    for (int i = 0; i < MIDI_CARD_OUTPUTS; i++)
    {
        if (m_uart[i].IsTransmitReady())
            requests = SET_BIT(requests, i);
    }

    return requests & m_state.output_mask;
}

// A new control word lowers the output and so drops the request
u8 MidiCard::GetTimerRequests(u64 clocks) const
{
    u64 tick = clocks / k_midi_card_clock_divisor;
    u8 requests = 0;

    for (int i = 0; i < 2; i++)
    {
        if (m_timer.PeekOutput(i, tick))
            requests = SET_BIT(requests, i);
    }

    return requests & m_state.timer_mask;
}

void MidiCard::UpdateIRQ(u64 clocks)
{
    u8 outputs = m_fitted ? GetOutputRequests() : 0;
    u8 timers = m_fitted ? GetTimerRequests(clocks) : 0;

    if ((outputs != 0) && !m_output_irq)
        TraceIRQ(k_midi_card_output_irq, outputs);

    if ((timers != 0) && !m_timer_irq)
        TraceIRQ(k_midi_card_timer_irq, timers);

    m_output_irq = outputs != 0;
    m_timer_irq = timers != 0;
    m_pic->SetSharedIRQLine(k_midi_card_output_irq, PIC::PIC_SOURCE_MIDI, m_output_irq);
    m_pic->SetSharedIRQLine(k_midi_card_timer_irq, PIC::PIC_SOURCE_MIDI, m_timer_irq);
}

void MidiCard::UpdateNextEvent(u64 clocks)
{
    u64 next = GT_NO_EVENT;

    for (int i = 0; m_fitted && (i < MIDI_CARD_OUTPUTS); i++)
    {
        next = MIN(next, m_uart[i].GetEventClocks());

        if (IS_NOT_SET_BIT(m_state.timer_mask, i))
            continue;

        u64 tick = clocks / k_midi_card_clock_divisor;
        u64 edge = m_timer.GetNextEdge(i, tick);

        if (edge != k_i8253_no_edge)
            next = MIN(next, edge * k_midi_card_clock_divisor);
    }

    m_scheduler->Schedule(SCHEDULER_EVENT_MIDI, next);
}

void MidiCard::TraceRegister(u16 port, u8 value)
{
    GT_Trace_Entry entry = {};
    entry.type = TRACE_MIDI;
    entry.event = TRACE_MIDI_REGISTER;
    entry.midi.port = port;
    entry.midi.value = value;
    m_trace_logger->TraceLog(entry);
}

void MidiCard::TraceByte(int output, u8 value)
{
    GT_Trace_Entry entry = {};
    entry.type = TRACE_MIDI;
    entry.event = TRACE_MIDI_BYTE;
    entry.midi.output = (u8)output;
    entry.midi.value = value;
    m_trace_logger->TraceLog(entry);
}

void MidiCard::TraceIRQ(int line, u8 causes)
{
    if (!IsValidPointer(m_trace_logger) || !m_trace_logger->IsEventEnabled(TRACE_MIDI, TRACE_MIDI_IRQ))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_MIDI;
    entry.event = TRACE_MIDI_IRQ;
    entry.midi.line = (u8)line;
    entry.midi.value = causes;
    m_trace_logger->TraceLog(entry);
}

void MidiCard::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);

    for (int i = 0; i < MIDI_CARD_OUTPUTS; i++)
        m_uart[i].SaveState(stream);

    m_timer.SaveState(stream);
}

void MidiCard::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);

    for (int i = 0; i < MIDI_CARD_OUTPUTS; i++)
        m_uart[i].LoadState(stream);

    m_timer.LoadState(stream);
    SanitizeState();
}

void MidiCard::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.output_mask);
    G_SERIALIZE(serializer, m_state.input_mask);
    G_SERIALIZE(serializer, m_state.timer_mask);
    G_SERIALIZE(serializer, m_state.fifo_control);
    G_SERIALIZE_ARRAY(serializer, m_state.bytes, MIDI_CARD_OUTPUTS);
}

void MidiCard::SanitizeState()
{
    m_state.timer_mask &= k_midi_card_timers;
    UpdateIRQ(m_scheduler->GetClocks());
    UpdateNextEvent(m_scheduler->GetClocks());
}
