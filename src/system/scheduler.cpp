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

#include "scheduler.h"
#include "../common/state_serializer.h"

Scheduler::Scheduler()
{
    m_state.clocks = 0;

    for (int i = 0; i < SCHEDULER_EVENT_COUNT; i++)
        m_state.events[i] = GT_NO_EVENT;

    m_state.cycle_remainder = 0;
    m_next_event_clocks = GT_NO_EVENT;
    m_next_event = SCHEDULER_EVENT_PIT;
    m_cpu_clock_rate = GT_MACHINE_CLOCK_RATE;
}

Scheduler::~Scheduler()
{
}

void Scheduler::Init()
{
    Reset();
}

void Scheduler::Reset()
{
    m_state.clocks = 0;
    m_state.cycle_remainder = 0;
    m_state.cycles = 0;
    m_state.clock_remainder = 0;

    for (int i = 0; i < SCHEDULER_EVENT_COUNT; i++)
        m_state.events[i] = GT_NO_EVENT;

    UpdateNextEvent();
}

void Scheduler::SetCPUClockRate(u32 rate)
{
    m_cpu_clock_rate = rate > 0 ? rate : GT_MACHINE_CLOCK_RATE;
    m_state.cycle_remainder = 0;
    m_state.clock_remainder = 0;
}

void Scheduler::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer, GT_SAVESTATE_VERSION);
}

// Devices schedule their events again when they load their own state
void Scheduler::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);
    SanitizeState();
}

void Scheduler::Serialize(StateSerializer& serializer, int version)
{
    G_SERIALIZE(serializer, m_state.clocks);
    G_SERIALIZE(serializer, m_state.cycle_remainder);

    if (version >= GT_SAVESTATE_TSC_VERSION)
    {
        G_SERIALIZE(serializer, m_state.cycles);
        G_SERIALIZE(serializer, m_state.clock_remainder);
    }
    else
    {
        m_state.cycles = 0;
        m_state.clock_remainder = 0;
    }
}

void Scheduler::SanitizeState()
{
    if (m_state.cycle_remainder >= m_cpu_clock_rate)
        m_state.cycle_remainder = 0;

    if (m_state.clock_remainder >= GT_MACHINE_CLOCK_RATE)
        m_state.clock_remainder = 0;

    for (int i = 0; i < SCHEDULER_EVENT_COUNT; i++)
        m_state.events[i] = GT_NO_EVENT;

    UpdateNextEvent();
}
