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

#ifndef I8253_H
#define I8253_H

#include <iostream>
#include "../common/common.h"

#define I8253_COUNTER_COUNT 3

class StateSerializer;

class I8253
{
public:
    struct I8253_Counter
    {
        u16 reload;
        u16 count_latch;
        u8 write_latch;
        u8 access;
        u8 mode;
        bool bcd;
        bool programmed;
        bool counting;
        bool count_latched;
        bool read_high;
        bool write_high;
        bool latch_high;
        u64 load;
        u32 count;
        u32 phase;
        bool reload_pending;
        u64 pending_load;
        u32 pending_count;
        u32 pending_phase;
    };

    struct I8253_State
    {
        I8253_Counter counters[I8253_COUNTER_COUNT];
    };

public:
    I8253();
    ~I8253();
    void Init();
    void Reset();
    u8 ReadCounter(int index, u64 tick);
    void WriteCounter(int index, u8 value, u64 tick);
    void WriteControl(u8 value, u64 tick);
    bool GetOutput(int index, u64 tick);
    u64 GetNextRisingEdge(int index, u64 tick) const;
    I8253_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void LoadCount(I8253_Counter& counter, u16 raw, u64 tick);
    void CommitReload(I8253_Counter& counter, u64 tick);
    u16 GetCount(I8253_Counter& counter, u64 tick);
    u32 GetEffectiveCount(u16 raw, bool bcd) const;
    u16 EncodeBCD(u32 value) const;
    u64 GetEdgeAfter(u8 mode, u64 load, u32 count, u32 phase, u64 tick) const;
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    I8253_State m_state;
};

static const u64 k_i8253_no_edge = 0xFFFFFFFFFFFFFFFFULL;

#include "i8253_inline.h"

#endif /* I8253_H */
