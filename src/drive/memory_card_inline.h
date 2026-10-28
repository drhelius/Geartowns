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

#ifndef MEMORY_CARD_INLINE_H
#define MEMORY_CARD_INLINE_H

#include "memory_card.h"

INLINE bool MemoryCard::IsInserted() const
{
    return m_state.inserted;
}

INLINE bool MemoryCard::IsWriteProtected() const
{
    return m_state.write_protected;
}

INLINE void MemoryCard::SetWriteProtected(bool write_protected)
{
    m_state.write_protected = write_protected;
}

INLINE bool MemoryCard::IsDirty() const
{
    return m_state.dirty;
}

INLINE void MemoryCard::ClearDirty()
{
    m_state.dirty = false;
}

INLINE u32 MemoryCard::GetWriteCount() const
{
    return m_write_count;
}

INLINE u32 MemoryCard::GetSize() const
{
    return m_state.size;
}

INLINE const u8* MemoryCard::GetImage() const
{
    return m_image;
}

INLINE u64 MemoryCard::GetSession() const
{
    return m_session;
}

INLINE u64 MemoryCard::GetLastWrite() const
{
    return m_last_write;
}

INLINE u32 MemoryCard::GetWindowBase(int window) const
{
    return m_window_base[window & 1];
}

INLINE u8 MemoryCard::ReadCard(u32 offset) const
{
    if (!m_state.inserted || m_state.attribute || offset >= m_state.size)
        return 0xFF;

    return m_image[offset];
}

INLINE MemoryCard::MemoryCard_State* MemoryCard::GetState()
{
    return &m_state;
}

#endif /* MEMORY_CARD_INLINE_H */
