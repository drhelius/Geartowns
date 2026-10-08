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

#ifndef PIT_H
#define PIT_H

#include <iostream>
#include "../common/common.h"
#include "i8253.h"

class PIC;
class Scheduler;
class StateSerializer;
class TraceLogger;

class PIT
{
public:
    struct PIT_State
    {
        u8 timer_latch;
        u8 timer_enable;
        bool sound;
        bool sound_memory;
        u64 settled_tick;
    };

public:
    PIT();
    ~PIT();
    void Init(PIC* pic, Scheduler* scheduler);
    void SetTraceLogger(TraceLogger* trace_logger);
    void Reset();
    u8 Read(u16 port, u64 clocks);
    u8 Peek(u16 port, u64 clocks) const;
    void Write(u16 port, u8 value, u64 clocks);
    void Synchronize(u64 clocks);
    void HandleEvent(u64 clocks);
    void SetMemoryBuzzer(bool enabled);
    I8253* GetPIT(int index);
    u64 GetTick(int channel, u64 clocks) const;
    PIT_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    u8 GetTimerLatch(u64 tick) const;
    u64 GetTickClocks(u64 tick) const;
    void UpdateIRQ();
    void UpdateNextEvent();
    void TraceTimeout(u8 previous_latch);
    void TraceWrite(u16 port, u8 value);
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    PIC* m_pic;
    Scheduler* m_scheduler;
    TraceLogger* m_trace_logger;
    I8253 m_pit[2];
    PIT_State m_state;
};

// 1,228,800 Hz and 307,200 Hz are 48 and 12 ticks every 625 clocks of the 16 MHz CPU
static const u64 k_pit_fast_ticks = 48;
static const u64 k_pit_base_ticks = 12;
static const u64 k_pit_clock_divisor = 625;
static const int k_pit_serial_channel = 4;

#include "pit_inline.h"

#endif /* PIT_H */
