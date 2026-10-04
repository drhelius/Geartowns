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

#ifndef SCHEDULER_INLINE_H
#define SCHEDULER_INLINE_H

#include "scheduler.h"

INLINE u64 Scheduler::GetClocks() const
{
    return m_state.clocks;
}

INLINE void Scheduler::AddClocks(u32 clocks)
{
    m_state.clocks += clocks;
}

// Clocks to run before returning: a whole slice, cut short by the next event or the limit, never zero
INLINE u32 Scheduler::GetSliceClocks(u64 limit) const
{
    u64 end = MIN(MIN(m_state.clocks + k_scheduler_slice_clocks, m_next_event_clocks), limit);
    return end > m_state.clocks ? (u32)(end - m_state.clocks) : 1;
}

INLINE bool Scheduler::IsEventDue() const
{
    return m_state.clocks >= m_next_event_clocks;
}

// The device that owns the event must schedule it again while handling it
INLINE Scheduler_Event Scheduler::PopEvent()
{
    Scheduler_Event event = m_next_event;
    m_state.events[event] = GT_NO_EVENT;
    UpdateNextEvent();
    return event;
}

INLINE void Scheduler::Schedule(Scheduler_Event event, u64 clocks)
{
    m_state.events[event] = clocks;

    if (clocks <= m_next_event_clocks)
    {
        m_next_event_clocks = clocks;
        m_next_event = event;
    }
    else if (event == m_next_event)
        UpdateNextEvent();
}

INLINE u64 Scheduler::GetEventClocks(Scheduler_Event event) const
{
    return m_state.events[event];
}

INLINE Scheduler::Scheduler_State* Scheduler::GetState()
{
    return &m_state;
}

INLINE void Scheduler::UpdateNextEvent()
{
    m_next_event_clocks = m_state.events[0];
    m_next_event = (Scheduler_Event)0;

    for (int i = 1; i < SCHEDULER_EVENT_COUNT; i++)
    {
        if (m_state.events[i] < m_next_event_clocks)
        {
            m_next_event_clocks = m_state.events[i];
            m_next_event = (Scheduler_Event)i;
        }
    }
}

#endif /* SCHEDULER_INLINE_H */
