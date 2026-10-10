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

INLINE u32 Scheduler::GetCPUClockRate() const
{
    return m_cpu_clock_rate;
}

INLINE u64 Scheduler::GetClocks() const
{
    return m_state.clocks;
}

INLINE void Scheduler::AddClocks(u32 clocks)
{
    m_state.clocks += clocks;
}

// CPU cycles become machine clocks at the configured CPU speed
// The remainder carries over so no cycle is lost between slices
INLINE void Scheduler::AddCycles(u32 cycles)
{
    if (m_cpu_clock_rate == GT_CPU_CLOCK_RATE)
    {
        m_state.clocks += cycles;
        return;
    }

    u64 scaled = ((u64)cycles * GT_CPU_CLOCK_RATE) + m_state.cycle_remainder;
    m_state.clocks += scaled / m_cpu_clock_rate;
    m_state.cycle_remainder = (u32)(scaled % m_cpu_clock_rate);
}

// Clocks to run before returning: a whole slice, cut short by the next event or the limit, never zero
INLINE u32 Scheduler::GetSliceClocks(u64 limit) const
{
    u64 end = MIN(MIN(m_state.clocks + k_scheduler_slice_clocks, m_next_event_clocks), limit);
    return end > m_state.clocks ? (u32)(end - m_state.clocks) : 1;
}

// CPU cycles that cover a whole slice of machine clocks
INLINE u32 Scheduler::GetSliceCycles(u32 clocks) const
{
    if (m_cpu_clock_rate == GT_CPU_CLOCK_RATE)
        return clocks;

    return (u32)((((u64)clocks * m_cpu_clock_rate) + GT_CPU_CLOCK_RATE - 1) / GT_CPU_CLOCK_RATE);
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

INLINE u64 Scheduler::GetNextEventClocks() const
{
    return m_next_event_clocks;
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
