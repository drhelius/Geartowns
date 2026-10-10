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

#ifndef I386_DECODE_INLINE_H
#define I386_DECODE_INLINE_H

// Executed instruction bytes are read straight from the host memory of the code page
// m_fetch_remaining counts the bytes that can be consumed without checks:
// inside the page, the CS limit and the 15-byte instruction length
// Passive fetches read the bytes gathered for the debugger instead
// In both cases next_eip is the cursor
template<bool passive>
INLINE bool I386::FetchCode8(InstructionContext& instruction, u8& value)
{
    if (passive)
    {
        if (m_passive_index >= m_passive_count)
            return false;

        value = m_passive_bytes[m_passive_index++];
        instruction.next_eip++;
        return true;
    }

    if (unlikely(m_fetch_remaining == 0))
        return FetchCodeSlow8(instruction, value);

    value = *m_fetch_pointer++;
    m_fetch_remaining--;
    instruction.next_eip++;
    return true;
}

template<bool passive>
INLINE bool I386::FetchCode16(InstructionContext& instruction, u16& value)
{
    if (!passive && likely(m_fetch_remaining >= 2))
    {
        value = read_u16_le(m_fetch_pointer);
        m_fetch_pointer += 2;
        m_fetch_remaining -= 2;
        instruction.next_eip += 2;
        return true;
    }

    u8 low = 0;
    u8 high = 0;

    if (!FetchCode8<passive>(instruction, low))
        return false;

    if (!FetchCode8<passive>(instruction, high))
        return false;

    value = (u16)low | ((u16)high << 8);
    return true;
}

template<bool passive>
INLINE bool I386::FetchCode32(InstructionContext& instruction, u32& value)
{
    if (!passive && likely(m_fetch_remaining >= 4))
    {
        value = read_u32_le(m_fetch_pointer);
        m_fetch_pointer += 4;
        m_fetch_remaining -= 4;
        instruction.next_eip += 4;
        return true;
    }

    u16 low = 0;
    u16 high = 0;

    if (!FetchCode16<passive>(instruction, low))
        return false;

    if (!FetchCode16<passive>(instruction, high))
        return false;

    value = (u32)low | ((u32)high << 16);
    return true;
}

// Effective address from ModR/M, SIB and displacement with the current registers
INLINE void I386::CalculateEffectiveOffset(InstructionContext& instruction)
{
    u8 mod = instruction.modrm >> 6;
    u8 rm = instruction.modrm & 7;
    u32 offset = (u32)instruction.displacement;
    bool stack = false;

    if (instruction.address_size == 2)
    {
        switch (rm)
        {
            case 0:
                // [BX+SI]
                offset += m_state.registers[I386_REG_EBX].low + m_state.registers[I386_REG_ESI].low;
                break;
            case 1:
                // [BX+DI]
                offset += m_state.registers[I386_REG_EBX].low + m_state.registers[I386_REG_EDI].low;
                break;
            case 2:
                // [BP+SI]
                offset += m_state.registers[I386_REG_EBP].low + m_state.registers[I386_REG_ESI].low;
                stack = true;
                break;
            case 3:
                // [BP+DI]
                offset += m_state.registers[I386_REG_EBP].low + m_state.registers[I386_REG_EDI].low;
                stack = true;
                break;
            case 4:
                // [SI]
                offset += m_state.registers[I386_REG_ESI].low;
                break;
            case 5:
                // [DI]
                offset += m_state.registers[I386_REG_EDI].low;
                break;
            case 6:
                // [disp16] for mod 0, otherwise [BP]
                if (mod != 0)
                {
                    offset += m_state.registers[I386_REG_EBP].low;
                    stack = true;
                }

                break;
            default:
                // [BX]
                offset += m_state.registers[I386_REG_EBX].low;
                break;
        }

        offset &= 0xFFFF;
    }
    else if (rm == 4)
    {
        u8 base = instruction.sib & 7;
        u8 index = (instruction.sib >> 3) & 7;
        u8 scale = instruction.sib >> 6;
        bool has_base = mod != 0 || base != 5;
        u32 base_value = has_base ? m_state.registers[base].value : 0;

        stack = has_base && (base == I386_REG_EBP || base == I386_REG_ESP);

        // Without an index register a non-zero scale applies to the base
        if (index != 4)
            offset += base_value + (m_state.registers[index].value << scale);
        else if (scale != 0 && has_base)
            offset += base_value << scale;
        else
            offset += base_value;
    }
    else if (mod != 0 || rm != 5)
    {
        offset += m_state.registers[rm].value;
        stack = rm == I386_REG_EBP;
    }

    instruction.effective_offset = offset;
    instruction.segment = instruction.segment_override != 0xFF ? instruction.segment_override :
        (u8)(stack ? I386_SEGMENT_SS : I386_SEGMENT_DS);
}

