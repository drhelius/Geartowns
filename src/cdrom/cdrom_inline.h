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

#ifndef CDROM_INLINE_H
#define CDROM_INLINE_H

#include "cdrom.h"

INLINE void CdRom::Synchronize(u64 clocks)
{
    if (clocks >= GetNextClocks())
        RunEvents(clocks);
}

INLINE u64 CdRom::GetNextClocks() const
{
    u64 next = (m_state.event != CDROM_EVENT_NONE) ? m_state.event_clocks : GT_NO_EVENT;

    if ((m_state.execute_clocks != 0) && (m_state.execute_clocks < next))
        next = m_state.execute_clocks;

    if ((m_state.cdda_end_clocks != 0) && (m_state.cdda_end_clocks < next))
        next = m_state.cdda_end_clocks;

    return next;
}

INLINE void CdRom::HandleEvent(u64 clocks)
{
    Synchronize(clocks);
    UpdateNextEvent();
}

INLINE u32 CdRom::GetAccessCount() const
{
    return m_access_count;
}

INLINE bool CdRom::IsBusy() const
{
    return (m_state.event == CDROM_EVENT_SEEK_DONE) || (m_state.event == CDROM_EVENT_SECTOR) ||
        (m_state.event == CDROM_EVENT_READ_ERROR) || (m_state.event == CDROM_EVENT_TOC_ENTRY);
}

INLINE CdRom::CdRom_State* CdRom::GetState()
{
    return &m_state;
}

#endif /* CDROM_INLINE_H */
