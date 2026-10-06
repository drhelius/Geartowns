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

#include <new>
#include "trace_logger.h"

static u64 k_trace_no_clocks = 0;

TraceLogger::TraceLogger()
{
    InitPointer(m_buffer);
    m_clocks = &k_trace_no_clocks;
    m_capacity = 0;
    m_position = 0;
    m_count = 0;
    m_flags = 0;
    m_active_flags = 0;
    m_running = false;
    m_active = false;
    m_sequence = 0;
}

TraceLogger::~TraceLogger()
{
    SafeDeleteArray(m_buffer);
}

void TraceLogger::Init(const u64* clocks)
{
    m_clocks = IsValidPointer(clocks) ? clocks : &k_trace_no_clocks;
}

void TraceLogger::Clear()
{
    m_position = 0;
    m_count = 0;
}

// The buffer is only allocated when tracing starts, so a machine that never traces pays nothing
bool TraceLogger::SetCapacity(u32 capacity)
{
    capacity = MAX(capacity, 1000U);

    if (capacity == m_capacity && IsValidPointer(m_buffer))
        return true;

    GT_Trace_Entry* buffer = new (std::nothrow) GT_Trace_Entry[capacity];

    if (!IsValidPointer(buffer))
        return false;

    SafeDeleteArray(m_buffer);
    m_buffer = buffer;
    m_capacity = capacity;
    Clear();
    return true;
}

u32 TraceLogger::GetCapacity() const
{
    return m_capacity;
}

void TraceLogger::Start(u32 flags)
{
    if (!IsValidPointer(m_buffer) && !SetCapacity(GT_TRACE_DEFAULT_CAPACITY))
        return;

    m_flags = flags & TRACE_FLAG_ALL;
    m_running = true;
    UpdateActiveFlags();
}

void TraceLogger::Stop()
{
    m_running = false;
    UpdateActiveFlags();
}

bool TraceLogger::IsRunning() const
{
    return m_running;
}

void TraceLogger::SetActive(bool active)
{
    m_active = active;
    UpdateActiveFlags();
}

u32 TraceLogger::GetFlags() const
{
    return m_flags;
}

u32 TraceLogger::GetCount() const
{
    return m_count;
}

// Entries logged since the machine started, so readers can tell which entries are new
u64 TraceLogger::GetSequence() const
{
    return m_sequence;
}

// Index 0 is the oldest retained entry
const GT_Trace_Entry& TraceLogger::GetEntry(u32 index) const
{
    u32 first = m_count < m_capacity ? 0 : m_position;
    return m_buffer[(first + index) % m_capacity];
}

void TraceLogger::UpdateActiveFlags()
{
    m_active_flags = (m_running && m_active && IsValidPointer(m_buffer)) ? m_flags : 0;
}
