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

#ifndef I8253_INLINE_H
#define I8253_INLINE_H

#include "i8253.h"

INLINE bool I8253::GetOutput(int index, u64 tick)
{
    I8253_Counter& counter = m_state.counters[index];

    if (!counter.counting)
        return !counter.programmed || counter.mode != 0;

    CommitReload(counter, tick);

    if (tick < counter.load)
        return counter.mode != 0;

    u64 elapsed = tick - counter.load + counter.phase;

    switch (counter.mode)
    {
        case 0:
            return elapsed >= counter.count;
        case 2:
            return elapsed % counter.count != counter.count - 1;
        case 3:
            return elapsed % counter.count < (counter.count + 1) / 2;
        case 4:
            return elapsed != counter.count;
        default:
            return true;
    }
}

INLINE u64 I8253::GetNextRisingEdge(int index, u64 tick) const
{
    const I8253_Counter& counter = m_state.counters[index];

    if (!counter.counting)
        return k_i8253_no_edge;

    u64 edge = GetEdgeAfter(counter.mode, counter.load, counter.count, counter.phase, tick);

    // A pending reload takes over the schedule from its boundary on
    if (counter.reload_pending && edge > counter.pending_load)
        edge = GetEdgeAfter(counter.mode, counter.pending_load, counter.pending_count, counter.pending_phase,
            MAX(tick, counter.pending_load - 1));

    return edge;
}

INLINE I8253::I8253_State* I8253::GetState()
{
    return &m_state;
}

INLINE void I8253::CommitReload(I8253_Counter& counter, u64 tick)
{
    if (!counter.reload_pending || tick < counter.pending_load)
        return;

    counter.load = counter.pending_load;
    counter.count = counter.pending_count;
    counter.phase = counter.pending_phase;
    counter.reload_pending = false;
}

INLINE u64 I8253::GetEdgeAfter(u8 mode, u64 load, u32 count, u32 phase, u64 tick) const
{
    switch (mode)
    {
        case 0:
            return load + count > tick ? load + count : k_i8253_no_edge;
        case 4:
            return load + count + 1 > tick ? load + count + 1 : k_i8253_no_edge;
        case 2:
        case 3:
        {
            // A divisor of one never lets OUT change
            if (count < 2)
                return k_i8253_no_edge;

            if (tick < load)
                return load + count - phase;

            u64 periods = (tick - load + phase) / count + 1;
            return load + periods * count - phase;
        }
        default:
            return k_i8253_no_edge;
    }
}

#endif /* I8253_INLINE_H */
