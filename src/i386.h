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

#ifndef I386_H
#define I386_H

#include "common.h"
#include "debug_memory.h"

class Memory;

enum I386_Segment_Register
{
    I386_SEGMENT_ES = 0,
    I386_SEGMENT_CS,
    I386_SEGMENT_SS,
    I386_SEGMENT_DS,
    I386_SEGMENT_FS,
    I386_SEGMENT_GS,
    I386_SEGMENT_COUNT
};

struct I386_Debug_Segment_State
{
    u16 selector;
    u32 base;
    u32 limit;
    u32 access;
    bool present;
};

struct I386_Debug_State
{
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
    u32 esi;
    u32 edi;
    u32 ebp;
    u32 esp;
    u32 eip;
    u32 eflags;
    u32 cr0;
    u32 cr2;
    u32 cr3;
    I386_Debug_Segment_State segment[I386_SEGMENT_COUNT];
    bool available;
};

class I386
{
public:
    I386();
    ~I386();
    void Init(Memory* memory);
    void Reset();
    bool TryPeekLogical(I386_Segment_Register segment, u32 offset,
        u8& value) const;
    bool TryPeekLogical(u16 selector, u32 offset, u8& value) const;
    bool TryPeekCode(u32 eip, u8& value) const;
    bool TryPeekLinear(u32 linear, u8& value) const;
    bool TryTranslateLinear(u32 linear, u32& physical) const;
    bool DebugTranslateLogical(I386_Segment_Register segment, u32 offset,
        GT_Debug_Memory_Translation& translation) const;
    bool DebugTranslateLogical(u16 selector, u32 offset,
        GT_Debug_Memory_Translation& translation) const;
    bool DebugTranslateLinear(u32 linear,
        GT_Debug_Memory_Translation& translation) const;
    bool CopyDebugState(I386_Debug_State& state) const;
    bool GetDebugRegisterValue(const char* name, u32& value) const;
    void SetDebugState(const I386_Debug_State& state);

private:
    bool TranslateLinearPassive(u32 linear, u32& physical,
        u32& pde_address, u32& pte_address, u32& page_flags,
        char* reason, size_t reason_size) const;
    bool ReadPhysical32(u32 physical, u32& value) const;
    const I386_Debug_Segment_State* FindSegment(u16 selector) const;

private:
    Memory* m_memory;
    I386_Debug_State m_debug_state;
};

#endif /* I386_H */
