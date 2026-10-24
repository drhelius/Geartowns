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

bool I386::OPCode0F_0x08()
{
    // INVD
    return OPCodes0F_INVD();
}

bool I386::OPCode0F_0x09()
{
    // WBINVD
    return OPCodes0F_INVD();
}

bool I386::OPCode0F_0xB0()
{
    // CMPXCHG r/m8,r8
    return OPCodes0F_CMPXCHG();
}

bool I386::OPCode0F_0xB1()
{
    // CMPXCHG r/m16/32,r16/32
    return OPCodes0F_CMPXCHG();
}

bool I386::OPCode0F_0xC0()
{
    // XADD r/m8,r8
    return OPCodes0F_XADD();
}

bool I386::OPCode0F_0xC1()
{
    // XADD r/m16/32,r16/32
    return OPCodes0F_XADD();
}

bool I386::OPCode0F_0xC8()
{
    // BSWAP EAX
    return OPCodes0F_BSWAP();
}

bool I386::OPCode0F_0xC9()
{
    // BSWAP ECX
    return OPCodes0F_BSWAP();
}

bool I386::OPCode0F_0xCA()
{
    // BSWAP EDX
    return OPCodes0F_BSWAP();
}

bool I386::OPCode0F_0xCB()
{
    // BSWAP EBX
    return OPCodes0F_BSWAP();
}

bool I386::OPCode0F_0xCC()
{
    // BSWAP ESP
    return OPCodes0F_BSWAP();
}

bool I386::OPCode0F_0xCD()
{
    // BSWAP EBP
    return OPCodes0F_BSWAP();
}

bool I386::OPCode0F_0xCE()
{
    // BSWAP ESI
    return OPCodes0F_BSWAP();
}

bool I386::OPCode0F_0xCF()
{
    // BSWAP EDI
    return OPCodes0F_BSWAP();
}

bool I386::OPCodes0F_INVD()
{
    if (!m_i486)
        return OPCodes_Invalid();

    u32 clocks = m_instruction.opcode2 == 0x08 ? 4 : 5;

    if (!DecodeAndStart(false, 0, clocks, clocks))
        return false;

    if (m_state.execution_mode != I386_MODE_REAL && m_state.current_privilege_level != 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_CMPXCHG()
{
    if (!m_i486)
        return OPCodes_Invalid();

    if (!DecodeAndStart(true, 0, 6, 7))
        return false;

    int width = m_instruction.opcode2 == 0xB0 ? 8 : m_instruction.operand_size * 8;
    u32 destination = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, destination))
        return false;

    u32 accumulator = GetRegister(I386_REG_EAX, width);
    bool equal = accumulator == destination;

    if (!WriteRM(m_instruction, width, equal ? GetRegister(m_instruction.reg, width) : destination, *m_bus_context))
        return false;

    Sub(accumulator, destination, 0, width);

    if (!equal)
    {
        SetRegister(I386_REG_EAX, width, destination);

        if (m_instruction.memory_operand)
            m_step.clocks += 3;
    }

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_XADD()
{
    if (!m_i486)
        return OPCodes_Invalid();

    if (!DecodeAndStart(true, 0, 3, 4))
        return false;

    int width = m_instruction.opcode2 == 0xC0 ? 8 : m_instruction.operand_size * 8;
    u32 destination = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, destination))
        return false;

    u32 source = GetRegister(m_instruction.reg, width);
    u32 sum = (destination + source) & GetMask(width);

    if (m_instruction.memory_operand)
    {
        if (!WriteRM(m_instruction, width, sum, *m_bus_context))
            return false;

        SetRegister(m_instruction.reg, width, destination);
    }
    else
    {
        // A register added to itself keeps the sum
        SetRegister(m_instruction.reg, width, destination);
        SetRegister(m_instruction.rm, width, sum);
    }

    Add(destination, source, 0, width);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_BSWAP()
{
    if (!m_i486)
        return OPCodes_Invalid();

    if (!DecodeAndStart(false, 0, 1, 1))
        return false;

    u32_union& reg = m_state.registers[m_instruction.opcode2 & 7];
    u32 value = reg.value;

    if (m_instruction.operand_size == 2)
        reg.low = 0;
    else
        reg.value = (value >> 24) | ((value >> 8) & 0x0000FF00U) | ((value << 8) & 0x00FF0000U) | (value << 24);

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_INVLPG()
{
    if (!m_instruction.memory_operand)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (m_state.execution_mode != I386_MODE_REAL && m_state.current_privilege_level != 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    m_step.clocks = 12 + m_address_clocks;

    if (!InvalidateTLBPage(m_state.segments[m_instruction.segment].base + m_instruction.effective_offset))
        m_step.clocks--;

    CommitEIP(m_instruction);
    return true;
}
