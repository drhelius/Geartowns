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

// The 486's checked step, out of the 386's way
NO_INLINE void I386::RunChecked486Step()
{
    RunCheckedStep<true>();
}

NO_INLINE u32 I386::RunInstruction486(GT_Bus_Access_Context& context)
{
    bool timed = m_bus_timed;
    m_io_start_cycles = 0;
    SetBusContext(context);
    RunCheckedStep<true>();

    if (timed)
        m_step.clocks += CarryBusTime();

    return (u32)m_step.clocks;
}

void I386::Capture486Step()
{
    m_486_continuation = m_state.repeat.active;
    m_486_protected = m_state.execution_mode == I386_MODE_PROTECTED;
    m_486_extra = 0;

    for (int i = 0; i < I386_REG_COUNT; i++)
        m_486_registers[i] = m_state.registers[i].value;
}

// Form kinds: memory or register form clocks, a group by ModR/M reg, real and VM86 or protected mode clocks, real
// and VM86 clocks or the handler's protected mode path, not taken or taken, SETcc, a string instruction, data
// dependent, DIV and IDIV by width, LEA, the handler's clocks, a port instruction and the software interrupts
enum I486_Form_Kind
{
    I486_FIXED = 0,
    I486_GROUP,
    I486_MODE,
    I486_FAR,
    I486_JUMP,
    I486_SETCC,
    I486_STRING,
    I486_DATA,
    I486_DIVIDE,
    I486_LEA,
    I486_KEEP,
    I486_PORT,
    I486_INTERRUPT
};

// The registers an instruction writes: always, the ModR/M reg or register r/m, the opcode's register, ECX when
// repeated, AX for FNSTSW AX. Byte forms name AL to BL by the low two bits. Stack and XLAT address through ESP and EBX
static const u8 k_i486_reg = 0x01;
static const u8 k_i486_rm = 0x02;
static const u8 k_i486_opcode = 0x04;
static const u8 k_i486_count = 0x08;
static const u8 k_i486_byte = 0x10;
static const u8 k_i486_stack = 0x20;
static const u8 k_i486_xlat = 0x40;
static const u8 k_i486_status = 0x80;

static const u8 k_eax = 1 << I386_REG_EAX;
static const u8 k_ecx = 1 << I386_REG_ECX;
static const u8 k_edx = 1 << I386_REG_EDX;
static const u8 k_esp = 1 << I386_REG_ESP;
static const u8 k_ebp = 1 << I386_REG_EBP;
static const u8 k_esi = 1 << I386_REG_ESI;
static const u8 k_edi = 1 << I386_REG_EDI;

enum I486_Group
{
    I486_GROUP_1 = 0,
    I486_GROUP_SHIFT_IMMEDIATE,
    I486_GROUP_SHIFT_1,
    I486_GROUP_SHIFT_CL,
    I486_GROUP_3_BYTE,
    I486_GROUP_3,
    I486_GROUP_5,
    I486_GROUP_POP,
    I486_GROUP_MOVE,
    I486_GROUP_0F00,
    I486_GROUP_0F01,
    I486_GROUP_0FBA,
    I486_GROUP_CONTROL,
    I486_GROUP_TEST,
    I486_GROUP_COUNT
};

struct I486_Form
{
    u8 kind;
    u8 reg;
    u8 memory;
    u8 fixed;
    u8 flags;
};

// PRM486 Table 10.1, cache hit, by one-byte and 0F opcode, and by ModR/M reg in the groups. Filled once
static I486_Form k_i486_forms[512];
static I486_Form k_i486_groups[I486_GROUP_COUNT][8];

static void SetForm(I486_Form& form, u8 kind, u8 reg, u8 memory, u8 fixed, u8 flags)
{
    form.kind = kind;
    form.reg = reg;
    form.memory = memory;
    form.fixed = fixed;
    form.flags = flags;
}

static void SetForms(int first, int last, u8 kind, u8 reg, u8 memory, u8 fixed, u8 flags)
{
    for (int i = first; i <= last; i++)
        SetForm(k_i486_forms[i], kind, reg, memory, fixed, flags);
}

static void SetGroup(int group, int first, int last, u8 kind, u8 reg, u8 memory, u8 fixed, u8 flags)
{
    for (int i = first; i <= last; i++)
        SetForm(k_i486_groups[group][i], kind, reg, memory, fixed, flags);
}

