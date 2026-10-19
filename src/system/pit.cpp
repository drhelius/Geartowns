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

#include "pit.h"
#include "pic.h"
#include "../audio/audio.h"
#include "../common/state_serializer.h"

PIT::PIT()
{
    InitPointer(m_pic);
    InitPointer(m_scheduler);
    InitPointer(m_audio);
    InitPointer(m_trace_logger);
    m_state.timer_latch = 0;
    m_state.timer_enable = 0;
    m_state.sound = false;
    m_state.sound_memory = false;
    m_state.settled_tick = 0;
    m_state.timer2_control = k_pit_timer2_disable;
    m_state.timer2_period = 0;
    m_state.timer2_expiry = GT_NO_EVENT;
    m_features = 0;
}

PIT::~PIT()
{
}

void PIT::Init(PIC* pic, Scheduler* scheduler, Audio* audio)
{
    m_pic = pic;
    m_scheduler = scheduler;
    m_audio = audio;
    m_pit[0].Init();
    m_pit[1].Init();
    Reset();
}

void PIT::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void PIT::SetFeatures(u32 features)
{
    m_features = features;
}

bool PIT::Decodes(u16 port) const
{
    switch (port)
    {
        case 0x0026:
        case 0x0027:
            return (m_features & GT_MACHINE_FEATURE_FREE_RUN_TIMER) != 0;
        case 0x0068:
        case 0x006A:
        case 0x006B:
            return (m_features & GT_MACHINE_FEATURE_INTERVAL_TIMER2) != 0;
        default:
            return true;
    }
}

void PIT::Reset()
{
    m_pit[0].Reset();
    m_pit[1].Reset();
    m_state.timer_latch = 0;
    m_state.timer_enable = 0;
    m_state.sound = false;
    m_state.sound_memory = false;
    m_state.settled_tick = 0;
    m_state.timer2_control = k_pit_timer2_disable;
    m_state.timer2_period = 0;
    m_state.timer2_expiry = GT_NO_EVENT;
    UpdateIRQ();
    UpdateNextEvent();
}

u8 PIT::Read(u16 port, u64 clocks)
{
    if (port < 0x0040)
        return Peek(port, clocks);

    Synchronize(clocks);

    if (port == 0x0060)
        return m_state.timer_latch | (m_state.timer_enable << 2) | (m_state.sound ? 0x10 : 0x00);

    if (port >= 0x0068)
        return ReadTimer2(port);

    int chip = (port >> 4) & 0x01;
    int index = (port >> 1) & 0x03;

    // The control port is write only
    if (index == 3)
        return 0xFF;

    return m_pit[chip].ReadCounter(index, GetTick(chip * 3 + index, clocks));
}

u8 PIT::Peek(u16 port, u64 clocks) const
{
    if (port < 0x0040)
        return Decodes(port) ? (u8)((clocks / k_pit_microsecond_clocks) >> ((port & 0x01) * 8)) : 0xFF;

    if (port == 0x0060)
    {
        u64 tick = GetTick(0, clocks);
        u8 latch = tick > m_state.settled_tick ? GetTimerLatch(tick) : m_state.timer_latch;
        return latch | (m_state.timer_enable << 2) | (m_state.sound ? 0x10 : 0x00);
    }

    if (port >= 0x0068)
    {
        if (!Decodes(port))
            return 0xFF;

        if (port == 0x0068)
            return GetTimer2Status(clocks, NULL);

        return port == 0x006A ? (u8)m_state.timer2_period : (u8)(m_state.timer2_period >> 8);
    }

    int chip = (port >> 4) & 0x01;
    int index = (port >> 1) & 0x03;

    if (index == 3)
        return 0xFF;

    return m_pit[chip].PeekCounter(index, GetTick(chip * 3 + index, clocks));
}

