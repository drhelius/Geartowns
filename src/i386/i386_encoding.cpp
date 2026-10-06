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
#include "../system/memory.h"

// Instruction encoding:
// M = ModR/M byte, P = prefix, immediate I8 = byte,
// I16 = word, EN = ENTER word+byte, IV = operand size,
// FP = far pointer (operand size + selector), MO = address size,
// G3 = TEST in Group 3 only
#define M k_i386_encoding_modrm
#define P k_i386_encoding_prefix
#define I8 k_i386_immediate_byte
#define I16 k_i386_immediate_word
#define EN k_i386_immediate_enter
#define IV k_i386_immediate_operand
#define FP k_i386_immediate_far
#define MO k_i386_immediate_address
#define G3 k_i386_immediate_group3

const u8 I386::k_opcode_encoding[512] =
{
// Primary map
/*          0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F */
/* 0x00 */  M,    M,    M,    M,    I8,   IV,   0,    0,    M,    M,    M,    M,    I8,   IV,   0,    0,
/* 0x10 */  M,    M,    M,    M,    I8,   IV,   0,    0,    M,    M,    M,    M,    I8,   IV,   0,    0,
/* 0x20 */  M,    M,    M,    M,    I8,   IV,   P,    0,    M,    M,    M,    M,    I8,   IV,   P,    0,
/* 0x30 */  M,    M,    M,    M,    I8,   IV,   P,    0,    M,    M,    M,    M,    I8,   IV,   P,    0,
/* 0x40 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x50 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x60 */  0,    0,    M,    M,    P,    P,    P,    P,    IV,   M|IV, I8,   M|I8, 0,    0,    0,    0,
/* 0x70 */  I8,   I8,   I8,   I8,   I8,   I8,   I8,   I8,   I8,   I8,   I8,   I8,   I8,   I8,   I8,   I8,
/* 0x80 */  M|I8, M|IV, M|I8, M|I8, M,    M,    M,    M,    M,    M,    M,    M,    M,    M,    M,    M,
/* 0x90 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    FP,   0,    0,    0,    0,    0,
/* 0xA0 */  MO,   MO,   MO,   MO,   0,    0,    0,    0,    I8,   IV,   0,    0,    0,    0,    0,    0,
/* 0xB0 */  I8,   I8,   I8,   I8,   I8,   I8,   I8,   I8,   IV,   IV,   IV,   IV,   IV,   IV,   IV,   IV,
/* 0xC0 */  M|I8, M|I8, I16,  0,    M,    M,    M|I8, M|IV, EN,   0,    I16,  0,    0,    I8,   0,    0,
/* 0xD0 */  M,    M,    M,    M,    I8,   I8,   0,    0,    M,    M,    M,    M,    M,    M,    M,    M,
/* 0xE0 */  I8,   I8,   I8,   I8,   I8,   I8,   I8,   I8,   IV,   IV,   FP,   I8,   0,    0,    0,    0,
/* 0xF0 */  P,    0,    P,    P,    0,    0,    M|G3, M|G3, 0,    0,    0,    0,    0,    0,    M,    M,

// Two-byte map (0x0F xx)
/*          0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F */
/* 0x00 */  M,    M,    M,    M,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x10 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x20 */  M,    M,    M,    M,    M,    M,    M,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x30 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x40 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x50 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x60 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x70 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0x80 */  IV,   IV,   IV,   IV,   IV,   IV,   IV,   IV,   IV,   IV,   IV,   IV,   IV,   IV,   IV,   IV,
/* 0x90 */  M,    M,    M,    M,    M,    M,    M,    M,    M,    M,    M,    M,    M,    M,    M,    M,
/* 0xA0 */  0,    0,    0,    M,    M|I8, M,    0,    0,    0,    0,    0,    M,    M|I8, M,    0,    M,
/* 0xB0 */  0,    0,    M,    M,    M,    M,    M,    M,    M,    M,    M|I8, M,    M,    M,    M,    M,
/* 0xC0 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0xD0 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0xE0 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 0xF0 */  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0
};

#undef M
#undef P
#undef I8
#undef I16
#undef EN
#undef IV
#undef FP
#undef MO
#undef G3