void I386::Init486Forms()
{
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;
    SetForms(0, 511, I486_KEEP, 0, 0, 0, 0);

    for (int i = 0; i < I486_GROUP_COUNT; i++)
        SetGroup(i, 0, 7, I486_KEEP, 0, 0, 0, 0);

    // ADD, OR, ADC, SBB, AND, SUB, XOR and CMP
    for (int operation = 0; operation < 8; operation++)
    {
        int base = operation * 8;
        bool compare = operation == 7;
        u8 write_rm = compare ? 0 : k_i486_rm;
        u8 write_reg = compare ? 0 : k_i486_reg;
        u8 eax = compare ? 0 : k_eax;

        SetForms(base, base, I486_FIXED, 1, compare ? 2 : 3, 0, write_rm | k_i486_byte);
        SetForms(base + 1, base + 1, I486_FIXED, 1, compare ? 2 : 3, 0, write_rm);
        SetForms(base + 2, base + 2, I486_FIXED, 1, 2, 0, write_reg | k_i486_byte);
        SetForms(base + 3, base + 3, I486_FIXED, 1, 2, 0, write_reg);
        SetForms(base + 4, base + 5, I486_FIXED, 1, 1, eax, 0);
    }

    // PUSH and POP segment, DAA, DAS, AAA and AAS
    SetForms(0x06, 0x06, I486_FIXED, 3, 3, 0, k_i486_stack);
    SetForms(0x0E, 0x0E, I486_FIXED, 3, 3, 0, k_i486_stack);
    SetForms(0x16, 0x16, I486_FIXED, 3, 3, 0, k_i486_stack);
    SetForms(0x1E, 0x1E, I486_FIXED, 3, 3, 0, k_i486_stack);
    SetForms(0x07, 0x07, I486_MODE, 3, 9, 0, k_i486_stack);
    SetForms(0x17, 0x17, I486_MODE, 3, 9, 0, k_i486_stack);
    SetForms(0x1F, 0x1F, I486_MODE, 3, 9, 0, k_i486_stack);
    SetForms(0x27, 0x27, I486_FIXED, 2, 2, k_eax, 0);
    SetForms(0x2F, 0x2F, I486_FIXED, 2, 2, k_eax, 0);
    SetForms(0x37, 0x37, I486_FIXED, 3, 3, k_eax, 0);
    SetForms(0x3F, 0x3F, I486_FIXED, 3, 3, k_eax, 0);

    // INC, DEC, PUSH and POP r16/32, PUSHA, POPA, BOUND, ARPL, PUSH imm, IMUL imm, INS and OUTS, Jcc rel8
    SetForms(0x40, 0x4F, I486_FIXED, 1, 1, 0, k_i486_opcode);
    SetForms(0x50, 0x57, I486_FIXED, 1, 1, 0, k_i486_stack);
    SetForms(0x58, 0x5F, I486_FIXED, 1, 1, 0, k_i486_opcode | k_i486_stack);
    SetForms(0x60, 0x60, I486_FIXED, 11, 11, 0, k_i486_stack);
    SetForms(0x61, 0x61, I486_FIXED, 9, 9, (u8)~k_esp, k_i486_stack);
    SetForms(0x62, 0x62, I486_FIXED, 7, 7, 0, 0);
    SetForms(0x63, 0x63, I486_FIXED, 9, 9, 0, k_i486_rm);
    SetForms(0x68, 0x68, I486_FIXED, 1, 1, 0, k_i486_stack);
    SetForms(0x6A, 0x6A, I486_FIXED, 1, 1, 0, k_i486_stack);
    SetForms(0x69, 0x69, I486_DATA, 0, 0, 0, k_i486_reg);
    SetForms(0x6B, 0x6B, I486_DATA, 0, 0, 0, k_i486_reg);
    SetForms(0x6C, 0x6D, I486_PORT, 0, 0, k_edi, k_i486_count);
    SetForms(0x6E, 0x6F, I486_PORT, 0, 0, k_esi, k_i486_count);
    SetForms(0x70, 0x7F, I486_JUMP, 1, 3, 0, 0);

    // Group 1, TEST, XCHG, MOV, LEA, MOV segment and POP r/m
    SetForms(0x80, 0x80, I486_GROUP, I486_GROUP_1, 0, 0, k_i486_byte);
    SetForms(0x81, 0x81, I486_GROUP, I486_GROUP_1, 0, 0, 0);
    SetForms(0x82, 0x82, I486_GROUP, I486_GROUP_1, 0, 0, k_i486_byte);
    SetForms(0x83, 0x83, I486_GROUP, I486_GROUP_1, 0, 0, 0);
    SetForms(0x84, 0x85, I486_FIXED, 1, 2, 0, 0);
    SetForms(0x86, 0x86, I486_FIXED, 3, 5, 0, k_i486_reg | k_i486_rm | k_i486_byte);
    SetForms(0x87, 0x87, I486_FIXED, 3, 5, 0, k_i486_reg | k_i486_rm);
    SetForms(0x88, 0x88, I486_FIXED, 1, 1, 0, k_i486_rm | k_i486_byte);
    SetForms(0x89, 0x89, I486_FIXED, 1, 1, 0, k_i486_rm);
    SetForms(0x8A, 0x8A, I486_FIXED, 1, 1, 0, k_i486_reg | k_i486_byte);
    SetForms(0x8B, 0x8B, I486_FIXED, 1, 1, 0, k_i486_reg);
    SetForms(0x8C, 0x8C, I486_FIXED, 3, 3, 0, k_i486_rm);
    SetForms(0x8D, 0x8D, I486_LEA, 0, 0, 0, k_i486_reg);
    SetForms(0x8E, 0x8E, I486_MODE, 3, 9, 0, 0);
    SetForms(0x8F, 0x8F, I486_GROUP, I486_GROUP_POP, 0, 0, 0);

    // NOP, XCHG eAX, CBW, CWD, CALL far, WAIT, PUSHF, POPF, SAHF, LAHF, MOV moffs and TEST eAX,imm
    SetForms(0x90, 0x90, I486_FIXED, 1, 1, 0, 0);
    SetForms(0x91, 0x97, I486_FIXED, 3, 3, k_eax, k_i486_opcode);
    SetForms(0x98, 0x98, I486_FIXED, 3, 3, k_eax, 0);
    SetForms(0x99, 0x99, I486_FIXED, 3, 3, k_edx, 0);
    SetForms(0x9A, 0x9A, I486_FAR, 18, 18, 0, 0);
    SetForms(0x9B, 0x9B, I486_FIXED, 1, 1, 0, 0);
    SetForms(0x9C, 0x9C, I486_MODE, 4, 3, 0, k_i486_stack);
    SetForms(0x9D, 0x9D, I486_MODE, 9, 6, 0, k_i486_stack);
    SetForms(0x9E, 0x9E, I486_FIXED, 2, 2, 0, 0);
    SetForms(0x9F, 0x9F, I486_FIXED, 3, 3, k_eax, 0);
    SetForms(0xA0, 0xA1, I486_FIXED, 1, 1, k_eax, 0);
    SetForms(0xA2, 0xA3, I486_FIXED, 1, 1, 0, 0);
    SetForms(0xA8, 0xA9, I486_FIXED, 1, 1, 0, 0);

    // MOVS, CMPS, STOS, LODS and SCAS
    SetForms(0xA4, 0xA7, I486_STRING, 0, 0, k_esi | k_edi, k_i486_count);
    SetForms(0xAA, 0xAB, I486_STRING, 0, 0, k_edi, k_i486_count);
    SetForms(0xAC, 0xAD, I486_STRING, 0, 0, k_eax | k_esi, k_i486_count);
    SetForms(0xAE, 0xAF, I486_STRING, 0, 0, k_edi, k_i486_count);

    // MOV r,imm, shifts, RET, LES, LDS, MOV r/m,imm, ENTER, LEAVE, RET far, INT, INTO and IRET
    SetForms(0xB0, 0xB7, I486_FIXED, 1, 1, 0, k_i486_opcode | k_i486_byte);
    SetForms(0xB8, 0xBF, I486_FIXED, 1, 1, 0, k_i486_opcode);
    SetForms(0xC0, 0xC0, I486_GROUP, I486_GROUP_SHIFT_IMMEDIATE, 0, 0, k_i486_byte);
    SetForms(0xC1, 0xC1, I486_GROUP, I486_GROUP_SHIFT_IMMEDIATE, 0, 0, 0);
    SetForms(0xC2, 0xC3, I486_FIXED, 5, 5, 0, k_i486_stack);
    SetForms(0xC4, 0xC5, I486_MODE, 6, 12, 0, k_i486_reg);
    SetForms(0xC6, 0xC6, I486_GROUP, I486_GROUP_MOVE, 0, 0, k_i486_byte);
    SetForms(0xC7, 0xC7, I486_GROUP, I486_GROUP_MOVE, 0, 0, 0);
    SetForms(0xC8, 0xC8, I486_DATA, 0, 0, k_ebp | k_esp, 0);
    SetForms(0xC9, 0xC9, I486_FIXED, 5, 5, k_ebp | k_esp, 0);
    SetForms(0xCA, 0xCA, I486_FAR, 14, 14, 0, 0);
    SetForms(0xCB, 0xCB, I486_FAR, 13, 13, 0, 0);
    SetForms(0xCC, 0xCE, I486_INTERRUPT, 3, 3, 0, 0);
    SetForms(0xCF, 0xCF, I486_FAR, 15, 15, 0, 0);
    SetForms(0xD0, 0xD0, I486_GROUP, I486_GROUP_SHIFT_1, 0, 0, k_i486_byte);
    SetForms(0xD1, 0xD1, I486_GROUP, I486_GROUP_SHIFT_1, 0, 0, 0);
    SetForms(0xD2, 0xD2, I486_GROUP, I486_GROUP_SHIFT_CL, 0, 0, k_i486_byte);
    SetForms(0xD3, 0xD3, I486_GROUP, I486_GROUP_SHIFT_CL, 0, 0, 0);

    // AAM, AAD, SALC, XLAT, FNSTSW AX, LOOP, JCXZ, ports, CALL and JMP
    SetForms(0xD4, 0xD4, I486_FIXED, 15, 15, k_eax, 0);
    SetForms(0xD5, 0xD5, I486_FIXED, 14, 14, k_eax, 0);
    SetForms(0xD6, 0xD6, I486_KEEP, 0, 0, k_eax, 0);
    SetForms(0xD7, 0xD7, I486_FIXED, 4, 4, k_eax, k_i486_xlat);
    SetForms(0xDF, 0xDF, I486_KEEP, 0, 0, 0, k_i486_status);
    SetForms(0xE0, 0xE1, I486_JUMP, 6, 9, k_ecx, 0);
    SetForms(0xE2, 0xE2, I486_JUMP, 6, 7, k_ecx, 0);
    SetForms(0xE3, 0xE3, I486_JUMP, 5, 8, 0, 0);
    SetForms(0xE4, 0xE5, I486_PORT, 0, 0, k_eax, 0);
    SetForms(0xE6, 0xE7, I486_PORT, 0, 0, 0, 0);
    SetForms(0xE8, 0xE8, I486_FIXED, 3, 3, 0, k_i486_stack);
    SetForms(0xE9, 0xE9, I486_FIXED, 3, 3, 0, 0);
    SetForms(0xEA, 0xEA, I486_FAR, 17, 17, 0, 0);
    SetForms(0xEB, 0xEB, I486_FIXED, 3, 3, 0, 0);
    SetForms(0xEC, 0xED, I486_PORT, 0, 0, k_eax, 0);
    SetForms(0xEE, 0xEF, I486_PORT, 0, 0, 0, 0);

    // HLT, CMC, groups 3, 4 and 5 and the flag instructions
    SetForms(0xF4, 0xF4, I486_FIXED, 4, 4, 0, 0);
    SetForms(0xF5, 0xF5, I486_FIXED, 2, 2, 0, 0);
    SetForms(0xF6, 0xF6, I486_GROUP, I486_GROUP_3_BYTE, 0, 0, k_i486_byte);
    SetForms(0xF7, 0xF7, I486_GROUP, I486_GROUP_3, 0, 0, 0);
    SetForms(0xF8, 0xF9, I486_FIXED, 2, 2, 0, 0);
    SetForms(0xFA, 0xFB, I486_FIXED, 5, 5, 0, 0);
    SetForms(0xFC, 0xFD, I486_FIXED, 2, 2, 0, 0);
    SetForms(0xFE, 0xFE, I486_GROUP, I486_GROUP_5, 0, 0, k_i486_byte);
    SetForms(0xFF, 0xFF, I486_GROUP, I486_GROUP_5, 0, 0, 0);

    // 0F: groups 6 and 7, LAR, LSL, CLTS, MOV to and from CR, DR and TR
    SetForms(0x100, 0x100, I486_GROUP, I486_GROUP_0F00, 0, 0, 0);
    SetForms(0x101, 0x101, I486_GROUP, I486_GROUP_0F01, 0, 0, 0);
    SetForms(0x102, 0x102, I486_FIXED, 11, 11, 0, k_i486_reg);
    SetForms(0x103, 0x103, I486_FIXED, 10, 10, 0, k_i486_reg);
    SetForms(0x106, 0x106, I486_FIXED, 7, 7, 0, 0);
    SetForms(0x120, 0x120, I486_FIXED, 4, 4, 0, k_i486_rm);
    SetForms(0x121, 0x121, I486_FIXED, 9, 9, 0, k_i486_rm);
    SetForms(0x122, 0x122, I486_GROUP, I486_GROUP_CONTROL, 0, 0, 0);
    SetForms(0x123, 0x123, I486_FIXED, 10, 10, 0, 0);
    SetForms(0x124, 0x124, I486_GROUP, I486_GROUP_TEST, 0, 0, 0);
    SetForms(0x126, 0x126, I486_FIXED, 4, 4, 0, 0);

    // Jcc, SETcc, PUSH and POP FS and GS, the bit instructions, SHLD, SHRD, IMUL, CMPXCHG, LSS, LFS, LGS, MOVZX,
    // MOVSX, BSF, BSR, XADD and BSWAP
    SetForms(0x180, 0x18F, I486_JUMP, 1, 3, 0, 0);
    SetForms(0x190, 0x19F, I486_SETCC, 0, 0, 0, k_i486_rm | k_i486_byte);
    SetForms(0x1A0, 0x1A0, I486_FIXED, 3, 3, 0, k_i486_stack);
    SetForms(0x1A8, 0x1A8, I486_FIXED, 3, 3, 0, k_i486_stack);
    SetForms(0x1A1, 0x1A1, I486_MODE, 3, 9, 0, k_i486_stack);
    SetForms(0x1A9, 0x1A9, I486_MODE, 3, 9, 0, k_i486_stack);
    SetForms(0x1A3, 0x1A3, I486_FIXED, 3, 8, 0, 0);
    SetForms(0x1AB, 0x1AB, I486_FIXED, 6, 13, 0, k_i486_rm);
    SetForms(0x1B3, 0x1B3, I486_FIXED, 6, 13, 0, k_i486_rm);
    SetForms(0x1BB, 0x1BB, I486_FIXED, 6, 13, 0, k_i486_rm);
    SetForms(0x1A4, 0x1A4, I486_FIXED, 2, 3, 0, k_i486_rm);
    SetForms(0x1AC, 0x1AC, I486_FIXED, 2, 3, 0, k_i486_rm);
    SetForms(0x1A5, 0x1A5, I486_FIXED, 3, 4, 0, k_i486_rm);
    SetForms(0x1AD, 0x1AD, I486_FIXED, 3, 4, 0, k_i486_rm);
    SetForms(0x1AF, 0x1AF, I486_DATA, 0, 0, 0, k_i486_reg);
    SetForms(0x1B0, 0x1B0, I486_KEEP, 0, 0, k_eax, k_i486_rm | k_i486_byte);
    SetForms(0x1B1, 0x1B1, I486_KEEP, 0, 0, k_eax, k_i486_rm);
    SetForms(0x1B2, 0x1B2, I486_MODE, 6, 12, 0, k_i486_reg);
    SetForms(0x1B4, 0x1B5, I486_MODE, 6, 12, 0, k_i486_reg);
    SetForms(0x1B6, 0x1B7, I486_FIXED, 3, 3, 0, k_i486_reg);
    SetForms(0x1BA, 0x1BA, I486_GROUP, I486_GROUP_0FBA, 0, 0, 0);
    SetForms(0x1BC, 0x1BD, I486_DATA, 0, 0, 0, k_i486_reg);
    SetForms(0x1BE, 0x1BF, I486_FIXED, 3, 3, 0, k_i486_reg);
    SetForms(0x1C0, 0x1C0, I486_FIXED, 3, 4, 0, k_i486_reg | k_i486_rm | k_i486_byte);
    SetForms(0x1C1, 0x1C1, I486_FIXED, 3, 4, 0, k_i486_reg | k_i486_rm);
    SetForms(0x1C8, 0x1CF, I486_FIXED, 1, 1, 0, k_i486_opcode);

    // Group 1: CMP only reads
    SetGroup(I486_GROUP_1, 0, 6, I486_FIXED, 1, 3, 0, k_i486_rm);
    SetGroup(I486_GROUP_1, 7, 7, I486_FIXED, 1, 2, 0, 0);

    // Rotates and shifts. RCL and RCR by a count depend on it
    SetGroup(I486_GROUP_SHIFT_IMMEDIATE, 0, 7, I486_FIXED, 2, 4, 0, k_i486_rm);
    SetGroup(I486_GROUP_SHIFT_IMMEDIATE, 2, 3, I486_DATA, 0, 0, 0, k_i486_rm);
    SetGroup(I486_GROUP_SHIFT_1, 0, 7, I486_FIXED, 3, 4, 0, k_i486_rm);
    SetGroup(I486_GROUP_SHIFT_CL, 0, 7, I486_FIXED, 3, 4, 0, k_i486_rm);
    SetGroup(I486_GROUP_SHIFT_CL, 2, 3, I486_DATA, 0, 0, 0, k_i486_rm);

    // Group 3: TEST, NOT, NEG, the multiplies by the multiplier and DIV and IDIV by the width
    for (int byte = 0; byte < 2; byte++)
    {
        int group = byte != 0 ? I486_GROUP_3_BYTE : I486_GROUP_3;
        u8 result = byte != 0 ? k_eax : k_eax | k_edx;

        SetGroup(group, 0, 1, I486_FIXED, 1, 2, 0, 0);
        SetGroup(group, 2, 3, I486_FIXED, 1, 3, 0, k_i486_rm);
        SetGroup(group, 4, 5, I486_DATA, 0, 0, result, 0);
        SetGroup(group, 6, 6, I486_DIVIDE, 0, 0, result, 0);
        SetGroup(group, 7, 7, I486_DIVIDE, 3, 4, result, 0);
    }

    // Groups 4 and 5: INC, DEC, near and far CALL and JMP and PUSH
    SetGroup(I486_GROUP_5, 0, 1, I486_FIXED, 1, 3, 0, k_i486_rm);
    SetGroup(I486_GROUP_5, 2, 2, I486_FIXED, 5, 5, 0, k_i486_stack);
    SetGroup(I486_GROUP_5, 3, 3, I486_FAR, 17, 17, 0, 0);
    SetGroup(I486_GROUP_5, 4, 4, I486_FIXED, 5, 5, 0, 0);
    SetGroup(I486_GROUP_5, 5, 5, I486_FAR, 13, 13, 0, 0);
    SetGroup(I486_GROUP_5, 6, 6, I486_FIXED, 4, 4, 0, k_i486_stack);
    SetGroup(I486_GROUP_POP, 0, 0, I486_FIXED, 4, 5, 0, k_i486_rm | k_i486_stack);
    SetGroup(I486_GROUP_MOVE, 0, 0, I486_FIXED, 1, 1, 0, k_i486_rm);

    // Groups 6, 7 and 8: SLDT, STR, LLDT, LTR, VERR, VERW, SGDT, SIDT, LGDT, LIDT, SMSW, LMSW, BT, BTS, BTR and BTC
    SetGroup(I486_GROUP_0F00, 0, 1, I486_FIXED, 2, 3, 0, k_i486_rm);
    SetGroup(I486_GROUP_0F00, 2, 2, I486_FIXED, 11, 11, 0, 0);
    SetGroup(I486_GROUP_0F00, 3, 3, I486_FIXED, 20, 20, 0, 0);
    SetGroup(I486_GROUP_0F00, 4, 5, I486_FIXED, 11, 11, 0, 0);
    SetGroup(I486_GROUP_0F01, 0, 1, I486_FIXED, 10, 10, 0, 0);
    SetGroup(I486_GROUP_0F01, 2, 3, I486_FIXED, 12, 12, 0, 0);
    SetGroup(I486_GROUP_0F01, 4, 4, I486_FIXED, 2, 3, 0, k_i486_rm);
    SetGroup(I486_GROUP_0F01, 6, 6, I486_FIXED, 13, 13, 0, 0);
    SetGroup(I486_GROUP_0FBA, 4, 4, I486_FIXED, 3, 3, 0, 0);
    SetGroup(I486_GROUP_0FBA, 5, 7, I486_FIXED, 6, 8, 0, k_i486_rm);

    // MOV CR0 and MOV r32,TR3
    SetGroup(I486_GROUP_CONTROL, 0, 7, I486_FIXED, 4, 4, 0, 0);
    SetGroup(I486_GROUP_CONTROL, 0, 0, I486_FIXED, 17, 17, 0, 0);
    SetGroup(I486_GROUP_TEST, 0, 7, I486_FIXED, 4, 4, 0, k_i486_rm);
    SetGroup(I486_GROUP_TEST, 3, 3, I486_FIXED, 3, 3, 0, k_i486_rm);
}

