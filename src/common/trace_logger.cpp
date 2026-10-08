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
    m_position = 0;
    m_count = 0;
    m_capacity = 0;
    m_enabled_flags = 0;
    m_active_flags = 0;
    m_active = false;

    for (int i = 0; i < TRACE_TYPE_COUNT; i++)
        m_event_filters[i] = 0xFFFFFFFFU;

    m_total_logged = 0;
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

void TraceLogger::Reset()
{
    m_position = 0;
    m_count = 0;
    m_total_logged = 0;
}

// The buffer is only allocated when the debugger asks for it, so a machine that never traces pays nothing
bool TraceLogger::SetCapacity(u32 capacity)
{
#if !defined(GT_DISABLE_DISASSEMBLER)
    if (capacity == 0)
        return false;

    if (capacity == m_capacity && IsValidPointer(m_buffer))
        return true;

    GT_Trace_Entry* buffer = new (std::nothrow) GT_Trace_Entry[capacity];

    if (!IsValidPointer(buffer))
        return false;

    SafeDeleteArray(m_buffer);
    m_buffer = buffer;
    m_capacity = capacity;
    UpdateEnabled();
    Reset();
    return true;
#else
    UNUSED(capacity);
    return false;
#endif
}

void TraceLogger::SetActive(bool active)
{
    m_active = active;
    UpdateEnabled();
}

void TraceLogger::SetEnabledFlags(u32 flags)
{
    m_enabled_flags = flags;
    UpdateEnabled();
}

void TraceLogger::SetEventFilter(GT_Trace_Type type, u32 filter)
{
    if (type < TRACE_TYPE_COUNT)
        m_event_filters[type] = filter;
}

u32 TraceLogger::GetEnabledFlags() const
{
    return m_enabled_flags;
}

u32 TraceLogger::GetEventFilter(GT_Trace_Type type) const
{
    return type < TRACE_TYPE_COUNT ? m_event_filters[type] : 0;
}

u32 TraceLogger::GetCount() const
{
    return m_count;
}

u32 TraceLogger::GetCapacity() const
{
    return m_capacity;
}

// Entries logged since the last reset, so disk output can tell which entries are new
u64 TraceLogger::GetTotalLogged() const
{
    return m_total_logged;
}

// Entries logged since the machine started, never reset
u64 TraceLogger::GetSequence() const
{
    return m_sequence;
}

// Index 0 is the oldest retained entry
const GT_Trace_Entry& TraceLogger::GetEntry(u32 index) const
{
    static const GT_Trace_Entry k_empty = {};

    if (!IsValidPointer(m_buffer) || index >= m_count)
        return k_empty;

    u32 first = m_count < m_capacity ? 0 : m_position;
    return m_buffer[(first + index) % m_capacity];
}

void TraceLogger::UpdateEnabled()
{
    m_active_flags = (m_active && IsValidPointer(m_buffer)) ? m_enabled_flags : 0;
}
