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

#ifndef IDLE_FLUSH_H
#define IDLE_FLUSH_H

#include "types.h"

static const int k_idle_flush_hard_disk_frames = 120;
static const int k_idle_flush_floppy_frames = 180;
static const int k_idle_flush_cmos_frames = 180;
static const int k_idle_flush_memory_card_frames = 180;

class IdleFlush
{
public:
    IdleFlush()
    {
        Reset();
    }

    void Reset()
    {
        m_write_count = 0;
        m_idle_frames = 0;
        m_failed = false;
    }

    bool Tick(u32 write_count, bool dirty, int frames)
    {
        if (write_count != m_write_count)
        {
            m_write_count = write_count;
            m_idle_frames = 0;
            return false;
        }

        if (!dirty || (++m_idle_frames < frames))
            return false;

        m_idle_frames = 0;
        return true;
    }

    bool Done(bool saved)
    {
        bool report = !saved && !m_failed;
        m_failed = !saved;
        m_idle_frames = 0;
        return report;
    }

    bool HasFailed() const
    {
        return m_failed;
    }

private:
    u32 m_write_count;
    int m_idle_frames;
    bool m_failed;
};

#endif /* IDLE_FLUSH_H */