// Passive decode for the debugger: prefixes are parsed here, the rest shares the execution decoder
bool I386::DecodeInstructionPassive(InstructionContext& instruction, u32 eip, bool default32)
{
    memset(&instruction, 0, sizeof(instruction));
    instruction.start_eip = eip;
    instruction.next_eip = eip;
    instruction.segment_override = 0xFF;

    bool operand_override = false;
    bool address_override = false;
    bool parsing_prefixes = true;

    while (parsing_prefixes)
    {
        u8 value = 0;

        if (!FetchCode8<true>(instruction, value))
            return false;

        switch (value)
        {
            case 0x26:
                // ES segment override
                instruction.segment_override = I386_SEGMENT_ES;
                break;
            case 0x2E:
                // CS segment override
                instruction.segment_override = I386_SEGMENT_CS;
                break;
            case 0x36:
                // SS segment override
                instruction.segment_override = I386_SEGMENT_SS;
                break;
            case 0x3E:
                // DS segment override
                instruction.segment_override = I386_SEGMENT_DS;
                break;
            case 0x64:
                // FS segment override
                instruction.segment_override = I386_SEGMENT_FS;
                break;
            case 0x65:
                // GS segment override
                instruction.segment_override = I386_SEGMENT_GS;
                break;
            case 0x66:
                // Operand-size override
                operand_override = true;
                break;
            case 0x67:
                // Address-size override
                address_override = true;
                break;
            case 0xF0:
                // LOCK
                instruction.lock = true;
                break;
            case 0xF2:
                // REPNE
                instruction.repeat = 2;
                break;
            case 0xF3:
                // REP/REPE
                instruction.repeat = 3;
                break;
            default:
                instruction.opcode = value;
                parsing_prefixes = false;
                break;
        }
    }

    instruction.operand_size = (default32 != operand_override) ? 4 : 2;
    instruction.address_size = (default32 != address_override) ? 4 : 2;

    if (instruction.opcode == 0x0F)
    {
        instruction.two_byte = true;

        if (!FetchCode8<true>(instruction, instruction.opcode2))
            return false;
    }

    if (OPCodeHasModRM(instruction.two_byte, instruction.two_byte ? instruction.opcode2 : instruction.opcode))
    {
        if (!DecodeModRM<true>(instruction))
            return false;
    }

    return DecodeImmediate<true>(instruction);
}

// Byte-by-byte operand decode: LOCK, table-driven immediates and windows that cannot be crossed directly
NO_INLINE bool I386::DecodeOperandsSlow(bool modrm, int immediate_size)
{
    bool ok = true;

    if (modrm)
    {
        if (!DecodeModRM<false>(m_instruction))
            return false;
    }

    if (unlikely(m_instruction.lock) && !IsLockAllowed(m_instruction))
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (immediate_size == -3 || immediate_size == -4)
        immediate_size = m_instruction.reg <= 1 ? (immediate_size == -3 ? 1 : m_instruction.operand_size) : 0;

    if (immediate_size < 0)
        ok = DecodeImmediate<false>(m_instruction);
    else if (immediate_size == 1)
    {
        u8 value = 0;
        ok = FetchCode8<false>(m_instruction, value);
        m_instruction.immediate = value;
    }
    else if (immediate_size == 2)
    {
        u16 value = 0;
        ok = FetchCode16<false>(m_instruction, value);
        m_instruction.immediate = value;
    }
    else if (immediate_size == 4)
        ok = FetchCode32<false>(m_instruction, m_instruction.immediate);

    return ok;
}

bool I386::DecodeInstructionForDebugger(const I386_Segment& code_segment, u32 eip, I386_Decode_State& state)
{
    // Gather the bytes visible to passive reads, stopping at the CS limit or unreadable memory
    u8 bytes[GT_I386_MAX_INSTRUCTION_LENGTH];
    u32 count = 0;

    while (count < GT_I386_MAX_INSTRUCTION_LENGTH && (code_segment.attributes & I386_SEGMENT_PRESENT) != 0 &&
        (u64)eip + count <= code_segment.limit && TryPeekLinear(code_segment.base + eip + count, bytes[count]))
        count++;

    InstructionContext instruction;

    m_passive_bytes = bytes;
    m_passive_count = count;
    m_passive_index = 0;

    if (!DecodeInstructionPassive(instruction, eip, (code_segment.attributes & I386_SEGMENT_DEFAULT_32) != 0))
        return false;

    FillDecodeState(instruction, bytes, instruction.next_eip - eip, state);
    state.invalid_lock = instruction.lock && !IsLockAllowed(instruction);
    return true;
}

bool I386::OPCodeHasModRM(bool two_byte, u8 opcode) const
{
    return (k_opcode_encoding[two_byte ? 256 + opcode : opcode] & k_i386_encoding_modrm) != 0;
}

