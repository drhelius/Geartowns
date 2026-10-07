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

#include <limits.h>
#include "i386.h"
#include "i386_opcodes_inline.h"

u32 I386::GetMultiplyClocks(u32 multiplier, int width, bool signed_multiplier, bool memory_operand) const
{
    u32 value = multiplier & GetMask(width);
    bool negative = signed_multiplier && (value & GetSignBit(width)) != 0;
    u32 significant = negative ? ~value & GetMask(width) : value;
    u32 steps = 0;

    while (significant != 0)
    {
        significant >>= 1;
        steps++;
    }

    if (negative)
    {
        u32 trailing_zeroes = 0;

        while ((value & (1U << trailing_zeroes)) == 0)
            trailing_zeroes++;

        steps = MAX(steps, trailing_zeroes + 4);
    }

    steps = MIN(MAX(steps, 3U), (u32)width);
    return steps + 6 + (memory_operand ? 3 : 0);
}

u32 I386::GetBitScanClocks(u32 value, int width, bool reverse) const
{
    value &= GetMask(width);

    if (value == 0)
        return 6;

    u32 zeroes = 0;

    if (reverse)
    {
        u32 bit = GetSignBit(width);

        while ((value & bit) == 0)
        {
            zeroes++;
            bit >>= 1;
        }

        return 9 + zeroes * 3;
    }

    u32 bit = 1;

    while ((value & bit) == 0)
    {
        zeroes++;
        bit <<= 1;
    }

    return zeroes == 0 ? 10 : 11 + zeroes * 3;
}

INLINE bool I386::OPCodes_TEST_RM_Immediate(int width, u32 operand)
{
    Logic(operand & m_instruction.immediate, width);
    return true;
}

INLINE bool I386::OPCodes_NOT_RM(int width, u32 operand)
{
    return WriteRM(m_instruction, width, ~operand, *m_bus_context);
}

INLINE bool I386::OPCodes_NEG_RM(int width, u32 operand)
{
    u32 value = Sub(0, operand, 0, width);
    return WriteRM(m_instruction, width, value, *m_bus_context);
}

INLINE bool I386::OPCodes_MUL_RM(int width, u32 operand)
{
    m_step.clocks = GetMultiplyClocks(operand, width, false, m_instruction.memory_operand) + m_address_clocks;

    if (width == 8)
    {
        u16 product = (u16)GetRegister8(0) * (u8)operand;

        m_state.registers[I386_REG_EAX].low = product;
        m_state.eflags &= ~(I386_FLAG_CF | I386_FLAG_OF);

        if ((product & 0xFF00) != 0)
            m_state.eflags |= I386_FLAG_CF | I386_FLAG_OF;
    }
    else if (width == 16)
    {
        u32 product = (u32)m_state.registers[I386_REG_EAX].low * (u16)operand;

        m_state.registers[I386_REG_EAX].low = (u16)product;
        m_state.registers[I386_REG_EDX].low = (u16)(product >> 16);
        m_state.eflags &= ~(I386_FLAG_CF | I386_FLAG_OF);

        if ((product >> 16) != 0)
            m_state.eflags |= I386_FLAG_CF | I386_FLAG_OF;
    }
    else
    {
        u64 product = (u64)m_state.registers[I386_REG_EAX].value * operand;

        m_state.registers[I386_REG_EAX].value = (u32)product;
        m_state.registers[I386_REG_EDX].value = (u32)(product >> 32);
        m_state.eflags &= ~(I386_FLAG_CF | I386_FLAG_OF);

        if ((product >> 32) != 0)
            m_state.eflags |= I386_FLAG_CF | I386_FLAG_OF;
    }

    return true;
}

