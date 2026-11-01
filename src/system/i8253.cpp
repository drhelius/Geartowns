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

#include "i8253.h"
#include "../common/state_serializer.h"

I8253::I8253()
{
    m_read_back = false;
    Reset();
}

I8253::~I8253()
{
}

void I8253::Init()
{
    Reset();
}

// The 8254 adds the read-back command to the 8253
void I8253::SetReadBack(bool enabled)
{
    m_read_back = enabled;
}

void I8253::Reset()
{
    memset(m_state.counters, 0, sizeof(m_state.counters));

    for (int i = 0; i < I8253_COUNTER_COUNT; i++)
    {
        m_state.counters[i].count = 1;
        m_state.counters[i].pending_count = 1;
        m_state.counters[i].gate = true;
        m_state.counters[i].null_until = k_i8253_no_edge;
    }
}

u8 I8253::ReadCounter(int index, u64 tick)
{
    I8253_Counter& counter = m_state.counters[index];

    if (!counter.programmed)
        return 0x00;

    if (counter.status_latched)
    {
        counter.status_latched = false;
        return counter.status_latch;
    }

    u16 value = 0;
    bool high = counter.access == 2;

    if (counter.count_latched)
    {
        value = counter.count_latch;

        if (counter.access == 3)
        {
            high = counter.latch_high;
            counter.latch_high = !counter.latch_high;
        }

        counter.count_latched = counter.latch_high;
    }
    else
    {
        value = GetCount(counter, tick);

        if (counter.access == 3)
        {
            high = counter.read_high;
            counter.read_high = !counter.read_high;
        }
    }

    return high ? (u8)(value >> 8) : (u8)value;
}

// The byte the next read returns, without toggling the read flip-flops
u8 I8253::PeekCounter(int index, u64 tick) const
{
    const I8253_Counter& counter = m_state.counters[index];

    if (!counter.programmed)
        return 0x00;

    if (counter.status_latched)
        return counter.status_latch;

    u16 value = 0;
    bool high = counter.access == 2;

    if (counter.count_latched)
    {
        value = counter.count_latch;

        if (counter.access == 3)
            high = counter.latch_high;
    }
    else
    {
        value = PeekCount(index, tick);

        if (counter.access == 3)
            high = counter.read_high;
    }

    return high ? (u8)(value >> 8) : (u8)value;
}

// The live count of the counting element, ignoring any latched value
u16 I8253::PeekCount(int index, u64 tick) const
{
    I8253_Counter counter = m_state.counters[index];
    return GetCount(counter, tick);
}

void I8253::WriteCounter(int index, u8 value, u64 tick)
{
    I8253_Counter& counter = m_state.counters[index];

    if (!counter.programmed)
        return;

    CommitReload(counter, tick);

    switch (counter.access)
    {
        case 1:
            LoadCount(counter, value, tick);
            break;
        case 2:
            LoadCount(counter, (u16)(value << 8), tick);
            break;
        default:
            if (counter.write_high)
            {
                counter.write_high = false;
                LoadCount(counter, (u16)(counter.write_latch | (value << 8)), tick);
                break;
            }

            counter.write_latch = value;
            counter.write_high = true;

            // The first byte of a new count stops a mode 0 count
            if (counter.mode == 0)
                counter.counting = false;

            break;
    }
}

void I8253::WriteControl(u8 value, u64 tick)
{
    int index = value >> 6;

    // SC=3 is the 8254 read-back command, not present on the 8253
    if (index >= I8253_COUNTER_COUNT)
    {
        if (m_read_back)
            ReadBack(value, tick);

        return;
    }

    I8253_Counter& counter = m_state.counters[index];
    u8 access = (value >> 4) & 0x03;

    if (access == 0)
    {
        // A pending latch keeps its snapshot until it has been read
        if (counter.programmed && !counter.count_latched)
        {
            counter.count_latch = GetCount(counter, tick);
            counter.count_latched = true;
            counter.latch_high = false;
        }

        return;
    }

    u8 mode = (value >> 1) & 0x07;

    counter.access = access;
    counter.mode = mode > 5 ? mode - 4 : mode;
    counter.bcd = (value & 0x01) != 0;
    counter.programmed = true;
    counter.counting = false;
    counter.count_latched = false;
    counter.read_high = false;
    counter.write_high = false;
    counter.latch_high = false;
    counter.reload_pending = false;
    counter.status_latched = false;
    counter.null_until = k_i8253_no_edge;
}