template<bool passive>
bool I386::DecodeImmediate(InstructionContext& instruction)
{
    u8 opcode = instruction.two_byte ? instruction.opcode2 : instruction.opcode;
    int operand = instruction.operand_size;
    int address = instruction.address_size;

    // ENTER imm16,imm8
    if (!instruction.two_byte && opcode == 0xC8)
    {
        u16 value16 = 0;
        u8 value8 = 0;
        bool ok = FetchCode16<passive>(instruction, value16);

        instruction.immediate = value16;

        if (ok)
        {
            ok = FetchCode8<passive>(instruction, value8);
            instruction.immediate2 = value8;
        }

        return ok;
    }

    // CALL/JMP far ptr16:16/32
    if (!instruction.two_byte && (opcode == 0x9A || opcode == 0xEA))
    {
        bool ok;
        u16 value16 = 0;

        if (operand == 2)
        {
            u16 offset = 0;
            ok = FetchCode16<passive>(instruction, offset);
            instruction.immediate = offset;
        }
        else
        {
            u32 value32 = 0;
            ok = FetchCode32<passive>(instruction, value32);
            instruction.immediate = value32;
        }

        if (ok)
        {
            ok = FetchCode16<passive>(instruction, value16);
            instruction.immediate2 = value16;
        }

        return ok;
    }

    int immediate_size = GetImmediateSize(instruction.two_byte, opcode, instruction.reg, operand, address);

    if (immediate_size == 1)
    {
        u8 value = 0;
        bool ok = FetchCode8<passive>(instruction, value);
        instruction.immediate = value;
        return ok;
    }

    if (immediate_size == 2)
    {
        u16 value = 0;
        bool ok = FetchCode16<passive>(instruction, value);
        instruction.immediate = value;
        return ok;
    }

    if (immediate_size == 4)
    {
        u32 value = 0;
        bool ok = FetchCode32<passive>(instruction, value);
        instruction.immediate = value;
        return ok;
    }

    return true;
}

template bool I386::DecodeImmediate<false>(InstructionContext&);
template bool I386::DecodeImmediate<true>(InstructionContext&);

// Passive translation for branch timing: page tables are read without TLB use, A/D updates or faults
INLINE bool I386::TranslateCodePassive(u32 linear, u32& physical) const
{
    if (likely((m_state.cr0 & 0x80000000U) == 0))
    {
        physical = linear;
        return true;
    }

    u32 pde_address = (m_state.cr3 & 0xFFFFF000U) + ((linear >> 20) & 0xFFCU);
    const u8* directory = m_read_pages[pde_address >> 12];

    if (IsValidPointer(directory))
    {
        u32 pde = read_u32_le(directory + (pde_address & 0xFFF));

        if ((pde & 1) == 0)
            return false;

        u32 pte_address = (pde & 0xFFFFF000U) + ((linear >> 10) & 0xFFCU);
        const u8* table = m_read_pages[pte_address >> 12];

        if (IsValidPointer(table))
        {
            u32 pte = read_u32_le(table + (pte_address & 0xFFF));

            if ((pte & 1) == 0)
                return false;

            physical = (pte & 0xFFFFF000U) | (linear & 0xFFF);
            return true;
        }
    }

    // Page tables outside the direct page map. The result goes through a local so callers keep theirs in registers
    u32 result = 0;
    bool translated = TranslatePagedPassive(linear, result);
    physical = result;
    return translated;
}

u32 I386::GetNextInstructionComponents() const
{
    u32 window_offset = m_state.eip - m_code_window_eip;

    // Window path: most near transfers land inside the code window, which holds the bytes the target fetches
    if (window_offset < m_code_window_size)
    {
        u32 count = MIN(m_code_window_size - window_offset, (u32)GT_I386_MAX_INSTRUCTION_LENGTH);
        bool truncated = true;
        u32 components = CountInstructionComponents(m_code_window + window_offset, count, truncated);

        if (!truncated)
            return components;
    }

    const I386_Segment& code = m_state.segments[I386_SEGMENT_CS];

    // Direct path: the target bytes sit inside one mapped page and the CS limit
    if ((code.attributes & (I386_SEGMENT_PRESENT | I386_SEGMENT_EXPAND_DOWN)) == I386_SEGMENT_PRESENT &&
        m_state.eip <= code.limit)
    {
        u32 linear = code.base + m_state.eip;
        u32 physical = linear;

        if ((m_state.cr0 & 0x80000000U) == 0 || TranslateCodePassive(linear, physical))
        {
            const u8* page = m_read_pages[physical >> 12];

            if (IsValidPointer(page))
            {
                u64 available = MIN((u64)code.limit - m_state.eip + 1, (u64)(0x1000 - (linear & 0xFFF)));
                u32 count = (u32)MIN(available, (u64)GT_I386_MAX_INSTRUCTION_LENGTH);
                bool truncated = true;
                u32 components = CountInstructionComponents(page + (physical & 0xFFF), count, truncated);

                if (!truncated)
                    return components;
            }
        }
    }

    return GetNextInstructionComponentsChecked();
}

