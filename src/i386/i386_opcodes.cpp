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
#include "../system/io.h"

bool I386::OPCode0x00()
{
    // ADD r/m8,r8
    return OPCodes_ALU<0, 0, 8>();
}

bool I386::OPCode0x01()
{
    // ADD r/m16/32,r16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<0, 1, 32>() : OPCodes_ALU<0, 1, 16>();
}

bool I386::OPCode0x02()
{
    // ADD r8,r/m8
    return OPCodes_ALU<0, 2, 8>();
}

bool I386::OPCode0x03()
{
    // ADD r16/32,r/m16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<0, 3, 32>() : OPCodes_ALU<0, 3, 16>();
}

bool I386::OPCode0x04()
{
    // ADD AL,imm8
    return OPCodes_ALU<0, 4, 8>();
}

bool I386::OPCode0x05()
{
    // ADD AX/EAX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<0, 5, 32>() : OPCodes_ALU<0, 5, 16>();
}

bool I386::OPCode0x06()
{
    // PUSH ES
    return OPCodes_PUSH_Segment();
}

bool I386::OPCode0x07()
{
    // POP ES
    return OPCodes_POP_Segment();
}

bool I386::OPCode0x08()
{
    // OR r/m8,r8
    return OPCodes_ALU<1, 0, 8>();
}

bool I386::OPCode0x09()
{
    // OR r/m16/32,r16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<1, 1, 32>() : OPCodes_ALU<1, 1, 16>();
}

bool I386::OPCode0x0A()
{
    // OR r8,r/m8
    return OPCodes_ALU<1, 2, 8>();
}

bool I386::OPCode0x0B()
{
    // OR r16/32,r/m16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<1, 3, 32>() : OPCodes_ALU<1, 3, 16>();
}

bool I386::OPCode0x0C()
{
    // OR AL,imm8
    return OPCodes_ALU<1, 4, 8>();
}

bool I386::OPCode0x0D()
{
    // OR AX/EAX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<1, 5, 32>() : OPCodes_ALU<1, 5, 16>();
}

bool I386::OPCode0x0E()
{
    // PUSH CS
    return OPCodes_PUSH_Segment();
}

bool I386::OPCode0x0F()
{
    // Two-byte opcode escape
    return DispatchOPCode0F();
}

bool I386::OPCode0x10()
{
    // ADC r/m8,r8
    return OPCodes_ALU<2, 0, 8>();
}

bool I386::OPCode0x11()
{
    // ADC r/m16/32,r16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<2, 1, 32>() : OPCodes_ALU<2, 1, 16>();
}

bool I386::OPCode0x12()
{
    // ADC r8,r/m8
    return OPCodes_ALU<2, 2, 8>();
}

bool I386::OPCode0x13()
{
    // ADC r16/32,r/m16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<2, 3, 32>() : OPCodes_ALU<2, 3, 16>();
}

bool I386::OPCode0x14()
{
    // ADC AL,imm8
    return OPCodes_ALU<2, 4, 8>();
}

bool I386::OPCode0x15()
{
    // ADC AX/EAX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<2, 5, 32>() : OPCodes_ALU<2, 5, 16>();
}

bool I386::OPCode0x16()
{
    // PUSH SS
    return OPCodes_PUSH_Segment();
}

bool I386::OPCode0x17()
{
    // POP SS
    return OPCodes_POP_Segment();
}

bool I386::OPCode0x18()
{
    // SBB r/m8,r8
    return OPCodes_ALU<3, 0, 8>();
}

bool I386::OPCode0x19()
{
    // SBB r/m16/32,r16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<3, 1, 32>() : OPCodes_ALU<3, 1, 16>();
}

bool I386::OPCode0x1A()
{
    // SBB r8,r/m8
    return OPCodes_ALU<3, 2, 8>();
}

bool I386::OPCode0x1B()
{
    // SBB r16/32,r/m16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<3, 3, 32>() : OPCodes_ALU<3, 3, 16>();
}

bool I386::OPCode0x1C()
{
    // SBB AL,imm8
    return OPCodes_ALU<3, 4, 8>();
}

bool I386::OPCode0x1D()
{
    // SBB AX/EAX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<3, 5, 32>() : OPCodes_ALU<3, 5, 16>();
}

bool I386::OPCode0x1E()
{
    // PUSH DS
    return OPCodes_PUSH_Segment();
}

bool I386::OPCode0x1F()
{
    // POP DS
    return OPCodes_POP_Segment();
}

bool I386::OPCode0x20()
{
    // AND r/m8,r8
    return OPCodes_ALU<4, 0, 8>();
}

bool I386::OPCode0x21()
{
    // AND r/m16/32,r16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<4, 1, 32>() : OPCodes_ALU<4, 1, 16>();
}

bool I386::OPCode0x22()
{
    // AND r8,r/m8
    return OPCodes_ALU<4, 2, 8>();
}

bool I386::OPCode0x23()
{
    // AND r16/32,r/m16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<4, 3, 32>() : OPCodes_ALU<4, 3, 16>();
}

bool I386::OPCode0x24()
{
    // AND AL,imm8
    return OPCodes_ALU<4, 4, 8>();
}

bool I386::OPCode0x25()
{
    // AND AX/EAX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<4, 5, 32>() : OPCodes_ALU<4, 5, 16>();
}

bool I386::OPCode0x26()
{
    // ES segment override
    return PrefixSegment(I386_SEGMENT_ES);
}

bool I386::OPCode0x27()
{
    // DAA
    return OPCodes_DAA();
}

bool I386::OPCode0x28()
{
    // SUB r/m8,r8
    return OPCodes_ALU<5, 0, 8>();
}

bool I386::OPCode0x29()
{
    // SUB r/m16/32,r16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<5, 1, 32>() : OPCodes_ALU<5, 1, 16>();
}

bool I386::OPCode0x2A()
{
    // SUB r8,r/m8
    return OPCodes_ALU<5, 2, 8>();
}

bool I386::OPCode0x2B()
{
    // SUB r16/32,r/m16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<5, 3, 32>() : OPCodes_ALU<5, 3, 16>();
}

bool I386::OPCode0x2C()
{
    // SUB AL,imm8
    return OPCodes_ALU<5, 4, 8>();
}

bool I386::OPCode0x2D()
{
    // SUB AX/EAX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<5, 5, 32>() : OPCodes_ALU<5, 5, 16>();
}

bool I386::OPCode0x2E()
{
    // CS segment override
    return PrefixSegment(I386_SEGMENT_CS);
}

bool I386::OPCode0x2F()
{
    // DAS
    return OPCodes_DAS();
}

bool I386::OPCode0x30()
{
    // XOR r/m8,r8
    return OPCodes_ALU<6, 0, 8>();
}

bool I386::OPCode0x31()
{
    // XOR r/m16/32,r16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<6, 1, 32>() : OPCodes_ALU<6, 1, 16>();
}

bool I386::OPCode0x32()
{
    // XOR r8,r/m8
    return OPCodes_ALU<6, 2, 8>();
}

bool I386::OPCode0x33()
{
    // XOR r16/32,r/m16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<6, 3, 32>() : OPCodes_ALU<6, 3, 16>();
}

bool I386::OPCode0x34()
{
    // XOR AL,imm8
    return OPCodes_ALU<6, 4, 8>();
}

bool I386::OPCode0x35()
{
    // XOR AX/EAX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<6, 5, 32>() : OPCodes_ALU<6, 5, 16>();
}

bool I386::OPCode0x36()
{
    // SS segment override
    return PrefixSegment(I386_SEGMENT_SS);
}

