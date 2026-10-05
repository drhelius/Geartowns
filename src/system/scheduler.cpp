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

    m_next_event_clocks = GT_NO_EVENT;
    m_next_event = SCHEDULER_EVENT_PIT;
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

    for (int i = 0; i < SCHEDULER_EVENT_COUNT; i++)
        m_state.events[i] = GT_NO_EVENT;

    UpdateNextEvent();
}

void Scheduler::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

// Devices schedule their events again when they load their own state
void Scheduler::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void Scheduler::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.clocks);
}

void Scheduler::SanitizeState()
{
    for (int i = 0; i < SCHEDULER_EVENT_COUNT; i++)
        m_state.events[i] = GT_NO_EVENT;

    UpdateNextEvent();
}