template<bool passive>
INLINE bool I386::DecodeModRM(InstructionContext& instruction)
{
    u8 modrm = 0;

    if (!FetchCode8<passive>(instruction, modrm))
        return false;

    instruction.modrm = modrm;
    instruction.reg = (modrm >> 3) & 7;
    instruction.rm = modrm & 7;
    instruction.memory_operand = modrm < 0xC0;

    if (modrm >= 0xC0)
        return true;

    u8 mod = modrm >> 6;
    u8 rm = modrm & 7;
    s32 displacement = 0;
    bool ok = true;

    if (instruction.address_size == 2)
    {
        if ((mod == 0 && rm == 6) || mod == 2)
        {
            u16 value = 0;
            ok = FetchCode16<passive>(instruction, value);
            displacement = mod == 2 ? (s16)value : value;
        }
        else if (mod == 1)
        {
            u8 value = 0;
            ok = FetchCode8<passive>(instruction, value);
            displacement = (s8)value;
        }

        if (!passive)
            m_address_clocks = rm <= 3 ? 1 : 0;
    }
    else
    {
        u8 base = rm;

        if (rm == 4)
        {
            if (!FetchCode8<passive>(instruction, instruction.sib))
                return false;

            base = instruction.sib & 7;

            if (!passive)
                m_address_clocks = ((instruction.sib >> 3) & 7) != 4 && (mod != 0 || base != 5) ? 1 : 0;
        }

        if (mod == 2 || (mod == 0 && base == 5))
        {
            u32 value = 0;
            ok = FetchCode32<passive>(instruction, value);
            displacement = (s32)value;
        }
        else if (mod == 1)
        {
            u8 value = 0;
            ok = FetchCode8<passive>(instruction, value);
            displacement = (s8)value;
        }
    }

    if (!ok)
        return false;

    instruction.displacement = displacement;
    CalculateEffectiveOffset(instruction);
    return true;
}

// Fixed immediates and the Group 3 TEST immediate are decoded directly. Other negative sizes use the opcode table
INLINE bool I386::HasDirectOperands(bool modrm, int immediate_size) const
{
    return !m_instruction.lock && (immediate_size >= 0 || (modrm && (immediate_size == -3 || immediate_size == -4)));
}

// Bytes the operands take from the fetch window. A SIB byte beyond the window only shows that more are needed
INLINE u32 I386::GetOperandsLength(bool modrm, int immediate_size, const u8* window, u32 available) const
{
    if (!modrm)
        return (u32)immediate_size;

    if (available == 0)
        return 1;

    u8 value = window[0];
    u8 mod = value >> 6;
    u8 rm = value & 7;
    u32 address_bytes = 0;

    if (mod != 3)
    {
        if (m_instruction.address_size == 2)
            address_bytes = (mod == 0 && rm == 6) || mod == 2 ? 2 : mod == 1 ? 1 : 0;
        else if (rm == 4)
        {
            if (available < 2)
                return 2;

            address_bytes = 1 + (mod == 2 || (mod == 0 && (window[1] & 7) == 5) ? 4 : mod == 1 ? 1 : 0);
        }
        else
            address_bytes = mod == 2 || (mod == 0 && rm == 5) ? 4 : mod == 1 ? 1 : 0;
    }

    if (immediate_size < 0)
        immediate_size = ((value >> 3) & 7) <= 1 ? (immediate_size == -3 ? 1 : m_instruction.operand_size) : 0;

    return 1 + address_bytes + (u32)immediate_size;
}

// Decodes ModR/M, SIB, displacement and immediate straight from the fetch window when it holds all of them.
// Everything else decodes byte by byte, fetching the same bytes
INLINE bool I386::DecodeOperands(bool modrm, int immediate_size)
{
    InstructionContext& instruction = m_instruction;

    if (unlikely(!HasDirectOperands(modrm, immediate_size)))
        return DecodeOperandsSlow(modrm, immediate_size);

    const u8* window = m_fetch_pointer;
    u32 available = m_fetch_remaining;
    u32 length = GetOperandsLength(modrm, immediate_size, window, available);

    if (unlikely(available < length))
        return DecodeOperandsSlow(modrm, immediate_size);

    if (modrm)
    {
        u8 value = window[0];
        u8 mod = value >> 6;
        u8 rm = value & 7;
        u8 reg = (value >> 3) & 7;

        if (immediate_size < 0)
            immediate_size = reg <= 1 ? (immediate_size == -3 ? 1 : instruction.operand_size) : 0;

        u32 address_bytes = length - 1 - (u32)immediate_size;

        instruction.modrm = value;
        instruction.reg = reg;
        instruction.rm = rm;
        instruction.memory_operand = mod != 3;

        if (mod != 3)
        {
            const u8* bytes = window + 1;
            s32 displacement = 0;

            if (instruction.address_size == 2)
            {
                if (address_bytes == 2)
                    displacement = mod == 2 ? (s16)read_u16_le(bytes) : (s32)read_u16_le(bytes);
                else if (address_bytes == 1)
                    displacement = (s8)bytes[0];

                m_address_clocks = rm <= 3 ? 1 : 0;
            }
            else
            {
                if (rm == 4)
                {
                    u8 sib = *bytes++;
                    instruction.sib = sib;
                    m_address_clocks = ((sib >> 3) & 7) != 4 && (mod != 0 || (sib & 7) != 5) ? 1 : 0;
                    address_bytes--;
                }

                if (address_bytes == 4)
                    displacement = (s32)read_u32_le(bytes);
                else if (address_bytes == 1)
                    displacement = (s8)bytes[0];
            }

            instruction.displacement = displacement;
            CalculateEffectiveOffset(instruction);
        }
    }

    const u8* immediate = window + length - immediate_size;

    if (immediate_size == 1)
        instruction.immediate = immediate[0];
    else if (immediate_size == 2)
        instruction.immediate = read_u16_le(immediate);
    else if (immediate_size == 4)
        instruction.immediate = read_u32_le(immediate);

    m_fetch_pointer = window + length;
    m_fetch_remaining = available - length;
    instruction.next_eip += length;
    return true;
}

