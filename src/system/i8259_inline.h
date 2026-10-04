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

#ifndef I8259_INLINE_H
#define I8259_INLINE_H

#include "i8259.h"

INLINE void I8259::SetInputLine(int line, bool high)
{
    u8 mask = (u8)(1 << line);

    if (high == ((m_input_levels & mask) != 0))
        return;

    // A request only stands while its input is high, in both trigger modes
    if (high)
    {
        m_input_levels |= mask;
        m_irr |= mask;
    }
    else
    {
        m_input_levels &= ~mask;
        m_irr &= ~mask;
    }

    UpdateOutput();
}

INLINE bool I8259::IsIRQAsserted() const
{
    return m_int_output;
}

INLINE u8 I8259::GetVector(int line) const
{
    return (u8)((m_icw2 & 0xF8) | line);
}

INLINE bool I8259::IsCascadeLine(int line) const
{
    return m_is_master && (m_icw1 & k_i8259_icw1_sngl) == 0 && (m_icw3 & (1 << line)) != 0;
}

INLINE u8 I8259::GetIRR() const
{
    return m_irr;
}

INLINE u8 I8259::GetISR() const
{
    return m_isr;
}

INLINE u8 I8259::GetIMR() const
{
    return m_imr;
}

INLINE u8 I8259::GetInputLevels() const
{
    return m_input_levels;
}

INLINE int I8259::FindHighest(u8 bits) const
{
    for (int rank = 0; rank < 8; rank++)
    {
        int line = (m_lowest_priority + 1 + rank) & 7;

        if ((bits & (1 << line)) != 0)
            return line;
    }

    return -1;
}

INLINE int I8259::GetRequestLine() const
{
    u8 requests = m_irr & ~m_imr;

    if (requests == 0)
        return -1;

    u8 blocking = m_isr;

    if (m_special_mask)
        blocking &= ~m_imr;

    bool nested = m_is_master && (m_icw4 & k_i8259_icw4_sfnm) != 0;

    for (int rank = 0; rank < 8; rank++)
    {
        int line = (m_lowest_priority + 1 + rank) & 7;
        u8 mask = (u8)(1 << line);
        bool in_service = (blocking & mask) != 0;

        // Special fully nested mode lets a cascade input pass its own in-service level
        if (in_service && !(nested && IsCascadeLine(line)))
            return -1;

        if ((requests & mask) != 0)
            return line;

        if (in_service)
            return -1;
    }

    return -1;
}

INLINE void I8259::UpdateOutput()
{
    m_int_output = m_init_step == I8259_INIT_READY && GetRequestLine() >= 0;
}

#endif /* I8259_INLINE_H */
