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

#ifndef I8259_H
#define I8259_H

#include <iostream>
#include "../common/common.h"

class StateSerializer;

class I8259
{
public:
    enum I8259_Init_Step
    {
        I8259_INIT_READY = 0,
        I8259_INIT_ICW2,
        I8259_INIT_ICW3,
        I8259_INIT_ICW4
    };

    struct I8259_State
    {
        u8 irr;
        u8 isr;
        u8 imr;
        u8 input_levels;
        u8 icw1;
        u8 icw2;
        u8 icw3;
        u8 icw4;
        I8259_Init_Step init_step;
        u8 lowest_priority;
        bool read_isr;
        bool poll_pending;
        bool special_mask;
        bool rotate_on_aeoi;
        bool int_output;
    };

public:
    I8259();
    ~I8259();
    void Init(bool is_master);
    void Reset();
    u8 Read(int a0);
    void Write(int a0, u8 value);
    void SetInputLine(int line, bool high);
    bool IsIRQAsserted() const;
    int Acknowledge();
    u8 GetVector(int line) const;
    bool IsCascadeLine(int line) const;
    I8259_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void WriteICW1(u8 value);
    void WriteICW(u8 value);
    void WriteOCW2(u8 value);
    void WriteOCW3(u8 value);
    int EndOfInterrupt();
    int FindHighest(u8 bits) const;
    int GetRequestLine() const;
    void UpdateOutput();
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    I8259_State m_state;
    bool m_is_master;
};

static const u8 k_i8259_icw1_ic4 = 0x01;
static const u8 k_i8259_icw1_sngl = 0x02;
static const u8 k_i8259_icw1_ltim = 0x08;
static const u8 k_i8259_icw1_init = 0x10;
static const u8 k_i8259_icw4_upm = 0x01;
static const u8 k_i8259_icw4_aeoi = 0x02;
static const u8 k_i8259_icw4_sfnm = 0x10;
static const u8 k_i8259_ocw3_ris = 0x01;
static const u8 k_i8259_ocw3_rr = 0x02;
static const u8 k_i8259_ocw3_poll = 0x04;
static const u8 k_i8259_ocw3_select = 0x08;
static const u8 k_i8259_ocw3_smm = 0x20;
static const u8 k_i8259_ocw3_esmm = 0x40;

#include "i8259_inline.h"

#endif /* I8259_H */
