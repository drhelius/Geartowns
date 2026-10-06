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

#ifndef FLOPPY_DISK_INLINE_H
#define FLOPPY_DISK_INLINE_H

#include "floppy_disk.h"

INLINE bool FloppyDisk::IsInserted() const
{
    return m_state.inserted;
}

INLINE bool FloppyDisk::IsWriteProtected() const
{
    return m_state.write_protected;
}

INLINE void FloppyDisk::SetWriteProtected(bool write_protected)
{
    m_state.write_protected = write_protected;
}

INLINE bool FloppyDisk::IsDirty() const
{
    return m_state.dirty;
}

INLINE void FloppyDisk::ClearDirty()
{
    m_state.dirty = false;
}

INLINE u32 FloppyDisk::GetBaseCRC() const
{
    return m_state.base_crc;
}

INLINE FloppyDisk_Media FloppyDisk::GetMedia() const
{
    return (FloppyDisk_Media)m_state.media;
}

INLINE u32 FloppyDisk::GetRPM() const
{
    return m_state.rpm;
}

INLINE const u8* FloppyDisk::GetImage() const
{
    return m_image;
}

INLINE u32 FloppyDisk::GetImageSize() const
{
    return m_state.size;
}

INLINE u8 FloppyDisk::ReadByte(u32 offset, u8 fill) const
{
    return offset < m_state.size ? m_image[offset] : fill;
}

INLINE void FloppyDisk::WriteByte(u32 offset, u8 value)
{
    if (offset >= m_state.size)
        return;

    m_image[offset] = value;
    m_state.dirty = true;
}

INLINE FloppyDisk::FloppyDisk_State* FloppyDisk::GetState()
{
    return &m_state;
}

#endif /* FLOPPY_DISK_INLINE_H */