bool I386::OPCode0x37()
{
    // AAA
    return OPCodes_AAA();
}

bool I386::OPCode0x38()
{
    // CMP r/m8,r8
    return OPCodes_ALU<7, 0, 8>();
}

bool I386::OPCode0x39()
{
    // CMP r/m16/32,r16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<7, 1, 32>() : OPCodes_ALU<7, 1, 16>();
}

bool I386::OPCode0x3A()
{
    // CMP r8,r/m8
    return OPCodes_ALU<7, 2, 8>();
}

bool I386::OPCode0x3B()
{
    // CMP r16/32,r/m16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<7, 3, 32>() : OPCodes_ALU<7, 3, 16>();
}

bool I386::OPCode0x3C()
{
    // CMP AL,imm8
    return OPCodes_ALU<7, 4, 8>();
}

bool I386::OPCode0x3D()
{
    // CMP AX/EAX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU<7, 5, 32>() : OPCodes_ALU<7, 5, 16>();
}

bool I386::OPCode0x3E()
{
    // DS segment override
    return PrefixSegment(I386_SEGMENT_DS);
}

bool I386::OPCode0x3F()
{
    // AAS
    return OPCodes_AAS();
}

bool I386::OPCode0x40()
{
    // INC AX/EAX
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, false>() : OPCodes_INC_DEC_Register<16, false>();
}

bool I386::OPCode0x41()
{
    // INC CX/ECX
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, false>() : OPCodes_INC_DEC_Register<16, false>();
}

bool I386::OPCode0x42()
{
    // INC DX/EDX
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, false>() : OPCodes_INC_DEC_Register<16, false>();
}

bool I386::OPCode0x43()
{
    // INC BX/EBX
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, false>() : OPCodes_INC_DEC_Register<16, false>();
}

bool I386::OPCode0x44()
{
    // INC SP/ESP
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, false>() : OPCodes_INC_DEC_Register<16, false>();
}

bool I386::OPCode0x45()
{
    // INC BP/EBP
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, false>() : OPCodes_INC_DEC_Register<16, false>();
}

bool I386::OPCode0x46()
{
    // INC SI/ESI
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, false>() : OPCodes_INC_DEC_Register<16, false>();
}

bool I386::OPCode0x47()
{
    // INC DI/EDI
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, false>() : OPCodes_INC_DEC_Register<16, false>();
}

bool I386::OPCode0x48()
{
    // DEC AX/EAX
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, true>() : OPCodes_INC_DEC_Register<16, true>();
}

bool I386::OPCode0x49()
{
    // DEC CX/ECX
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, true>() : OPCodes_INC_DEC_Register<16, true>();
}

bool I386::OPCode0x4A()
{
    // DEC DX/EDX
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, true>() : OPCodes_INC_DEC_Register<16, true>();
}

bool I386::OPCode0x4B()
{
    // DEC BX/EBX
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, true>() : OPCodes_INC_DEC_Register<16, true>();
}

bool I386::OPCode0x4C()
{
    // DEC SP/ESP
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, true>() : OPCodes_INC_DEC_Register<16, true>();
}

bool I386::OPCode0x4D()
{
    // DEC BP/EBP
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, true>() : OPCodes_INC_DEC_Register<16, true>();
}

bool I386::OPCode0x4E()
{
    // DEC SI/ESI
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, true>() : OPCodes_INC_DEC_Register<16, true>();
}

bool I386::OPCode0x4F()
{
    // DEC DI/EDI
    return m_instruction.operand_size == 4 ?
        OPCodes_INC_DEC_Register<32, true>() : OPCodes_INC_DEC_Register<16, true>();
}

bool I386::OPCode0x50()
{
    // PUSH AX/EAX
    return m_instruction.operand_size == 4 ? OPCodes_PUSH_Register<32>() : OPCodes_PUSH_Register<16>();
}

bool I386::OPCode0x51()
{
    // PUSH CX/ECX
    return m_instruction.operand_size == 4 ? OPCodes_PUSH_Register<32>() : OPCodes_PUSH_Register<16>();
}

bool I386::OPCode0x52()
{
    // PUSH DX/EDX
    return m_instruction.operand_size == 4 ? OPCodes_PUSH_Register<32>() : OPCodes_PUSH_Register<16>();
}

bool I386::OPCode0x53()
{
    // PUSH BX/EBX
    return m_instruction.operand_size == 4 ? OPCodes_PUSH_Register<32>() : OPCodes_PUSH_Register<16>();
}

bool I386::OPCode0x54()
{
    // PUSH SP/ESP
    return m_instruction.operand_size == 4 ? OPCodes_PUSH_Register<32>() : OPCodes_PUSH_Register<16>();
}

bool I386::OPCode0x55()
{
    // PUSH BP/EBP
    return m_instruction.operand_size == 4 ? OPCodes_PUSH_Register<32>() : OPCodes_PUSH_Register<16>();
}

bool I386::OPCode0x56()
{
    // PUSH SI/ESI
    return m_instruction.operand_size == 4 ? OPCodes_PUSH_Register<32>() : OPCodes_PUSH_Register<16>();
}

bool I386::OPCode0x57()
{
    // PUSH DI/EDI
    return m_instruction.operand_size == 4 ? OPCodes_PUSH_Register<32>() : OPCodes_PUSH_Register<16>();
}

bool I386::OPCode0x58()
{
    // POP AX/EAX
    return m_instruction.operand_size == 4 ? OPCodes_POP_Register<32>() : OPCodes_POP_Register<16>();
}

bool I386::OPCode0x59()
{
    // POP CX/ECX
    return m_instruction.operand_size == 4 ? OPCodes_POP_Register<32>() : OPCodes_POP_Register<16>();
}

bool I386::OPCode0x5A()
{
    // POP DX/EDX
    return m_instruction.operand_size == 4 ? OPCodes_POP_Register<32>() : OPCodes_POP_Register<16>();
}

bool I386::OPCode0x5B()
{
    // POP BX/EBX
    return m_instruction.operand_size == 4 ? OPCodes_POP_Register<32>() : OPCodes_POP_Register<16>();
}

bool I386::OPCode0x5C()
{
    // POP SP/ESP
    return m_instruction.operand_size == 4 ? OPCodes_POP_Register<32>() : OPCodes_POP_Register<16>();
}

bool I386::OPCode0x5D()
{
    // POP BP/EBP
    return m_instruction.operand_size == 4 ? OPCodes_POP_Register<32>() : OPCodes_POP_Register<16>();
}

bool I386::OPCode0x5E()
{
    // POP SI/ESI
    return m_instruction.operand_size == 4 ? OPCodes_POP_Register<32>() : OPCodes_POP_Register<16>();
}

bool I386::OPCode0x5F()
{
    // POP DI/EDI
    return m_instruction.operand_size == 4 ? OPCodes_POP_Register<32>() : OPCodes_POP_Register<16>();
}

bool I386::OPCode0x60()
{
    // PUSHA/PUSHAD
    return OPCodes_PUSHA();
}

bool I386::OPCode0x61()
{
    // POPA/POPAD
    return OPCodes_POPA();
}

bool I386::OPCode0x62()
{
    // BOUND r16/32,m16&16/32&32
    return OPCodes_BOUND();
}

bool I386::OPCode0x63()
{
    // ARPL r/m16,r16
    return OPCodes_ARPL();
}

bool I386::OPCode0x64()
{
    // FS segment override
    return PrefixSegment(I386_SEGMENT_FS);
}

