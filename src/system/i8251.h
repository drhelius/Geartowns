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
#ifndef I8251_H
#define I8251_H

#include <iostream>
#include "../common/common.h"

class StateSerializer;

class I8251
{
public:
    enum I8251_Step
    {
        I8251_STEP_MODE = 0,
        I8251_STEP_SYNC_1,
        I8251_STEP_SYNC_2,
        I8251_STEP_COMMAND
    };

    struct I8251_State
    {
        u8 step;
        u8 mode;
        u8 command;
        u8 holding;
        bool holding_full;
        u8 shift;
        bool shift_full;
        u64 shift_clocks;
    };

public:
    I8251();
    ~I8251();
    void Init(u32 clock_divisor);
    void Reset();
    u8 ReadStatus() const;
    void WriteData(u8 value, u64 clocks);
    void WriteControl(u8 value, u64 clocks);
    bool Shift(u64 clocks, u8& value);
    bool IsTransmitReady() const;
    u64 GetEventClocks() const;
    I8251_State* GetState();
    const I8251_State* GetState() const;
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void LoadShift(u64 clocks);
    u32 GetCharacterClocks() const;
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    I8251_State m_state;
    u32 m_clock_divisor;
};

static const u8 k_i8251_command_transmit = 0x01;
static const u8 k_i8251_command_reset = 0x40;
static const u8 k_i8251_status_tx_ready = 0x01;
static const u8 k_i8251_status_tx_empty = 0x04;

#include "i8251_inline.h"

#endif /* I8251_H */