INLINE const I486_Form& I386::Get486Form(const InstructionContext& c, u8& flags)
{
    const I486_Form& form = k_i486_forms[c.two_byte ? 256 + c.opcode2 : c.opcode];
    flags = form.flags;

    if (form.kind != I486_GROUP)
        return form;

    const I486_Form& member = k_i486_groups[form.reg][c.reg];
    flags = (u8)(member.flags | (flags & k_i486_byte));
    return member;
}

// protected_mode: the mode the instruction started in, an IRET into VM86 has left it
INLINE u32 I386::Get486FormClocks(const I486_Form& form, const InstructionContext& c, bool protected_mode) const
{
    switch (form.kind)
    {
        case I486_FIXED:
            return c.memory_operand ? form.memory : form.reg;
        case I486_MODE:
            return protected_mode ? form.memory : form.reg;
        case I486_FAR:
            return protected_mode ? k_i486_keep : form.reg;
        case I486_JUMP:
            return Is486Taken(c) ? form.memory : form.reg;
        case I486_SETCC:
        {
            bool taken = Is486Taken(c);
            return c.memory_operand ? (taken ? 3 : 4) : (taken ? 4 : 3);
        }
        case I486_STRING:
            return Get486StringClocks(c, false);
        case I486_DATA:
            return k_i486_data;
        case I486_DIVIDE:
        {
            u32 width = c.opcode == 0xF6 ? 1 : c.operand_size;
            return (width == 1 ? 16 : width == 2 ? 24 : 40) + (c.memory_operand ? form.memory : form.reg);
        }
        case I486_LEA:
            return Has486Index(c) ? 2 : 1;
        case I486_INTERRUPT:
            return m_step.exception ? k_i486_keep : form.reg;
        default:
            return k_i486_keep;
    }
}

