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

#ifndef PROFILER_H
#define PROFILER_H

#include "common.h"

#define GT_PROFILER_MAX_FUNCTIONS 8192
#define GT_PROFILER_HASH_BITS 14
#define GT_PROFILER_HASH_SIZE (1 << GT_PROFILER_HASH_BITS)
#define GT_PROFILER_MAX_DEPTH 256
#define GT_PROFILER_ROOT 0
#define GT_PROFILER_INVALID 0xFFFF

static_assert(GT_PROFILER_MAX_FUNCTIONS < GT_PROFILER_HASH_SIZE, "Profiler hash table too small");

enum GT_Profiler_Function_Type : u8
{
    PROFILER_FUNCTION_ROOT = 0,
    PROFILER_FUNCTION_CALL,
    PROFILER_FUNCTION_INTERRUPT
};

struct GT_Profiler_Function
{
    u64 inclusive_cycles;
    u64 exclusive_cycles;
    u32 address;
    u32 calls;
    u32 completed;
    u32 min_cycles;
    u32 max_cycles;
    u8 vector;
    GT_Profiler_Function_Type type;
};

struct GT_Profiler_Frame
{
    u64 enter_cycle;
    u64 interrupt_cycles;
    u16 function;
    bool interrupt;
    bool outermost;
};

class Profiler
{
public:
    Profiler();
    ~Profiler();
    void Init(const u64* clocks);
    void Reset();
    void ResetStack();
    void Enable(bool enable);
    void SetActive(bool active);
    INLINE bool IsEnabled() const;
    void Sync();
    void Enter(u32 address, bool interrupt, u8 vector);
    void Return();
    void CountFrame();
    const GT_Profiler_Function* GetFunctions() const;
    u32 GetFunctionCount() const;
    u64 GetTotalCycles() const;
    u32 GetFrames() const;

private:
    void UpdateEnabled();
    void InitFunction(u16 index, u32 address, u8 vector, GT_Profiler_Function_Type type);
    u16 FindFunction(u32 address, u8 vector, GT_Profiler_Function_Type type);
    void Charge(u64 cycle);
    void Leave(u64 cycle);
    void AddSample(GT_Profiler_Function* function, u64 cycles);
    u16 GetCurrentFunction() const;
    bool IsOutermost(u16 function, bool interrupt) const;

private:
    GT_Profiler_Function* m_functions;
    u16* m_hash;
    u32 m_function_count;
    GT_Profiler_Frame m_stack[GT_PROFILER_MAX_DEPTH];
    int m_depth;
    bool m_enabled;
    bool m_enable_request;
    bool m_active;
    u64 m_last_cycle;
    u64 m_total_cycles;
    u64 m_interrupt_cycles;
    u32 m_frames;
    const u64* m_clocks;
};

// Calls and returns are only counted while the profiler is enabled and the debugger runs the machine
INLINE bool Profiler::IsEnabled() const
{
    return m_enabled;
}

#endif /* PROFILER_H */
