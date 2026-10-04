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

#ifndef CDROM_MOCK_INLINE_H
#define CDROM_MOCK_INLINE_H

#include "cdrom_mock.h"

INLINE void CDROMMock::Synchronize(u64 clocks)
{
    if (m_state.busy && clocks >= m_state.execute_clocks)
        ExecuteCommand();
}

INLINE void CDROMMock::HandleEvent(u64 clocks)
{
    Synchronize(clocks);
    UpdateNextEvent();
}

INLINE CDROMMock::CDROMMock_State* CDROMMock::GetState()
{
    return &m_state;
}

#endif /* CDROM_MOCK_INLINE_H */