INLINE u8 I386::Get486FormWritten(const I486_Form& form, u8 flags, const InstructionContext& c)
{
    u32 mask = (flags & k_i486_byte) != 0 ? 3 : 7;
    u32 written = form.fixed;

    if ((flags & k_i486_reg) != 0)
        written |= 1U << (c.reg & mask);

    if ((flags & k_i486_rm) != 0 && !c.memory_operand)
        written |= 1U << (c.rm & mask);

    if ((flags & k_i486_opcode) != 0)
        written |= 1U << ((c.two_byte ? c.opcode2 : c.opcode) & mask);

    if ((flags & k_i486_count) != 0 && c.repeat != 0)
        written |= k_ecx;

    if ((flags & k_i486_status) != 0 && c.modrm == 0xE0)
        written |= k_eax;

    return (u8)written;
}

INLINE int I386::Get486FormBase(u8 flags, const InstructionContext& c)
{
    static const s8 k_base16[8] = { I386_REG_EBX, I386_REG_EBX, I386_REG_EBP, I386_REG_EBP, I386_REG_ESI, I386_REG_EDI,
        I386_REG_EBP, I386_REG_EBX };

    if (c.memory_operand)
    {
        u8 mod = c.modrm >> 6;
        u8 rm = c.modrm & 7;

        if (c.address_size == 2)
            return mod == 0 && rm == 6 ? -1 : k_base16[rm];

        if (rm == 4)
            return mod == 0 && (c.sib & 7) == 5 ? -1 : c.sib & 7;

        return mod == 0 && rm == 5 ? -1 : rm;
    }

    if ((flags & k_i486_stack) != 0)
        return I386_REG_ESP;

    return (flags & k_i486_xlat) != 0 ? I386_REG_EBX : -1;
}