INLINE bool I386::OPCodes_IMUL_RM(int width, u32 operand)
{
    m_step.clocks = GetMultiplyClocks(operand, width, true, m_instruction.memory_operand) + m_address_clocks;

    if (width == 8)
    {
        s16 product = (s16)(s8)GetRegister8(0) * (s8)(u8)operand;
        bool overflow = product < -128 || product > 127;

        m_state.registers[I386_REG_EAX].low = (u16)product;
        m_state.eflags = overflow ? m_state.eflags | I386_FLAG_CF | I386_FLAG_OF :
            m_state.eflags & ~(I386_FLAG_CF | I386_FLAG_OF);
    }
    else if (width == 16)
    {
        s32 product = (s32)(s16)m_state.registers[I386_REG_EAX].low * (s16)(u16)operand;
        bool overflow = product < -32768 || product > 32767;

        m_state.registers[I386_REG_EAX].low = (u16)product;
        m_state.registers[I386_REG_EDX].low = (u16)((u32)product >> 16);
        m_state.eflags = overflow ? m_state.eflags | I386_FLAG_CF | I386_FLAG_OF :
            m_state.eflags & ~(I386_FLAG_CF | I386_FLAG_OF);
    }
    else
    {
        s64 product = (s64)(s32)m_state.registers[I386_REG_EAX].value * (s32)operand;
        bool overflow = product < (s64)INT_MIN || product > (s64)INT_MAX;

        m_state.registers[I386_REG_EAX].value = (u32)product;
        m_state.registers[I386_REG_EDX].value = (u32)((u64)product >> 32);
        m_state.eflags = overflow ? m_state.eflags | I386_FLAG_CF | I386_FLAG_OF :
            m_state.eflags & ~(I386_FLAG_CF | I386_FLAG_OF);
    }

    return true;
}

INLINE bool I386::OPCodes_DIV_RM(int width, u32 operand)
{
    if (operand == 0)
        return RaiseException(0, I386_EXCEPTION_FAULT);

    if (width == 8)
    {
        u16 dividend = m_state.registers[I386_REG_EAX].low;
        u32 quotient = dividend / (u8)operand;

        if (quotient > 0xFF)
            return RaiseException(0, I386_EXCEPTION_FAULT);

        SetRegister8(0, (u8)quotient);
        SetRegister8(4, (u8)(dividend % (u8)operand));
    }
    else if (width == 16)
    {
        u32 dividend = ((u32)m_state.registers[I386_REG_EDX].low << 16) | m_state.registers[I386_REG_EAX].low;
        u32 quotient = dividend / (u16)operand;

        if (quotient > 0xFFFF)
            return RaiseException(0, I386_EXCEPTION_FAULT);

        m_state.registers[I386_REG_EAX].low = (u16)quotient;
        m_state.registers[I386_REG_EDX].low = (u16)(dividend % (u16)operand);
    }
    else
    {
        u64 dividend = ((u64)m_state.registers[I386_REG_EDX].value << 32) | m_state.registers[I386_REG_EAX].value;
        u64 quotient = dividend / operand;

        if (quotient > 0xFFFFFFFFULL)
            return RaiseException(0, I386_EXCEPTION_FAULT);

        m_state.registers[I386_REG_EAX].value = (u32)quotient;
        m_state.registers[I386_REG_EDX].value = (u32)(dividend % operand);
    }

    return true;
}

INLINE bool I386::OPCodes_IDIV_RM(int width, u32 operand)
{
    if (operand == 0)
        return RaiseException(0, I386_EXCEPTION_FAULT);

    if (width == 8)
    {
        s16 dividend = (s16)m_state.registers[I386_REG_EAX].low;
        s16 divisor = (s8)(u8)operand;
        s16 quotient = dividend / divisor;

        if (quotient < -128)
        {
            u16 adjusted_bits = (u16)dividend;
            adjusted_bits &= ~0x4000U;

            if ((adjusted_bits & 0x8000U) != 0)
                adjusted_bits |= 0x4000U;

            s16 adjusted_dividend = (s16)adjusted_bits;
            s16 adjusted_quotient = adjusted_dividend / divisor;

            if (adjusted_quotient == -128)
            {
                dividend = adjusted_dividend;
                quotient = adjusted_quotient;
            }
        }

        if (quotient < -128 || quotient > 127)
            return RaiseException(0, I386_EXCEPTION_FAULT);

        SetRegister8(0, (u8)quotient);
        SetRegister8(4, (u8)(dividend % divisor));
    }
    else if (width == 16)
    {
        s32 dividend = (s32)(((u32)m_state.registers[I386_REG_EDX].low << 16) | m_state.registers[I386_REG_EAX].low);
        s32 divisor = (s16)(u16)operand;

        if (dividend == INT_MIN && divisor == -1)
            return RaiseException(0, I386_EXCEPTION_FAULT);

        s32 quotient = dividend / divisor;

        if (quotient < -32768 || quotient > 32767)
            return RaiseException(0, I386_EXCEPTION_FAULT);

        m_state.registers[I386_REG_EAX].low = (u16)quotient;
        m_state.registers[I386_REG_EDX].low = (u16)(dividend % divisor);
    }
    else
    {
        s64 dividend = (s64)(((u64)m_state.registers[I386_REG_EDX].value << 32) |
            m_state.registers[I386_REG_EAX].value);
        s64 divisor = (s32)operand;

        if (dividend == (s64)0x8000000000000000ULL && divisor == -1)
            return RaiseException(0, I386_EXCEPTION_FAULT);

        s64 quotient = dividend / divisor;

        if (quotient < (s64)INT_MIN || quotient > (s64)INT_MAX)
            return RaiseException(0, I386_EXCEPTION_FAULT);

        m_state.registers[I386_REG_EAX].value = (u32)quotient;
        m_state.registers[I386_REG_EDX].value = (u32)(dividend % divisor);
    }

    return true;
}

