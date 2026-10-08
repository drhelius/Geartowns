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

#include <stdint.h>
#include "i386.h"
#include "i386_opcodes_inline.h"
#include "../system/io.h"

INLINE bool I386::OPCodes_INS(int width, u32 destination_offset)
{
    u16 port = m_state.registers[I386_REG_EDX].low;
    u32 value = width == 8 ? 0xFF : width == 16 ? 0xFFFF : 0xFFFFFFFFU;

    if (IsValidPointer(m_io))
    {
        if (width == 8)
            value = m_io->Read8(port, *m_bus_context);
        else if (width == 16)
            value = m_io->Read16(port, *m_bus_context);
        else
            value = m_io->Read32(port, *m_bus_context);
    }

    if (unlikely(m_debugger_io_checks))
        RecordDebuggerIO(port, value, (u32)width >> 3, false);

    bool ok = WriteMemory(I386_SEGMENT_ES, destination_offset, width, value, *m_bus_context);
    m_bus_context->end_batch = true;
    return ok;
}

INLINE bool I386::OPCodes_OUTS(int width, int source_segment, u32 source_offset)
{
    u32 value = 0;
    bool ok = ReadMemory(source_segment, source_offset, width, *m_bus_context, value);

    if (ok && IsValidPointer(m_io))
    {
        u16 port = m_state.registers[I386_REG_EDX].low;

        if (unlikely(m_debugger_io_checks))
            RecordDebuggerIO(port, value, (u32)width >> 3, true);

        if (width == 8)
            m_io->Write8(port, (u8)value, *m_bus_context);
        else if (width == 16)
            m_io->Write16(port, (u16)value, *m_bus_context);
        else
            m_io->Write32(port, value, *m_bus_context);
    }

    m_bus_context->end_batch = true;
    return ok;
}

INLINE bool I386::OPCodes_MOVS(int width, int source_segment, u32 source_offset, u32 destination_offset)
{
    u32 value = 0;

    if (!ReadMemory(source_segment, source_offset, width, *m_bus_context, value))
        return false;

    return WriteMemory(I386_SEGMENT_ES, destination_offset, width, value, *m_bus_context);
}

INLINE bool I386::OPCodes_CMPS(int width, int source_segment, u32 source_offset, u32 destination_offset)
{
    u32 source_value = 0;
    u32 destination_value = 0;

    if (!ReadMemory(source_segment, source_offset, width, *m_bus_context, source_value))
        return false;

    if (!ReadMemory(I386_SEGMENT_ES, destination_offset, width, *m_bus_context, destination_value))
        return false;

    Sub(source_value, destination_value, 0, width);
    return true;
}

INLINE bool I386::OPCodes_STOS(int width, u32 destination_offset)
{
    return WriteMemory(I386_SEGMENT_ES, destination_offset, width, GetRegister(I386_REG_EAX, width), *m_bus_context);
}

INLINE bool I386::OPCodes_LODS(int width, int source_segment, u32 source_offset)
{
    u32 value = 0;

    if (!ReadMemory(source_segment, source_offset, width, *m_bus_context, value))
        return false;

    SetRegister(I386_REG_EAX, width, value);
    return true;
}

INLINE bool I386::OPCodes_SCAS(int width, u32 destination_offset)
{
    u32 value = 0;

    if (!ReadMemory(I386_SEGMENT_ES, destination_offset, width, *m_bus_context, value))
        return false;

    Sub(GetRegister(I386_REG_EAX, width), value, 0, width);
    return true;
}

bool I386::OPCodes_String()
{
    if (m_instruction.opcode >= 0x6C && m_instruction.opcode <= 0x6F && DeferIO())
        return true;

    if (!DecodeOperands(false, 0))
        return false;

    PrepareString();

    if (!StartExecution(GetStringClocks(false)))
        return false;

    return ContinueRepeat();
}

