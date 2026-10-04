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

#ifndef TOWNS_KEYBOARD_H
#define TOWNS_KEYBOARD_H

#include <iostream>
#include "../common/common.h"

#define TOWNS_KEYBOARD_FIFO_SIZE 32

class TownsPIC;
class Scheduler;
class StateSerializer;

// JIS keyboard behind the 8042 interface at 0600h-0604h
class TownsKeyboard
{
public:
    struct TownsKeyboard_State
    {
        u8 fifo[TOWNS_KEYBOARD_FIFO_SIZE];
        u8 fifo_read;
        u8 fifo_count;
        bool irq_enabled;
        bool kbint;
        u8 last_command;
        bool rearm_pending;
        u64 rearm_clocks;
        bool keys[GT_KEY_COUNT];
        u8 repeat_key;
        u64 repeat_clocks;
        u16 repeat_delay;
        u16 repeat_interval;
        u32 dropped_events;
    };

public:
    TownsKeyboard();
    ~TownsKeyboard();
    void Init(TownsPIC* pic, Scheduler* scheduler);
    void Reset();
    u8 Read(u16 port, u64 clocks);
    void Write(u16 port, u8 value, u64 clocks);
    void Synchronize(u64 clocks);
    void HandleEvent(u64 clocks);
    void KeyPressed(GT_Keys key);
    void KeyReleased(GT_Keys key);
    void ReleaseAllKeys();
    bool IsKeyPressed(GT_Keys key) const;
    TownsKeyboard_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void WriteCommand(u8 value);
    void ResetController();
    void SendResetResponse(int count);
    void PushEvent(u8 key, bool pressed);
    bool IsValidKey(GT_Keys key) const;
    bool IsRepeatKey(u8 key) const;
    void UpdateIRQ();
    void UpdateNextEvent();
    void Serialize(StateSerializer& serializer);

private:
    TownsPIC* m_pic;
    Scheduler* m_scheduler;
    TownsKeyboard_State m_state;
};

static const int k_towns_keyboard_irq = 1;
static const u64 k_towns_keyboard_rearm_clocks = GT_CPU_CLOCK_RATE / 1200;
static const u16 k_towns_keyboard_repeat_delay = 400;
static const u16 k_towns_keyboard_repeat_interval = 30;

#include "towns_keyboard_inline.h"

#endif /* TOWNS_KEYBOARD_H */