bool I386::OPCode0x65()
{
    // GS segment override
    return PrefixSegment(I386_SEGMENT_GS);
}

bool I386::OPCode0x66()
{
    // Operand-size override
    return PrefixOperandSize();
}

bool I386::OPCode0x67()
{
    // Address-size override
    return PrefixAddressSize();
}

bool I386::OPCode0x68()
{
    // PUSH imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_PUSH_Immediate<32>() : OPCodes_PUSH_Immediate<16>();
}

bool I386::OPCode0x69()
{
    // IMUL r16/32,r/m16/32,imm16/32
    return OPCodes_IMUL_Immediate();
}

bool I386::OPCode0x6A()
{
    // PUSH imm8
    return m_instruction.operand_size == 4 ? OPCodes_PUSH_Immediate<32>() : OPCodes_PUSH_Immediate<16>();
}

bool I386::OPCode0x6B()
{
    // IMUL r16/32,r/m16/32,imm8
    return OPCodes_IMUL_Immediate();
}

bool I386::OPCode0x6C()
{
    // INS m8,DX
    return OPCodes_String();
}

bool I386::OPCode0x6D()
{
    // INS m16/32,DX
    return OPCodes_String();
}

bool I386::OPCode0x6E()
{
    // OUTS DX,m8
    return OPCodes_String();
}

bool I386::OPCode0x6F()
{
    // OUTS DX,m16/32
    return OPCodes_String();
}

bool I386::OPCode0x70()
{
    // JO rel8
    return OPCodes_Jcc<0, false>();
}

bool I386::OPCode0x71()
{
    // JNO rel8
    return OPCodes_Jcc<1, false>();
}

bool I386::OPCode0x72()
{
    // JB/JNAE/JC rel8
    return OPCodes_Jcc<2, false>();
}

bool I386::OPCode0x73()
{
    // JNB/JAE/JNC rel8
    return OPCodes_Jcc<3, false>();
}

bool I386::OPCode0x74()
{
    // JZ/JE rel8
    return OPCodes_Jcc<4, false>();
}

bool I386::OPCode0x75()
{
    // JNZ/JNE rel8
    return OPCodes_Jcc<5, false>();
}

bool I386::OPCode0x76()
{
    // JBE/JNA rel8
    return OPCodes_Jcc<6, false>();
}

bool I386::OPCode0x77()
{
    // JNBE/JA rel8
    return OPCodes_Jcc<7, false>();
}

bool I386::OPCode0x78()
{
    // JS rel8
    return OPCodes_Jcc<8, false>();
}

bool I386::OPCode0x79()
{
    // JNS rel8
    return OPCodes_Jcc<9, false>();
}

bool I386::OPCode0x7A()
{
    // JP/JPE rel8
    return OPCodes_Jcc<10, false>();
}

bool I386::OPCode0x7B()
{
    // JNP/JPO rel8
    return OPCodes_Jcc<11, false>();
}

bool I386::OPCode0x7C()
{
    // JL/JNGE rel8
    return OPCodes_Jcc<12, false>();
}

bool I386::OPCode0x7D()
{
    // JNL/JGE rel8
    return OPCodes_Jcc<13, false>();
}

bool I386::OPCode0x7E()
{
    // JLE/JNG rel8
    return OPCodes_Jcc<14, false>();
}

bool I386::OPCode0x7F()
{
    // JNLE/JG rel8
    return OPCodes_Jcc<15, false>();
}

bool I386::OPCode0x80()
{
    // Group 1 r/m8,imm8
    return OPCodes_ALU_Immediate<8, false>();
}

bool I386::OPCode0x81()
{
    // Group 1 r/m16/32,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_ALU_Immediate<32, false>() : OPCodes_ALU_Immediate<16, false>();
}

bool I386::OPCode0x82()
{
    // Group 1 r/m8,imm8 alias
    return OPCodes_ALU_Immediate<8, false>();
}

bool I386::OPCode0x83()
{
    // Group 1 r/m16/32,imm8
    return m_instruction.operand_size == 4 ? OPCodes_ALU_Immediate<32, true>() : OPCodes_ALU_Immediate<16, true>();
}

bool I386::OPCode0x84()
{
    // TEST r/m8,r8
    return OPCodes_TEST_RM<8>();
}

bool I386::OPCode0x85()
{
    // TEST r/m16/32,r16/32
    return m_instruction.operand_size == 4 ? OPCodes_TEST_RM<32>() : OPCodes_TEST_RM<16>();
}

bool I386::OPCode0x86()
{
    // XCHG r/m8,r8
    return OPCodes_XCHG_RM();
}

bool I386::OPCode0x87()
{
    // XCHG r/m16/32,r16/32
    return OPCodes_XCHG_RM();
}

bool I386::OPCode0x88()
{
    // MOV r/m8,r8
    return OPCodes_MOV_RM<8, false>();
}

bool I386::OPCode0x89()
{
    // MOV r/m16/32,r16/32
    return m_instruction.operand_size == 4 ? OPCodes_MOV_RM<32, false>() : OPCodes_MOV_RM<16, false>();
}

bool I386::OPCode0x8A()
{
    // MOV r8,r/m8
    return OPCodes_MOV_RM<8, true>();
}

bool I386::OPCode0x8B()
{
    // MOV r16/32,r/m16/32
    return m_instruction.operand_size == 4 ? OPCodes_MOV_RM<32, true>() : OPCodes_MOV_RM<16, true>();
}

bool I386::OPCode0x8C()
{
    // MOV r/m16,Sreg
    return OPCodes_MOV_RM_Segment();
}

bool I386::OPCode0x8D()
{
    // LEA r16/32,m
    return m_instruction.operand_size == 4 ? OPCodes_LEA<32>() : OPCodes_LEA<16>();
}

bool I386::OPCode0x8E()
{
    // MOV Sreg,r/m16
    return OPCodes_MOV_Segment_RM();
}

bool I386::OPCode0x8F()
{
    // POP r/m16/32
    return OPCodes_POP_RM();
}

bool I386::OPCode0x90()
{
    // NOP
    return OPCodes_NOP();
}

bool I386::OPCode0x91()
{
    // XCHG AX/EAX,CX/ECX
    return OPCodes_XCHG_Accumulator();
}

bool I386::OPCode0x92()
{
    // XCHG AX/EAX,DX/EDX
    return OPCodes_XCHG_Accumulator();
}

bool I386::OPCode0x93()
{
    // XCHG AX/EAX,BX/EBX
    return OPCodes_XCHG_Accumulator();
}

bool I386::OPCode0x94()
{
    // XCHG AX/EAX,SP/ESP
    return OPCodes_XCHG_Accumulator();
}

bool I386::OPCode0x95()
{
    // XCHG AX/EAX,BP/EBP
    return OPCodes_XCHG_Accumulator();
}

bool I386::OPCode0x96()
{
    // XCHG AX/EAX,SI/ESI
    return OPCodes_XCHG_Accumulator();
}

bool I386::OPCode0x97()
{
    // XCHG AX/EAX,DI/EDI
    return OPCodes_XCHG_Accumulator();
}

bool I386::OPCode0x98()
{
    // CBW/CWDE
    return OPCodes_CBW_CWDE();
}

bool I386::OPCode0x99()
{
    // CWD/CDQ
    return OPCodes_CWD_CDQ();
}

bool I386::OPCode0x9A()
{
    // CALL ptr16:16/32
    u16 cs = m_state.segments[I386_SEGMENT_CS].selector;
    u32 base = m_state.segments[I386_SEGMENT_CS].base;
    return TrackCall(OPCodes_CALL_Far(), cs, base);
}