bool I386::ContinueRepeat()
{
    bool repeat = false;

    if (!ExecuteStringElement(repeat))
        return false;

    if (repeat)
    {
        if (!m_state.repeat.active)
        {
            m_state.repeat.start_eip = m_instruction.start_eip;
            m_state.repeat.next_eip = m_instruction.next_eip;
            m_state.repeat.opcode = m_instruction.opcode;
            m_state.repeat.operand_size = m_instruction.operand_size;
            m_state.repeat.address_size = m_instruction.address_size;
            m_state.repeat.repeat = m_instruction.repeat;
            m_state.repeat.segment_override = m_instruction.segment_override;
            m_state.repeat.active = true;
        }

        m_state.eip = m_instruction.start_eip;
        m_step.instruction_completed = false;
    }
    else
    {
        m_state.repeat.active = false;
        CommitEIP(m_instruction);
        m_step.instruction_completed = true;
    }

    return true;
}

bool I386::ExecuteStringElement(bool& repeat)
{
    u8 opcode = m_instruction.opcode;
    int width = m_string.width;
    int address_width = m_string.address_width;
    bool prefixed = m_string.repeated;
    u32 count = GetRegister(I386_REG_ECX, address_width);

    if (prefixed && count == 0)
    {
        repeat = false;
        return true;
    }

    u32 source_offset = GetRegister(I386_REG_ESI, address_width);
    u32 destination_offset = GetRegister(I386_REG_EDI, address_width);
    int source_segment = m_string.source_segment;
    bool ok = true;

    if (opcode >= 0x6C && opcode <= 0x6F)
    {
        bool allowed = false;

        if (!CheckIOPermission(m_state.registers[I386_REG_EDX].low, width, *m_bus_context, allowed))
            return false;

        if (!allowed)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);
    }

    switch (opcode)
    {
        case 0x6C:
        case 0x6D:
            // INS m8,DX / INS m16/32,DX
            ok = OPCodes_INS(width, destination_offset);
            break;
        case 0x6E:
        case 0x6F:
            // OUTS DX,m8 / OUTS DX,m16/32
            ok = OPCodes_OUTS(width, source_segment, source_offset);
            break;
        case 0xA4:
        case 0xA5:
            // MOVS m8,m8 / MOVS m16/32,m16/32
            ok = OPCodes_MOVS(width, source_segment, source_offset, destination_offset);
            break;
        case 0xA6:
        case 0xA7:
            // CMPS m8,m8 / CMPS m16/32,m16/32
            ok = OPCodes_CMPS(width, source_segment, source_offset, destination_offset);
            break;
        case 0xAA:
        case 0xAB:
            // STOS m8 / STOS m16/32
            ok = OPCodes_STOS(width, destination_offset);
            break;
        case 0xAC:
        case 0xAD:
            // LODS m8 / LODS m16/32
            ok = OPCodes_LODS(width, source_segment, source_offset);
            break;
        case 0xAE:
        case 0xAF:
            // SCAS m8 / SCAS m16/32
            ok = OPCodes_SCAS(width, destination_offset);
            break;
        default:
            return RaiseException(6, I386_EXCEPTION_FAULT);
    }

    if (!ok)
        return false;

    u32 delta = (u32)width >> 3;

    if ((m_state.eflags & I386_FLAG_DF) != 0)
        delta = 0 - delta;

    if ((m_string.index_mask & 1) != 0)
        SetRegister(I386_REG_ESI, address_width, source_offset + delta);

    if ((m_string.index_mask & 2) != 0)
        SetRegister(I386_REG_EDI, address_width, destination_offset + delta);

    if (!prefixed)
    {
        repeat = false;
        return true;
    }

    count = Truncate(count - 1, address_width);
    SetRegister(I386_REG_ECX, address_width, count);

    repeat = count != 0;

    if (repeat && (opcode == 0xA6 || opcode == 0xA7 || opcode == 0xAE || opcode == 0xAF))
    {
        bool zf = (m_state.eflags & I386_FLAG_ZF) != 0;
        repeat = m_instruction.repeat == 3 ? zf : !zf;
    }

    return true;
}