INLINE bool I386::OPCodes_INC_DEC_RM(int width, bool decrement)
{
    u32 value = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, value))
        return false;

    u32 old_cf = m_state.eflags & I386_FLAG_CF;

    value = !decrement ? Add(value, 1, 0, width) : Sub(value, 1, 0, width);
    m_state.eflags = (m_state.eflags & ~I386_FLAG_CF) | old_cf;

    if (!WriteRM(m_instruction, width, value, *m_bus_context))
        return false;

    CommitEIP(m_instruction);
    return true;
}

INLINE bool I386::OPCodes_INC_RM(int width)
{
    return OPCodes_INC_DEC_RM(width, false);
}

INLINE bool I386::OPCodes_DEC_RM(int width)
{
    return OPCodes_INC_DEC_RM(width, true);
}

INLINE bool I386::OPCodes_Near_Transfer_RM(int width, bool call)
{
    u32 target = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, target))
        return false;

    if (call)
    {
        target = Truncate(target, width);

        if (target > m_state.segments[I386_SEGMENT_CS].limit)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        if (!StackPushSized(m_instruction.next_eip, width, *m_bus_context))
            return false;

        m_state.eip = target;
    }
    else if (!BranchTo(target, width))
        return false;

    m_step.clocks += GetNextInstructionComponents();
    return true;
}

INLINE bool I386::OPCodes_CALL_Near_RM(int width)
{
    return OPCodes_Near_Transfer_RM(width, true);
}

INLINE bool I386::OPCodes_JMP_Near_RM(int width)
{
    return OPCodes_Near_Transfer_RM(width, false);
}

INLINE bool I386::OPCodes_Far_Transfer_Memory(int width, bool call)
{
    if (!m_instruction.memory_operand)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    u32 target = 0;
    u32 selector = 0;
    u32 selector_offset =
        Truncate(m_instruction.effective_offset + m_instruction.operand_size, m_instruction.address_size * 8);

    if (!ReadMemory(m_instruction.segment, m_instruction.effective_offset, width, *m_bus_context, target))
        return false;

    if (!ReadMemory(m_instruction.segment, selector_offset, 16, *m_bus_context, selector))
        return false;

    if (m_state.execution_mode == I386_MODE_PROTECTED)
        return ProtectedFarTransfer((u16)selector, target, width, call, m_instruction.next_eip, *m_bus_context,
            m_step.clocks, true);

    u32 checked_target = width == 16 ? (u16)target : target;

    if (checked_target > GetFarTransferLimit())
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    if (call)
    {
        if (!StackPushSized(m_state.segments[I386_SEGMENT_CS].selector, width, *m_bus_context))
            return false;

        if (!StackPushSized(m_instruction.next_eip, width, *m_bus_context))
            return false;
    }

    if (!FarTransfer((u16)selector, target, width))
        return false;

    m_step.clocks += GetNextInstructionComponents();
    return true;
}

INLINE bool I386::OPCodes_CALL_Far_Memory(int width)
{
    return OPCodes_Far_Transfer_Memory(width, true);
}

INLINE bool I386::OPCodes_JMP_Far_Memory(int width)
{
    return OPCodes_Far_Transfer_Memory(width, false);
}

INLINE bool I386::OPCodes_PUSH_RM(int width)
{
    u32 value = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, value))
        return false;

    if (!StackPushSized(value, width, *m_bus_context))
        return false;

    CommitEIP(m_instruction);
    return true;
}

