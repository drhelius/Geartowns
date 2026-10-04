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

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <iostream>
#include "../common/common.h"

class StateSerializer;

enum Scheduler_Event
{
    SCHEDULER_EVENT_PIT = 0,
    SCHEDULER_EVENT_VIDEO,
    SCHEDULER_EVENT_CDROM,
    SCHEDULER_EVENT_COUNT
};

class Scheduler
{
public:
    struct Scheduler_State
    {
        u64 clocks;
        u64 events[SCHEDULER_EVENT_COUNT];
    };

public:
    Scheduler();
    ~Scheduler();
    void Init();
    void Reset();
    u64 GetClocks() const;
    void AddClocks(u32 clocks);
    u32 GetSliceClocks(u64 limit) const;
    bool IsEventDue() const;
    Scheduler_Event PopEvent();
    void Schedule(Scheduler_Event event, u64 clocks);
    u64 GetEventClocks(Scheduler_Event event) const;
    Scheduler_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void UpdateNextEvent();
    void Serialize(StateSerializer& serializer);

private:
    Scheduler_State m_state;
    u64 m_next_event_clocks;
    Scheduler_Event m_next_event;
};

// Longest CPU run before control returns to the main loop
static const u32 k_scheduler_slice_clocks = 20000;

#include "scheduler_inline.h"

#endif /* SCHEDULER_H */