INLINE bool I386::StartExecution(u32 clocks)
{
    m_step.clocks = clocks + m_address_clocks;

    if (unlikely(m_debug_step && (m_state.debug_registers[7] & 0xFF) != 0 && (m_state.eflags & I386_FLAG_RF) == 0))
    {
        if (!CheckInstructionBreakpoint())
            return false;
    }

    if (unlikely(m_instruction.lock && m_state.execution_mode == I386_MODE_VM86 && GetIOPrivilegeLevel() < 3))
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    return true;
}

INLINE bool I386::DecodeAndStart(bool modrm, int immediate_size, u32 register_clocks, u32 memory_clocks)
{
    if (!DecodeOperands(modrm, immediate_size))
        return false;

    return StartExecution(m_instruction.memory_operand ? memory_clocks : register_clocks);
}

// Resets the per-instruction state and dispatches the opcode from the code window. Guest bytes may alias the CPU
// state, so the opcode is loaded first and the fetch state is stored once after it
INLINE bool I386::StartInstruction()
{
    InstructionContext& instruction = m_instruction;
    u32 eip = m_state.eip;

    instruction.start_eip = eip;
    instruction.immediate = 0;
    memcpy(&instruction.operand_size, &m_instruction_defaults.operand_size, 8);
#if !defined(GT_DISABLE_DISASSEMBLER)
    instruction.call_return_size = 0;
#endif

    m_instruction_restored = false;
    m_address_clocks = 0;

    if (unlikely(eip - m_code_window_eip >= m_code_window_size))
        OpenCodeWindow(eip);

    u32 offset = eip - m_code_window_eip;

    if (unlikely(offset >= m_code_window_size))
    {
        instruction.next_eip = eip;
        m_fetch_remaining = 0;
        return DispatchOPCode();
    }

    const u8* window = m_code_window + offset;
    u8 opcode = window[0];

    m_fetch_pointer = window + 1;
    m_fetch_remaining = MIN(m_code_window_size - offset, (u32)GT_I386_MAX_INSTRUCTION_LENGTH) - 1;
    instruction.next_eip = eip + 1;
    instruction.opcode = opcode;
    return k_opcodes[opcode](this);
}

// Prefixes and the two-byte escape update the instruction state and dispatch the following byte
INLINE bool I386::DispatchOPCode()
{
    u8 opcode = 0;

    if (!FetchCode8<false>(m_instruction, opcode))
        return false;

    m_instruction.opcode = opcode;
    return k_opcodes[opcode](this);
}

INLINE bool I386::DispatchOPCode0F()
{
    m_instruction.two_byte = true;

    if (!FetchCode8<false>(m_instruction, m_instruction.opcode2))
        return false;

    return k_opcodes_0f[m_instruction.opcode2](this);
}

INLINE bool I386::PrefixSegment(u8 segment)
{
    m_instruction.segment_override = segment;
    return DispatchOPCode();
}

INLINE bool I386::PrefixOperandSize()
{
    m_instruction.operand_size = m_default_size == 4 ? 2 : 4;
    return DispatchOPCode();
}

INLINE bool I386::PrefixAddressSize()
{
    m_instruction.address_size = m_default_size == 4 ? 2 : 4;
    return DispatchOPCode();
}

INLINE bool I386::PrefixLock()
{
    m_instruction.lock = true;
    return DispatchOPCode();
}

INLINE bool I386::PrefixRepeat(u8 repeat)
{
    m_instruction.repeat = repeat;
    return DispatchOPCode();
}

#endif /* I386_DECODE_INLINE_H */
