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
#ifndef I8251_INLINE_H
#define I8251_INLINE_H

#include "i8251.h"

INLINE u64 I8251::GetEventClocks() const
{
    return m_state.shift_full ? m_state.shift_clocks : GT_NO_EVENT;
}

INLINE I8251::I8251_State* I8251::GetState()
{
    return &m_state;
}

INLINE const I8251::I8251_State* I8251::GetState() const
{
    return &m_state;
}

#endif /* I8251_INLINE_H */
