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
#include "profiler.h"

static u64 k_profiler_no_clocks = 0;

Profiler::Profiler()
{
    InitPointer(m_functions);
    InitPointer(m_hash);
    InitPointer(m_stack);
    m_clocks = &k_profiler_no_clocks;
    m_function_count = 0;
    m_depth = 0;
    m_last_cycle = 0;
    m_total_cycles = 0;
    m_interrupt_cycles = 0;
    m_frames = 0;
    m_running = false;
    m_active_request = false;
    m_active = false;
}

Profiler::~Profiler()
{
    SafeDeleteArray(m_functions);
    SafeDeleteArray(m_hash);
    SafeDeleteArray(m_stack);
}

void Profiler::Init(const u64* clocks)
{
    m_clocks = IsValidPointer(clocks) ? clocks : &k_profiler_no_clocks;
}

// Function 0 is the root, which takes the cycles spent outside any tracked call
void Profiler::Reset()
{
    m_function_count = 0;
    m_depth = 0;
    m_total_cycles = 0;
    m_interrupt_cycles = 0;
    m_frames = 0;
    m_last_cycle = *m_clocks;

    if (!IsValidPointer(m_functions))
        return;

    for (int i = 0; i < GT_PROFILER_HASH_SIZE; i++)
        m_hash[i] = GT_PROFILER_INVALID;

    memset(&m_functions[0], 0, sizeof(m_functions[0]));
    m_functions[0].min_cycles = 0xFFFFFFFF;
    m_functions[0].type = PROFILER_FUNCTION_ROOT;
    m_function_count = 1;
}

// The tables are only allocated when profiling starts
void Profiler::Start()
{
    if (!IsValidPointer(m_functions))
    {
        m_functions = new (std::nothrow) GT_Profiler_Function[GT_PROFILER_MAX_FUNCTIONS];
        m_hash = new (std::nothrow) u16[GT_PROFILER_HASH_SIZE];
        m_stack = new (std::nothrow) Frame[GT_PROFILER_MAX_DEPTH];

        if (!IsValidPointer(m_functions) || !IsValidPointer(m_hash) || !IsValidPointer(m_stack))
        {
            SafeDeleteArray(m_functions);
            SafeDeleteArray(m_hash);
            SafeDeleteArray(m_stack);
            return;
        }

        Reset();
    }

    m_running = true;
    UpdateActive();
}

void Profiler::Stop()
{
    m_running = false;
    UpdateActive();
}

bool Profiler::IsRunning() const
{
    return m_running;
}

void Profiler::SetActive(bool active)
{
    m_active_request = active;
    UpdateActive();
}

// Time while inactive is not charged to anything
void Profiler::UpdateActive()
{
    bool active = m_running && m_active_request && IsValidPointer(m_functions);

    if (active == m_active)
        return;

    if (m_active)
        Charge();

    m_active = active;
    m_last_cycle = *m_clocks;
}

void Profiler::Enter(u32 address, bool interrupt, u8 vector)
{
    Charge();
    u16 index = FindFunction(address, interrupt, vector);

    if (index == GT_PROFILER_INVALID)
        return;

    // A stack deeper than the debugger call stack drops its outermost frame like the call stack does
    if (m_depth == GT_PROFILER_MAX_DEPTH)
    {
        memmove(&m_stack[0], &m_stack[1], sizeof(Frame) * (GT_PROFILER_MAX_DEPTH - 1));
        m_depth--;
    }

    m_functions[index].calls++;

    Frame& frame = m_stack[m_depth];
    frame.enter_cycle = m_last_cycle;
    frame.interrupt_cycles = m_interrupt_cycles;
    frame.function = index;
    frame.interrupt = interrupt;
    frame.outermost = !IsOnStack(index);
    m_depth++;
}

// Interrupt handlers that ran during a call are not part of its cycles
void Profiler::Leave()
{
    if (m_depth == 0)
        return;

    Charge();
    m_depth--;

    Frame& frame = m_stack[m_depth];
    GT_Profiler_Function& function = m_functions[frame.function];
    u64 elapsed = m_last_cycle - frame.enter_cycle;
    u64 interrupted = m_interrupt_cycles - frame.interrupt_cycles;
    u64 cycles = elapsed > interrupted ? elapsed - interrupted : 0;

    if (frame.outermost)
    {
        function.inclusive_cycles += cycles;
        function.completed++;
        function.min_cycles = MIN(function.min_cycles, (u32)MIN(cycles, (u64)0xFFFFFFFF));
        function.max_cycles = MAX(function.max_cycles, (u32)MIN(cycles, (u64)0xFFFFFFFF));
    }

    if (frame.interrupt)
        m_interrupt_cycles += cycles;
}

void Profiler::CountFrame()
{
    if (m_active)
        m_frames++;
}

const GT_Profiler_Function* Profiler::GetFunctions() const
{
    return m_functions;
}

u32 Profiler::GetFunctionCount() const
{
    return m_function_count;
}

u64 Profiler::GetTotalCycles() const
{
    return m_total_cycles;
}

u32 Profiler::GetFrames() const
{
    return m_frames;
}

u16 Profiler::FindFunction(u32 address, bool interrupt, u8 vector)
{
    u32 key = interrupt ? 0x80000000U | vector : address;
    u32 slot = (key * 2654435761U) >> (32 - GT_PROFILER_HASH_BITS);

    while (true)
    {
        u16 index = m_hash[slot];

        if (index == GT_PROFILER_INVALID)
        {
            if (m_function_count >= GT_PROFILER_MAX_FUNCTIONS)
                return GT_PROFILER_INVALID;

            index = (u16)m_function_count++;
            GT_Profiler_Function& function = m_functions[index];
            memset(&function, 0, sizeof(function));
            function.address = address;
            function.min_cycles = 0xFFFFFFFF;
            function.type = interrupt ? PROFILER_FUNCTION_INTERRUPT : PROFILER_FUNCTION_CALL;
            function.vector = vector;
            m_hash[slot] = index;
            return index;
        }

        const GT_Profiler_Function& function = m_functions[index];
        bool match = interrupt ? function.type == PROFILER_FUNCTION_INTERRUPT && function.vector == vector :
            function.type == PROFILER_FUNCTION_CALL && function.address == address;

        if (match)
            return index;

        slot = (slot + 1) & (GT_PROFILER_HASH_SIZE - 1);
    }
}

void Profiler::Charge()
{
    u64 cycle = *m_clocks;

    if (cycle <= m_last_cycle)
        return;

    u64 cycles = cycle - m_last_cycle;
    m_functions[m_depth > 0 ? m_stack[m_depth - 1].function : 0].exclusive_cycles += cycles;
    m_total_cycles += cycles;
    m_last_cycle = cycle;
}

bool Profiler::IsOnStack(u16 function) const
{
    for (int i = 0; i < m_depth; i++)
    {
        if (m_stack[i].function == function)
            return true;
    }

    return false;
}