// T1 replaces the step's clocks with the 486's cache-hit count, PRM486 Table 10.1, plus 1 per prefix byte, the AGI
// clock, 3 per misaligned access and the cold paths' extras. Forms whose handlers count 486 clocks keep them
void I386::Apply486Clocks()
{
    const InstructionContext& c = m_instruction;
    u8 flags = 0;
    const I486_Form& form = Get486Form(c, flags);
    bool port = form.kind == I486_PORT;
    u32 clocks = 0;
    u32 stalls = 0;

    if (m_486_continuation)
        clocks = port ? (u32)m_step.clocks : Get486StringClocks(c, true);
    else
    {
        clocks = Get486FormClocks(form, c, m_486_protected);

        if (clocks == k_i486_keep)
            clocks = (u32)m_step.clocks;
        else if (clocks == k_i486_data)
            clocks = Get486DataClocks(c);

        // INT n, INT 3 and a taken INTO entered the gate, whose clocks are already the 486's
        if (m_step.exception)
            clocks += c.opcode == 0xCE ? 2 : c.opcode == 0xCD && m_state.execution_mode != I386_MODE_PROTECTED ? 4 : 0;

        if (!port)
            clocks += Get486PrefixClocks(c);

        int base = Get486FormBase(flags, c);

        if (base >= 0 && (m_486_written & (1 << base)) != 0)
            stalls++;
    }

    // Only memory operands, the stack, MOV moffs, strings, ENTER and LEAVE access data
    if (!port && (c.memory_operand || (flags & k_i486_stack) != 0 || form.kind == I486_STRING ||
        (!c.two_byte && ((c.opcode & 0xFC) == 0xA0 || (c.opcode & 0xFE) == 0xC8))))
        stalls += Get486Misaligned();

    m_486_written = m_step.exception ? 0 : Get486FormWritten(form, flags, c);
    m_486_bus_clocks = clocks + m_486_extra;
    m_step.clocks = clocks + stalls + m_486_extra;
}

