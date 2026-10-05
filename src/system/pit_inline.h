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

#ifndef PIT_INLINE_H
#define PIT_INLINE_H

#include "pit.h"
#include "scheduler.h"

INLINE void PIT::Synchronize(u64 clocks)
{
    u64 tick = GetTick(0, clocks);

    if (tick <= m_state.settled_tick)
        return;

    u8 latch = m_state.timer_latch;

    // Rising OUT edges latch the timeouts whether their IRQ is enabled or not
    for (int i = 0; i < 2; i++)
    {
        u8 mask = (u8)(1 << i);

        if ((m_state.timer_latch & mask) == 0 && m_pit[0].GetNextRisingEdge(i, m_state.settled_tick) <= tick)
            m_state.timer_latch |= mask;
    }

    m_state.settled_tick = tick;

    if (latch != m_state.timer_latch)
    {
        UpdateIRQ();
        UpdateNextEvent();
    }
}

INLINE void PIT::HandleEvent(u64 clocks)
{
    Synchronize(clocks);
    UpdateNextEvent();
}

// The CFF98 buzzer enable, the buzzer sounds with either this or SOUND
INLINE void PIT::SetMemoryBuzzer(bool enabled)
{
    m_state.sound_memory = enabled;
}

INLINE I8253* PIT::GetPIT(int index)
{
    return &m_pit[index];
}

INLINE PIT::PIT_State* PIT::GetState()
{
    return &m_state;
}

INLINE u64 PIT::GetTick(int channel, u64 clocks) const
{
    u64 ticks = channel == k_pit_serial_channel ? k_pit_fast_ticks : k_pit_base_ticks;
    return (clocks * ticks) / k_pit_clock_divisor;
}

INLINE u64 PIT::GetTickClocks(u64 tick) const
{
    return (tick * k_pit_clock_divisor + k_pit_base_ticks - 1) / k_pit_base_ticks;
}

#endif /* PIT_INLINE_H */