// Checked path: gather the bytes visible to passive reads across pages and limits
NO_INLINE u32 I386::GetNextInstructionComponentsChecked() const
{
    TimingCursor cursor;
    cursor.eip = m_state.eip;
    cursor.length = 0;
    cursor.remaining = 0;
    cursor.translated = false;

    u8 bytes[GT_I386_MAX_INSTRUCTION_LENGTH];
    u32 count = 0;
    bool truncated = true;

    while (count < GT_I386_MAX_INSTRUCTION_LENGTH && FetchTimingByte(cursor, bytes[count]))
        count++;

    return CountInstructionComponents(bytes, count, truncated);
}

INLINE bool I386::FetchTimingByte(TimingCursor& cursor, u8& value) const
{
    if (cursor.length >= GT_I386_MAX_INSTRUCTION_LENGTH)
        return false;

    if (cursor.remaining != 0)
    {
        value = *cursor.data++;
        cursor.remaining--;
    }
    else
    {
        const I386_Segment& code = m_state.segments[I386_SEGMENT_CS];

        if (!IsValidSegmentOffset(code, cursor.eip, m_state.execution_mode == I386_MODE_PROTECTED))
            return false;

        u32 linear = code.base + cursor.eip;
        u32 page = linear & 0xFFFFF000U;

        if (!cursor.translated || page != cursor.linear_page)
        {
            u32 physical = 0;

            if (!TryTranslateLinear(linear, physical))
                return false;

            cursor.linear_page = page;
            cursor.physical_page = physical & 0xFFFFF000U;
            cursor.translated = true;
        }

        u32 physical = cursor.physical_page | (linear & 0xFFF);
        u32 upper_limit = code.limit;

        if (m_state.execution_mode == I386_MODE_PROTECTED && (code.attributes & I386_SEGMENT_EXPAND_DOWN) != 0)
            upper_limit = (code.attributes & I386_SEGMENT_DEFAULT_32) != 0 ? 0xFFFFFFFFU : 0xFFFFU;

        u64 available = MIN((u64)upper_limit - cursor.eip + 1, (u64)(0x1000 - (linear & 0xFFF)));
        u32 count = (u32)MIN(available, (u64)(GT_I386_MAX_INSTRUCTION_LENGTH - cursor.length));
        const u8* data = m_memory->GetPhysicalReadSpan(physical, count);

        if (IsValidPointer(data))
        {
            value = data[0];
            cursor.data = data + 1;
            cursor.remaining = (u8)(count - 1);
        }
        else if (!m_memory->TryPeekPhysical(physical, value))
            return false;
    }

    cursor.eip++;
    cursor.length++;
    return true;
}

INLINE u32 I386::CountInstructionComponents(const u8* bytes, u32 count, bool& truncated) const
{
    u32 index = 0;
    u32 components = 0;
    bool operand_override = false;
    bool address_override = false;
    u8 opcode = 0;
    u8 encoding = 0;

    truncated = true;

    while (true)
    {
        if (index >= count)
            return components == 0 ? 1 : components;

        opcode = bytes[index++];
        encoding = k_opcode_encoding[opcode];
        components++;

        if ((encoding & k_i386_encoding_prefix) == 0)
            break;

        operand_override = operand_override || opcode == 0x66;
        address_override = address_override || opcode == 0x67;
    }

    bool default32 = m_default_size == 4;
    int operand_size = (default32 != operand_override) ? 4 : 2;
    int address_size = (default32 != address_override) ? 4 : 2;

    if (opcode == 0x0F)
    {
        if (index >= count)
            return components;

        opcode = bytes[index++];
        encoding = k_opcode_encoding[256 + opcode];
        components++;
    }

    u8 reg = 0;

    if ((encoding & k_i386_encoding_modrm) != 0)
    {
        if (index >= count)
            return components;

        u8 modrm = bytes[index++];
        components++;

        u8 mod = modrm >> 6;
        reg = (modrm >> 3) & 7;
        u8 rm = modrm & 7;
        u32 displacement_size = 0;

        if (mod != 3 && address_size == 2)
        {
            if ((mod == 0 && rm == 6) || mod == 2)
                displacement_size = 2;
            else if (mod == 1)
                displacement_size = 1;
        }
        else if (mod != 3)
        {
            bool direct = mod == 0 && rm == 5;

            if (rm == 4)
            {
                if (index >= count)
                    return components;

                u8 sib = bytes[index++];
                components++;
                direct = mod == 0 && (sib & 7) == 5;
            }

            if (direct || mod == 2)
                displacement_size = 4;
            else if (mod == 1)
                displacement_size = 1;
        }

        if (displacement_size != 0)
        {
            components++;

            if (count - index < displacement_size)
                return components;

            index += displacement_size;
        }
    }

    u32 immediate_size = (u32)GetEncodedImmediateSize(encoding, opcode, reg, operand_size, address_size);

    if (immediate_size != 0)
    {
        components++;

        if (count - index < immediate_size)
            return components;
    }

    truncated = false;
    return components;
}