// Flags follow the 80386 barrel shifter, including its carry for out-of-range multiples of the width
template<int width>
INLINE u32 I386::RotateShift(u32 value, int shift_operation, u32 count)
{
    const u32 mask = (u32)((1ULL << width) - 1);
    const u32 sign = 1U << (width - 1);

    value &= mask;
    count &= 0x1F;

    if (count == 0)
        return value;

    u32 original_value = value;
    bool carry = (m_state.eflags & I386_FLAG_CF) != 0;
    bool overflow = false;

    switch (shift_operation)
    {
        case I386_SHIFT_ROL:
        {
            u32 rotate = count & (width - 1);

            if (rotate != 0)
                value = ((value << rotate) | (value >> (width - rotate))) & mask;

            carry = (value & 1) != 0;
            overflow = ((value & sign) != 0) != carry;
            break;
        }
        case I386_SHIFT_ROR:
        {
            u32 rotate = count & (width - 1);

            if (rotate != 0)
                value = ((value >> rotate) | (value << (width - rotate))) & mask;

            carry = (value & sign) != 0;
            overflow = ((value & sign) != 0) != ((value & (sign >> 1)) != 0);
            break;
        }
        case I386_SHIFT_RCL:
        case I386_SHIFT_RCR:
        {
            const u32 bits = width + 1;
            u32 rotate = width == 32 ? count : count % bits;
            u64 ring = ((u64)value << 1) | (carry ? 1 : 0);

            if (rotate != 0)
            {
                if (shift_operation == I386_SHIFT_RCL)
                    ring = (ring << rotate) | (ring >> (bits - rotate));
                else
                    ring = (ring >> rotate) | (ring << (bits - rotate));

                ring &= (1ULL << bits) - 1;
            }

            carry = (ring & 1) != 0;
            value = (u32)(ring >> 1);

            if (shift_operation == I386_SHIFT_RCL)
                overflow = ((value & sign) != 0) != carry;
            else
                overflow = ((value & sign) != 0) != ((value & (sign >> 1)) != 0);

            break;
        }
        case I386_SHIFT_SHL:
        case I386_SHIFT_SAL:
            carry = count <= (u32)width && ((value >> (width - count)) & 1) != 0;
            value = (u32)((u64)value << count) & mask;

            if (count > (u32)width && (count % (u32)width) == 0)
                carry = (original_value & 1) != 0;

            overflow = ((value & sign) != 0) != carry;
            break;
        case I386_SHIFT_SHR:
            carry = count <= (u32)width && ((value >> (count - 1)) & 1) != 0;
            value >>= count;

            if (count > (u32)width && (count % (u32)width) == 0)
                carry = (original_value & sign) != 0;

            overflow = ((value & sign) != 0) != ((value & (sign >> 1)) != 0);
            break;
        default:
            // SAR
            carry = count < (u32)width ? ((value >> (count - 1)) & 1) != 0 : (value & sign) != 0;

            if (count >= (u32)width)
                value = (value & sign) != 0 ? mask : 0;
            else
            {
                u32 extension = (value & sign) != 0 ? (mask << (width - count)) & mask : 0;
                value = (value >> count) | extension;
            }

            if (count > (u32)width && (count % (u32)width) == 0)
                carry = (original_value & sign) != 0;

            break;
    }

    u32 flags = m_state.eflags & ~(I386_FLAG_CF | I386_FLAG_OF);
    flags |= carry ? I386_FLAG_CF : 0;
    flags |= overflow ? I386_FLAG_OF : 0;

    if (shift_operation >= I386_SHIFT_SHL)
        flags = (flags & ~(I386_FLAG_PF | I386_FLAG_ZF | I386_FLAG_SF)) | GetSZP(value, width);

    m_state.eflags = flags;
    return value;
}

// Group 2 shifts and rotates, specialized by operand width and count source
template<int width, int source>
bool I386::OPCodes_Group2()
{
    if (!DecodeOperands(true, source == I386_SHIFT_COUNT_IMMEDIATE ? 1 : 0))
        return false;

    u32 count = source == I386_SHIFT_COUNT_IMMEDIATE ? m_instruction.immediate :
        source == I386_SHIFT_COUNT_ONE ? 1 : m_state.registers[I386_REG_ECX].byte0;
    bool through_carry = m_instruction.reg == 2 || m_instruction.reg == 3;
    u32 clocks = through_carry ? (m_instruction.memory_operand ? 10 : 9) : (m_instruction.memory_operand ? 7 : 3);

    if (through_carry && (count & 0x1F) > (u32)width)
        clocks = 10 + 6 * (((count & 0x1F) - 1) / width);

    if (!StartExecution(clocks))
        return false;

    u32 value = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, value))
        return false;

    if ((count & 0x1F) != 0)
    {
        value = RotateShift<width>(value, m_instruction.reg, count);

        if (!WriteRM(m_instruction, width, value, *m_bus_context))
            return false;
    }

    CommitEIP(m_instruction);
    return true;
}

