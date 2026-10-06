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

#ifndef I386_INLINE_H
#define I386_INLINE_H

#include "i386.h"

INLINE I386_State* I386::GetState()
{
    return &m_state;
}

INLINE u32 I386::GetRegister(int index, int width) const
{
    if (width == 8)
        return GetRegister8(index);

    if (width == 16)
        return m_state.registers[index].low;

    return m_state.registers[index].value;
}

INLINE void I386::SetRegister(int index, int width, u32 value)
{
    if (width == 8)
        SetRegister8(index, (u8)value);
    else if (width == 16)
        m_state.registers[index].low = (u16)value;
    else
        m_state.registers[index].value = value;
}

INLINE u8 I386::GetRegister8(int index) const
{
    const u8* bytes = (const u8*)&m_state.registers[index & 3];
    return bytes[index >> 2];
}

INLINE void I386::SetRegister8(int index, u8 value)
{
    u8* bytes = (u8*)&m_state.registers[index & 3];
    bytes[index >> 2] = value;
}

INLINE u32 I386::GetMask(int width) const
{
    if (width == 8)
        return 0xFF;

    if (width == 16)
        return 0xFFFF;

    return 0xFFFFFFFFU;
}

INLINE u32 I386::GetSignBit(int width) const
{
    if (width == 8)
        return 0x80;

    if (width == 16)
        return 0x8000;

    return 0x80000000U;
}

INLINE u32 I386::Truncate(u64 value, int width) const
{
    return (u32)value & GetMask(width);
}

INLINE s32 I386::SignExtend(u32 value, int width) const
{
    if (width == 8)
        return (s32)(s8)(u8)value;

    if (width == 16)
        return (s32)(s16)(u16)value;

    return (s32)value;
}

INLINE bool I386::ReadRM(const InstructionContext& instruction, int width, GT_Bus_Access_Context& context, u32& value)
{
    if (!instruction.memory_operand)
    {
        value = GetRegister(instruction.rm, width);
        return true;
    }

    return ReadMemory(instruction.segment, instruction.effective_offset, width, context, value);
}

INLINE bool I386::WriteRM(const InstructionContext& instruction, int width, u32 value, GT_Bus_Access_Context& context)
{
    if (!instruction.memory_operand)
    {
        SetRegister(instruction.rm, width, value);
        return true;
    }

    return WriteMemory(instruction.segment, instruction.effective_offset, width, value, context);
}

INLINE bool I386::LogicalToLinear(int segment, u32 offset, u32 size, bool write, bool stack, u32& linear, bool execute)
{
    if (unlikely(m_state.execution_mode == I386_MODE_PROTECTED))
    {
        if (segment >= 0 && segment < I386_SEGMENT_COUNT && size != 0)
        {
            u8 required = execute ? k_i386_segment_fast_execute :
                write ? k_i386_segment_fast_write : k_i386_segment_fast_read;

            if ((m_segment_access_flags[segment] & required) != 0 && (u64)offset + size - 1 <= 0xFFFFFFFFULL)
            {
                linear = offset;
                return true;
            }
        }

        return LogicalToLinearProtected(segment, offset, size, write, stack, execute, linear);
    }

    UNUSED(write);
    UNUSED(execute);

    if (segment < 0 || segment >= I386_SEGMENT_COUNT || size == 0)
        return RaiseException(stack ? 12 : 13, I386_EXCEPTION_FAULT, true, 0);

    const I386_Segment& state = m_state.segments[segment];
    u64 end = (u64)offset + size - 1;

    if ((state.attributes & I386_SEGMENT_PRESENT) == 0 || end > state.limit)
        return RaiseException(stack || segment == I386_SEGMENT_SS ? 12 : 13, I386_EXCEPTION_FAULT, true, 0);

    linear = state.base + offset;
    return true;
}

INLINE bool I386::TranslateLinear(u32 linear, bool write, GT_Bus_Access_Context& context, u32& physical, bool supervisor)
{
    if (likely((m_state.cr0 & 0x80000000U) == 0))
    {
        physical = linear;
        return true;
    }

    return TranslateLinearPaged(linear, write, context, physical, supervisor);
}