// Clocks that depend on the form alone, or on a taken jump, a repeated LOOP or a true SETcc condition, for the
// translated blocks
u32 I386::Get486Clocks(const InstructionContext& c, bool taken) const
{
    u8 flags = 0;
    const I486_Form& form = Get486Form(c, flags);

    if (form.kind == I486_JUMP)
        return taken ? form.memory : form.reg;

    return Get486FormClocks(form, c, m_state.execution_mode == I386_MODE_PROTECTED);
}


// MOVS, CMPS, STOS, LODS and SCAS. A repeated one counts its start with the first element, and REP MOVS of one
// element takes 13
u32 I386::Get486StringClocks(const InstructionContext& c, bool continuation) const
{
    // MOVS, CMPS, -, -, STOS, LODS and SCAS: alone and per repeated element
    static const u8 k_single[6] = { 7, 8, 0, 5, 5, 6 };
    static const u8 k_element[6] = { 3, 7, 0, 4, 4, 5 };
    int index = (c.opcode - 0xA4) >> 1;
    u32 single = k_single[index];
    u32 element = k_element[index];

    if (c.repeat == 0)
        return single;

    if (continuation)
        return element;

    u32 count = c.address_size == 2 ? m_486_registers[I386_REG_ECX] & 0xFFFF : m_486_registers[I386_REG_ECX];

    if (count == 0)
        return 5;

    if (c.opcode <= 0xA5)
        return count == 1 ? 13 : 12 + element;

    return 7 + element;
}

u32 I386::Get486DataClocks(const InstructionContext& c) const
{
    bool memory = c.memory_operand;

    if (c.two_byte)
    {
        // IMUL r,r/m multiplies by r/m
        if (c.opcode2 == 0xAF)
            return Get486MultiplyClocks(Peek486Operand(c, c.operand_size * 8), c.operand_size * 8, true);

        u32 source = Peek486Operand(c, c.operand_size * 8);

        if (source == 0)
            return memory ? 7 : 6;

        u32 extra = memory ? 1 : 0;

        // BSF by the lowest set bit's byte, nibble and bit, BSR by the highest bit's position
        if (c.opcode2 == 0xBC)
        {
            u32 position = 0;

            while ((source & (1U << position)) == 0)
                position++;

            return 8 + extra + 4 * (position / 8 + 1) + 3 * ((position % 8) / 4 + 1) + 3 * (position % 4 + 1);
        }

        u32 position = 31;

        while ((source & (1U << position)) == 0)
            position--;

        return 7 + extra + 3 * (32 - position);
    }

    switch (c.opcode)
    {
        case 0x69:
            return Get486MultiplyClocks(c.immediate, c.operand_size * 8, true);
        case 0x6B:
            return Get486MultiplyClocks((u32)SignExtend(c.immediate, 8), c.operand_size * 8, true);
        case 0xC8:
        {
            // ENTER by its nesting level
            u32 level = c.immediate2 & 0x1F;
            return level == 0 ? 14 : level == 1 ? 17 : 17 + 3 * level;
        }
        case 0xF6:
        case 0xF7:
        {
            int width = c.opcode == 0xF6 ? 8 : c.operand_size * 8;
            return Get486MultiplyClocks(Peek486Operand(c, width), width, c.reg == 5);
        }
        default:
        {
            // RCL and RCR, 7 more for each full rotation of the operand
            int width = c.opcode == 0xC0 || c.opcode == 0xD2 ? 8 : c.operand_size * 8;
            u32 count = (c.opcode <= 0xC1 ? c.immediate : m_486_registers[I386_REG_ECX]) & 0x1F;
            u32 clocks = count <= (u32)width ? 8 : (count / (u32)width) * 7 + 9;
            return clocks + (memory ? 1 : 0);
        }
    }
}

