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

#ifndef SYSTEM_CONTROL_INLINE_H
#define SYSTEM_CONTROL_INLINE_H

#include "system_control.h"

INLINE void SystemControl::RequestCPUReset(u8 cause)
{
    m_state.reset_cause |= cause;
    m_state.reset_pending = true;
}

INLINE bool SystemControl::IsCPUResetPending() const
{
    return m_state.reset_pending;
}

INLINE void SystemControl::AcknowledgeCPUReset()
{
    m_state.reset_pending = false;
}

INLINE SystemControl::SystemControl_State* SystemControl::GetState()
{
    return &m_state;
}

#endif /* SYSTEM_CONTROL_INLINE_H */