u32 I386::RunRepeatBatch(u32 budget, u32& clocks)
{
    u8 opcode = m_state.repeat.opcode;
    bool move = opcode == 0xA4 || opcode == 0xA5;

    if ((!move && opcode != 0xAA && opcode != 0xAB) || (m_state.cr0 & 0x80000000U) != 0 || m_trace_enabled ||
        (m_state.debug_registers[7] & 0xFF) != 0 ||
        (m_state.eflags & I386_FLAG_TF) != 0 || IsValidPointer(m_bus_context->observe_memory_write) ||
        m_bus_context->end_batch)
        return 0;

    int address_width = m_state.repeat.address_size * 8;
    u32 count = GetRegister(I386_REG_ECX, address_width);

    if (count == 0 || budget == 0)
        return 0;

    u32 size = (opcode & 1) != 0 ? m_state.repeat.operand_size : 1;
    u32 source = GetRegister(I386_REG_ESI, address_width);
    u32 destination = GetRegister(I386_REG_EDI, address_width);
    int segment = m_state.repeat.segment_override == 0xFF ? I386_SEGMENT_DS : m_state.repeat.segment_override;
    s64 destination_end = (s64)((u64)destination + size - 1);

    if (destination_end > m_write_limits[I386_SEGMENT_ES])
        return 0;

    if (move && (s64)((u64)source + size - 1) > m_read_limits[segment])
        return 0;

    u32 destination_linear = m_state.segments[I386_SEGMENT_ES].base + destination;
    u8* output = GetWriteHost(destination_linear, size);

    if (!IsValidPointer(output))
        return 0;

    u32 source_linear = m_state.segments[segment].base + source;
    const u8* input = move ? GetReadHost(source_linear, size) : NULL;

    if (move && !IsValidPointer(input))
        return 0;

    u32 iteration_clocks = move ? 4 : 5;
    u32 elements = (u32)MIN((u64)count, ((u64)budget + iteration_clocks - 1) / iteration_clocks);
    u32 address_mask = address_width == 16 ? 0xFFFFU : 0xFFFFFFFFU;
    bool reverse = (m_state.eflags & I386_FLAG_DF) != 0;

    if (reverse)
    {
        elements = MIN(elements, (destination_linear & 0xFFF) / size + 1);
        elements = (u32)MIN((u64)elements, (u64)destination / size + 1);

        if (move)
        {
            elements = MIN(elements, (source_linear & 0xFFF) / size + 1);
            elements = (u32)MIN((u64)elements, (u64)source / size + 1);
        }
    }
    else
    {
        elements = MIN(elements, (0x1000 - (destination_linear & 0xFFF)) / size);
        elements = (u32)MIN((u64)elements, ((u64)address_mask - destination + 1) / size);
        elements = (u32)MIN((u64)elements, ((u64)m_write_limits[I386_SEGMENT_ES] - destination + 1) / size);

        if (move)
        {
            elements = MIN(elements, (0x1000 - (source_linear & 0xFFF)) / size);
            elements = (u32)MIN((u64)elements, ((u64)address_mask - source + 1) / size);
            elements = (u32)MIN((u64)elements, ((u64)m_read_limits[segment] - source + 1) / size);
        }
    }

    if (elements == 0)
        return 0;

    u32 bytes = elements * size;

    if (move)
    {
        const u8* input_start = reverse ? input - (elements - 1) * size : input;
        u8* output_start = reverse ? output - (elements - 1) * size : output;
        uintptr_t input_address = (uintptr_t)input_start;
        uintptr_t output_address = (uintptr_t)output_start;
        uintptr_t distance = input_address < output_address ?
            output_address - input_address : input_address - output_address;

        if (distance >= bytes)
            memcpy(output_start, input_start, bytes);
        else
        {
            for (u32 i = 0; i < elements; i++)
            {
                // Read the complete element before writing, even for misaligned overlap
                u32 value = LoadHost(input, size * 8);

                StoreHost(output, value, size * 8);

                if (i + 1 < elements)
                {
                    input += reverse ? -(s32)size : (s32)size;
                    output += reverse ? -(s32)size : (s32)size;
                }
            }
        }
    }
    else
    {
        u32 value = m_state.registers[I386_REG_EAX].value;
        u8* start = reverse ? output - (elements - 1) * size : output;

        if (size == 1)
            memset(start, (u8)value, bytes);
        else
        {
            for (u32 i = 0; i < bytes; i += size)
                StoreHost(start + i, value, size * 8);
        }
    }

    u32 delta = reverse ? 0U - bytes : bytes;

    if (move)
        SetRegister(I386_REG_ESI, address_width, source + delta);

    SetRegister(I386_REG_EDI, address_width, destination + delta);
    SetRegister(I386_REG_ECX, address_width, count - elements);

    m_state.repeat.active = count != elements;
    m_state.eip = m_state.repeat.active ? m_state.repeat.start_eip : m_state.repeat.next_eip;

    // A REP restarted after an interrupt keeps RF until it completes, as on the slow path
    if (!m_state.repeat.active)
        m_state.eflags &= ~I386_FLAG_RF;

    m_step.clocks = iteration_clocks;
    m_step.steps = 1;
    m_step.instruction_completed = !m_state.repeat.active;
    m_step.end_batch = false;
    m_step.exception = false;

    clocks = elements * iteration_clocks;
    return elements;
}

