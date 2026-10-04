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

#ifndef I386_OPCODES_INLINE_H
#define I386_OPCODES_INLINE_H

#include "i386.h"

INLINE u32 I386::GetSZP(u32 value, int width) const
{
    value &= GetMask(width);

    if (width == 8)
        return k_szp_flags[value];

    return (k_szp_flags[value & 0xFF] & I386_FLAG_PF) | ((u32)(value == 0) << 6) | (((value >> (width - 1)) & 1) << 7);
}

INLINE u32 I386::Add(u32 left, u32 right, u32 carry, int width)
{
    u32 mask = GetMask(width);
    left &= mask;
    right &= mask;

    u64 sum = (u64)left + right + carry;
    u32 value = (u32)sum & mask;

    u32 flags = (u32)(sum >> width) & I386_FLAG_CF;
    flags |= (left ^ right ^ value) & I386_FLAG_AF;
    flags |= (((~(left ^ right) & (left ^ value)) >> (width - 1)) & 1) << 11;

    m_eflags = (m_eflags & ~(I386_FLAG_CF | I386_FLAG_PF | I386_FLAG_AF | I386_FLAG_ZF | I386_FLAG_SF | I386_FLAG_OF)) |
        flags | GetSZP(value, width);
    return value;
}

INLINE u32 I386::Sub(u32 left, u32 right, u32 borrow, int width)
{
    u32 mask = GetMask(width);
    left &= mask;
    right &= mask;

    u64 subtrahend = (u64)right + borrow;
    u32 value = (left - right - borrow) & mask;

    u32 flags = (u64)left < subtrahend ? I386_FLAG_CF : 0;
    flags |= (left ^ right ^ value) & I386_FLAG_AF;
    flags |= ((((left ^ right) & (left ^ value)) >> (width - 1)) & 1) << 11;

    m_eflags = (m_eflags & ~(I386_FLAG_CF | I386_FLAG_PF | I386_FLAG_AF | I386_FLAG_ZF | I386_FLAG_SF | I386_FLAG_OF)) |
        flags | GetSZP(value, width);
    return value;
}

INLINE u32 I386::Logic(u32 value, int width)
{
    value &= GetMask(width);
    m_eflags = (m_eflags & ~(I386_FLAG_CF | I386_FLAG_PF | I386_FLAG_ZF | I386_FLAG_SF | I386_FLAG_OF)) |
        GetSZP(value, width);
    return value;
}

INLINE void I386::SetSZP(u32 value, int width)
{
    m_eflags = (m_eflags & ~(I386_FLAG_PF | I386_FLAG_ZF | I386_FLAG_SF)) | GetSZP(value, width);
}

INLINE u32 I386::ALU(int operation, u32 left, u32 right, int width)
{
    u32 carry = (m_eflags & I386_FLAG_CF) != 0 ? 1 : 0;

    switch (operation)
    {
        case 0:
            // ADD
            return Add(left, right, 0, width);
        case 1:
            // OR
            return Logic(left | right, width);
        case 2:
            // ADC
            return Add(left, right, carry, width);
        case 3:
            // SBB
            return Sub(left, right, carry, width);
        case 4:
            // AND
            return Logic(left & right, width);
        case 5:
            // SUB
            return Sub(left, right, 0, width);
        case 6:
            // XOR
            return Logic(left ^ right, width);
        default:
            // CMP
            return Sub(left, right, 0, width);
    }
}

