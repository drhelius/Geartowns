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

#ifndef X87_INLINE_H
#define X87_INLINE_H

#include "x87.h"

INLINE X87::X87_State* X87::GetState()
{
    return &m_state;
}

INLINE X87_Flavor X87::GetFlavor() const
{
    return m_flavor;
}

INLINE bool X87::IsErrorPending() const
{
    return (m_state.status & 0x0080) != 0;
}

INLINE u16 X87::GetStatusWord() const
{
    return m_state.status;
}

INLINE int X87::GetTop() const
{
    return (m_state.status >> 11) & 7;
}

INLINE void X87::SetTop(int top)
{
    m_state.status = (u16)((m_state.status & ~0x3800) | ((top & 7) << 11));
}

INLINE int X87::GetTag(int index) const
{
    return (m_state.tag >> (((GetTop() + index) & 7) * 2)) & 3;
}

INLINE void X87::SetTag(int index, int tag)
{
    int shift = ((GetTop() + index) & 7) * 2;
    m_state.tag = (u16)((m_state.tag & ~(3 << shift)) | ((tag & 3) << shift));
}

INLINE bool X87::IsEmpty(int index) const
{
    return GetTag(index) == 3;
}

INLINE const X87::X87_Register& X87::Read(int index) const
{
    return m_state.registers[(GetTop() + index) & 7];
}

INLINE void X87::Write(const X87_Register& value, int index)
{
    m_state.registers[(GetTop() + index) & 7] = value;
    SetTag(index, 0);
}

INLINE void X87::Push()
{
    SetTop(GetTop() - 1);
}

INLINE void X87::Pop()
{
    SetTag(0, 3);
    SetTop(GetTop() + 1);
}

INLINE void X87::SetConditionCodes(u16 codes)
{
    m_state.status = (u16)((m_state.status & ~0x4700) | codes);
}

INLINE void X87::ClearC1()
{
    m_state.status &= ~0x0200;
}

#endif /* X87_INLINE_H */