void PIT::Write(u16 port, u8 value, u64 clocks)
{
    Synchronize(clocks);

    if (port == 0x0060)
    {
        bool sound = (value & 0x04) != 0;

        if (sound != m_state.sound)
            m_audio->Synchronize(clocks);

        if ((value & 0x80) != 0)
            m_state.timer_latch &= ~0x01;

        m_state.timer_enable = value & 0x03;
        m_state.sound = sound;

        if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_TIMER)))
            TraceWrite(port, value);

        UpdateIRQ();
        UpdateNextEvent();
        return;
    }

    if (port >= 0x0068)
    {
        WriteTimer2(port, value, clocks);
        return;
    }

    int chip = (port >> 4) & 0x01;
    int index = (port >> 1) & 0x03;
    int counter = index == 3 ? value >> 6 : index;
    u64 tick = GetTick(chip * 3 + counter, clocks);
    bool timeout = chip == 0 && counter < 2;
    bool output = timeout && m_pit[0].GetOutput(counter, tick);

    // The buzzer plays the previous count up to this write
    if (chip == 0 && counter == k_pit_buzzer_channel)
        m_audio->Synchronize(clocks);

    if (index == 3)
        m_pit[chip].WriteControl(value, tick);
    else
        m_pit[chip].WriteCounter(index, value, tick);

    // Loading a channel 1 count acknowledges its timeout
    if (chip == 0 && index == 1)
        m_state.timer_latch &= ~0x02;

    // Reprogramming can raise OUT at once
    if (timeout && !output && m_pit[0].GetOutput(counter, tick))
        m_state.timer_latch |= (u8)(1 << counter);

    if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_TIMER)))
        TraceWrite(port, value);

    UpdateIRQ();
    UpdateNextEvent();
}

u8 PIT::ReadTimer2(u16 port)
{
    if (!Decodes(port))
        return 0xFF;

    if (port != 0x0068)
        return port == 0x006A ? (u8)m_state.timer2_period : (u8)(m_state.timer2_period >> 8);

    u8 status = m_state.timer2_control;
    m_state.timer2_control &= ~(k_pit_timer2_pending | k_pit_timer2_overrun);
    UpdateIRQ();
    UpdateNextEvent();
    return status;
}

void PIT::WriteTimer2(u16 port, u8 value, u64 clocks)
{
    if (!Decodes(port))
        return;

    switch (port)
    {
        case 0x0068:
            if ((value & k_pit_timer2_disable) != 0)
            {
                m_state.timer2_control = k_pit_timer2_disable;
                m_state.timer2_expiry = GT_NO_EVENT;
            }
            else if ((m_state.timer2_control & k_pit_timer2_disable) != 0)
            {
                m_state.timer2_control = 0;
                m_state.timer2_expiry = clocks + GetTimer2Clocks();
            }
            break;
        // A new period applies from the next interval
        case 0x006A:
            m_state.timer2_period = (m_state.timer2_period & 0xFF00) | value;
            break;
        case 0x006B:
            m_state.timer2_period = (u16)((m_state.timer2_period & 0x00FF) | (value << 8));
            break;
    }

    UpdateIRQ();
    UpdateNextEvent();
}

u64 PIT::GetTimer2Clocks() const
{
    return (m_state.timer2_period == 0 ? 0x10000ULL : (u64)m_state.timer2_period) * k_pit_microsecond_clocks;
}

u8 PIT::GetTimer2Status(u64 clocks, u64* expiry) const
{
    u8 status = m_state.timer2_control;
    u64 next = m_state.timer2_expiry;

    if (clocks >= next)
    {
        u64 timeouts = (clocks - next) / GetTimer2Clocks() + 1;
        next += timeouts * GetTimer2Clocks();

        if ((status & k_pit_timer2_pending) != 0 || timeouts > 1)
            status |= k_pit_timer2_overrun;

        status |= k_pit_timer2_pending;
    }

    if (IsValidPointer(expiry))
        *expiry = next;

    return status;
}

void PIT::SettleTimer2(u64 clocks)
{
    m_state.timer2_control = GetTimer2Status(clocks, &m_state.timer2_expiry);
    UpdateIRQ();
    UpdateNextEvent();
}

// Reading CFF98h turns the buzzer on and writing it turns it off
void PIT::SetMemoryBuzzer(bool enabled)
{
    if (enabled == m_state.sound_memory)
        return;

    m_audio->Synchronize(m_scheduler->GetClocks());
    m_state.sound_memory = enabled;
}

void PIT::UpdateIRQ()
{
    bool timer2 = (m_state.timer2_control & (k_pit_timer2_disable | k_pit_timer2_pending)) == k_pit_timer2_pending;
    m_pic->SetIRQLine(0, ((m_state.timer_latch & m_state.timer_enable) != 0) || timer2);
}