bool I386::OPCode0x9B()
{
    // WAIT/FWAIT
    return OPCodes_WAIT();
}

bool I386::OPCode0x9C()
{
    // PUSHF/PUSHFD
    return OPCodes_PUSHF();
}

bool I386::OPCode0x9D()
{
    // POPF/POPFD
    return OPCodes_POPF();
}

bool I386::OPCode0x9E()
{
    // SAHF
    return OPCodes_SAHF();
}

bool I386::OPCode0x9F()
{
    // LAHF
    return OPCodes_LAHF();
}

bool I386::OPCode0xA0()
{
    // MOV AL,moffs8
    return OPCodes_MOV_Moffs();
}

bool I386::OPCode0xA1()
{
    // MOV AX/EAX,moffs16/32
    return OPCodes_MOV_Moffs();
}

bool I386::OPCode0xA2()
{
    // MOV moffs8,AL
    return OPCodes_MOV_Moffs();
}

bool I386::OPCode0xA3()
{
    // MOV moffs16/32,AX/EAX
    return OPCodes_MOV_Moffs();
}

bool I386::OPCode0xA4()
{
    // MOVS m8,m8
    return OPCodes_String();
}

bool I386::OPCode0xA5()
{
    // MOVS m16/32,m16/32
    return OPCodes_String();
}

bool I386::OPCode0xA6()
{
    // CMPS m8,m8
    return OPCodes_String();
}

bool I386::OPCode0xA7()
{
    // CMPS m16/32,m16/32
    return OPCodes_String();
}

bool I386::OPCode0xA8()
{
    // TEST AL,imm8
    return OPCodes_TEST_Accumulator();
}

bool I386::OPCode0xA9()
{
    // TEST AX/EAX,imm16/32
    return OPCodes_TEST_Accumulator();
}

bool I386::OPCode0xAA()
{
    // STOS m8
    return OPCodes_String();
}

bool I386::OPCode0xAB()
{
    // STOS m16/32
    return OPCodes_String();
}

bool I386::OPCode0xAC()
{
    // LODS m8
    return OPCodes_String();
}

bool I386::OPCode0xAD()
{
    // LODS m16/32
    return OPCodes_String();
}

bool I386::OPCode0xAE()
{
    // SCAS m8
    return OPCodes_String();
}

bool I386::OPCode0xAF()
{
    // SCAS m16/32
    return OPCodes_String();
}

bool I386::OPCode0xB0()
{
    // MOV AL,imm8
    return OPCodes_MOV_Immediate<8>();
}

bool I386::OPCode0xB1()
{
    // MOV CL,imm8
    return OPCodes_MOV_Immediate<8>();
}

bool I386::OPCode0xB2()
{
    // MOV DL,imm8
    return OPCodes_MOV_Immediate<8>();
}

bool I386::OPCode0xB3()
{
    // MOV BL,imm8
    return OPCodes_MOV_Immediate<8>();
}

bool I386::OPCode0xB4()
{
    // MOV AH,imm8
    return OPCodes_MOV_Immediate<8>();
}

bool I386::OPCode0xB5()
{
    // MOV CH,imm8
    return OPCodes_MOV_Immediate<8>();
}

bool I386::OPCode0xB6()
{
    // MOV DH,imm8
    return OPCodes_MOV_Immediate<8>();
}

bool I386::OPCode0xB7()
{
    // MOV BH,imm8
    return OPCodes_MOV_Immediate<8>();
}

bool I386::OPCode0xB8()
{
    // MOV AX/EAX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_MOV_Immediate<32>() : OPCodes_MOV_Immediate<16>();
}

bool I386::OPCode0xB9()
{
    // MOV CX/ECX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_MOV_Immediate<32>() : OPCodes_MOV_Immediate<16>();
}

bool I386::OPCode0xBA()
{
    // MOV DX/EDX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_MOV_Immediate<32>() : OPCodes_MOV_Immediate<16>();
}

bool I386::OPCode0xBB()
{
    // MOV BX/EBX,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_MOV_Immediate<32>() : OPCodes_MOV_Immediate<16>();
}

bool I386::OPCode0xBC()
{
    // MOV SP/ESP,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_MOV_Immediate<32>() : OPCodes_MOV_Immediate<16>();
}

bool I386::OPCode0xBD()
{
    // MOV BP/EBP,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_MOV_Immediate<32>() : OPCodes_MOV_Immediate<16>();
}

bool I386::OPCode0xBE()
{
    // MOV SI/ESI,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_MOV_Immediate<32>() : OPCodes_MOV_Immediate<16>();
}

bool I386::OPCode0xBF()
{
    // MOV DI/EDI,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_MOV_Immediate<32>() : OPCodes_MOV_Immediate<16>();
}

bool I386::OPCode0xC0()
{
    // Group 2 r/m8,imm8
    return OPCodes_Group2<8, I386_SHIFT_COUNT_IMMEDIATE>();
}

bool I386::OPCode0xC1()
{
    // Group 2 r/m16/32,imm8
    return m_instruction.operand_size == 4 ?
        OPCodes_Group2<32, I386_SHIFT_COUNT_IMMEDIATE>() : OPCodes_Group2<16, I386_SHIFT_COUNT_IMMEDIATE>();
}

bool I386::OPCode0xC2()
{
    // RET near imm16
    return TrackReturn(m_instruction.operand_size == 4 ? OPCodes_RET_Near<32>() : OPCodes_RET_Near<16>());
}

bool I386::OPCode0xC3()
{
    // RET near
    return TrackReturn(m_instruction.operand_size == 4 ? OPCodes_RET_Near<32>() : OPCodes_RET_Near<16>());
}

bool I386::OPCode0xC4()
{
    // LES r16/32,m16:16/32
    return OPCodes_LES_LDS();
}

bool I386::OPCode0xC5()
{
    // LDS r16/32,m16:16/32
    return OPCodes_LES_LDS();
}

bool I386::OPCode0xC6()
{
    // MOV r/m8,imm8
    return OPCodes_MOV_RM_Immediate<8>();
}

bool I386::OPCode0xC7()
{
    // MOV r/m16/32,imm16/32
    return m_instruction.operand_size == 4 ? OPCodes_MOV_RM_Immediate<32>() : OPCodes_MOV_RM_Immediate<16>();
}

bool I386::OPCode0xC8()
{
    // ENTER imm16,imm8
    return OPCodes_ENTER();
}

bool I386::OPCode0xC9()
{
    // LEAVE
    return OPCodes_LEAVE();
}

bool I386::OPCode0xCA()
{
    // RET far imm16
    return TrackReturn(OPCodes_RET_Far());
}

bool I386::OPCode0xCB()
{
    // RET far
    return TrackReturn(OPCodes_RET_Far());
}

bool I386::OPCode0xCC()
{
    // INT 3
    return OPCodes_INT();
}

bool I386::OPCode0xCD()
{
    // INT imm8
    return OPCodes_INT();
}

bool I386::OPCode0xCE()
{
    // INTO
    return OPCodes_INT();
}

bool I386::OPCode0xCF()
{
    // IRET/IRETD
    return TrackReturn(OPCodes_IRET());
}

bool I386::OPCode0xD0()
{
    // Group 2 r/m8,1
    return OPCodes_Group2<8, I386_SHIFT_COUNT_ONE>();
}

bool I386::OPCode0xD1()
{
    // Group 2 r/m16/32,1
    return m_instruction.operand_size == 4 ?
        OPCodes_Group2<32, I386_SHIFT_COUNT_ONE>() : OPCodes_Group2<16, I386_SHIFT_COUNT_ONE>();
}

