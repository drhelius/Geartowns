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
#include "../common/state_serializer.h"

PIT::PIT()
{
    InitPointer(m_pic);
    InitPointer(m_scheduler);
    m_state.timer_latch = 0;
    m_state.timer_enable = 0;
    m_state.sound = false;
    m_state.sound_memory = false;
    m_state.settled_tick = 0;
}

PIT::~PIT()
{
}

void PIT::Init(PIC* pic, Scheduler* scheduler)
{
    m_pic = pic;
    m_scheduler = scheduler;
    m_pit[0].Init();
    m_pit[1].Init();
    Reset();
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
    UpdateIRQ();
    UpdateNextEvent();
}

u8 PIT::Read(u16 port, u64 clocks)
{
    Synchronize(clocks);

    if (port == 0x0060)
        return m_state.timer_latch | (m_state.timer_enable << 2) | (m_state.sound ? 0x10 : 0x00);

    int chip = (port >> 4) & 0x01;
    int index = (port >> 1) & 0x03;

    // The control port is write only
    if (index == 3)
        return 0xFF;

    return m_pit[chip].ReadCounter(index, GetTick(chip * 3 + index, clocks));
}

void PIT::Write(u16 port, u8 value, u64 clocks)
{
    Synchronize(clocks);

    if (port == 0x0060)
    {
        if ((value & 0x80) != 0)
            m_state.timer_latch &= ~0x01;

        m_state.timer_enable = value & 0x03;
        m_state.sound = (value & 0x04) != 0;
        UpdateIRQ();
        UpdateNextEvent();
        return;
    }

    int chip = (port >> 4) & 0x01;
    int index = (port >> 1) & 0x03;
    int counter = index == 3 ? value >> 6 : index;
    u64 tick = GetTick(chip * 3 + counter, clocks);
    bool timeout = chip == 0 && counter < 2;
    bool output = timeout && m_pit[0].GetOutput(counter, tick);

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

    UpdateIRQ();
    UpdateNextEvent();
}

void PIT::UpdateIRQ()
{
    m_pic->SetIRQLine(0, (m_state.timer_latch & m_state.timer_enable) != 0);
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

    m_scheduler->Schedule(SCHEDULER_EVENT_PIT, next_event);
}

void PIT::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    m_pit[0].SaveState(stream);
    m_pit[1].SaveState(stream);
}

void PIT::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    m_pit[0].LoadState(stream);
    m_pit[1].LoadState(stream);
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
    UpdateIRQ();
    UpdateNextEvent();
}