template<int operation, int form, int width>
INLINE bool I386::OPCodes_ALU()
{
    static const u8 k_alu_memory_clocks[8][4] =
    {
        { 7, 7, 6, 6 }, { 6, 6, 7, 7 }, { 7, 7, 6, 6 }, { 6, 6, 7, 7 },
        { 6, 6, 7, 7 }, { 6, 6, 7, 7 }, { 6, 6, 7, 7 }, { 5, 5, 6, 6 }
    };

    if (!DecodeOperands(form <= 3, form >= 4 ? width / 8 : 0))
        return false;

    if (!StartExecution(form <= 3 && m_instruction.memory_operand ? k_alu_memory_clocks[operation][form & 3] : 2))
        return false;

    if (operation != 7 && form <= 1 && m_instruction.memory_operand)
    {
        u8* data = GetRMWHost(m_instruction.segment, m_instruction.effective_offset, width);

        if (IsValidPointer(data))
        {
            u32 right = GetRegister(m_instruction.reg, width);
            StoreHost(data, ALU(operation, LoadHost(data, width), right, width), width);
            CommitEIP(m_instruction);
            return true;
        }
    }

    u32 left = 0;
    u32 right = 0;
    bool destination_rm = form <= 1;

    if (form <= 3)
    {
        if (!ReadRM(m_instruction, width, *m_bus_context, destination_rm ? left : right))
            return false;

        if (destination_rm)
            right = GetRegister(m_instruction.reg, width);
        else
            left = GetRegister(m_instruction.reg, width);
    }
    else
    {
        left = GetRegister(I386_REG_EAX, width);
        right = m_instruction.immediate;
    }

    u32 value = ALU(operation, left, right, width);

    if (operation != 7)
    {
        if (form <= 3)
        {
            if (destination_rm)
            {
                if (!WriteRM(m_instruction, width, value, *m_bus_context))
                    return false;
            }
            else
                SetRegister(m_instruction.reg, width, value);
        }
        else
            SetRegister(I386_REG_EAX, width, value);
    }

    CommitEIP(m_instruction);
    return true;
}

template<int width, bool sign_extend>
INLINE bool I386::OPCodes_ALU_Immediate()
{
    if (!DecodeOperands(true, sign_extend ? 1 : width / 8))
        return false;

    if (!StartExecution(m_instruction.memory_operand ? (m_instruction.reg == 7 ? 5 : 7) : 2))
        return false;

    u32 immediate = m_instruction.immediate;

    if (sign_extend)
        immediate = (u32)SignExtend(immediate, 8);

    if (m_instruction.reg != 7 && m_instruction.memory_operand)
    {
        u8* data = GetRMWHost(m_instruction.segment, m_instruction.effective_offset, width);

        if (IsValidPointer(data))
        {
            StoreHost(data, ALU(m_instruction.reg, LoadHost(data, width), immediate, width), width);
            CommitEIP(m_instruction);
            return true;
        }
    }

    u32 left = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, left))
        return false;

    u32 value = ALU(m_instruction.reg, left, immediate, width);

    if (m_instruction.reg != 7)
    {
        if (!WriteRM(m_instruction, width, value, *m_bus_context))
            return false;
    }

    CommitEIP(m_instruction);
    return true;
}

template<int width, bool load>
INLINE bool I386::OPCodes_MOV_RM()
{
    if (!DecodeOperands(true, 0))
        return false;

    if (!StartExecution(m_instruction.memory_operand && load ? 4 : 2))
        return false;

    if (load)
    {
        u32 value = 0;

        if (!ReadRM(m_instruction, width, *m_bus_context, value))
            return false;

        SetRegister(m_instruction.reg, width, value);
    }
    else
    {
        if (!WriteRM(m_instruction, width, GetRegister(m_instruction.reg, width), *m_bus_context))
            return false;
    }

    CommitEIP(m_instruction);
    return true;
}

template<int width>
INLINE bool I386::OPCodes_MOV_Immediate()
{
    if (!DecodeOperands(false, width / 8))
        return false;

    if (!StartExecution(2))
        return false;

    SetRegister(m_instruction.opcode & 7, width, m_instruction.immediate);
    CommitEIP(m_instruction);
    return true;
}