bool I386::OPCode0xD2()
{
    // Group 2 r/m8,CL
    return OPCodes_Group2<8, I386_SHIFT_COUNT_CL>();
}

bool I386::OPCode0xD3()
{
    // Group 2 r/m16/32,CL
    return m_instruction.operand_size == 4 ?
        OPCodes_Group2<32, I386_SHIFT_COUNT_CL>() : OPCodes_Group2<16, I386_SHIFT_COUNT_CL>();
}

bool I386::OPCode0xD4()
{
    // AAM imm8
    return OPCodes_AAM();
}

bool I386::OPCode0xD5()
{
    // AAD imm8
    return OPCodes_AAD();
}

bool I386::OPCode0xD6()
{
    // SALC (undocumented)
    return OPCodes_SALC();
}

bool I386::OPCode0xD7()
{
    // XLAT
    return OPCodes_XLAT();
}

bool I386::OPCode0xD8()
{
    // ESC D8
    return OPCodes_Escape();
}

bool I386::OPCode0xD9()
{
    // ESC D9
    return OPCodes_Escape();
}

bool I386::OPCode0xDA()
{
    // ESC DA
    return OPCodes_Escape();
}

bool I386::OPCode0xDB()
{
    // ESC DB
    return OPCodes_Escape();
}

bool I386::OPCode0xDC()
{
    // ESC DC
    return OPCodes_Escape();
}

bool I386::OPCode0xDD()
{
    // ESC DD
    return OPCodes_Escape();
}

bool I386::OPCode0xDE()
{
    // ESC DE
    return OPCodes_Escape();
}

bool I386::OPCode0xDF()
{
    // ESC DF
    return OPCodes_Escape();
}

bool I386::OPCode0xE0()
{
    // LOOPNE/LOOPNZ rel8
    return OPCodes_LOOP();
}

bool I386::OPCode0xE1()
{
    // LOOPE/LOOPZ rel8
    return OPCodes_LOOP();
}

bool I386::OPCode0xE2()
{
    // LOOP rel8
    return OPCodes_LOOP();
}

bool I386::OPCode0xE3()
{
    // JCXZ/JECXZ rel8
    return OPCodes_JCXZ();
}

bool I386::OPCode0xE4()
{
    // IN AL,imm8
    return OPCodes_IN();
}

bool I386::OPCode0xE5()
{
    // IN AX/EAX,imm8
    return OPCodes_IN();
}

bool I386::OPCode0xE6()
{
    // OUT imm8,AL
    return OPCodes_OUT();
}

bool I386::OPCode0xE7()
{
    // OUT imm8,AX/EAX
    return OPCodes_OUT();
}

bool I386::OPCode0xE8()
{
    // CALL near rel16/32
    u16 cs = m_state.segments[I386_SEGMENT_CS].selector;
    u32 base = m_state.segments[I386_SEGMENT_CS].base;
    return TrackCall(m_instruction.operand_size == 4 ? OPCodes_CALL_Near<32>() : OPCodes_CALL_Near<16>(), cs, base);
}

bool I386::OPCode0xE9()
{
    // JMP near rel16/32
    return OPCodes_JMP_Near();
}

bool I386::OPCode0xEA()
{
    // JMP far ptr16:16/32
    return OPCodes_JMP_Far();
}

bool I386::OPCode0xEB()
{
    // JMP short rel8
    return OPCodes_JMP_Short();
}

bool I386::OPCode0xEC()
{
    // IN AL,DX
    return OPCodes_IN();
}

bool I386::OPCode0xED()
{
    // IN AX/EAX,DX
    return OPCodes_IN();
}

bool I386::OPCode0xEE()
{
    // OUT DX,AL
    return OPCodes_OUT();
}

bool I386::OPCode0xEF()
{
    // OUT DX,AX/EAX
    return OPCodes_OUT();
}

bool I386::OPCode0xF0()
{
    // LOCK
    return PrefixLock();
}

// bool I386::OPCode0xF1()
// {
// }

bool I386::OPCode0xF2()
{
    // REPNE
    return PrefixRepeat(2);
}

bool I386::OPCode0xF3()
{
    // REP/REPE
    return PrefixRepeat(3);
}

bool I386::OPCode0xF4()
{
    // HLT
    return OPCodes_HLT();
}

bool I386::OPCode0xF5()
{
    // CMC
    return OPCodes_CMC();
}

bool I386::OPCode0xF6()
{
    // Group 3 r/m8
    return OPCodes_Group3<8>();
}

bool I386::OPCode0xF7()
{
    // Group 3 r/m16/32
    return m_instruction.operand_size == 4 ? OPCodes_Group3<32>() : OPCodes_Group3<16>();
}

bool I386::OPCode0xF8()
{
    // CLC
    return OPCodes_CLC();
}

bool I386::OPCode0xF9()
{
    // STC
    return OPCodes_STC();
}

bool I386::OPCode0xFA()
{
    // CLI
    return OPCodes_CLI();
}

bool I386::OPCode0xFB()
{
    // STI
    return OPCodes_STI();
}

bool I386::OPCode0xFC()
{
    // CLD
    return OPCodes_CLD();
}

bool I386::OPCode0xFD()
{
    // STD
    return OPCodes_STD();
}

bool I386::OPCode0xFE()
{
    // Group 4 r/m8
    return OPCodes_Group4();
}

bool I386::OPCode0xFF()
{
    // Group 5 r/m16/32
    u16 cs = m_state.segments[I386_SEGMENT_CS].selector;
    u32 base = m_state.segments[I386_SEGMENT_CS].base;
    bool completed = m_instruction.operand_size == 4 ? OPCodes_Group5<32>() : OPCodes_Group5<16>();

    // CALL near and CALL far are the call stack entries of the group
    if (m_instruction.reg != 2 && m_instruction.reg != 3)
        return completed;

    return TrackCall(completed, cs, base);
}

bool I386::IsLockAllowed(const InstructionContext& instruction) const
{
    if (!instruction.memory_operand)
        return false;

    if (instruction.two_byte)
    {
        u8 opcode = instruction.opcode2;

        // BTS, BTR, and BTC with a register bit index
        if (opcode == 0xAB || opcode == 0xB3 || opcode == 0xBB)
            return true;

        // Group 8 permits LOCK on BTS, BTR, and BTC, but not BT
        return opcode == 0xBA && instruction.reg >= 5;
    }

    // ADD/OR/ADC/SBB/AND/SUB/XOR r/m,reg CMP never writes its operand
    u8 opcode = instruction.opcode;

    if (opcode <= 0x3F && (opcode & 7) <= 1 && ((opcode >> 3) & 7) != 7)
        return true;

    // Group 1 immediate ALU excluding CMP
    if (opcode >= 0x80 && opcode <= 0x83)
        return instruction.reg != 7;

    // XCHG with a memory operand always has an implicit lock
    if (opcode == 0x86 || opcode == 0x87)
        return true;

    // Group 3 NOT and NEG
    if (opcode == 0xF6 || opcode == 0xF7)
        return instruction.reg == 2 || instruction.reg == 3;

    // Group 4/5 INC and DEC
    if (opcode == 0xFE || opcode == 0xFF)
        return instruction.reg <= 1;

    return false;
}

bool I386::OPCodes_Invalid()
{
    u8 opcode = m_instruction.two_byte ? m_instruction.opcode2 : m_instruction.opcode;

    if (!DecodeAndStart(OPCodeHasModRM(m_instruction.two_byte, opcode), 0, 0, 0))
        return false;

    return RaiseException(6, I386_EXCEPTION_FAULT);
}

