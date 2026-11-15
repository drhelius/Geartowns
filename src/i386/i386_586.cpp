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

#include "i386.h"
#include "i386_opcodes_inline.h"
#include "x87.h"

static const u32 k_i586_msrs = 0x000F7BF7;
static const u32 k_i586_msr_tsc = 0x10;

bool I386::OPCode0F_0x30()
{
    return OPCodes0F_MSR();
}

bool I386::OPCode0F_0x31()
{
    return OPCodes0F_RDTSC();
}

bool I386::OPCode0F_0x32()
{
    return OPCodes0F_MSR();
}

bool I386::OPCode0F_0xC7()
{
    return OPCodes0F_CMPXCHG8B();
}

void I386::SetResetSignature(u32 signature)
{
    m_reset_edx = signature;
}

void I386::SetTimeStampSource(const u64* cycles)
{
    m_tsc_source = cycles;
}

void I386::SetGameTiming(bool blocks, bool interrupts)
{
    m_game_blocks = blocks;
    m_game_interrupts = interrupts;
}

bool I386::OPCodes0F_RDTSC()
{
    if (!m_i586)
        return OPCodes_Invalid();

    u32 clocks = m_state.current_privilege_level == 0 ? 6 : 11;

    if (!DecodeAndStart(false, 0, clocks, clocks))
        return false;

    if ((m_state.cr4 & k_i386_cr4_tsd) != 0 && m_state.current_privilege_level != 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u64 stamp = ReadTimeStamp();
    m_state.registers[I386_REG_EAX].value = (u32)stamp;
    m_state.registers[I386_REG_EDX].value = (u32)(stamp >> 32);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_MSR()
{
    if (!m_i586)
        return OPCodes_Invalid();

    bool write = m_instruction.opcode2 == 0x30;
    u32 clocks = write ? 30 : 20;

    if (!DecodeAndStart(false, 0, clocks, clocks))
        return false;

    if (m_state.execution_mode != I386_MODE_REAL && m_state.current_privilege_level != 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u32 index = m_state.registers[I386_REG_ECX].value;

    if (write)
    {
        u64 value = ((u64)m_state.registers[I386_REG_EDX].value << 32) | m_state.registers[I386_REG_EAX].value;

        if (!WriteMSR(index, value))
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);
    }
    else
    {
        u64 value = 0;

        if (!ReadMSR(index, value))
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        m_state.registers[I386_REG_EAX].value = (u32)value;
        m_state.registers[I386_REG_EDX].value = (u32)(value >> 32);
    }

    CommitEIP(m_instruction);
    return true;
}

bool I386::ReadMSR(u32 index, u64& value) const
{
    if (index >= I386_MSR_COUNT || ((k_i586_msrs >> index) & 1) == 0)
        return false;

    value = index == k_i586_msr_tsc ? ReadTimeStamp() : m_msrs[index];
    return true;
}

bool I386::PeekMSR(u32 index, u64& value) const
{
    if (index >= I386_MSR_COUNT || ((k_i586_msrs >> index) & 1) == 0)
        return false;

    value = index == k_i586_msr_tsc ? GetTimeStamp() : m_msrs[index];
    return true;
}

bool I386::WriteMSR(u32 index, u64 value)
{
    if (index >= I386_MSR_COUNT || ((k_i586_msrs >> index) & 1) == 0)
        return false;

    if (index == k_i586_msr_tsc)
        m_tsc_offset += value - ReadTimeStamp();
    else
        m_msrs[index] = value;

    return true;
}

bool I386::OPCodes0F_CMPXCHG8B()
{
    if (!m_i586)
        return OPCodes_Invalid();

    if (!DecodeOperands(true, 0))
        return false;

    if (!StartExecution(10))
        return false;

    if (m_instruction.reg != 1 || !m_instruction.memory_operand)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    GT_Bus_Access_Context& context = *m_bus_context;
    int segment = m_instruction.segment;
    u32 low_offset = m_instruction.effective_offset;
    u32 high_offset = Truncate(low_offset + 4, m_instruction.address_size * 8);

    if (!CheckMemoryAccess(segment, low_offset, 4, true, context))
        return false;

    if (!CheckMemoryAccess(segment, high_offset, 4, true, context))
        return false;

    u32 low = 0;
    u32 high = 0;

    if (!ReadMemory(segment, low_offset, 32, context, low))
        return false;

    if (!ReadMemory(segment, high_offset, 32, context, high))
        return false;

    bool equal = low == m_state.registers[I386_REG_EAX].value && high == m_state.registers[I386_REG_EDX].value;
    u32 new_low = equal ? m_state.registers[I386_REG_EBX].value : low;
    u32 new_high = equal ? m_state.registers[I386_REG_ECX].value : high;

    if (!WriteMemory(segment, low_offset, 32, new_low, context))
        return false;

    if (!WriteMemory(segment, high_offset, 32, new_high, context))
        return false;

    if (equal)
        m_state.eflags |= I386_FLAG_ZF;
    else
    {
        m_state.eflags &= ~I386_FLAG_ZF;
        m_state.registers[I386_REG_EAX].value = low;
        m_state.registers[I386_REG_EDX].value = high;
    }

    CommitEIP(m_instruction);
    return true;
}

enum I586_Kind
{
    I586_KEEP = 0,
    I586_FIXED,
    I586_GROUP,
    I586_JUMP,
    I586_LOOP,
    I586_STRING,
    I586_MODE,
    I586_SEGMENT,
    I586_WIDTH,
    I586_ROTATE,
    I586_SCAN,
    I586_ENTER,
    I586_PORT,
    I586_ESCAPE
};

enum I586_Group
{
    I586_GROUP_1 = 0,
    I586_GROUP_POP,
    I586_GROUP_MOVE,
    I586_GROUP_SHIFT_IMMEDIATE,
    I586_GROUP_SHIFT_1,
    I586_GROUP_SHIFT_CL,
    I586_GROUP_3,
    I586_GROUP_5,
    I586_GROUP_0F00,
    I586_GROUP_0F01,
    I586_GROUP_0FBA,
    I586_GROUP_CONTROL,
    I586_GROUP_DEBUG_READ,
    I586_GROUP_DEBUG_WRITE,
    I586_GROUP_COUNT
};

static const u8 k_i586_memory_np = 0x40;

struct I586_Form
{
    u8 kind;
    u8 pairing;
    u8 flags;
    u16 reg;
    u16 memory;
};

static I586_Form k_i586_forms[512];
static I586_Form k_i586_groups[I586_GROUP_COUNT][8];

static void Set586Forms(int first, int last, u8 kind, u16 reg, u16 memory, u8 pairing, u8 flags)
{
    for (int i = first; i <= last; i++)
    {
        I586_Form& form = k_i586_forms[i];
        form.kind = kind;
        form.reg = reg;
        form.memory = memory;
        form.pairing = pairing;
        form.flags = flags;
    }
}

static void Set586Group(int group, int first, int last, u8 kind, u16 reg, u16 memory, u8 pairing, u8 flags)
{
    for (int i = first; i <= last; i++)
    {
        I586_Form& form = k_i586_groups[group][i];
        form.kind = kind;
        form.reg = reg;
        form.memory = memory;
        form.pairing = pairing;
        form.flags = flags;
    }
}

static const I586_Form& Get586Form(int two_byte, u8 opcode, u8 reg)
{
    const I586_Form& form = k_i586_forms[two_byte ? 256 + opcode : opcode];

    if (form.kind != I586_GROUP)
        return form;

    return k_i586_groups[form.reg][reg & 7];
}

void I386::Init586Forms()
{
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;
    Set586Forms(0, 511, I586_KEEP, 0, 0, k_i586_np, 0);

    for (int i = 0; i < I586_GROUP_COUNT; i++)
        Set586Group(i, 0, 7, I586_KEEP, 0, 0, k_i586_np, 0);

    for (int operation = 0; operation < 8; operation++)
    {
        int base = operation * 8;
        bool compare = operation == 7;
        u8 pairing = operation == 2 || operation == 3 ? k_i586_pu : k_i586_uv;

        Set586Forms(base, base + 1, I586_FIXED, 1, compare ? 2 : 3, pairing, compare ? 0 : k_i586_rmw);
        Set586Forms(base + 2, base + 3, I586_FIXED, 1, 2, pairing, 0);
        Set586Forms(base + 4, base + 5, I586_FIXED, 1, 1, pairing, 0);
    }

    Set586Forms(0x06, 0x06, I586_FIXED, 1, 1, k_i586_np, 0);
    Set586Forms(0x0E, 0x0E, I586_FIXED, 1, 1, k_i586_np, 0);
    Set586Forms(0x16, 0x16, I586_FIXED, 1, 1, k_i586_np, 0);
    Set586Forms(0x1E, 0x1E, I586_FIXED, 1, 1, k_i586_np, 0);
    Set586Forms(0x07, 0x07, I586_MODE, 3, 0, k_i586_np, 0);
    Set586Forms(0x17, 0x17, I586_MODE, 3, 0, k_i586_np, 0);
    Set586Forms(0x1F, 0x1F, I586_MODE, 3, 0, k_i586_np, 0);
    Set586Forms(0x27, 0x27, I586_FIXED, 3, 3, k_i586_np, 0);
    Set586Forms(0x2F, 0x2F, I586_FIXED, 3, 3, k_i586_np, 0);
    Set586Forms(0x37, 0x37, I586_FIXED, 3, 3, k_i586_np, 0);
    Set586Forms(0x3F, 0x3F, I586_FIXED, 3, 3, k_i586_np, 0);

    Set586Forms(0x40, 0x4F, I586_FIXED, 1, 1, k_i586_uv, 0);
    Set586Forms(0x50, 0x57, I586_FIXED, 1, 1, k_i586_uv, k_i586_push);
    Set586Forms(0x58, 0x5F, I586_FIXED, 1, 1, k_i586_uv, k_i586_pop);
    Set586Forms(0x60, 0x61, I586_FIXED, 5, 5, k_i586_np, 0);
    Set586Forms(0x62, 0x62, I586_FIXED, 8, 8, k_i586_np, 0);
    Set586Forms(0x63, 0x63, I586_FIXED, 7, 7, k_i586_np, 0);
    Set586Forms(0x68, 0x68, I586_FIXED, 1, 1, k_i586_uv, k_i586_push);
    Set586Forms(0x6A, 0x6A, I586_FIXED, 1, 1, k_i586_uv, k_i586_push);
    Set586Forms(0x69, 0x69, I586_FIXED, 10, 10, k_i586_np, 0);
    Set586Forms(0x6B, 0x6B, I586_FIXED, 10, 10, k_i586_np, 0);
    Set586Forms(0x6C, 0x6F, I586_PORT, 0, 0, k_i586_np, 0);
    Set586Forms(0x70, 0x7F, I586_JUMP, 1, 1, k_i586_pv, 0);

    Set586Forms(0x80, 0x83, I586_GROUP, I586_GROUP_1, 0, k_i586_np, 0);
    Set586Forms(0x84, 0x85, I586_FIXED, 1, 2, k_i586_uv, 0);
    Set586Forms(0x86, 0x87, I586_FIXED, 3, 3, k_i586_np, 0);
    Set586Forms(0x88, 0x8B, I586_FIXED, 1, 1, k_i586_uv, 0);
    Set586Forms(0x8C, 0x8C, I586_FIXED, 1, 1, k_i586_np, 0);
    Set586Forms(0x8D, 0x8D, I586_FIXED, 1, 1, k_i586_uv, 0);
    Set586Forms(0x8E, 0x8E, I586_SEGMENT, 2, 3, k_i586_np, 0);
    Set586Forms(0x8F, 0x8F, I586_GROUP, I586_GROUP_POP, 0, k_i586_np, 0);

    Set586Forms(0x90, 0x90, I586_FIXED, 1, 1, k_i586_uv, 0);
    Set586Forms(0x91, 0x97, I586_FIXED, 2, 2, k_i586_np, 0);
    Set586Forms(0x98, 0x98, I586_FIXED, 3, 3, k_i586_np, 0);
    Set586Forms(0x99, 0x99, I586_FIXED, 2, 2, k_i586_np, 0);
    Set586Forms(0x9A, 0x9A, I586_MODE, 4, 0, k_i586_np, 0);
    Set586Forms(0x9B, 0x9B, I586_FIXED, 1, 1, k_i586_np, 0);
    Set586Forms(0x9C, 0x9C, I586_MODE, 3, 9, k_i586_np, 0);
    Set586Forms(0x9D, 0x9D, I586_MODE, 4, 14, k_i586_np, 0);
    Set586Forms(0x9E, 0x9F, I586_FIXED, 2, 2, k_i586_np, 0);
    Set586Forms(0xA0, 0xA3, I586_FIXED, 1, 1, k_i586_uv, 0);
    Set586Forms(0xA4, 0xA7, I586_STRING, 0, 0, k_i586_np, 0);
    Set586Forms(0xA8, 0xA9, I586_FIXED, 1, 1, k_i586_uv, 0);
    Set586Forms(0xAA, 0xAF, I586_STRING, 0, 0, k_i586_np, 0);
    Set586Forms(0xB0, 0xBF, I586_FIXED, 1, 1, k_i586_uv, 0);

    Set586Forms(0xC0, 0xC1, I586_GROUP, I586_GROUP_SHIFT_IMMEDIATE, 0, k_i586_np, 0);
    Set586Forms(0xC2, 0xC2, I586_FIXED, 3, 3, k_i586_np, 0);
    Set586Forms(0xC3, 0xC3, I586_FIXED, 2, 2, k_i586_np, 0);
    Set586Forms(0xC4, 0xC5, I586_MODE, 4, 0, k_i586_np, 0);
    Set586Forms(0xC6, 0xC7, I586_GROUP, I586_GROUP_MOVE, 0, k_i586_np, 0);
    Set586Forms(0xC8, 0xC8, I586_ENTER, 0, 0, k_i586_np, 0);
    Set586Forms(0xC9, 0xC9, I586_FIXED, 3, 3, k_i586_np, 0);
    Set586Forms(0xCA, 0xCB, I586_MODE, 4, 0, k_i586_np, 0);
    Set586Forms(0xCF, 0xCF, I586_MODE, 7, 0, k_i586_np, 0);
    Set586Forms(0xD0, 0xD1, I586_GROUP, I586_GROUP_SHIFT_1, 0, k_i586_np, 0);
    Set586Forms(0xD2, 0xD3, I586_GROUP, I586_GROUP_SHIFT_CL, 0, k_i586_np, 0);
    Set586Forms(0xD4, 0xD4, I586_FIXED, 18, 18, k_i586_np, 0);
    Set586Forms(0xD5, 0xD5, I586_FIXED, 10, 10, k_i586_np, 0);
    Set586Forms(0xD7, 0xD7, I586_FIXED, 4, 4, k_i586_np, 0);
    Set586Forms(0xD8, 0xDF, I586_ESCAPE, 0, 0, k_i586_np, 0);

    Set586Forms(0xE0, 0xE1, I586_LOOP, 7, 8, k_i586_np, 0);
    Set586Forms(0xE2, 0xE2, I586_LOOP, 5, 6, k_i586_np, 0);
    Set586Forms(0xE3, 0xE3, I586_LOOP, 6, 5, k_i586_np, 0);
    Set586Forms(0xE4, 0xE7, I586_PORT, 0, 0, k_i586_np, 0);
    Set586Forms(0xE8, 0xE8, I586_FIXED, 1, 1, k_i586_pv, k_i586_push);
    Set586Forms(0xE9, 0xE9, I586_FIXED, 1, 1, k_i586_pv, 0);
    Set586Forms(0xEA, 0xEA, I586_MODE, 3, 0, k_i586_np, 0);
    Set586Forms(0xEB, 0xEB, I586_FIXED, 1, 1, k_i586_pv, 0);
    Set586Forms(0xEC, 0xEF, I586_PORT, 0, 0, k_i586_np, 0);
    Set586Forms(0xF5, 0xF5, I586_FIXED, 2, 2, k_i586_np, 0);
    Set586Forms(0xF6, 0xF7, I586_GROUP, I586_GROUP_3, 0, k_i586_np, 0);
    Set586Forms(0xF8, 0xF9, I586_FIXED, 2, 2, k_i586_np, 0);
    Set586Forms(0xFA, 0xFB, I586_FIXED, 7, 7, k_i586_np, 0);
    Set586Forms(0xFC, 0xFD, I586_FIXED, 2, 2, k_i586_np, 0);
    Set586Forms(0xFE, 0xFF, I586_GROUP, I586_GROUP_5, 0, k_i586_np, 0);

    Set586Forms(0x100, 0x100, I586_GROUP, I586_GROUP_0F00, 0, k_i586_np, 0);
    Set586Forms(0x101, 0x101, I586_GROUP, I586_GROUP_0F01, 0, k_i586_np, 0);
    Set586Forms(0x102, 0x103, I586_FIXED, 8, 8, k_i586_np, 0);
    Set586Forms(0x106, 0x106, I586_FIXED, 10, 10, k_i586_np, 0);
    Set586Forms(0x108, 0x108, I586_FIXED, 15, 15, k_i586_np, 0);
    Set586Forms(0x109, 0x109, I586_FIXED, 2000, 2000, k_i586_np, 0);
    Set586Forms(0x120, 0x120, I586_FIXED, 4, 4, k_i586_np, 0);
    Set586Forms(0x121, 0x121, I586_GROUP, I586_GROUP_DEBUG_READ, 0, k_i586_np, 0);
    Set586Forms(0x122, 0x122, I586_GROUP, I586_GROUP_CONTROL, 0, k_i586_np, 0);
    Set586Forms(0x123, 0x123, I586_GROUP, I586_GROUP_DEBUG_WRITE, 0, k_i586_np, 0);
    Set586Forms(0x130, 0x130, I586_FIXED, 30, 30, k_i586_np, 0);
    Set586Forms(0x132, 0x132, I586_FIXED, 20, 20, k_i586_np, 0);
    Set586Forms(0x180, 0x18F, I586_JUMP, 1, 1, k_i586_pv, 0);
    Set586Forms(0x190, 0x19F, I586_FIXED, 1, 2, k_i586_np, 0);
    Set586Forms(0x1A0, 0x1A0, I586_FIXED, 1, 1, k_i586_np, 0);
    Set586Forms(0x1A8, 0x1A8, I586_FIXED, 1, 1, k_i586_np, 0);
    Set586Forms(0x1A1, 0x1A1, I586_MODE, 3, 0, k_i586_np, 0);
    Set586Forms(0x1A9, 0x1A9, I586_MODE, 3, 0, k_i586_np, 0);
    Set586Forms(0x1A2, 0x1A2, I586_FIXED, 12, 12, k_i586_np, 0);
    Set586Forms(0x1A3, 0x1A3, I586_FIXED, 4, 9, k_i586_np, 0);
    Set586Forms(0x1AB, 0x1AB, I586_FIXED, 7, 13, k_i586_np, 0);
    Set586Forms(0x1B3, 0x1B3, I586_FIXED, 7, 13, k_i586_np, 0);
    Set586Forms(0x1BB, 0x1BB, I586_FIXED, 7, 13, k_i586_np, 0);
    Set586Forms(0x1A4, 0x1A4, I586_FIXED, 4, 4, k_i586_np, 0);
    Set586Forms(0x1AC, 0x1AC, I586_FIXED, 4, 4, k_i586_np, 0);
    Set586Forms(0x1A5, 0x1A5, I586_FIXED, 4, 5, k_i586_np, 0);
    Set586Forms(0x1AD, 0x1AD, I586_FIXED, 4, 5, k_i586_np, 0);
    Set586Forms(0x1AF, 0x1AF, I586_FIXED, 10, 10, k_i586_np, 0);
    Set586Forms(0x1B0, 0x1B1, I586_FIXED, 5, 6, k_i586_np, 0);
    Set586Forms(0x1B2, 0x1B2, I586_MODE, 4, 0, k_i586_np, 0);
    Set586Forms(0x1B4, 0x1B5, I586_MODE, 4, 0, k_i586_np, 0);
    Set586Forms(0x1B6, 0x1B7, I586_FIXED, 3, 3, k_i586_np, 0);
    Set586Forms(0x1BA, 0x1BA, I586_GROUP, I586_GROUP_0FBA, 0, k_i586_np, 0);
    Set586Forms(0x1BC, 0x1BD, I586_SCAN, 0, 0, k_i586_np, 0);
    Set586Forms(0x1BE, 0x1BF, I586_FIXED, 3, 3, k_i586_np, 0);
    Set586Forms(0x1C0, 0x1C1, I586_FIXED, 3, 4, k_i586_np, 0);
    Set586Forms(0x1C7, 0x1C7, I586_FIXED, 10, 10, k_i586_np, 0);
    Set586Forms(0x1C8, 0x1CF, I586_FIXED, 1, 1, k_i586_np, 0);

    Set586Group(I586_GROUP_1, 0, 7, I586_FIXED, 1, 3, k_i586_uv, k_i586_rmw);
    Set586Group(I586_GROUP_1, 2, 3, I586_FIXED, 1, 3, k_i586_pu, k_i586_rmw);
    Set586Group(I586_GROUP_1, 7, 7, I586_FIXED, 1, 2, k_i586_uv, 0);
    Set586Group(I586_GROUP_POP, 0, 0, I586_FIXED, 1, 3, k_i586_uv, k_i586_pop | k_i586_memory_np);
    Set586Group(I586_GROUP_MOVE, 0, 0, I586_FIXED, 1, 1, k_i586_uv, 0);
    Set586Group(I586_GROUP_SHIFT_IMMEDIATE, 0, 7, I586_FIXED, 1, 3, k_i586_pu, k_i586_rmw);
    Set586Group(I586_GROUP_SHIFT_IMMEDIATE, 2, 3, I586_ROTATE, 8, 10, k_i586_pu, k_i586_rmw);
    Set586Group(I586_GROUP_SHIFT_1, 0, 7, I586_FIXED, 1, 3, k_i586_pu, k_i586_rmw);
    Set586Group(I586_GROUP_SHIFT_CL, 0, 7, I586_FIXED, 4, 4, k_i586_np, 0);
    Set586Group(I586_GROUP_SHIFT_CL, 2, 3, I586_ROTATE, 7, 9, k_i586_np, 0);
    Set586Group(I586_GROUP_3, 0, 1, I586_FIXED, 1, 2, k_i586_np, 0);
    Set586Group(I586_GROUP_3, 2, 3, I586_FIXED, 1, 3, k_i586_np, 0);
    Set586Group(I586_GROUP_3, 4, 7, I586_WIDTH, 0, 0, k_i586_np, 0);
    Set586Group(I586_GROUP_5, 0, 1, I586_FIXED, 1, 3, k_i586_uv, k_i586_rmw);
    Set586Group(I586_GROUP_5, 2, 2, I586_FIXED, 2, 2, k_i586_np, 0);
    Set586Group(I586_GROUP_5, 3, 3, I586_MODE, 5, 0, k_i586_np, 0);
    Set586Group(I586_GROUP_5, 4, 4, I586_FIXED, 2, 2, k_i586_np, 0);
    Set586Group(I586_GROUP_5, 5, 5, I586_MODE, 4, 0, k_i586_np, 0);
    Set586Group(I586_GROUP_5, 6, 6, I586_FIXED, 1, 2, k_i586_uv, k_i586_push | k_i586_memory_np);
    Set586Group(I586_GROUP_0F00, 0, 1, I586_FIXED, 2, 2, k_i586_np, 0);
    Set586Group(I586_GROUP_0F00, 2, 2, I586_FIXED, 9, 9, k_i586_np, 0);
    Set586Group(I586_GROUP_0F00, 3, 3, I586_FIXED, 10, 10, k_i586_np, 0);
    Set586Group(I586_GROUP_0F00, 4, 5, I586_FIXED, 7, 7, k_i586_np, 0);
    Set586Group(I586_GROUP_0F01, 0, 1, I586_FIXED, 4, 4, k_i586_np, 0);
    Set586Group(I586_GROUP_0F01, 2, 3, I586_FIXED, 6, 6, k_i586_np, 0);
    Set586Group(I586_GROUP_0F01, 4, 4, I586_FIXED, 4, 4, k_i586_np, 0);
    Set586Group(I586_GROUP_0F01, 6, 6, I586_FIXED, 8, 8, k_i586_np, 0);
    Set586Group(I586_GROUP_0F01, 7, 7, I586_FIXED, 29, 29, k_i586_np, 0);
    Set586Group(I586_GROUP_0FBA, 4, 4, I586_FIXED, 4, 4, k_i586_np, 0);
    Set586Group(I586_GROUP_0FBA, 5, 7, I586_FIXED, 7, 8, k_i586_np, 0);
    Set586Group(I586_GROUP_CONTROL, 0, 0, I586_FIXED, 22, 22, k_i586_np, 0);
    Set586Group(I586_GROUP_CONTROL, 2, 2, I586_FIXED, 12, 12, k_i586_np, 0);
    Set586Group(I586_GROUP_CONTROL, 3, 3, I586_FIXED, 21, 21, k_i586_np, 0);
    Set586Group(I586_GROUP_CONTROL, 4, 4, I586_FIXED, 14, 14, k_i586_np, 0);
    Set586Group(I586_GROUP_DEBUG_READ, 0, 3, I586_FIXED, 2, 2, k_i586_np, 0);
    Set586Group(I586_GROUP_DEBUG_READ, 4, 5, I586_FIXED, 12, 12, k_i586_np, 0);
    Set586Group(I586_GROUP_DEBUG_READ, 6, 7, I586_FIXED, 11, 11, k_i586_np, 0);
    Set586Group(I586_GROUP_DEBUG_WRITE, 0, 3, I586_FIXED, 11, 11, k_i586_np, 0);
    Set586Group(I586_GROUP_DEBUG_WRITE, 4, 5, I586_FIXED, 12, 12, k_i586_np, 0);
    Set586Group(I586_GROUP_DEBUG_WRITE, 6, 7, I586_FIXED, 11, 11, k_i586_np, 0);
}

static u32 Get586StringStart(u8 opcode, u8 repeat, u32 count)
{
    switch (opcode & 0xFE)
    {
        case 0xA4:
            return repeat == 0 ? 4 : count == 0 ? 6 : count == 1 ? 13 : 14;
        case 0xA6:
            return repeat == 0 ? 5 : count == 0 ? 7 : repeat == 0xF2 ? 13 : 12;
        case 0xAA:
            return repeat == 0 ? 3 : count == 0 ? 6 : 10;
        case 0xAC:
            return repeat == 0 ? 2 : count == 0 ? 7 : 10;
        default:
            return repeat == 0 ? 4 : count == 0 ? 7 : 12;
    }
}

static u32 Get586StringElement(u8 opcode)
{
    switch (opcode & 0xFE)
    {
        case 0xA4:
        case 0xAA:
            return 1;
        case 0xAC:
            return 3;
        default:
            return 4;
    }
}

u32 I386::Get586Clocks(const InstructionContext& c, bool taken) const
{
    const I586_Form& form = Get586Form(c.two_byte, c.two_byte ? c.opcode2 : c.opcode, c.reg);
    bool protected_mode = m_state.execution_mode == I386_MODE_PROTECTED;

    switch (form.kind)
    {
        case I586_FIXED:
        case I586_JUMP:
            return c.memory_operand ? form.memory : form.reg;
        case I586_LOOP:
            return taken ? form.reg : form.memory;
        case I586_MODE:
            return !protected_mode ? form.reg : form.memory != 0 ? form.memory : k_i486_keep;
        case I586_SEGMENT:
            return protected_mode ? k_i486_keep : c.memory_operand ? form.memory : form.reg;
        case I586_STRING:
        {
            u32 count = c.address_size == 2 ? m_486_registers[I386_REG_ECX] & 0xFFFF : m_486_registers[I386_REG_ECX];
            return Get586StringStart(c.opcode, c.repeat, count);
        }
        case I586_WIDTH:
        {
            u32 width = c.opcode == 0xF6 ? 1 : c.operand_size;

            if (c.reg == 6)
                return width == 1 ? 17 : width == 2 ? 25 : 41;

            if (c.reg == 7)
                return width == 1 ? 22 : width == 2 ? 30 : 46;

            return width == 4 ? 10 : 11;
        }
        case I586_ROTATE:
        {
            u32 width = c.opcode == 0xC0 || c.opcode == 0xD2 ? 8 : (u32)c.operand_size * 8;
            u32 count = (c.opcode <= 0xC1 ? c.immediate : m_486_registers[I386_REG_ECX]) & 0x1F;
            u32 base = c.memory_operand ? form.memory : form.reg;
            return count <= width ? base : (count / width) * 7 + base + 1;
        }
        case I586_SCAN:
        {
            u32 width = (u32)c.operand_size * 8;
            u32 source = Peek486Operand(c, (int)width);
            u32 extra = c.memory_operand ? 1 : 0;

            if (source == 0)
                return 6 + extra;

            if (c.opcode2 == 0xBC)
            {
                u32 position = 0;

                while ((source & (1U << position)) == 0)
                    position++;

                return 8 + extra + 4 * (position / 8 + 1) + 3 * ((position % 8) / 4 + 1) + 3 * (position % 4 + 1);
            }

            u32 position = width - 1;

            while ((source & (1U << position)) == 0)
                position--;

            return 7 + extra + 2 * (width - position);
        }
        case I586_ENTER:
        {
            u32 level = c.immediate2 & 0x1F;
            return level == 0 ? 11 : level == 1 ? 15 : 15 + 2 * level;
        }
        default:
            return k_i486_keep;
    }
}

u32 I386::Get586EscapeClocks(const InstructionContext& c, u32& busy) const
{
    u8 opcode = c.opcode;
    u8 reg = (c.modrm >> 3) & 7;
    bool registers = c.modrm >= 0xC0;
    u32 precision = IsValidPointer(m_x87) ? (m_x87->GetState()->control >> 8) & 3 : 3;
    u32 divide = precision == 0 ? 19 : precision == 2 ? 33 : 39;
    bool real = m_state.execution_mode != I386_MODE_PROTECTED;
    busy = 0;

    if (opcode == 0xD8 || opcode == 0xDC || (opcode == 0xDE && registers))
    {
        if (opcode == 0xDE && reg == 3)
            return 1;

        if (reg >= 6)
        {
            busy = divide;
            return 1;
        }

        return 1;
    }

    if (opcode == 0xDA || opcode == 0xDE)
    {
        if (registers)
            return 1;

        if (reg >= 6)
        {
            busy = divide + 3;
            return 1;
        }

        return 4;
    }

    if (opcode == 0xD9)
    {
        if (!registers)
        {
            static const u8 k_memory[8] = { 1, 1, 2, 2, 37, 7, 50, 2 };
            return reg == 4 && !real ? 33 : k_memory[reg];
        }

        static const u8 k_specials[32] =
        {
            1, 1, 1, 1, 1, 21, 1, 1, 2, 3, 3, 3, 3, 3, 2, 1,
            57, 109, 145, 118, 13, 45, 1, 1, 40, 105, 1, 110, 15, 26, 93, 93
        };

        if (reg == 0 || reg == 1 || reg == 2)
            return 1;

        if (c.modrm == 0xFA)
        {
            busy = 70;
            return 1;
        }

        return c.modrm >= 0xE0 ? k_specials[c.modrm - 0xE0] : 1;
    }

    if (opcode == 0xDB)
    {
        if (registers)
            return c.modrm == 0xE2 ? 9 : c.modrm == 0xE3 ? 12 : 1;

        static const u8 k_memory[8] = { 1, 1, 6, 6, 1, 3, 1, 3 };
        return k_memory[reg];
    }

    if (opcode == 0xDD)
    {
        if (registers)
            return 1;

        if (reg == 4)
            return !real ? 70 : c.operand_size == 4 ? 95 : 75;

        if (reg == 6)
            return !real ? 124 : c.operand_size == 4 ? 151 : 127;

        static const u8 k_memory[8] = { 1, 1, 2, 2, 0, 1, 0, 2 };
        return k_memory[reg];
    }

    if (registers)
        return c.modrm == 0xE0 ? 2 : 1;

    static const u8 k_memory[8] = { 1, 1, 6, 6, 53, 1, 151, 6 };
    return k_memory[reg];
}

u32 I386::Get586Pairing(const InstructionContext& c, u32 prefixes)
{
    u32 opcode = c.two_byte ? c.opcode2 : c.opcode;
    const I586_Form& form = Get586Form(c.two_byte, (u8)opcode, c.reg);
    u32 pairing = (u32)form.pairing << 24;
    u32 flags = form.flags & (k_i586_push | k_i586_pop);

    if (c.memory_operand && (form.flags & k_i586_memory_np) != 0)
        pairing = k_i586_np << 24;

    if (c.memory_operand && (form.flags & k_i586_rmw) != 0)
        flags |= k_i586_rmw;

    if (form.kind == I586_ESCAPE)
    {
        u8 reg = (c.modrm >> 3) & 7;
        bool registers = c.modrm >= 0xC0;
        bool arithmetic = (c.opcode == 0xD8 || c.opcode == 0xDC) || (c.opcode == 0xDE && registers && reg != 3 && reg != 2);
        bool load = (c.opcode == 0xD9 && reg == 0) || (c.opcode == 0xDD && !registers && reg == 0);
        bool special = c.opcode == 0xD9 && (c.modrm == 0xE0 || c.modrm == 0xE1 || c.modrm == 0xE4);
        bool compare = (c.opcode == 0xDD && registers && (reg == 4 || reg == 5)) || (c.opcode == 0xDA && c.modrm == 0xE9);

        if (arithmetic || load || special || compare)
            flags |= k_i586_fx;

        if (reg == 1 && (c.opcode == 0xD8 || c.opcode == 0xDC || (c.opcode == 0xDE && registers)))
            flags |= k_i586_fmul;

        if (c.opcode == 0xD9 && registers && reg == 1)
            flags |= k_i586_fxch;
    }

    if (prefixes != 0)
        flags |= k_i586_single;

    if (c.memory_operand && !c.two_byte && (c.opcode == 0x80 || c.opcode == 0x81 || c.opcode == 0x83 ||
        c.opcode == 0xC6 || c.opcode == 0xC7))
    {
        u8 mod = c.modrm >> 6;
        u8 rm = c.modrm & 7;
        bool displacement = mod == 1 || mod == 2 || (mod == 0 && (c.address_size == 2 ? rm == 6 :
            rm == 5 || (rm == 4 && (c.sib & 7) == 5)));

        if (displacement)
            flags |= k_i586_single;
    }

    u32 length = c.next_eip - c.start_eip;

    if (length > 7 + prefixes + (c.repeat != 0 ? 1 : 0))
        flags |= k_i586_alone;

    return pairing | flags;
}

u32 I386::Get586Address(const InstructionContext& c)
{
    int base = Get486Base(c);
    u32 address = base >= 0 ? 1U << base : 0;

    if (!c.memory_operand)
        return address;

    u8 rm = c.modrm & 7;

    if (c.address_size == 2)
    {
        if (rm <= 3)
            address |= 1U << ((rm & 1) != 0 ? I386_REG_EDI : I386_REG_ESI);
    }
    else if (rm == 4 && ((c.sib >> 3) & 7) != 4)
        address |= 1U << ((c.sib >> 3) & 7);

    return address;
}

u32 I386::Get586Named(const InstructionContext& c)
{
    u32 named = Get586Address(c);

    if (c.two_byte)
        return named;

    u8 opcode = c.opcode;
    bool byte = (opcode < 0x40 && (opcode & 1) == 0) || opcode == 0x80 || opcode == 0x82 || opcode == 0x84 ||
        opcode == 0x86 || opcode == 0x88 || opcode == 0x8A || opcode == 0xC0 || opcode == 0xC6 || opcode == 0xD0 ||
        opcode == 0xD2 || opcode == 0xF6 || opcode == 0xFE;
    u32 mask = byte ? 3 : 7;
    bool rm = !c.memory_operand;

    if (opcode < 0x40 && (opcode & 7) < 4)
        return named | (1U << (c.reg & mask)) | (rm ? 1U << (c.rm & mask) : 0);

    if (opcode < 0x40 && (opcode & 7) < 6)
        return named | (1U << I386_REG_EAX);

    if (opcode >= 0x40 && opcode <= 0x5F)
        return named | (1U << (opcode & 7));

    if ((opcode >= 0x84 && opcode <= 0x8B) || opcode == 0x8D)
        return named | (1U << (c.reg & mask)) | (rm && opcode != 0x8D ? 1U << (c.rm & mask) : 0);

    if ((opcode >= 0x80 && opcode <= 0x83) || opcode == 0x8F || opcode == 0xC0 || opcode == 0xC1 ||
        (opcode >= 0xC6 && opcode <= 0xC7) || (opcode >= 0xD0 && opcode <= 0xD3) || opcode >= 0xF6)
        return named | (rm ? 1U << (c.rm & mask) : 0);

    if (opcode >= 0x90 && opcode <= 0x97)
        return named | (1U << I386_REG_EAX) | (1U << (opcode & 7));

    if ((opcode >= 0xA0 && opcode <= 0xA3) || opcode == 0xA8 || opcode == 0xA9)
        return named | (1U << I386_REG_EAX);

    if (opcode >= 0xB0 && opcode <= 0xBF)
        return named | (1U << (opcode & (opcode <= 0xB7 ? 3 : 7)));

    return named;
}

u32 I386::Get586Accesses(const InstructionContext& c)
{
    if (!c.two_byte && c.opcode == 0x8D)
        return 0;

    switch (GetBusPattern(c) & k_i386_bus_pattern_mask)
    {
        case I386_BUS_READ:
        case I386_BUS_WRITE:
            return c.memory_operand ? 1 : 0;
        case I386_BUS_RMW:
            return c.memory_operand ? 2 : 0;
        case I386_BUS_PUSH_READ:
        case I386_BUS_POP_WRITE:
            return c.memory_operand ? 2 : 1;
        case I386_BUS_PUSH:
        case I386_BUS_POP:
        case I386_BUS_MOFFS_READ:
        case I386_BUS_MOFFS_WRITE:
        case I386_BUS_CALL:
        case I386_BUS_RET:
        case I386_BUS_JUMP_RM:
            return 1;
        case I386_BUS_CALL_RM:
        case I386_BUS_FAR:
            return 2;
        case I386_BUS_STRING:
            return (c.opcode & 0xFC) == 0xA4 ? 2 : 1;
        case I386_BUS_PUSHA:
        case I386_BUS_POPA:
            return 8;
        default:
            return c.memory_operand ? 1 : 0;
    }
}

u32 I386::Count586Prefixes(const InstructionContext& c, u32 prefixes)
{
    return prefixes + (c.two_byte && (c.opcode2 & 0xF0) != 0x80 ? 1 : 0);
}

bool I386::Is586Backward(const InstructionContext& c)
{
    if (!c.two_byte)
        return (s8)c.immediate < 0;

    return c.operand_size == 2 ? (s16)c.immediate < 0 : (s32)c.immediate < 0;
}

u32 I386::Pair586(u32 state, u32 pairing, u32 written, u32 clocks, u32 named, u32& charged)
{
    u32 kind = pairing >> 24;
    u32 fmul = (pairing & k_i586_fmul) != 0 && (state & k_i586_fmul) != 0 ? 1 : 0;
    bool open = (state & k_i586_open) != 0;

    if (open && (state & k_i586_fx) != 0 && (pairing & k_i586_fxch) != 0)
    {
        charged = 0;
        return (state & k_i586_fmul) | k_i586_paired;
    }

    if (open && (state & k_i586_fx) == 0 && (kind == k_i586_uv || kind == k_i586_pv) &&
        (pairing & (k_i586_single | k_i586_alone)) == 0)
    {
        u32 dependent = ((state >> 8) & 0xFF) & named;

        if (((state & pairing) & (k_i586_push | k_i586_pop)) != 0)
            dependent &= ~(1U << I386_REG_ESP);

        if (dependent == 0)
        {
            u32 first = (state >> 16) & 0xFF;
            charged = (clocks > first ? clocks - first : 0) + ((state & pairing & k_i586_rmw) != 0 ? 2 : 0);
            return k_i586_paired;
        }
    }

    charged = clocks + fmul;

    if ((pairing & k_i586_alone) != 0)
        return (pairing & k_i586_fmul);

    if ((pairing & k_i586_fx) != 0)
        return k_i586_open | k_i586_fx | (pairing & k_i586_fmul);

    if (kind != k_i586_uv && kind != k_i586_pu)
        return 0;

    return k_i586_open | (pairing & (k_i586_push | k_i586_pop | k_i586_rmw)) | ((written & 0xFF) << 8) |
        (MIN(clocks, 255U) << 16);
}

u32 I386::Wait586FPU(u32 start, u32 busy)
{
    u64 now = GetTimeStamp() - m_bus_context->synchronized_cycles + start;
    u32 stall = m_586_fpu_free > now ? (u32)MIN(m_586_fpu_free - now, (u64)0xFFFF) : 0;

    if (busy != 0)
        m_586_fpu_free = now + stall + busy;

    return stall;
}

bool I386::Is586FPUFree(const InstructionContext& c)
{
    u8 modrm = c.modrm;

    if (c.memory_operand)
        return c.opcode == 0xD9 && ((modrm >> 3) & 7) == 5;

    if (c.opcode == 0xD9)
        return modrm < 0xD0 || modrm == 0xE0 || modrm == 0xE1 || modrm == 0xE8 || modrm == 0xEE;

    if (c.opcode == 0xDD)
        return modrm >= 0xD0 && modrm < 0xE0;

    return c.opcode == 0xDF && modrm == 0xE0;
}

u32 I386::Fold586Memory(u32 weight)
{
    m_586_fraction += weight;
    u32 whole = m_586_fraction >> 8;
    m_586_fraction &= 0xFF;
    return whole;
}

void I386::UpdateBus586(const BusTiming& bus)
{
    memset(m_586_device_wait, 0, sizeof(m_586_device_wait));
    m_586_memory_wait = 0;

    if (!m_i586_timing)
        return;

    m_586_memory_wait = (bus.cache[0] + 0x80) >> 8;

    for (int c = 0; c < GT_BUS_CLASS_COUNT; c++)
    {
        if (c == GT_BUS_DRAM || c == GT_BUS_ROM)
            continue;

        for (int k = 0; k < 2; k++)
            m_586_device_wait[c][k] = (bus.cycle[c][k] + bus.slow[c][k] + 0x8000) >> 16;
    }
}

void I386::Apply586Clocks()
{
    const InstructionContext& c = m_instruction;
    u32 pending = m_586_wait;
    m_586_wait = 0;

    const I586_Form& form = Get586Form(c.two_byte, c.two_byte ? c.opcode2 : c.opcode, c.reg);
    bool port = form.kind == I586_PORT;

    if (m_486_continuation)
    {
        u32 element = port ? (u32)m_step.clocks : Get586StringElement(c.opcode);
        m_586_pair = 0;
        m_step.clocks = element + Fold586Memory(Get586Accesses(c) * m_586_memory_wait) + pending;
        return;
    }

    bool escape = form.kind == I586_ESCAPE;
    bool jump = form.kind == I586_JUMP || form.kind == I586_LOOP;
    bool taken = jump && Is486Taken(c);
    u32 prefixes = port ? 0 : Count586Prefixes(c, m_486_prefixes);
    u32 busy = 0;
    u32 clocks = 0;

    if (port)
        clocks = (u32)m_step.clocks;
    else if (escape)
        clocks = Get586EscapeClocks(c, busy);
    else
    {
        clocks = Get586Clocks(c, taken);

        if (clocks == k_i486_keep)
        {
            clocks = Get486Clocks(c, Is486Taken(c));

            if (clocks == k_i486_keep)
                clocks = (u32)m_step.clocks;
            else if (clocks == k_i486_data)
                clocks = Get486DataClocks(c);
        }
    }

    u32 pairing = Get586Pairing(c, m_486_prefixes);
    u32 written = Get486Written(c);
    u32 implicit = (pairing & (k_i586_push | k_i586_pop)) != 0 ? 1U << I386_REG_ESP : 0;
    u32 charged = 0;
    u32 state = Pair586(m_586_pair, pairing, written | implicit, clocks, Get586Named(c), charged);
    bool paired = (state & k_i586_paired) != 0;
    u32 stalls = prefixes + ((m_486_written & Get586Address(c)) != 0 ? 1 : 0);

    if (jump && taken != Is586Backward(c))
        stalls += paired && form.kind == I586_JUMP ? 4 : 3;

    if (escape && !Is586FPUFree(c))
        stalls += Wait586FPU(m_io_start_cycles, busy);

    stalls += Fold586Memory(Get586Accesses(c) * m_586_memory_wait);

    m_486_written = m_step.exception ? 0 : (u8)(paired ? m_486_written | written : written);
    m_586_pair = m_step.exception ? 0 : state & ~k_i586_paired;
    m_step.clocks = charged + stalls + m_486_extra + pending;
}