// 10 plus the multiplier's significant bits, at least 3, or 5 for a negative signed multiplier
u32 I386::Get486MultiplyClocks(u32 multiplier, int width, bool signed_multiplier) const
{
    u32 value = multiplier & GetMask(width);
    bool negative = signed_multiplier && (value & GetSignBit(width)) != 0;
    u32 magnitude = negative ? (~value + 1) & GetMask(width) : value;
    u32 bits = 0;

    while (magnitude != 0)
    {
        magnitude >>= 1;
        bits++;
    }

    return 10 + MAX(bits, negative ? 5U : 3U);
}

// The r/m operand as it was before the instruction: the captured register, or memory, which these forms don't write
u32 I386::Peek486Operand(const InstructionContext& c, int width) const
{
    if (!c.memory_operand)
    {
        if (width == 8)
            return c.rm < 4 ? m_486_registers[c.rm] & 0xFF : (m_486_registers[c.rm & 3] >> 8) & 0xFF;

        return m_486_registers[c.rm] & GetMask(width);
    }

    u32 linear = m_state.segments[c.segment].base + c.effective_offset;
    u32 value = 0;

    for (int i = 0; i < width / 8; i++)
    {
        u8 byte = 0xFF;
        TryPeekLinear(linear + (u32)i, byte);
        value |= (u32)byte << (i * 8);
    }

    return value;
}

bool I386::Is486Taken(const InstructionContext& c) const
{
    if (c.two_byte)
        return CheckCondition(c.opcode2 & 15);

    u8 opcode = c.opcode;

    if (opcode < 0x80)
        return CheckCondition(opcode & 15);

    u32 count = GetRegister(I386_REG_ECX, c.address_size * 8);

    if (opcode == 0xE3)
        return count == 0;

    bool zero = (m_state.eflags & I386_FLAG_ZF) != 0;
    return count != 0 && (opcode == 0xE2 || (opcode == 0xE1) == zero);
}

bool I386::Has486Index(const InstructionContext& c)
{
    u8 rm = c.modrm & 7;

    if (c.address_size == 2)
        return rm <= 3;

    return rm == 4 && ((c.sib >> 3) & 7) != 4;
}

int I386::Get486Base(const InstructionContext& c)
{
    u8 flags = 0;
    Get486Form(c, flags);
    return Get486FormBase(flags, c);
}

u8 I386::Get486Written(const InstructionContext& c)
{
    u8 flags = 0;
    const I486_Form& form = Get486Form(c, flags);
    return Get486FormWritten(form, flags, c);
}

// One clock per segment, operand size, address size and LOCK prefix byte
u32 I386::Get486PrefixClocks(const InstructionContext& c) const
{
    if (c.segment_override == 0xFF && c.operand_size == m_default_size && c.address_size == m_default_size && !c.lock)
        return 0;

    u32 length = MIN(c.next_eip - c.start_eip, (u32)GT_I386_MAX_INSTRUCTION_LENGTH);
    u32 offset = c.start_eip - m_code_window_eip;

    if (offset < m_code_window_size && length <= m_code_window_size - offset)
        return Count486Prefixes(m_code_window + offset, length);

    u8 bytes[GT_I386_MAX_INSTRUCTION_LENGTH];

    for (u32 i = 0; i < length; i++)
    {
        bytes[i] = 0;
        TryPeekLinear(m_state.segments[I386_SEGMENT_CS].base + c.start_eip + i, bytes[i]);
    }

    return Count486Prefixes(bytes, length);
}

// 1 for the prefixes that take a clock, 2 for REP and REPNE, which belong to their string instruction
static u8 k_i486_prefixes[256];

u32 I386::Count486Prefixes(const u8* bytes, u32 length)
{
    if (k_i486_prefixes[0x66] == 0)
    {
        static const u8 k_counted[9] = { 0x26, 0x2E, 0x36, 0x3E, 0x64, 0x65, 0x66, 0x67, 0xF0 };

        for (int i = 0; i < 9; i++)
            k_i486_prefixes[k_counted[i]] = 1;

        k_i486_prefixes[0xF2] = 2;
        k_i486_prefixes[0xF3] = 2;
    }

    u32 count = 0;

    for (u32 i = 0; i < length && k_i486_prefixes[bytes[i]] != 0; i++)
        count += k_i486_prefixes[bytes[i]] & 1;

    return count;
}