bool I386::OPCodes_ARPL()
{
    if (!DecodeAndStart(true, 0, 0, 0))
        return false;

    if (m_state.execution_mode != I386_MODE_PROTECTED)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    m_step.clocks = (m_instruction.memory_operand ? 21 : 20) + m_address_clocks;

    u32 destination = 0;

    if (!ReadRM(m_instruction, 16, *m_bus_context, destination))
        return false;

    u32 source = GetRegister(m_instruction.reg, 16);

    if ((destination & 3) < (source & 3))
    {
        destination = (destination & ~3U) | (source & 3);

        if (!WriteRM(m_instruction, 16, destination, *m_bus_context))
            return false;

        m_state.eflags |= I386_FLAG_ZF;
    }
    else
        m_state.eflags &= ~I386_FLAG_ZF;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_DAA()
{
    if (!DecodeAndStart(false, 0, 4, 4))
        return false;

    u8 old_al = GetRegister8(0);
    bool old_cf = (m_state.eflags & I386_FLAG_CF) != 0;
    u8 value = old_al;

    if ((value & 0x0F) > 9 || (m_state.eflags & I386_FLAG_AF) != 0)
    {
        value = (u8)(value + 6);
        m_state.eflags |= I386_FLAG_AF;
    }
    else
        m_state.eflags &= ~I386_FLAG_AF;

    if (old_al > 0x99 || old_cf)
    {
        value = (u8)(value + 0x60);
        m_state.eflags |= I386_FLAG_CF;
    }
    else
        m_state.eflags &= ~I386_FLAG_CF;

    SetRegister8(0, value);
    SetSZP(value, 8);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_DAS()
{
    if (!DecodeAndStart(false, 0, 4, 4))
        return false;

    u8 old_al = GetRegister8(0);
    bool old_cf = (m_state.eflags & I386_FLAG_CF) != 0;
    u8 value = old_al;
    bool new_cf = false;

    if ((value & 0x0F) > 9 || (m_state.eflags & I386_FLAG_AF) != 0)
    {
        if (old_al < 6 || old_cf)
            new_cf = true;

        value = (u8)(value - 6);
        m_state.eflags |= I386_FLAG_AF;
    }
    else
        m_state.eflags &= ~I386_FLAG_AF;

    if (old_al > 0x99 || old_cf)
    {
        value = (u8)(value - 0x60);
        new_cf = true;
    }

    if (new_cf)
        m_state.eflags |= I386_FLAG_CF;
    else
        m_state.eflags &= ~I386_FLAG_CF;

    SetRegister8(0, value);
    SetSZP(value, 8);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_AAA()
{
    if (!DecodeAndStart(false, 0, 4, 4))
        return false;

    u16 ax = m_state.registers[I386_REG_EAX].low;

    if ((ax & 0x0F) > 9 || (m_state.eflags & I386_FLAG_AF) != 0)
    {
        ax = (u16)(ax + 0x0106);
        m_state.eflags |= I386_FLAG_AF | I386_FLAG_CF;
    }
    else
        m_state.eflags &= ~(I386_FLAG_AF | I386_FLAG_CF);

    m_state.registers[I386_REG_EAX].low = ax & 0xFF0F;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_AAS()
{
    if (!DecodeAndStart(false, 0, 4, 4))
        return false;

    u16 ax = m_state.registers[I386_REG_EAX].low;

    if ((ax & 0x0F) > 9 || (m_state.eflags & I386_FLAG_AF) != 0)
    {
        ax = (u16)(ax - 0x0106);
        m_state.eflags |= I386_FLAG_AF | I386_FLAG_CF;
    }
    else
        m_state.eflags &= ~(I386_FLAG_AF | I386_FLAG_CF);

    m_state.registers[I386_REG_EAX].low = ax & 0xFF0F;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_BOUND()
{
    if (!DecodeAndStart(true, 0, 10, 10))
        return false;

    int operand_width = m_instruction.operand_size * 8;

    if (!m_instruction.memory_operand)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    u32 lower = 0;
    u32 upper = 0;
    u32 upper_offset =
        Truncate(m_instruction.effective_offset + m_instruction.operand_size, m_instruction.address_size * 8);

    if (!ReadMemory(m_instruction.segment, m_instruction.effective_offset, operand_width, *m_bus_context, lower))
        return false;

    if (!ReadMemory(m_instruction.segment, upper_offset, operand_width, *m_bus_context, upper))
        return false;

    s32 value = SignExtend(GetRegister(m_instruction.reg, operand_width), operand_width);

    if (value < SignExtend(lower, operand_width) || value > SignExtend(upper, operand_width))
    {
        m_step.clocks++;
        return RaiseException(5, I386_EXCEPTION_FAULT);
    }

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_IMUL_Immediate()
{
    if (!DecodeOperands(true, m_instruction.opcode == 0x69 ? m_instruction.operand_size : 1))
        return false;

    // The immediate is the multiplier
    // 6B sign-extends it to the operand size first
    int multiplier_width = m_instruction.operand_size * 8;
    u32 multiplier = m_instruction.opcode == 0x6B ? (u32)SignExtend(m_instruction.immediate, 8) :
        m_instruction.immediate;
    u32 clocks = GetMultiplyClocks(multiplier, multiplier_width, true, m_instruction.memory_operand);

    if (!StartExecution(clocks))
        return false;

    u8 opcode = m_instruction.opcode;
    int operand_width = m_instruction.operand_size * 8;
    u32 source = 0;

    if (!ReadRM(m_instruction, operand_width, *m_bus_context, source))
        return false;

    s64 left = SignExtend(source, operand_width);
    s64 right = SignExtend(m_instruction.immediate, opcode == 0x6B ? 8 : operand_width);
    s64 product = left * right;
    u32 value = Truncate((u64)product, operand_width);

    SetRegister(m_instruction.reg, operand_width, value);

    bool overflow = product != (s64)SignExtend(value, operand_width);

    m_state.eflags &= ~(I386_FLAG_CF | I386_FLAG_OF);

    if (overflow)
        m_state.eflags |= I386_FLAG_CF | I386_FLAG_OF;

    CommitEIP(m_instruction);
    return true;
}

template<int width>
bool I386::OPCodes_TEST_RM()
{
    if (!DecodeAndStart(true, 0, 2, 5))
        return false;

    u32 value = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, value))
        return false;

    Logic(value & GetRegister(m_instruction.reg, width), width);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_XCHG_RM()
{
    if (!DecodeAndStart(true, 0, 3, 5))
        return false;

    u8 opcode = m_instruction.opcode;
    int operand_width = m_instruction.operand_size * 8;
    int width = opcode == 0x86 ? 8 : operand_width;
    u32 rm_value = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, rm_value))
        return false;

    if (!WriteRM(m_instruction, width, GetRegister(m_instruction.reg, width), *m_bus_context))
        return false;

    SetRegister(m_instruction.reg, width, rm_value);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_MOV_RM_Segment()
{
    if (!DecodeAndStart(true, 0, 2, 2))
        return false;

    int operand_width = m_instruction.operand_size * 8;

    if (m_instruction.reg >= I386_SEGMENT_COUNT)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (m_instruction.memory_operand)
    {
        if (!WriteRM(m_instruction, 16, m_state.segments[m_instruction.reg].selector, *m_bus_context))
            return false;
    }
    else
        SetRegister(m_instruction.rm, operand_width, m_state.segments[m_instruction.reg].selector);

    CommitEIP(m_instruction);
    return true;
}

template<int width>
bool I386::OPCodes_LEA()
{
    if (!DecodeAndStart(true, 0, 2, 2))
        return false;

    if (!m_instruction.memory_operand)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    SetRegister(m_instruction.reg, width, m_instruction.effective_offset);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_MOV_Segment_RM()
{
    if (!DecodeAndStart(true, 0, 2, 5))
        return false;

    if (m_instruction.reg >= I386_SEGMENT_COUNT || m_instruction.reg == I386_SEGMENT_CS)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (m_state.execution_mode == I386_MODE_PROTECTED)
        m_step.clocks = (m_instruction.memory_operand ? 19 : 18) + m_address_clocks;

    u32 value = 0;

    if (!ReadRM(m_instruction, 16, *m_bus_context, value))
        return false;

    if (!LoadSegment(m_instruction.reg, (u16)value, *m_bus_context))
        return false;

    if (m_instruction.reg == I386_SEGMENT_SS)
    {
        m_state.interrupt_shadow = I386_SHADOW_MOV_SS;
        m_state.interrupt_shadow_steps = 2;
    }

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_NOP()
{
    if (!DecodeAndStart(false, 0, 3, 3))
        return false;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_XCHG_Accumulator()
{
    if (!DecodeAndStart(false, 0, 3, 3))
        return false;

    u8 opcode = m_instruction.opcode;
    int operand_width = m_instruction.operand_size * 8;
    int reg = opcode & 7;
    u32 value = GetRegister(reg, operand_width);

    SetRegister(reg, operand_width, GetRegister(I386_REG_EAX, operand_width));
    SetRegister(I386_REG_EAX, operand_width, value);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_CBW_CWDE()
{
    if (!DecodeAndStart(false, 0, 3, 3))
        return false;

    int operand_width = m_instruction.operand_size * 8;

    if (operand_width == 16)
        m_state.registers[I386_REG_EAX].low = (u16)(s16)(s8)GetRegister8(0);
    else
        m_state.registers[I386_REG_EAX].value = (u32)(s32)(s16)m_state.registers[I386_REG_EAX].low;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_CWD_CDQ()
{
    if (!DecodeAndStart(false, 0, 2, 2))
        return false;

    int operand_width = m_instruction.operand_size * 8;

    if (operand_width == 16)
        m_state.registers[I386_REG_EDX].low = (m_state.registers[I386_REG_EAX].low & 0x8000) != 0 ? 0xFFFF : 0;
    else
        m_state.registers[I386_REG_EDX].value =
            (m_state.registers[I386_REG_EAX].value & 0x80000000U) != 0 ? 0xFFFFFFFFU : 0;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_WAIT()
{
    if (!DecodeAndStart(false, 0, 6, 6))
        return false;

    if ((m_state.cr0 & 0x0A) == 0x0A)
        return RaiseException(7, I386_EXCEPTION_FAULT);

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_SAHF()
{
    if (!DecodeAndStart(false, 0, 3, 3))
        return false;

    m_state.eflags = (m_state.eflags & ~0xD5U) | (m_state.registers[I386_REG_EAX].byte1 & 0xD5U) | I386_FLAG_FIXED;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_LAHF()
{
    if (!DecodeAndStart(false, 0, 2, 2))
        return false;

    m_state.registers[I386_REG_EAX].byte1 = (u8)m_state.eflags;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_MOV_Moffs()
{
    if (!DecodeOperands(false, m_instruction.address_size))
        return false;

    if (!StartExecution((m_instruction.opcode == 0xA0 || m_instruction.opcode == 0xA1) ? 4 : 2))
        return false;

    u8 opcode = m_instruction.opcode;
    int operand_width = m_instruction.operand_size * 8;
    int width = (opcode == 0xA0 || opcode == 0xA2) ? 8 : operand_width;
    int segment = m_instruction.segment_override == 0xFF ? I386_SEGMENT_DS : m_instruction.segment_override;

    if (opcode <= 0xA1)
    {
        u32 value = 0;

        if (!ReadMemory(segment, m_instruction.immediate, width, *m_bus_context, value))
            return false;

        SetRegister(I386_REG_EAX, width, value);
    }
    else
    {
        if (!WriteMemory(segment, m_instruction.immediate, width, GetRegister(I386_REG_EAX, width), *m_bus_context))
            return false;
    }

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_TEST_Accumulator()
{
    if (!DecodeAndStart(false, m_instruction.opcode == 0xA8 ? 1 : m_instruction.operand_size, 2, 2))
        return false;

    u8 opcode = m_instruction.opcode;
    int operand_width = m_instruction.operand_size * 8;
    int width = opcode == 0xA8 ? 8 : operand_width;

    Logic(GetRegister(I386_REG_EAX, width) & m_instruction.immediate, width);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_LES_LDS()
{
    if (!DecodeAndStart(true, 0, 7, 7))
        return false;

    u8 opcode = m_instruction.opcode;
    int operand_width = m_instruction.operand_size * 8;

    if (!m_instruction.memory_operand)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (m_state.execution_mode == I386_MODE_PROTECTED)
        m_step.clocks = 22 + m_address_clocks;

    u32 offset = 0;
    u32 selector = 0;
    u32 selector_offset =
        Truncate(m_instruction.effective_offset + m_instruction.operand_size, m_instruction.address_size * 8);

    if (!ReadMemory(m_instruction.segment, m_instruction.effective_offset, operand_width, *m_bus_context, offset))
        return false;

    if (!ReadMemory(m_instruction.segment, selector_offset, 16, *m_bus_context, selector))
        return false;

    if (!LoadSegment(opcode == 0xC4 ? I386_SEGMENT_ES : I386_SEGMENT_DS, (u16)selector, *m_bus_context))
        return false;

    SetRegister(m_instruction.reg, operand_width, offset);
    CommitEIP(m_instruction);
    return true;
}

template<int width>
bool I386::OPCodes_MOV_RM_Immediate()
{
    if (!DecodeAndStart(true, width / 8, 2, 2))
        return false;

    if (m_instruction.reg != 0)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (!WriteRM(m_instruction, width, m_instruction.immediate, *m_bus_context))
        return false;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_AAM()
{
    if (!DecodeAndStart(false, 1, 17, 17))
        return false;

    u8 base = (u8)m_instruction.immediate;

    if (base == 0)
    {
        // The divider faults after its first internal shift. AX is not committed, but 286/386 hardware exposes SZP
        // from this value
        SetSZP(GetRegister8(0) >> 1, 8);
        return RaiseException(0, I386_EXCEPTION_FAULT);
    }

    u8 value = GetRegister8(0);

    SetRegister8(4, value / base);
    SetRegister8(0, value % base);
    SetSZP(GetRegister8(0), 8);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_AAD()
{
    if (!DecodeOperands(false, 1))
        return false;

    if (!StartExecution(GetMultiplyClocks(m_instruction.immediate, 8, false, false) + 9))
        return false;

    u8 value = (u8)(GetRegister8(4) * (u8)m_instruction.immediate + GetRegister8(0));

    SetRegister8(0, value);
    SetRegister8(4, 0);
    SetSZP(value, 8);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_SALC()
{
    if (!DecodeOperands(false, 0))
        return false;

    if (!StartExecution((m_state.eflags & I386_FLAG_CF) != 0 ? 4 : 3))
        return false;

    SetRegister8(0, (m_state.eflags & I386_FLAG_CF) != 0 ? 0xFF : 0);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_XLAT()
{
    if (!DecodeAndStart(false, 0, 5, 5))
        return false;

    u32 offset = m_instruction.address_size == 2 ? (u16)(m_state.registers[I386_REG_EBX].low + GetRegister8(0)) :
        m_state.registers[I386_REG_EBX].value + GetRegister8(0);
    int segment = m_instruction.segment_override == 0xFF ? I386_SEGMENT_DS : m_instruction.segment_override;
    u32 value = 0;

    if (!ReadMemory(segment, offset, 8, *m_bus_context, value))
        return false;

    SetRegister8(0, (u8)value);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_IN()
{
    if (DeferIO())
        return true;

    u8 opcode = m_instruction.opcode;
    bool immediate_port = opcode == 0xE4 || opcode == 0xE5;

    if (!DecodeOperands(false, immediate_port ? 1 : 0))
        return false;

    if (!StartExecution(immediate_port ? 12 : 13))
        return false;

    int operand_width = m_instruction.operand_size * 8;
    int width = (opcode == 0xE4 || opcode == 0xEC) ? 8 : operand_width;
    u16 port = immediate_port ? (u8)m_instruction.immediate : m_state.registers[I386_REG_EDX].low;
    bool allowed = false;

    if (!CheckIOPermission(port, width, *m_bus_context, allowed))
        return false;

    if (m_state.execution_mode != I386_MODE_REAL)
    {
        bool permission_check = m_state.execution_mode == I386_MODE_VM86 ||
            m_state.current_privilege_level > GetIOPrivilegeLevel();

        m_step.clocks = permission_check ? (immediate_port ? 26 : 27) : (immediate_port ? 6 : 7);
    }

    if (!allowed)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

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

    SetRegister(I386_REG_EAX, width, value);
    m_bus_context->end_batch = true;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_OUT()
{
    if (DeferIO())
        return true;

    u8 opcode = m_instruction.opcode;
    bool immediate_port = opcode == 0xE6 || opcode == 0xE7;

    if (!DecodeOperands(false, immediate_port ? 1 : 0))
        return false;

    if (!StartExecution(immediate_port ? 10 : 11))
        return false;

    int operand_width = m_instruction.operand_size * 8;
    int width = (opcode == 0xE6 || opcode == 0xEE) ? 8 : operand_width;
    u16 port = immediate_port ? (u8)m_instruction.immediate : m_state.registers[I386_REG_EDX].low;
    bool allowed = false;

    if (!CheckIOPermission(port, width, *m_bus_context, allowed))
        return false;

    if (m_state.execution_mode != I386_MODE_REAL)
    {
        bool permission_check = m_state.execution_mode == I386_MODE_VM86 ||
            m_state.current_privilege_level > GetIOPrivilegeLevel();

        m_step.clocks = permission_check ? (immediate_port ? 24 : 25) : (immediate_port ? 4 : 5);
    }

    if (!allowed)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u32 value = GetRegister(I386_REG_EAX, width);

    if (IsValidPointer(m_io))
    {
        if (width == 8)
            m_io->Write8(port, (u8)value, *m_bus_context);
        else if (width == 16)
            m_io->Write16(port, (u16)value, *m_bus_context);
        else
            m_io->Write32(port, value, *m_bus_context);
    }

    if (unlikely(m_debugger_io_checks))
        RecordDebuggerIO(port, value, (u32)width >> 3, true);

    m_bus_context->end_batch = true;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_HLT()
{
    if (!DecodeAndStart(false, 0, 5, 5))
        return false;

    if (m_state.execution_mode != I386_MODE_REAL && m_state.current_privilege_level != 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    CommitEIP(m_instruction);
    m_state.halted = true;
    m_step.end_batch = true;
    return true;
}

bool I386::OPCodes_CMC()
{
    if (!DecodeAndStart(false, 0, 2, 2))
        return false;

    m_state.eflags ^= I386_FLAG_CF;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_CLC()
{
    if (!DecodeAndStart(false, 0, 2, 2))
        return false;

    m_state.eflags &= ~I386_FLAG_CF;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_STC()
{
    if (!DecodeAndStart(false, 0, 2, 2))
        return false;

    m_state.eflags |= I386_FLAG_CF;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_CLI()
{
    if (!DecodeAndStart(false, 0, 3, 3))
        return false;

    if (m_state.execution_mode != I386_MODE_REAL && m_state.current_privilege_level > GetIOPrivilegeLevel())
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    m_state.eflags &= ~I386_FLAG_IF;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_STI()
{
    if (!DecodeAndStart(false, 0, 3, 3))
        return false;

    if (m_state.execution_mode != I386_MODE_REAL && m_state.current_privilege_level > GetIOPrivilegeLevel())
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    m_state.eflags |= I386_FLAG_IF;
    m_state.interrupt_shadow = I386_SHADOW_STI;
    m_state.interrupt_shadow_steps = 2;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_CLD()
{
    if (!DecodeAndStart(false, 0, 2, 2))
        return false;

    m_state.eflags &= ~I386_FLAG_DF;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_STD()
{
    if (!DecodeAndStart(false, 0, 2, 2))
        return false;

    m_state.eflags |= I386_FLAG_DF;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_Escape()
{
    if (!DecodeAndStart(true, 0, 2, 6))
        return false;

    if ((m_state.cr0 & 0x0C) != 0)
        return RaiseException(7, I386_EXCEPTION_FAULT);

    if (!m_instruction.memory_operand)
    {
        CommitEIP(m_instruction);
        return true;
    }

    u8 opcode = m_instruction.opcode;
    u8 operation = m_instruction.reg;
    u32 size = 0;
    bool write = false;

    if (opcode == 0xD8 || opcode == 0xDA)
        size = 4;
    else if (opcode == 0xDC)
        size = 8;
    else if (opcode == 0xDE)
        size = 2;
    else if (opcode == 0xD9)
    {
        if (operation == 0)
            size = 4;
        else if (operation == 2 || operation == 3)
        {
            size = 4;
            write = true;
        }
        else if (operation == 4 || operation == 6)
        {
            size = m_instruction.operand_size == 2 ? 14 : 28;
            write = operation == 6;
        }
        else if (operation == 5 || operation == 7)
        {
            size = 2;
            write = operation == 7;
        }
    }
    else if (opcode == 0xDB)
    {
        if (operation == 0)
            size = 4;
        else if (operation == 2 || operation == 3)
        {
            size = 4;
            write = true;
        }
        else if (operation == 5 || operation == 7)
        {
            size = 10;
            write = operation == 7;
        }
    }
    else if (opcode == 0xDD)
    {
        if (operation == 0)
            size = 8;
        else if (operation == 2 || operation == 3)
        {
            size = 8;
            write = true;
        }
        else if (operation == 4 || operation == 6)
        {
            size = m_instruction.operand_size == 2 ? 94 : 108;
            write = operation == 6;
        }
        else if (operation == 7)
        {
            size = 2;
            write = true;
        }
    }
    else
    {
        if (operation == 0)
            size = 2;
        else if (operation == 2 || operation == 3)
        {
            size = 2;
            write = true;
        }
        else if (operation == 4)
            size = 10;
        else if (operation == 5)
            size = 8;
        else if (operation == 6)
        {
            size = 10;
            write = true;
        }
        else if (operation == 7)
        {
            size = 8;
            write = true;
        }
    }

    if (size != 0)
    {
        if (!ProbeMemory(m_instruction.segment, m_instruction.effective_offset, size, write, *m_bus_context))
            return false;
    }

    CommitEIP(m_instruction);
    return true;
}