void PIT::UpdateNextEvent()
{
    u64 next_event = GT_NO_EVENT;

    for (int i = 0; i < 2; i++)
    {
        u8 mask = (u8)(1 << i);

        // Only an enabled timeout with its latch clear can change IRQ0
        if ((m_state.timer_enable & mask) == 0 || (m_state.timer_latch & mask) != 0)
            continue;

        u64 edge = m_pit[0].GetNextRisingEdge(i, m_state.settled_tick);

        if (edge != k_i8253_no_edge)
            next_event = MIN(next_event, GetTickClocks(edge));
    }

    if ((m_state.timer2_control & (k_pit_timer2_disable | k_pit_timer2_pending)) == 0)
        next_event = MIN(next_event, m_state.timer2_expiry);

    m_scheduler->Schedule(SCHEDULER_EVENT_PIT, next_event);
}

// Channels 0 and 1 latch their timeouts in 0060h, the source of IRQ0
void PIT::TraceTimeout(u8 previous_latch)
{
    GT_Trace_Entry entry = {};
    entry.type = TRACE_TIMER;
    entry.event = TRACE_TIMER_TIMEOUT;
    entry.timer.value = m_state.timer_latch & ~previous_latch;
    entry.timer.latch = m_state.timer_latch;
    entry.timer.enable = m_state.timer_enable;
    entry.timer.sound = m_state.sound ? 1 : 0;
    m_trace_logger->TraceLog(entry);
}

void PIT::TraceWrite(u16 port, u8 value)
{
    int chip = (port >> 4) & 0x01;
    int index = (port >> 1) & 0x03;
    u8 event = port == 0x0060 ? TRACE_TIMER_INTERRUPT_CONTROL : index == 3 ? TRACE_TIMER_CONTROL : TRACE_TIMER_COUNTER;

    if (!m_trace_logger->IsEventEnabled(TRACE_TIMER, event))
        return;

    int counter = event == TRACE_TIMER_CONTROL ? value >> 6 : index;
    GT_Trace_Entry entry = {};
    entry.type = TRACE_TIMER;
    entry.event = event;
    entry.timer.chip = (u8)chip;
    entry.timer.counter = (u8)counter;
    entry.timer.value = value;
    entry.timer.latch = m_state.timer_latch;
    entry.timer.enable = m_state.timer_enable;
    entry.timer.sound = m_state.sound ? 1 : 0;

    if (event != TRACE_TIMER_INTERRUPT_CONTROL && counter < I8253_COUNTER_COUNT)
    {
        const I8253::I8253_Counter& state = m_pit[chip].GetState()->counters[counter];
        entry.timer.reload = state.reload;
        entry.timer.access = state.access;
        entry.timer.mode = state.mode;
        entry.timer.bcd = state.bcd ? 1 : 0;
        entry.timer.complete = !state.write_high && state.programmed ? 1 : 0;
    }

    m_trace_logger->TraceLog(entry);
}

void PIT::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    m_pit[0].SaveState(stream);
    m_pit[1].SaveState(stream);
    G_SERIALIZE(serializer, m_state.timer2_control);
    G_SERIALIZE(serializer, m_state.timer2_period);
    G_SERIALIZE(serializer, m_state.timer2_expiry);
}

void PIT::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    m_pit[0].LoadState(stream);
    m_pit[1].LoadState(stream);

    if (version >= 3)
    {
        G_SERIALIZE(serializer, m_state.timer2_control);
        G_SERIALIZE(serializer, m_state.timer2_period);
        G_SERIALIZE(serializer, m_state.timer2_expiry);
    }
    else
    {
        m_state.timer2_control = k_pit_timer2_disable;
        m_state.timer2_period = 0;
        m_state.timer2_expiry = GT_NO_EVENT;
    }

    SanitizeState();
}

void PIT::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.timer_latch);
    G_SERIALIZE(serializer, m_state.timer_enable);
    G_SERIALIZE(serializer, m_state.sound);
    G_SERIALIZE(serializer, m_state.sound_memory);
    G_SERIALIZE(serializer, m_state.settled_tick);
}

void PIT::SanitizeState()
{
    m_state.timer_latch &= 0x03;
    m_state.timer_enable &= 0x03;
    m_state.timer2_control &= k_pit_timer2_disable | k_pit_timer2_pending | k_pit_timer2_overrun;

    if ((m_state.timer2_control & k_pit_timer2_disable) != 0)
    {
        m_state.timer2_control = k_pit_timer2_disable;
        m_state.timer2_expiry = GT_NO_EVENT;
    }

    UpdateIRQ();
    UpdateNextEvent();
}