// 3 clocks for each data access to an address that isn't a multiple of its size, at the addresses the bus model uses
u32 I386::Get486Misaligned() const
{
    const InstructionContext& c = m_instruction;
    u8 pattern = GetBusPattern(c);
    u32 size = (pattern & k_i386_bus_byte) != 0 ? 1 : (pattern & k_i386_bus_word) != 0 ? 2 : c.operand_size;
    u32 stack_mask = m_stack32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 stack = m_state.registers[I386_REG_ESP].value;
    u32 stack_base = m_state.segments[I386_SEGMENT_SS].base;
    u32 linear = m_state.segments[c.segment].base + c.effective_offset;
    u32 mask = size - 1;
    u32 count = 0;

    switch (pattern & k_i386_bus_pattern_mask)
    {
        case I386_BUS_READ:
        case I386_BUS_WRITE:
            count = c.memory_operand && (linear & mask) != 0 ? 1 : 0;
            break;
        case I386_BUS_RMW:
            count = c.memory_operand && (linear & mask) != 0 ? 2 : 0;
            break;
        case I386_BUS_PUSH:
        case I386_BUS_CALL:
            count = ((stack_base + (stack & stack_mask)) & mask) != 0 ? 1 : 0;
            break;
        case I386_BUS_POP:
            count = ((stack_base + ((stack - size) & stack_mask)) & mask) != 0 ? 1 : 0;
            break;
        case I386_BUS_RET:
            count = ((stack_base + ((stack - size - (c.opcode == 0xC2 ? c.immediate : 0)) & stack_mask)) & mask) != 0 ?
                1 : 0;
            break;
        case I386_BUS_PUSH_READ:
            count = (c.memory_operand && (linear & mask) != 0 ? 1 : 0) +
                (((stack_base + (stack & stack_mask)) & mask) != 0 ? 1 : 0);
            break;
        case I386_BUS_POP_WRITE:
            count = (((stack_base + ((stack - size) & stack_mask)) & mask) != 0 ? 1 : 0) +
                (c.memory_operand && (linear & mask) != 0 ? 1 : 0);
            break;
        case I386_BUS_MOFFS_READ:
        case I386_BUS_MOFFS_WRITE:
        {
            u8 segment = c.segment_override == 0xFF ? (u8)I386_SEGMENT_DS : c.segment_override;
            count = ((m_state.segments[segment].base + c.immediate) & mask) != 0 ? 1 : 0;
            break;
        }
        case I386_BUS_STRING:
        {
            if (!m_string.executed)
                break;

            mask = (m_string.width >> 3) - 1;
            u32 source = m_state.segments[m_string.source_segment].base + m_string.source_offset;
            u32 destination = m_state.segments[I386_SEGMENT_ES].base + m_string.destination_offset;
            u8 opcode = c.opcode & 0xFE;
            bool uses_source = opcode == 0xA4 || opcode == 0xA6 || opcode == 0xAC;
            bool uses_destination = opcode != 0xAC;

            count = (uses_source && (source & mask) != 0 ? 1 : 0) +
                (uses_destination && (destination & mask) != 0 ? 1 : 0);
            break;
        }
        case I386_BUS_NONE:
        case I386_BUS_JUMP:
        case I386_BUS_JCC:
        case I386_BUS_LOOP:
        case I386_BUS_FAR:
        case I386_BUS_XLAT:
            break;
        default:
            count = Get486MisalignedOther(pattern, size, linear);
            break;
    }

    return count * 3;
}

// The rarer forms, with the pieces and addresses the bus model uses
NO_INLINE u32 I386::Get486MisalignedOther(u8 pattern, u32 size, u32 linear) const
{
    const InstructionContext& c = m_instruction;
    u32 stack_mask = m_stack32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 stack = m_state.registers[I386_REG_ESP].value;
    u32 stack_base = m_state.segments[I386_SEGMENT_SS].base;
    u32 mask = size - 1;
    u32 count = 0;

    switch (pattern & k_i386_bus_pattern_mask)
    {
        case I386_BUS_CALL_RM:
            count = (c.memory_operand && (linear & mask) != 0 ? 1 : 0) +
                (((stack_base + (stack & stack_mask)) & mask) != 0 ? 1 : 0);
            break;
        case I386_BUS_JUMP_RM:
            count = c.memory_operand && (linear & mask) != 0 ? 1 : 0;
            break;
        case I386_BUS_PUSHA:
            for (u32 i = 0; i < 8; i++)
                count += ((stack_base + ((stack + i * size) & stack_mask)) & mask) != 0 ? 1 : 0;
            break;
        case I386_BUS_POPA:
            for (u32 i = 0; i < 8; i++)
                count += ((stack_base + ((stack - (8 - i) * size) & stack_mask)) & mask) != 0 ? 1 : 0;
            break;
        case I386_BUS_ENTER:
            count = ((stack_base + (m_state.registers[I386_REG_EBP].value & stack_mask)) & mask) != 0 ? 1 : 0;
            break;
        case I386_BUS_LEAVE:
            count = ((stack_base + ((stack - size) & stack_mask)) & mask) != 0 ? 1 : 0;
            break;
        case I386_BUS_READ_FAR:
            count = c.memory_operand ? ((linear & mask) != 0 ? 1 : 0) + (((linear + size) & 1) != 0 ? 1 : 0) : 0;
            break;
        case I386_BUS_BOUND:
            count = c.memory_operand ? ((linear & mask) != 0 ? 1 : 0) + (((linear + size) & mask) != 0 ? 1 : 0) : 0;
            break;
        case I386_BUS_DESC_READ:
        case I386_BUS_DESC_WRITE:
            count = c.memory_operand ? ((linear & 1) != 0 ? 1 : 0) + (((linear + 2) & 3) != 0 ? 1 : 0) : 0;
            break;
        case I386_BUS_ESC:
        {
            if (!c.memory_operand || !IsValidPointer(m_x87))
                break;

            bool write = false;
            u32 operand = X87::GetMemoryOperandSize(c.opcode, c.modrm, c.operand_size == 4, write);

            for (u32 done = 0; done < operand; done += 4)
                count += ((linear + done) & (MIN(operand - done, 4U) - 1)) != 0 ? 1 : 0;
            break;
        }
        default:
            break;
    }

    return count;
}

// The extras PRM486 Table 10.1 lists over the gate's clocks: page faults, BOUND out of range and the IOPL-sensitive
// instructions in VM86 mode
u32 I386::Get486ExceptionClocks(u8 vector, bool vm86) const
{
    const InstructionContext& c = m_instruction;

    if (vector == 14 || (vector == 5 && !c.two_byte && c.opcode == 0x62))
        return 24;

    if (vector != 13 || !vm86 || c.two_byte)
        return 0;

    switch (c.opcode)
    {
        case 0xFA:
        case 0xFB:
        case 0x9D:
            // CLI, STI and POPF
            return 8;
        case 0xCD:
        case 0x9C:
        case 0xCF:
            // INT n, PUSHF and IRET
            return 9;
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
            // IN and OUT with a fixed port
            return 50;
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
            // IN and OUT with DX
            return 51;
        case 0x6C:
        case 0x6D:
        case 0x6E:
        case 0x6F:
            // INS and OUTS
            return c.repeat != 0 ? 51 : 50;
        default:
            return 0;
    }
}
