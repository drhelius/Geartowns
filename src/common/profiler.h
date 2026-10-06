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
#define GT_PROFILER_INVALID 0xFFFF

enum GT_Profiler_Function_Type
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
    u8 type;
    u8 vector;
};

class Profiler
{
public:
    Profiler();
    ~Profiler();
    void Init(const u64* clocks);
    void Reset();
    void Start();
    void Stop();
    bool IsRunning() const;
    void SetActive(bool active);
    INLINE bool IsActive() const;
    void Enter(u32 address, bool interrupt, u8 vector);
    void Leave();
    void CountFrame();
    const GT_Profiler_Function* GetFunctions() const;
    u32 GetFunctionCount() const;
    u64 GetTotalCycles() const;
    u32 GetFrames() const;

private:
    struct Frame
    {
        u64 enter_cycle;
        u64 interrupt_cycles;
        u16 function;
        bool interrupt;
        bool outermost;
    };

    void UpdateActive();
    u16 FindFunction(u32 address, bool interrupt, u8 vector);
    void Charge();
    bool IsOnStack(u16 function) const;

private:
    GT_Profiler_Function* m_functions;
    u16* m_hash;
    Frame* m_stack;
    const u64* m_clocks;
    u32 m_function_count;
    int m_depth;
    u64 m_last_cycle;
    u64 m_total_cycles;
    u64 m_interrupt_cycles;
    u32 m_frames;
    bool m_running;
    bool m_active_request;
    bool m_active;
};

// Calls and returns are only counted while the debugger runs the machine and the profiler is started
INLINE bool Profiler::IsActive() const
{
    return m_active;
}

#endif /* PROFILER_H */
