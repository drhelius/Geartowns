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

#include "i8259.h"
#include "../common/state_serializer.h"

I8259::I8259()
{
    m_is_master = false;
    Reset();
}

I8259::~I8259()
{
}

void I8259::Init(bool is_master)
{
    m_is_master = is_master;
    Reset();
}

void I8259::Reset()
{
    m_state.irr = 0;
    m_state.isr = 0;
    m_state.imr = 0xFF;
    m_state.input_levels = 0;
    m_state.icw1 = 0;
    m_state.icw2 = 0;
    m_state.icw3 = 0;
    m_state.icw4 = 0;
    m_state.init_step = I8259_INIT_READY;
    m_state.lowest_priority = 7;
    m_state.read_isr = false;
    m_state.poll_pending = false;
    m_state.special_mask = false;
    m_state.rotate_on_aeoi = false;
    m_state.int_output = false;
}

u8 I8259::Read(int a0)
{
    // An armed poll turns the next read into an acknowledge
    if (m_state.poll_pending)
    {
        m_state.poll_pending = false;
        int line = Acknowledge();
        return line < 0 ? 0x00 : (u8)(0x80 | line);
    }

    if (a0 != 0)
        return m_state.imr;

    return m_state.read_isr ? m_state.isr : m_state.irr;
}

void I8259::Write(int a0, u8 value)
{
    if (a0 == 0)
    {
        if ((value & k_i8259_icw1_init) != 0)
            WriteICW1(value);
        else if ((value & k_i8259_ocw3_select) != 0)
            WriteOCW3(value);
        else
            WriteOCW2(value);
    }
    else if (m_state.init_step != I8259_INIT_READY)
        WriteICW(value);
    else
        m_state.imr = value;

    UpdateOutput();
}

int I8259::Acknowledge()
{
    if (m_state.init_step != I8259_INIT_READY)
        return -1;

    int line = GetRequestLine();

    if (line < 0)
        return -1;

    u8 mask = (u8)(1 << line);

    if ((m_state.icw1 & k_i8259_icw1_ltim) == 0 && !IsCascadeLine(line))
        m_state.irr &= ~mask;

    if ((m_state.icw4 & k_i8259_icw4_aeoi) == 0)
        m_state.isr |= mask;
    else if (m_state.rotate_on_aeoi)
        m_state.lowest_priority = (u8)line;

    UpdateOutput();
    return line;
}

void I8259::WriteICW1(u8 value)
{
    m_state.icw1 = value;
    m_state.isr = 0;
    m_state.imr = 0;
    m_state.irr = (value & k_i8259_icw1_ltim) != 0 ? m_state.input_levels : 0;
    m_state.lowest_priority = 7;
    m_state.read_isr = false;
    m_state.poll_pending = false;
    m_state.special_mask = false;
    m_state.rotate_on_aeoi = false;

    if ((value & k_i8259_icw1_ic4) == 0)
        m_state.icw4 = 0;

    m_state.init_step = I8259_INIT_ICW2;
}

void I8259::WriteICW(u8 value)
{
    bool cascaded = (m_state.icw1 & k_i8259_icw1_sngl) == 0;
    bool icw4 = (m_state.icw1 & k_i8259_icw1_ic4) != 0;

    switch (m_state.init_step)
    {
        case I8259_INIT_ICW2:
            m_state.icw2 = value;
            m_state.init_step = cascaded ? I8259_INIT_ICW3 : (icw4 ? I8259_INIT_ICW4 : I8259_INIT_READY);
            break;
        case I8259_INIT_ICW3:
            m_state.icw3 = value;
            m_state.init_step = icw4 ? I8259_INIT_ICW4 : I8259_INIT_READY;
            break;
        case I8259_INIT_ICW4:
            m_state.icw4 = value;
            m_state.init_step = I8259_INIT_READY;
            break;
        default:
            break;
    }

    if (m_state.init_step == I8259_INIT_READY && (m_state.icw4 & k_i8259_icw4_upm) == 0)
        Debug("I8259 %s: MCS-80/85 mode is not supported", m_is_master ? "master" : "slave");
}

void I8259::WriteOCW2(u8 value)
{
    int line = value & 0x07;

    switch (value >> 5)
    {
        case 0:
            // Clear rotate on AEOI
            m_state.rotate_on_aeoi = false;
            break;
        case 1:
            // Non-specific EOI
            EndOfInterrupt();
            break;
        case 3:
            // Specific EOI
            m_state.isr &= ~(1 << line);
            break;
        case 4:
            // Set rotate on AEOI
            m_state.rotate_on_aeoi = true;
            break;
        case 5:
        {
            // Rotate on non-specific EOI
            int cleared = EndOfInterrupt();

            if (cleared >= 0)
                m_state.lowest_priority = (u8)cleared;

            break;
        }
        case 6:
            // Set priority
            m_state.lowest_priority = (u8)line;
            break;
        case 7:
            // Rotate on specific EOI
            m_state.isr &= ~(1 << line);
            m_state.lowest_priority = (u8)line;
            break;
        default:
            // No operation
            break;
    }
}

void I8259::WriteOCW3(u8 value)
{
    if ((value & k_i8259_ocw3_rr) != 0)
        m_state.read_isr = (value & k_i8259_ocw3_ris) != 0;

    if ((value & k_i8259_ocw3_esmm) != 0)
        m_state.special_mask = (value & k_i8259_ocw3_smm) != 0;

    if ((value & k_i8259_ocw3_poll) != 0)
        m_state.poll_pending = true;
}

int I8259::EndOfInterrupt()
{
    u8 in_service = m_state.isr;

    // Masked levels keep their in-service bit while special mask mode is on
    if (m_state.special_mask)
        in_service &= ~m_state.imr;

    int line = FindHighest(in_service);

    if (line >= 0)
        m_state.isr &= ~(1 << line);

    return line;
}

void I8259::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void I8259::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void I8259::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.irr);
    G_SERIALIZE(serializer, m_state.isr);
    G_SERIALIZE(serializer, m_state.imr);
    G_SERIALIZE(serializer, m_state.input_levels);
    G_SERIALIZE(serializer, m_state.icw1);
    G_SERIALIZE(serializer, m_state.icw2);
    G_SERIALIZE(serializer, m_state.icw3);
    G_SERIALIZE(serializer, m_state.icw4);
    G_SERIALIZE(serializer, m_state.init_step);
    G_SERIALIZE(serializer, m_state.lowest_priority);
    G_SERIALIZE(serializer, m_state.read_isr);
    G_SERIALIZE(serializer, m_state.poll_pending);
    G_SERIALIZE(serializer, m_state.special_mask);
    G_SERIALIZE(serializer, m_state.rotate_on_aeoi);
}

void I8259::SanitizeState()
{
    if (m_state.init_step > I8259_INIT_ICW4)
        m_state.init_step = I8259_INIT_READY;

    m_state.lowest_priority &= 0x07;

    // The INT output is derived state, rebuilt from the restored registers
    UpdateOutput();
}
