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

#ifndef FDC_MOCK_INLINE_H
#define FDC_MOCK_INLINE_H

#include "fdc_mock.h"

INLINE void FDCMock::Synchronize(u64 clocks)
{
    if (m_state.busy && clocks >= m_state.execute_clocks)
        CompleteCommand();
}

INLINE void FDCMock::HandleEvent(u64 clocks)
{
    Synchronize(clocks);
    UpdateNextEvent();
}

INLINE FDCMock::FDCMock_State* FDCMock::GetState()
{
    return &m_state;
}

#endif /* FDC_MOCK_INLINE_H */
