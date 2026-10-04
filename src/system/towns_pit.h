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

#ifndef TOWNS_PIT_H
#define TOWNS_PIT_H

#include <iostream>
#include "../common/common.h"
#include "i8253.h"

class TownsPIC;
class StateSerializer;

class TownsPIT
{
public:
    struct TownsPIT_State
    {
        u8 timer_latch;
        u8 timer_enable;
        bool sound;
        u64 settled_tick;
    };

public:
    TownsPIT();
    ~TownsPIT();
    void Init(TownsPIC* pic);
    void Reset();
    u8 Read(u16 port, u64 time_ns);
    void Write(u16 port, u8 value, u64 time_ns);
    void Synchronize(u64 time_ns);
    u64 GetNextEventTime() const;
    I8253* GetPIT(int index);
    TownsPIT_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    u64 GetTick(int channel, u64 time_ns) const;
    u64 GetTickTime(u64 tick) const;
    void UpdateIRQ();
    void UpdateNextEvent();
    void Serialize(StateSerializer& serializer);

private:
    TownsPIC* m_pic;
    I8253 m_pit[2];
    TownsPIT_State m_state;
    u64 m_next_event_time;
};

// 1,228,800 Hz and 307,200 Hz are 96 and 24 ticks every 78125 ns
static const u64 k_towns_pit_fast_ticks = 96;
static const u64 k_towns_pit_base_ticks = 24;
static const u64 k_towns_pit_ns_divisor = 78125;
static const int k_towns_pit_serial_channel = 4;

#include "towns_pit_inline.h"

#endif /* TOWNS_PIT_H */