template bool I386::OPCodes_Group2<8, I386::I386_SHIFT_COUNT_IMMEDIATE>();
template bool I386::OPCodes_Group2<16, I386::I386_SHIFT_COUNT_IMMEDIATE>();
template bool I386::OPCodes_Group2<32, I386::I386_SHIFT_COUNT_IMMEDIATE>();
template bool I386::OPCodes_Group2<8, I386::I386_SHIFT_COUNT_ONE>();
template bool I386::OPCodes_Group2<16, I386::I386_SHIFT_COUNT_ONE>();
template bool I386::OPCodes_Group2<32, I386::I386_SHIFT_COUNT_ONE>();
template bool I386::OPCodes_Group2<8, I386::I386_SHIFT_COUNT_CL>();
template bool I386::OPCodes_Group2<16, I386::I386_SHIFT_COUNT_CL>();
template bool I386::OPCodes_Group2<32, I386::I386_SHIFT_COUNT_CL>();

// Group 3 TEST, NOT, NEG, MUL, IMUL, DIV and IDIV, specialized by operand width
template<int width>
bool I386::OPCodes_Group3()
{
    static const u8 k_group3_register_clocks[3][8] =
    {
        { 2, 2, 2, 2, 9, 9, 14, 19 },
        { 2, 2, 2, 2, 9, 9, 22, 27 },
        { 2, 2, 2, 2, 9, 9, 38, 43 }
    };
    static const u8 k_group3_memory_extra_clocks[8] = { 3, 3, 4, 4, 3, 3, 3, 0 };

    if (!DecodeOperands(true, width == 8 ? -3 : -4))
        return false;

    u8 reg = m_instruction.reg;
    int size = width == 8 ? 0 : width == 16 ? 1 : 2;
    u32 clocks = k_group3_register_clocks[size][reg];

    if (m_instruction.memory_operand)
        clocks += k_group3_memory_extra_clocks[reg];

    if (!StartExecution(clocks))
        return false;

    u32 operand = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, operand))
        return false;

    bool ok;

    switch (reg)
    {
        case 0:
        case 1:
            // TEST r/m,imm (/0 and undocumented /1 alias)
            ok = OPCodes_TEST_RM_Immediate(width, operand);
            break;
        case 2:
            ok = OPCodes_NOT_RM(width, operand);
            break;
        case 3:
            ok = OPCodes_NEG_RM(width, operand);
            break;
        case 4:
            ok = OPCodes_MUL_RM(width, operand);
            break;
        case 5:
            ok = OPCodes_IMUL_RM(width, operand);
            break;
        case 6:
            ok = OPCodes_DIV_RM(width, operand);
            break;
        default:
            ok = OPCodes_IDIV_RM(width, operand);
            break;
    }

    if (!ok)
        return false;

    CommitEIP(m_instruction);
    return true;
}

template bool I386::OPCodes_Group3<8>();
template bool I386::OPCodes_Group3<16>();
template bool I386::OPCodes_Group3<32>();

bool I386::OPCodes_Group4()
{
    if (!DecodeOperands(true, 0))
        return false;

    if (!StartExecution(m_instruction.reg <= 1 ? (m_instruction.memory_operand ? 6 : 2) : 0))
        return false;

    switch (m_instruction.reg)
    {
        case 0:
            return OPCodes_INC_RM(8);
        case 1:
            return OPCodes_DEC_RM(8);
        default:
            return RaiseException(6, I386_EXCEPTION_FAULT);
    }
}

// Group 5 INC, DEC, CALL, JMP and PUSH, specialized by operand width
template<int width>
bool I386::OPCodes_Group5()
{
    static const u8 k_group5_register_clocks[8] = { 2, 2, 7, 0, 7, 0, 2, 0 };
    static const u8 k_group5_memory_clocks[8] = { 6, 6, 10, 22, 10, 43, 5, 0 };

    if (!DecodeOperands(true, 0))
        return false;

    u8 reg = m_instruction.reg;

    if (!StartExecution(m_instruction.memory_operand ? k_group5_memory_clocks[reg] : k_group5_register_clocks[reg]))
        return false;

    switch (m_instruction.reg)
    {
        case 0:
            return OPCodes_INC_RM(width);
        case 1:
            return OPCodes_DEC_RM(width);
        case 2:
            return OPCodes_CALL_Near_RM(width);
        case 3:
            return OPCodes_CALL_Far_Memory(width);
        case 4:
            return OPCodes_JMP_Near_RM(width);
        case 5:
            return OPCodes_JMP_Far_Memory(width);
        case 6:
            return OPCodes_PUSH_RM(width);
        default:
            return RaiseException(6, I386_EXCEPTION_FAULT);
    }
}

template bool I386::OPCodes_Group5<16>();
template bool I386::OPCodes_Group5<32>();