template<int width, bool decrement>
INLINE bool I386::OPCodes_INC_DEC_Register()
{
    if (!DecodeOperands(false, 0))
        return false;

    if (!StartExecution(2))
        return false;

    int reg = m_instruction.opcode & 7;
    u32 old_cf = m_eflags & I386_FLAG_CF;
    u32 operand = GetRegister(reg, width);
    u32 value = decrement ? Sub(operand, 1, 0, width) : Add(operand, 1, 0, width);

    m_eflags = (m_eflags & ~I386_FLAG_CF) | old_cf;
    SetRegister(reg, width, value);
    CommitEIP(m_instruction);
    return true;
}

template<int condition, bool near_jump>
INLINE bool I386::OPCodes_Jcc()
{
    if (!DecodeOperands(false, near_jump ? m_instruction.operand_size : 1))
        return false;

    bool taken = CheckCondition(condition);

    if (!StartExecution(near_jump && taken ? 7 : 3))
        return false;

    if (taken)
    {
        int width = m_instruction.operand_size * 8;
        u32 target = m_instruction.next_eip + SignExtend(m_instruction.immediate, near_jump ? width : 8);

        m_step.clocks = 7;

        if (!BranchTo(target, width))
            return false;

        m_step.clocks += GetNextInstructionComponents();
    }
    else
        CommitEIP(m_instruction);

    return true;
}

template<int width>
INLINE bool I386::OPCodes_PUSH_Register()
{
    if (!DecodeAndStart(false, 0, 2, 2))
        return false;

    if (!StackPush<width>(GetRegister(m_instruction.opcode & 7, width)))
        return false;

    CommitEIP(m_instruction);
    return true;
}

template<int width>
INLINE bool I386::OPCodes_POP_Register()
{
    if (!DecodeAndStart(false, 0, 4, 4))
        return false;

    u32 value = 0;

    if (!StackPop<width>(value))
        return false;

    SetRegister(m_instruction.opcode & 7, width, value);
    CommitEIP(m_instruction);
    return true;
}

template<int width>
INLINE bool I386::OPCodes_PUSH_Immediate()
{
    bool short_immediate = m_instruction.opcode == 0x6A;

    if (!DecodeAndStart(false, short_immediate ? 1 : width / 8, 2, 2))
        return false;

    u32 value = short_immediate ? (u32)(s32)(s8)m_instruction.immediate : m_instruction.immediate;

    if (!StackPush<width>(value))
        return false;

    CommitEIP(m_instruction);
    return true;
}

template<int width>
INLINE bool I386::OPCodes_CALL_Near()
{
    if (!DecodeAndStart(false, width / 8, 7, 7))
        return false;

    u32 target = m_instruction.next_eip + m_instruction.immediate;

    if (width == 16)
        target &= 0xFFFF;

    if (target > m_segments[I386_SEGMENT_CS].limit)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    if (!StackPush<width>(m_instruction.next_eip))
        return false;

    m_eip = target;
    m_step.clocks += GetNextInstructionComponents();
    return true;
}

template<int width>
INLINE bool I386::OPCodes_RET_Near()
{
    bool release_stack = m_instruction.opcode == 0xC2;

    if (!DecodeAndStart(false, release_stack ? 2 : 0, 10, 10))
        return false;

    u32 old_stack = m_stack32 ? m_registers[I386_REG_ESP].value : m_registers[I386_REG_ESP].low;
    u32 value = 0;

    if (!ReadMemory(I386_SEGMENT_SS, old_stack, width, *m_bus_context, value, true))
        return false;

    u32 target = width == 16 ? (u16)value : value;

    if (target > m_segments[I386_SEGMENT_CS].limit)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u32 stack = old_stack + width / 8 + (release_stack ? (u16)m_instruction.immediate : 0);

    if (m_stack32)
        m_registers[I386_REG_ESP].value = stack;
    else
        m_registers[I386_REG_ESP].low = (u16)stack;

    m_eip = target;
    m_step.clocks += GetNextInstructionComponents();
    return true;
}

#endif /* I386_OPCODES_INLINE_H */
