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
static u32 k_trace_no_pc = 0;

TraceLogger::TraceLogger()
{
    InitPointer(m_buffer);
    m_clocks = &k_trace_no_clocks;
    m_pc_base = &k_trace_no_pc;
    m_pc_offset = &k_trace_no_pc;
    m_position = 0;
    m_count = 0;
    m_capacity = 0;
    m_enabled_flags = 0;
    m_active_flags = 0;
    m_active = false;
    m_dispatching = false;

    for (int i = 0; i < TRACE_TYPE_COUNT; i++)
        m_event_filters[i] = 0xFFFFFFFFU;

    m_total_logged = 0;
    m_sequence = 0;
    ClearFilters(m_filters);
    m_code_filtered = false;
    m_port_filtered = false;
    m_vector_filtered = false;
}

TraceLogger::~TraceLogger()
{
    SafeDeleteArray(m_buffer);
}

TraceLogger* TraceLogger::GetDisabled()
{
    static TraceLogger k_disabled;
    return &k_disabled;
}

TraceLogger* TraceLogger::Resolve(TraceLogger* trace_logger)
{
    return IsValidPointer(trace_logger) ? trace_logger : GetDisabled();
}

void TraceLogger::Init(const u64* clocks)
{
    m_clocks = IsValidPointer(clocks) ? clocks : &k_trace_no_clocks;
}

void TraceLogger::SetPCSource(const u32* base, const u32* offset)
{
    bool valid = IsValidPointer(base) && IsValidPointer(offset);
    m_pc_base = valid ? base : &k_trace_no_pc;
    m_pc_offset = valid ? offset : &k_trace_no_pc;
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
    if (m_active && !active)
        Mark(TRACE_MARK_DEBUGGER_OFF, 0);

    m_active = active;
    UpdateEnabled();
}

void TraceLogger::Mark(u8 event, u64 value, u32 detail, u8 cause)
{
    if (!IsRecording())
        return;

    bool dispatching = m_dispatching;
    m_dispatching = true;
    GT_Trace_Entry* entry = Begin(TRACE_MARK, event);
    m_dispatching = dispatching;
    entry->mark.value = value;
    entry->mark.detail = detail;
    entry->mark.cause = cause;
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

u64 TraceLogger::GetOverwritten() const
{
    return m_total_logged - m_count;
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

void TraceLogger::ClearFilters(GT_Trace_Filters& filters)
{
    memset(&filters, 0, sizeof(filters));

    for (int i = 0; i < 8; i++)
        filters.vectors[i] = 0xFFFFFFFFU;
}

void TraceLogger::SetFilters(const GT_Trace_Filters& filters)
{
    m_filters = filters;
    m_filters.code_include_count = MIN(m_filters.code_include_count, (u8)GT_TRACE_FILTER_RANGES);
    m_filters.code_exclude_count = MIN(m_filters.code_exclude_count, (u8)GT_TRACE_FILTER_RANGES);
    m_filters.port_include_count = MIN(m_filters.port_include_count, (u8)GT_TRACE_FILTER_RANGES);
    m_filters.port_exclude_count = MIN(m_filters.port_exclude_count, (u8)GT_TRACE_FILTER_RANGES);
    m_filters.function_count = MIN(m_filters.function_count, (u8)GT_TRACE_FILTER_FUNCTIONS);
    m_code_filtered = m_filters.code_include_count != 0 || m_filters.code_exclude_count != 0;
    m_port_filtered = m_filters.port_include_count != 0 || m_filters.port_exclude_count != 0;
    m_vector_filtered = m_filters.function_count != 0;

    for (int i = 0; i < 8; i++)
        m_vector_filtered = m_vector_filtered || m_filters.vectors[i] != 0xFFFFFFFFU;
}

const GT_Trace_Filters& TraceLogger::GetFilters() const
{
    return m_filters;
}

bool TraceLogger::MatchesCode(u32 linear) const
{
    bool included = m_filters.code_include_count == 0;

    for (int i = 0; !included && i < m_filters.code_include_count; i++)
        included = linear >= m_filters.code_include[i].first && linear <= m_filters.code_include[i].last;

    for (int i = 0; included && i < m_filters.code_exclude_count; i++)
        included = linear < m_filters.code_exclude[i].first || linear > m_filters.code_exclude[i].last;

    return included;
}

bool TraceLogger::MatchesPort(u16 port, u8 event) const
{
    bool included = m_filters.port_include_count == 0;

    for (int i = 0; !included && i < m_filters.port_include_count; i++)
    {
        const GT_Trace_Range& range = m_filters.port_include[i];
        included = IS_SET_BIT(range.events, event) && port >= range.first && port <= range.last;
    }

    for (int i = 0; included && i < m_filters.port_exclude_count; i++)
    {
        const GT_Trace_Range& range = m_filters.port_exclude[i];
        included = !IS_SET_BIT(range.events, event) || port < range.first || port > range.last;
    }

    return included;
}

bool TraceLogger::MatchesFunction(u8 vector, u16 ax) const
{
    if (!IS_SET_BIT(m_filters.vectors[vector >> 5], vector & 31))
        return false;

    bool listed = false;

    for (int i = 0; i < m_filters.function_count; i++)
    {
        const GT_Trace_Function& function = m_filters.functions[i];

        if (function.vector != vector)
            continue;

        listed = true;

        if (function.match == TRACE_FUNCTION_ANY || (function.match == TRACE_FUNCTION_AH &&
            (ax >> 8) == (function.value & 0xFF)) || (function.match == TRACE_FUNCTION_AX && ax == function.value))
            return true;
    }

    return !listed;
}

void TraceLogger::UpdateEnabled()
{
    u32 flags = m_enabled_flags & ~TRACE_FLAG_MARK;
    m_active_flags = (m_active && IsValidPointer(m_buffer) && flags != 0) ? flags | TRACE_FLAG_MARK : 0;
}