INLINE u32 I386::GetStackPointer() const
{
    return GetStackAddressSize() == 32 ? m_state.registers[I386_REG_ESP].value : m_state.registers[I386_REG_ESP].low;
}

INLINE void I386::SetStackPointer(u32 value)
{
    if (GetStackAddressSize() == 32)
        m_state.registers[I386_REG_ESP].value = value;
    else
        m_state.registers[I386_REG_ESP].low = (u16)value;
}

INLINE int I386::GetStackAddressSize() const
{
    return (m_state.segments[I386_SEGMENT_SS].attributes & I386_SEGMENT_DEFAULT_32) != 0 ? 32 : 16;
}

INLINE bool I386::BranchTo(u32 target, int width)
{
    u32 checked_target = width == 16 ? (u16)target : target;

    if (checked_target > m_state.segments[I386_SEGMENT_CS].limit)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    m_state.eip = checked_target;
    return true;
}

INLINE bool I386::CheckCondition(int condition) const
{
    bool cf = (m_state.eflags & I386_FLAG_CF) != 0;
    bool pf = (m_state.eflags & I386_FLAG_PF) != 0;
    bool zf = (m_state.eflags & I386_FLAG_ZF) != 0;
    bool sf = (m_state.eflags & I386_FLAG_SF) != 0;
    bool of = (m_state.eflags & I386_FLAG_OF) != 0;

    switch (condition & 15)
    {
        case 0:
            // O
            return of;
        case 1:
            // NO
            return !of;
        case 2:
            // B, NAE, C
            return cf;
        case 3:
            // NB, AE, NC
            return !cf;
        case 4:
            // Z, E
            return zf;
        case 5:
            // NZ, NE
            return !zf;
        case 6:
            // BE, NA
            return cf || zf;
        case 7:
            // NBE, A
            return !cf && !zf;
        case 8:
            // S
            return sf;
        case 9:
            // NS
            return !sf;
        case 10:
            // P, PE
            return pf;
        case 11:
            // NP, PO
            return !pf;
        case 12:
            // L, NGE
            return sf != of;
        case 13:
            // NL, GE
            return sf == of;
        case 14:
            // LE, NG
            return zf || sf != of;
        default:
            // NLE, G
            return !zf && sf == of;
    }
}

INLINE void I386::CommitEIP(const InstructionContext& instruction)
{
    m_state.eip = instruction.next_eip;
}

INLINE int I386::GetEncodedImmediateSize(u8 encoding, u8 opcode, u8 reg, int operand_size, int address_size)
{
    switch (encoding & k_i386_immediate_mask)
    {
        case k_i386_immediate_byte:
            return 1;
        case k_i386_immediate_word:
            return 2;
        case k_i386_immediate_enter:
            return 3;
        case k_i386_immediate_operand:
            return operand_size;
        case k_i386_immediate_far:
            return operand_size + 2;
        case k_i386_immediate_address:
            return address_size;
        case k_i386_immediate_group3: // Only TEST (/0 and /1) has an immediate
            return reg <= 1 ? ((opcode & 1) != 0 ? operand_size : 1) : 0;
        default:
            return 0;
    }
}

INLINE int I386::GetImmediateSize(bool two_byte, u8 opcode, u8 reg, int operand_size, int address_size) const
{
    return GetEncodedImmediateSize(k_opcode_encoding[two_byte ? 256 + opcode : opcode], opcode, reg, operand_size,
        address_size);
}

INLINE bool I386::TryTranslateLinear(u32 linear, u32& physical) const
{
    if (likely((m_state.cr0 & 0x80000000U) == 0))
    {
        physical = linear;
        return true;
    }

    return TranslatePagedPassive(linear, physical);
}

INLINE bool I386::IsDebuggerHitPending() const
{
    return m_debugger_hit_pending;
}

#endif /* I386_INLINE_H */
