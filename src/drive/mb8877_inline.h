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

#ifndef MB8877_INLINE_H
#define MB8877_INLINE_H

#include "mb8877.h"

INLINE u64 MB8877::GetEventClocks() const
{
    return MIN(m_state.event_clocks, m_state.index_clocks);
}

INLINE u8 MB8877::ReadTrackRegister() const
{
    return m_state.track;
}

INLINE u8 MB8877::ReadSectorRegister() const
{
    return m_state.sector;
}

INLINE MB8877::MB8877_State* MB8877::GetState()
{
    return &m_state;
}

#endif /* MB8877_INLINE_H */
