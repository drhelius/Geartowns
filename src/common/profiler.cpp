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
#if !defined(GT_DISABLE_DISASSEMBLER)
    m_functions = new (std::nothrow) GT_Profiler_Function[GT_PROFILER_MAX_FUNCTIONS];
    m_hash = new (std::nothrow) u16[GT_PROFILER_HASH_SIZE];
#else
    InitPointer(m_functions);
    InitPointer(m_hash);
#endif
    m_clocks = &k_profiler_no_clocks;
    m_function_count = 0;
    m_depth = 0;
    m_enabled = false;
    m_enable_request = false;
    m_active = false;
    m_last_cycle = 0;
    m_total_cycles = 0;
    m_interrupt_cycles = 0;
    m_frames = 0;
    Reset();
}

Profiler::~Profiler()
{
    SafeDeleteArray(m_hash);
    SafeDeleteArray(m_functions);
}

void Profiler::Init(const u64* clocks)
{
    m_clocks = IsValidPointer(clocks) ? clocks : &k_profiler_no_clocks;
    ResetStack();
}

// Function 0 is the root, which takes the cycles spent outside any tracked call
void Profiler::Reset()
{
    m_function_count = 0;
    m_total_cycles = 0;
    m_frames = 0;

    if (IsValidPointer(m_hash))
    {
        for (int i = 0; i < GT_PROFILER_HASH_SIZE; i++)
            m_hash[i] = GT_PROFILER_INVALID;
    }

    if (IsValidPointer(m_functions))
        InitFunction(GT_PROFILER_ROOT, 0, 0, PROFILER_FUNCTION_ROOT);

    ResetStack();
}

void Profiler::ResetStack()
{
    m_depth = 0;
    m_interrupt_cycles = 0;
    m_last_cycle = *m_clocks;
}

void Profiler::Enable(bool enable)
{
    m_enable_request = enable;
    UpdateEnabled();
}

void Profiler::SetActive(bool active)
{
    m_active = active;
    UpdateEnabled();
}

void Profiler::Sync()
{
    if (m_enabled)
        Charge(*m_clocks);
}

void Profiler::Enter(u32 address, bool interrupt, u8 vector)
{
    u64 cycle = *m_clocks;
    Charge(cycle);

    u16 index = interrupt ? FindFunction(address, vector, PROFILER_FUNCTION_INTERRUPT) :
        FindFunction(address, 0, PROFILER_FUNCTION_CALL);

    // A full table still pushes a frame, so returns stay paired with the debugger call stack
    if (index == GT_PROFILER_INVALID)
        index = GT_PROFILER_ROOT;

    // A stack deeper than the debugger call stack drops its outermost frame like the call stack does
    if (m_depth >= GT_PROFILER_MAX_DEPTH)
    {
        memmove(&m_stack[0], &m_stack[1], sizeof(GT_Profiler_Frame) * (GT_PROFILER_MAX_DEPTH - 1));
        m_depth--;
    }

    GT_Profiler_Function* function = &m_functions[index];
    function->calls++;

    GT_Profiler_Frame* frame = &m_stack[m_depth];
    frame->enter_cycle = cycle;
    frame->interrupt_cycles = m_interrupt_cycles;
    frame->function = index;
    frame->interrupt = interrupt;
    frame->outermost = IsOutermost(index, interrupt);
    m_depth++;
}

void Profiler::Return()
{
    if (m_depth == 0)
        return;

    u64 cycle = *m_clocks;
    Charge(cycle);
    Leave(cycle);
}

void Profiler::CountFrame()
{
    if (m_enabled)
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

// Time while disabled is not charged to anything and calls made meanwhile are not on the stack
void Profiler::UpdateEnabled()
{
    bool enabled = m_enable_request && m_active && IsValidPointer(m_functions) && IsValidPointer(m_hash);

    if (enabled == m_enabled)
        return;

    Sync();
    m_enabled = enabled;
    ResetStack();
}

void Profiler::InitFunction(u16 index, u32 address, u8 vector, GT_Profiler_Function_Type type)
{
    GT_Profiler_Function* function = &m_functions[index];
    function->inclusive_cycles = 0;
    function->exclusive_cycles = 0;
    function->address = address;
    function->calls = 0;
    function->completed = 0;
    function->min_cycles = 0xFFFFFFFF;
    function->max_cycles = 0;
    function->vector = vector;
    function->type = type;

    if (index >= m_function_count)
        m_function_count = index + 1;
}

// Calls are keyed by their linear target and interrupts by their vector
u16 Profiler::FindFunction(u32 address, u8 vector, GT_Profiler_Function_Type type)
{
    bool interrupt = type == PROFILER_FUNCTION_INTERRUPT;
    u32 key = interrupt ? 0x80000000U | vector : address;
    u32 slot = (key * 2654435761U) >> (32 - GT_PROFILER_HASH_BITS);

    while (true)
    {
        u16 index = m_hash[slot];

        if (index == GT_PROFILER_INVALID)
        {
            if (m_function_count >= GT_PROFILER_MAX_FUNCTIONS)
                return GT_PROFILER_INVALID;

            index = (u16)m_function_count;
            InitFunction(index, address, vector, type);
            m_hash[slot] = index;
            return index;
        }

        const GT_Profiler_Function& function = m_functions[index];

        if (function.type == type && (interrupt ? function.vector == vector : function.address == address))
            return index;

        slot = (slot + 1) & (GT_PROFILER_HASH_SIZE - 1);
    }
}

void Profiler::Charge(u64 cycle)
{
    if (cycle <= m_last_cycle)
        return;

    u64 cycles = cycle - m_last_cycle;
    m_functions[GetCurrentFunction()].exclusive_cycles += cycles;
    m_total_cycles += cycles;
    m_last_cycle = cycle;
}

// Interrupt handlers that ran during a call are not part of its cycles
void Profiler::Leave(u64 cycle)
{
    m_depth--;
    GT_Profiler_Frame* frame = &m_stack[m_depth];
    GT_Profiler_Function* function = &m_functions[frame->function];

    u64 elapsed = (cycle > frame->enter_cycle) ? cycle - frame->enter_cycle : 0;
    u64 interrupt_cycles = m_interrupt_cycles - frame->interrupt_cycles;
    u64 cycles = (elapsed > interrupt_cycles) ? elapsed - interrupt_cycles : 0;

    if (frame->outermost)
    {
        function->inclusive_cycles += cycles;
        function->completed++;
    }

    AddSample(function, cycles);

    if (frame->interrupt)
        m_interrupt_cycles += cycles;
}

void Profiler::AddSample(GT_Profiler_Function* function, u64 cycles)
{
    u32 value = (cycles > 0xFFFFFFFF) ? 0xFFFFFFFF : (u32)cycles;

    if (value < function->min_cycles)
        function->min_cycles = value;

    if (value > function->max_cycles)
        function->max_cycles = value;
}

// A recursive call is only counted once, but an interrupt handler starts a new context
bool Profiler::IsOutermost(u16 function, bool interrupt) const
{
    if (interrupt)
        return true;

    for (int i = m_depth - 1; i >= 0; i--)
    {
        if (m_stack[i].function == function)
            return false;

        if (m_stack[i].interrupt)
            return true;
    }

    return true;
}

u16 Profiler::GetCurrentFunction() const
{
    if (m_depth > 0)
        return m_stack[m_depth - 1].function;

    return GT_PROFILER_ROOT;
}