// Bit 5 low latches the counts, bit 4 low the status bytes, of the counters in bits 1 to 3
void I8253::ReadBack(u8 value, u64 tick)
{
    for (int i = 0; i < I8253_COUNTER_COUNT; i++)
    {
        I8253_Counter& counter = m_state.counters[i];

        if ((value & (2 << i)) == 0 || !counter.programmed)
            continue;

        if ((value & 0x10) == 0 && !counter.status_latched)
        {
            counter.status_latch = (u8)((GetCounterOutput(counter, tick) ? 0x80 : 0x00) |
                (tick < counter.null_until ? 0x40 : 0x00) | (counter.access << 4) | (counter.mode << 1) |
                (counter.bcd ? 0x01 : 0x00));
            counter.status_latched = true;
        }

        if ((value & 0x20) == 0 && !counter.count_latched)
        {
            counter.count_latch = GetCount(counter, tick);
            counter.count_latched = true;
            counter.latch_high = false;
        }
    }
}

// A low gate holds modes 0, 2, 3 and 4, a rising gate reloads modes 2 and 3 and triggers modes 1 and 5
void I8253::SetGate(int index, bool gate, u64 tick)
{
    I8253_Counter& counter = m_state.counters[index];

    if (counter.gate == gate)
        return;

    CommitReload(counter, GetGateTick(counter, tick));
    counter.gate = gate;

    if (!gate)
    {
        counter.hold = tick;
        return;
    }

    if (!counter.programmed)
        return;

    switch (counter.mode)
    {
        case 0:
        case 4:
            if (counter.counting && tick > counter.hold)
            {
                counter.load += tick - counter.hold;

                if (counter.reload_pending)
                    counter.pending_load += tick - counter.hold;
            }

            break;
        default:
            if (!counter.counting && (counter.mode == 2 || counter.mode == 3))
                break;

            counter.counting = true;
            counter.load = tick + 1;
            counter.count = GetEffectiveCount(counter.reload, counter.bcd);
            counter.phase = 0;
            counter.reload_pending = false;
            counter.null_until = tick + 1;
            break;
    }
}

void I8253::LoadCount(I8253_Counter& counter, u16 raw, u64 tick)
{
    u32 count = GetEffectiveCount(raw, counter.bcd);
    counter.reload = raw;

    // Modes 1 and 5 wait for a gate trigger
    if (counter.mode == 1 || counter.mode == 5)
        return;

    // A held rate or square wave counter starts with the gate
    if (!counter.gate && (counter.mode == 2 || counter.mode == 3))
    {
        counter.counting = true;
        counter.load = tick + 1;
        counter.count = count;
        counter.phase = 0;
        counter.reload_pending = false;
        counter.hold = tick + 1;
        counter.null_until = k_i8253_no_edge;
        return;
    }

    // Rate and square wave counters take a new count at their next reload
    if (counter.counting && tick >= counter.load && (counter.mode == 2 || counter.mode == 3))
    {
        u64 elapsed = tick - counter.load + counter.phase;
        u64 position = elapsed % counter.count;
        u32 high = (counter.count + 1) / 2;
        u64 boundary = elapsed - position + counter.count;
        u32 phase = 0;

        // Square waves also reload halfway, and the new count then starts with its low half
        if (counter.mode == 3 && position < high)
        {
            boundary = elapsed - position + high;
            phase = (count + 1) / 2;
        }

        counter.reload_pending = true;
        counter.pending_load = counter.load + boundary - counter.phase;
        counter.pending_count = count;
        counter.pending_phase = phase;
        counter.null_until = counter.pending_load;
        return;
    }

    // A written count enters the counting element on the next clock
    counter.counting = true;
    counter.load = tick + 1;
    counter.count = count;
    counter.phase = 0;
    counter.reload_pending = false;
    counter.null_until = tick + 1;

    if (!counter.gate)
        counter.hold = tick + 1;
}

u16 I8253::GetCount(I8253_Counter& counter, u64 tick) const
{
    if (!counter.counting)
        return counter.reload;

    tick = GetGateTick(counter, tick);
    CommitReload(counter, tick);

    u32 count = counter.count;
    u32 value = count;

    if (tick >= counter.load)
    {
        u64 elapsed = tick - counter.load + counter.phase;

        switch (counter.mode)
        {
            case 2:
                value = count - (u32)(elapsed % count);
                break;
            case 3:
            {
                u32 position = (u32)(elapsed % count);
                u32 high = (count + 1) / 2;

                // Odd counts drop one on the first high clock and three on the first low clock
                if (position < high)
                    value = position == 0 ? count : count + (count & 1) - (position * 2);
                else
                {
                    position -= high;
                    value = position == 0 ? count : count - (count & 1) - (position * 2);
                }

                break;
            }
            default:
            {
                // Modes 0, 1, 4 and 5 keep wrapping after the terminal count
                u32 modulus = counter.bcd ? 10000 : 0x10000;
                value = (count + modulus - (u32)(elapsed % modulus)) % modulus;
                break;
            }
        }
    }

    return counter.bcd ? EncodeBCD(value) : (u16)value;
}