void I386::PrepareString()
{
    u8 opcode = m_instruction.opcode;

    m_string.width = (opcode & 1) != 0 ? m_instruction.operand_size * 8 : 8;
    m_string.address_width = m_instruction.address_size * 8;
    m_string.source_segment = m_instruction.segment_override == 0xFF ? I386_SEGMENT_DS : m_instruction.segment_override;
    m_string.repeated = m_instruction.repeat != 0;
    m_string.index_mask = 0;

    // ESI
    if (opcode == 0x6E || opcode == 0x6F || (opcode >= 0xA4 && opcode <= 0xA7) || opcode == 0xAC || opcode == 0xAD)
        m_string.index_mask |= 1;

    // EDI
    if (opcode == 0x6C || opcode == 0x6D || (opcode >= 0xA4 && opcode <= 0xA7) || opcode == 0xAA || opcode == 0xAB ||
        opcode == 0xAE || opcode == 0xAF)
        m_string.index_mask |= 2;

    switch (opcode & 0xFE)
    {
        case 0xA4:
            m_string.iteration_clocks = 4;
            break;
        case 0xA6:
            m_string.iteration_clocks = 9;
            break;
        case 0xAA:
            m_string.iteration_clocks = 5;
            break;
        case 0xAC:
            m_string.iteration_clocks = 6;
            break;
        case 0xAE:
            m_string.iteration_clocks = 8;
            break;
        default:
            m_string.iteration_clocks = 0;
            break;
    }
}

u32 I386::GetStringClocks(bool continuation) const
{
    u8 opcode = m_instruction.opcode;

    if (opcode >= 0xA4)
    {
        if (m_string.repeated)
        {
            u32 clocks = continuation ? 0 : (opcode <= 0xA5 ? 7 : 5);

            if (GetRegister(I386_REG_ECX, m_string.address_width) != 0)
                clocks += m_string.iteration_clocks;

            return clocks;
        }

        switch (opcode & 0xFE)
        {
            case 0xA4:
                return 7;
            case 0xA6:
                return 10;
            case 0xAA:
                return 4;
            case 0xAC:
                return 5;
            default:
                return 7;
        }
    }

    bool input = opcode <= 0x6D;
    bool real = m_state.execution_mode == I386_MODE_REAL;
    bool permission_check =
        !real && (m_state.execution_mode == I386_MODE_VM86 || m_state.current_privilege_level > GetIOPrivilegeLevel());

    if (!m_string.repeated)
        return real ? (input ? 15 : 14) : permission_check ? (input ? 29 : 28) : (input ? 9 : 8);

    u32 overhead = input ? (real ? 13 : permission_check ? 27 : 7) : (real ? 5 : permission_check ? 26 : 6);
    u32 clocks = continuation ? 0 : overhead;

    if (GetRegister(I386_REG_ECX, m_string.address_width) != 0)
        clocks += input ? 6 : real ? 12 : 5;

    return clocks;
}