// Counting stops at the falling gate in the modes a low gate holds
u64 I8253::GetGateTick(const I8253_Counter& counter, u64 tick) const
{
    if (counter.gate || counter.mode == 1 || counter.mode == 5)
        return tick;

    return MIN(tick, counter.hold);
}

u32 I8253::GetEffectiveCount(u16 raw, bool bcd) const
{
    if (!bcd)
        return raw == 0 ? 0x10000 : raw;

    u32 count = ((raw >> 12) & 0x0F) * 1000 + ((raw >> 8) & 0x0F) * 100 + ((raw >> 4) & 0x0F) * 10 + (raw & 0x0F);
    return count == 0 ? 10000 : count;
}

u16 I8253::EncodeBCD(u32 value) const
{
    value %= 10000;
    return (u16)(((value / 1000) << 12) | (((value / 100) % 10) << 8) | (((value / 10) % 10) << 4) | (value % 10));
}

void I8253::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer, GT_SAVESTATE_VERSION);
}

void I8253::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);
    SanitizeState();
}

// States from before the gates and read-back load with the gates high and no count or status pending
void I8253::Serialize(StateSerializer& serializer, int version)
{
    for (int i = 0; i < I8253_COUNTER_COUNT; i++)
    {
        G_SERIALIZE(serializer, m_state.counters[i].reload);
        G_SERIALIZE(serializer, m_state.counters[i].count_latch);
        G_SERIALIZE(serializer, m_state.counters[i].write_latch);
        G_SERIALIZE(serializer, m_state.counters[i].access);
        G_SERIALIZE(serializer, m_state.counters[i].mode);
        G_SERIALIZE(serializer, m_state.counters[i].bcd);
        G_SERIALIZE(serializer, m_state.counters[i].programmed);
        G_SERIALIZE(serializer, m_state.counters[i].counting);
        G_SERIALIZE(serializer, m_state.counters[i].count_latched);
        G_SERIALIZE(serializer, m_state.counters[i].read_high);
        G_SERIALIZE(serializer, m_state.counters[i].write_high);
        G_SERIALIZE(serializer, m_state.counters[i].latch_high);
        G_SERIALIZE(serializer, m_state.counters[i].load);
        G_SERIALIZE(serializer, m_state.counters[i].count);
        G_SERIALIZE(serializer, m_state.counters[i].phase);
        G_SERIALIZE(serializer, m_state.counters[i].reload_pending);
        G_SERIALIZE(serializer, m_state.counters[i].pending_load);
        G_SERIALIZE(serializer, m_state.counters[i].pending_count);
        G_SERIALIZE(serializer, m_state.counters[i].pending_phase);

        if (version < GT_SAVESTATE_8254_VERSION)
        {
            m_state.counters[i].gate = true;
            m_state.counters[i].hold = 0;
            m_state.counters[i].null_until = 0;
            m_state.counters[i].status_latch = 0;
            m_state.counters[i].status_latched = false;
            continue;
        }

        G_SERIALIZE(serializer, m_state.counters[i].gate);
        G_SERIALIZE(serializer, m_state.counters[i].hold);
        G_SERIALIZE(serializer, m_state.counters[i].null_until);
        G_SERIALIZE(serializer, m_state.counters[i].status_latch);
        G_SERIALIZE(serializer, m_state.counters[i].status_latched);
    }
}

void I8253::SanitizeState()
{
    for (int i = 0; i < I8253_COUNTER_COUNT; i++)
    {
        I8253_Counter& counter = m_state.counters[i];

        counter.access &= 0x03;

        if (counter.mode > 5)
            counter.mode = 0;

        // Counts and phases drive divisions, so a corrupt state must not reach zero
        counter.count = CLAMP(counter.count, 1U, 0x10000U);
        counter.pending_count = CLAMP(counter.pending_count, 1U, 0x10000U);
        counter.phase %= counter.count;
        counter.pending_phase %= counter.pending_count;
    }
}
