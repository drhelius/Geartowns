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

bool I386::OPCode0F_0x00()
{
    // Group 6
    return OPCodes0F_Group6();
}

bool I386::OPCode0F_0x01()
{
    // Group 7
    return OPCodes0F_Group7();
}

bool I386::OPCode0F_0x02()
{
    // LAR r16/32,r/m16
    return OPCodes0F_LAR_LSL();
}

bool I386::OPCode0F_0x03()
{
    // LSL r16/32,r/m16
    return OPCodes0F_LAR_LSL();
}

bool I386::OPCode0F_0x06()
{
    // CLTS
    return OPCodes0F_CLTS();
}

bool I386::OPCode0F_0x20()
{
    // MOV r32,CRn
    return OPCodes0F_MOV_Special();
}

bool I386::OPCode0F_0x21()
{
    // MOV r32,DRn
    return OPCodes0F_MOV_Special();
}

bool I386::OPCode0F_0x22()
{
    // MOV CRn,r32
    return OPCodes0F_MOV_Special();
}

bool I386::OPCode0F_0x23()
{
    // MOV DRn,r32
    return OPCodes0F_MOV_Special();
}

bool I386::OPCode0F_0x24()
{
    // MOV r32,TRn
    return OPCodes0F_MOV_Special();
}

bool I386::OPCode0F_0x26()
{
    // MOV TRn,r32
    return OPCodes0F_MOV_Special();
}

bool I386::OPCode0F_0x80()
{
    // JO rel16/32
    return OPCodes_Jcc<0, true>();
}

bool I386::OPCode0F_0x81()
{
    // JNO rel16/32
    return OPCodes_Jcc<1, true>();
}

bool I386::OPCode0F_0x82()
{
    // JB/JNAE/JC rel16/32
    return OPCodes_Jcc<2, true>();
}

bool I386::OPCode0F_0x83()
{
    // JNB/JAE/JNC rel16/32
    return OPCodes_Jcc<3, true>();
}

bool I386::OPCode0F_0x84()
{
    // JZ/JE rel16/32
    return OPCodes_Jcc<4, true>();
}

bool I386::OPCode0F_0x85()
{
    // JNZ/JNE rel16/32
    return OPCodes_Jcc<5, true>();
}

bool I386::OPCode0F_0x86()
{
    // JBE/JNA rel16/32
    return OPCodes_Jcc<6, true>();
}

bool I386::OPCode0F_0x87()
{
    // JNBE/JA rel16/32
    return OPCodes_Jcc<7, true>();
}

bool I386::OPCode0F_0x88()
{
    // JS rel16/32
    return OPCodes_Jcc<8, true>();
}

bool I386::OPCode0F_0x89()
{
    // JNS rel16/32
    return OPCodes_Jcc<9, true>();
}

bool I386::OPCode0F_0x8A()
{
    // JP/JPE rel16/32
    return OPCodes_Jcc<10, true>();
}

bool I386::OPCode0F_0x8B()
{
    // JNP/JPO rel16/32
    return OPCodes_Jcc<11, true>();
}

bool I386::OPCode0F_0x8C()
{
    // JL/JNGE rel16/32
    return OPCodes_Jcc<12, true>();
}

bool I386::OPCode0F_0x8D()
{
    // JNL/JGE rel16/32
    return OPCodes_Jcc<13, true>();
}

bool I386::OPCode0F_0x8E()
{
    // JLE/JNG rel16/32
    return OPCodes_Jcc<14, true>();
}

bool I386::OPCode0F_0x8F()
{
    // JNLE/JG rel16/32
    return OPCodes_Jcc<15, true>();
}

bool I386::OPCode0F_0x90()
{
    // SETO r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x91()
{
    // SETNO r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x92()
{
    // SETB/SETNAE/SETC r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x93()
{
    // SETNB/SETAE/SETNC r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x94()
{
    // SETZ/SETE r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x95()
{
    // SETNZ/SETNE r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x96()
{
    // SETBE/SETNA r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x97()
{
    // SETNBE/SETA r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x98()
{
    // SETS r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x99()
{
    // SETNS r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x9A()
{
    // SETP/SETPE r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x9B()
{
    // SETNP/SETPO r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x9C()
{
    // SETL/SETNGE r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x9D()
{
    // SETNL/SETGE r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x9E()
{
    // SETLE/SETNG r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0x9F()
{
    // SETNLE/SETG r/m8
    return OPCodes0F_SETcc();
}

bool I386::OPCode0F_0xA0()
{
    // PUSH FS
    return OPCodes0F_PUSH_FS_GS();
}

bool I386::OPCode0F_0xA1()
{
    // POP FS
    return OPCodes0F_POP_FS_GS();
}

bool I386::OPCode0F_0xA3()
{
    // BT r/m16/32,r16/32
    return OPCodes0F_Bit_Register();
}

bool I386::OPCode0F_0xA4()
{
    // SHLD r/m16/32,r16/32,imm8
    return OPCodes0F_DoubleShift();
}

bool I386::OPCode0F_0xA5()
{
    // SHLD r/m16/32,r16/32,CL
    return OPCodes0F_DoubleShift();
}

bool I386::OPCode0F_0xA8()
{
    // PUSH GS
    return OPCodes0F_PUSH_FS_GS();
}

bool I386::OPCode0F_0xA9()
{
    // POP GS
    return OPCodes0F_POP_FS_GS();
}

bool I386::OPCode0F_0xAB()
{
    // BTS r/m16/32,r16/32
    return OPCodes0F_Bit_Register();
}

bool I386::OPCode0F_0xAC()
{
    // SHRD r/m16/32,r16/32,imm8
    return OPCodes0F_DoubleShift();
}

bool I386::OPCode0F_0xAD()
{
    // SHRD r/m16/32,r16/32,CL
    return OPCodes0F_DoubleShift();
}

bool I386::OPCode0F_0xAF()
{
    // IMUL r16/32,r/m16/32
    return OPCodes0F_IMUL();
}

bool I386::OPCode0F_0xB2()
{
    // LSS r16/32,m16:16/32
    return OPCodes0F_LoadFarPointer();
}

bool I386::OPCode0F_0xB3()
{
    // BTR r/m16/32,r16/32
    return OPCodes0F_Bit_Register();
}

bool I386::OPCode0F_0xB4()
{
    // LFS r16/32,m16:16/32
    return OPCodes0F_LoadFarPointer();
}

bool I386::OPCode0F_0xB5()
{
    // LGS r16/32,m16:16/32
    return OPCodes0F_LoadFarPointer();
}

bool I386::OPCode0F_0xB6()
{
    // MOVZX r16/32,r/m8
    return m_instruction.operand_size == 4 ? OPCodes0F_MOVX<32, 8, false>() : OPCodes0F_MOVX<16, 8, false>();
}

bool I386::OPCode0F_0xB7()
{
    // MOVZX r16/32,r/m16
    return m_instruction.operand_size == 4 ? OPCodes0F_MOVX<32, 16, false>() : OPCodes0F_MOVX<16, 16, false>();
}

bool I386::OPCode0F_0xBA()
{
    // Group 8 r/m16/32,imm8
    return OPCodes0F_Group8();
}

bool I386::OPCode0F_0xBB()
{
    // BTC r/m16/32,r16/32
    return OPCodes0F_Bit_Register();
}

bool I386::OPCode0F_0xBC()
{
    // BSF r16/32,r/m16/32
    return OPCodes0F_BitScan();
}

bool I386::OPCode0F_0xBD()
{
    // BSR r16/32,r/m16/32
    return OPCodes0F_BitScan();
}

bool I386::OPCode0F_0xBE()
{
    // MOVSX r16/32,r/m8
    return m_instruction.operand_size == 4 ? OPCodes0F_MOVX<32, 8, true>() : OPCodes0F_MOVX<16, 8, true>();
}

bool I386::OPCode0F_0xBF()
{
    // MOVSX r16/32,r/m16
    return m_instruction.operand_size == 4 ? OPCodes0F_MOVX<32, 16, true>() : OPCodes0F_MOVX<16, 16, true>();
}

INLINE bool I386::StoreSystemSelector(InstructionContext& instruction, GT_Bus_Access_Context& context, u16 selector)
{
    if (!WriteRM(instruction, 16, selector, context))
        return false;

    CommitEIP(instruction);
    return true;
}

INLINE bool I386::OPCodes_SLDT()
{
    return StoreSystemSelector(m_instruction, *m_bus_context, m_state.ldtr.selector);
}

INLINE bool I386::OPCodes_STR()
{
    return StoreSystemSelector(m_instruction, *m_bus_context, m_state.task_register.selector);
}

INLINE bool I386::LoadSystemSelector(InstructionContext& instruction, GT_Bus_Access_Context& context,
    bool load_task_register)
{
    if (m_state.current_privilege_level != 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u32 value = 0;

    if (!ReadRM(instruction, 16, context, value))
        return false;

    u16 selector = (u16)value;

    if (!load_task_register && (selector & 0xFFFC) == 0)
    {
        ClearSegmentCache(selector, m_state.ldtr);
        CommitEIP(instruction);
        return true;
    }

    if ((selector & 0xFFFC) == 0 || (selector & 4) != 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    Descriptor descriptor;

    if (!ReadGDTDescriptor(selector, descriptor, context))
        return false;

    bool valid_type = !load_task_register ? descriptor.system && descriptor.type == 2 :
        descriptor.system && (descriptor.type == 1 || descriptor.type == 9);

    if (!valid_type)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    if (!descriptor.present)
        return RaiseException(11, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    if (load_task_register)
    {
        u8 busy_type = descriptor.type | 2;

        if (!SetDescriptorType(descriptor, busy_type, context))
            return false;

        descriptor.type = busy_type;
        descriptor.attributes &= ~I386_SEGMENT_TYPE_MASK;
        descriptor.attributes |= (u16)busy_type << I386_SEGMENT_TYPE_SHIFT;
        descriptor.attributes |= I386_SEGMENT_ACCESSED;

        LoadDescriptorCache(selector, descriptor, m_state.task_register);
    }
    else
        LoadDescriptorCache(selector, descriptor, m_state.ldtr);

    CommitEIP(instruction);
    return true;
}

INLINE bool I386::OPCodes_LLDT()
{
    return LoadSystemSelector(m_instruction, *m_bus_context, false);
}

INLINE bool I386::OPCodes_LTR()
{
    return LoadSystemSelector(m_instruction, *m_bus_context, true);
}

INLINE bool I386::VerifySegmentPermission(InstructionContext& instruction, GT_Bus_Access_Context& context, bool write)
{
    u32 value = 0;

    if (!ReadRM(instruction, 16, context, value))
        return false;

    Descriptor descriptor;
    bool valid = false;

    if (!ReadDescriptorNoFault((u16)value, descriptor, context, valid))
        return false;

    if (valid)
    {
        bool executable = (descriptor.attributes & I386_SEGMENT_EXECUTABLE) != 0;
        bool conforming = (descriptor.attributes & I386_SEGMENT_CONFORMING) != 0;
        bool privilege_allowed = conforming || descriptor.dpl >= MAX(m_state.current_privilege_level, value & 3);

        if (descriptor.system || !privilege_allowed)
            valid = false;
        else if (!write)
            valid = !executable || (descriptor.attributes & I386_SEGMENT_READABLE) != 0;
        else
            valid = !executable && (descriptor.attributes & I386_SEGMENT_WRITABLE) != 0;
    }

    if (valid)
        m_state.eflags |= I386_FLAG_ZF;
    else
        m_state.eflags &= ~I386_FLAG_ZF;

    CommitEIP(instruction);
    return true;
}

INLINE bool I386::OPCodes_VERR()
{
    return VerifySegmentPermission(m_instruction, *m_bus_context, false);
}

INLINE bool I386::OPCodes_VERW()
{
    return VerifySegmentPermission(m_instruction, *m_bus_context, true);
}

INLINE bool I386::StoreDescriptorTable(InstructionContext& instruction, GT_Bus_Access_Context& context,
    bool interrupt_table)
{
    if (!instruction.memory_operand)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    const I386_Descriptor_Table& table = !interrupt_table ? m_state.gdtr : m_state.idtr;
    u32 base_offset = Truncate(instruction.effective_offset + 2, instruction.address_size * 8);

    if (!CheckMemoryAccess(instruction.segment, instruction.effective_offset, 2, true, context))
        return false;

    if (!CheckMemoryAccess(instruction.segment, base_offset, 4, true, context))
        return false;

    if (!WriteMemory(instruction.segment, instruction.effective_offset, 16, table.limit, context))
        return false;

    if (!WriteMemory(instruction.segment, base_offset, 32, table.base, context))
        return false;

    CommitEIP(instruction);
    return true;
}

INLINE bool I386::LoadDescriptorTable(InstructionContext& instruction, GT_Bus_Access_Context& context,
    bool interrupt_table)
{
    if (!instruction.memory_operand)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (m_state.execution_mode != I386_MODE_REAL && m_state.current_privilege_level != 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u32 limit = 0;
    u32 base = 0;
    u32 base_offset = Truncate(instruction.effective_offset + 2, instruction.address_size * 8);

    if (!ReadMemory(instruction.segment, instruction.effective_offset, 16, context, limit))
        return false;

    if (!ReadMemory(instruction.segment, base_offset, 32, context, base))
        return false;

    if (instruction.operand_size == 2)
        base &= 0x00FFFFFFU;

    I386_Descriptor_Table& table = !interrupt_table ? m_state.gdtr : m_state.idtr;

    table.limit = (u16)limit;
    table.base = base;

    CommitEIP(instruction);
    return true;
}

INLINE bool I386::OPCodes_SGDT()
{
    return StoreDescriptorTable(m_instruction, *m_bus_context, false);
}

INLINE bool I386::OPCodes_SIDT()
{
    return StoreDescriptorTable(m_instruction, *m_bus_context, true);
}

INLINE bool I386::OPCodes_LGDT()
{
    return LoadDescriptorTable(m_instruction, *m_bus_context, false);
}

INLINE bool I386::OPCodes_LIDT()
{
    return LoadDescriptorTable(m_instruction, *m_bus_context, true);
}

INLINE bool I386::OPCodes_SMSW()
{
    int result_width = m_instruction.memory_operand ? 16 : m_instruction.operand_size * 8;

    if (!WriteRM(m_instruction, result_width, m_state.cr0, *m_bus_context))
        return false;

    CommitEIP(m_instruction);
    return true;
}

INLINE bool I386::OPCodes_LMSW()
{
    if (m_state.execution_mode != I386_MODE_REAL && m_state.current_privilege_level != 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u32 value = 0;

    if (!ReadRM(m_instruction, 16, *m_bus_context, value))
        return false;

    u32 old_pe = m_state.cr0 & 1;

    m_state.cr0 = (m_state.cr0 & ~0x0FU) | (value & 0x0F) | old_pe;
    UpdateExecutionMode();

    if (old_pe == 0 && (m_state.cr0 & 1) != 0)
    {
        m_state.current_privilege_level = 0;
        UpdateUserMode();
    }

    CommitEIP(m_instruction);
    return true;
}

INLINE bool I386::MoveControlRegister(u8 special_index, u8 general_index, bool write_special, u32 value)
{
    if (special_index != 0 && special_index != 2 && special_index != 3)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    u32* control_register = special_index == 0 ? &m_state.cr0 : special_index == 2 ? &m_state.cr2 : &m_state.cr3;

    if (write_special)
    {
        u32 old_cr0 = m_state.cr0;

        if (special_index == 0 && (value & 0x80000000U) != 0 && (value & 1) == 0)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        *control_register = value;

        if (special_index == 0)
        {
            UpdateExecutionMode();

            if ((old_cr0 & 1) == 0 && (m_state.cr0 & 1) != 0)
            {
                m_state.current_privilege_level = 0;
                UpdateUserMode();
            }
        }

        if (special_index == 0 || special_index == 3)
            FlushTLB();
    }
    else
        SetRegister(general_index, 32, *control_register);

    return true;
}

INLINE bool I386::MoveDebugRegister(u8 special_index, u8 general_index, bool write_special, u32 value)
{
    if (special_index == 4 || special_index == 5 || special_index > 7)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (write_special)
    {
        m_state.debug_registers[special_index] = value;
        UpdateStepMode();
        UpdateMemoryMode();
    }
    else
        SetRegister(general_index, 32, m_state.debug_registers[special_index]);

    return true;
}

INLINE bool I386::MoveTestRegister(u8 special_index, u8 general_index, bool write_special, u32 value)
{
    if (special_index < 6 || special_index > 7)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (write_special)
    {
        m_state.test_registers[special_index - 6] = value;

        // Writing TR6 issues the TLB test command
        if (special_index == 6)
            TestTLB();
    }
    else
        SetRegister(general_index, 32, m_state.test_registers[special_index - 6]);

    return true;
}

bool I386::OPCodes0F_BitOperation(int operation, u32 bit_index)
{
    int width = m_instruction.operand_size * 8;
    InstructionContext operand = m_instruction;
    u32 bit = bit_index & (width - 1);

    if (m_instruction.memory_operand && m_instruction.opcode2 != 0xBA)
    {
        s32 index = SignExtend(bit_index, width);
        s32 element = index / width;

        if (index < 0 && (index % width) != 0)
            element--;

        u32 adjusted = operand.effective_offset + element * m_instruction.operand_size;
        operand.effective_offset = m_instruction.address_size == 2 ? (u16)adjusted : adjusted;
    }

    u32 value = 0;

    if (!ReadRM(operand, width, *m_bus_context, value))
        return false;

    if ((value & (1U << bit)) != 0)
        m_state.eflags |= I386_FLAG_CF;
    else
        m_state.eflags &= ~I386_FLAG_CF;

    if (operation != 0)
    {
        if (operation == 1)
            value |= 1U << bit;
        else if (operation == 2)
            value &= ~(1U << bit);
        else
            value ^= 1U << bit;

        if (!WriteRM(operand, width, value, *m_bus_context))
            return false;
    }

    return true;
}

bool I386::OPCodes0F_Group6()
{
    static const u8 k_group6_register_clocks[8] = { 2, 23, 20, 23, 10, 15, 0, 0 };
    static const u8 k_group6_memory_clocks[8] = { 2, 27, 20, 27, 11, 16, 0, 0 };

    if (!DecodeOperands(true, 0))
        return false;

    u8 reg = m_instruction.reg;

    if (!StartExecution(m_instruction.memory_operand ? k_group6_memory_clocks[reg] : k_group6_register_clocks[reg]))
        return false;

    if (m_state.execution_mode != I386_MODE_PROTECTED)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    switch (m_instruction.reg)
    {
        case 0:
            return OPCodes_SLDT();
        case 1:
            return OPCodes_STR();
        case 2:
            return OPCodes_LLDT();
        case 3:
            return OPCodes_LTR();
        case 4:
            return OPCodes_VERR();
        case 5:
            return OPCodes_VERW();
        default:
            return RaiseException(6, I386_EXCEPTION_FAULT);
    }
}

bool I386::OPCodes0F_Group7()
{
    static const u8 k_group7_register_clocks[8] = { 9, 9, 11, 11, 2, 0, 10, 0 };
    static const u8 k_group7_memory_clocks[8] = { 9, 9, 11, 11, 2, 0, 13, 0 };

    if (!DecodeOperands(true, 0))
        return false;

    u8 reg = m_instruction.reg;

    if (!StartExecution(m_instruction.memory_operand ? k_group7_memory_clocks[reg] : k_group7_register_clocks[reg]))
        return false;

    switch (m_instruction.reg)
    {
        case 0:
            return OPCodes_SGDT();
        case 1:
            return OPCodes_SIDT();
        case 2:
            return OPCodes_LGDT();
        case 3:
            return OPCodes_LIDT();
        case 4:
            return OPCodes_SMSW();
        case 6:
            return OPCodes_LMSW();
        default:
            return RaiseException(6, I386_EXCEPTION_FAULT);
    }
}

bool I386::OPCodes0F_LAR_LSL()
{
    if (!DecodeOperands(true, 0))
        return false;

    if (!StartExecution((m_instruction.opcode2 == 0x02 ? 15 : 20) + (m_instruction.memory_operand ? 1 : 0)))
        return false;

    u8 opcode = m_instruction.opcode2;
    int width = m_instruction.operand_size * 8;

    if (m_state.execution_mode != I386_MODE_PROTECTED)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    u32 selector = 0;

    if (!ReadRM(m_instruction, 16, *m_bus_context, selector))
        return false;

    Descriptor descriptor;
    bool valid = false;

    if (!ReadDescriptorNoFault((u16)selector, descriptor, *m_bus_context, valid))
        return false;

    if (valid)
    {
        bool conforming = !descriptor.system && (descriptor.attributes & I386_SEGMENT_EXECUTABLE) != 0 &&
            (descriptor.attributes & I386_SEGMENT_CONFORMING) != 0;

        if (!conforming && descriptor.dpl < MAX(m_state.current_privilege_level, selector & 3))
            valid = false;
    }

    if (valid && descriptor.system)
    {
        u8 type = descriptor.type;

        // LAR accepts gates too, LSL only the TSS and LDT descriptors
        if (opcode == 0x02)
            valid = (type >= 1 && type <= 7) || type == 9 || type == 11 || type == 12 || type == 14 || type == 15;
        else
            valid = type == 1 || type == 2 || type == 3 || type == 9 || type == 11;
    }

    if (valid)
    {
        u32 value = opcode == 0x02 ? descriptor.high & 0x00FFFF00U : descriptor.limit;

        if (opcode == 0x03 && (descriptor.attributes & I386_SEGMENT_GRANULAR) != 0)
            m_step.clocks += 5;

        SetRegister(m_instruction.reg, width, value);
        m_state.eflags |= I386_FLAG_ZF;
    }
    else
        m_state.eflags &= ~I386_FLAG_ZF;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_CLTS()
{
    if (!DecodeAndStart(false, 0, 5, 5))
        return false;

    if (m_state.execution_mode != I386_MODE_REAL && m_state.current_privilege_level != 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    m_state.cr0 &= ~0x08U;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_MOV_Special()
{
    if (!DecodeOperands(true, 0))
        return false;

    u32 clocks = 12;

    switch (m_instruction.opcode2)
    {
        case 0x20:
            clocks = 6;
            break;
        case 0x21:
            clocks = m_instruction.reg <= 3 ? 22 : 14;
            break;
        case 0x22:
            clocks = m_instruction.reg == 0 ? 10 : m_instruction.reg == 2 ? 4 : 5;
            break;
        case 0x23:
            clocks = m_instruction.reg <= 3 ? 22 : 16;
            break;
        default:
            break;
    }

    if (!StartExecution(clocks))
        return false;

    u8 opcode = m_instruction.opcode2;

    if (m_instruction.memory_operand)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if ((opcode == 0x21 || opcode == 0x23) && (m_state.debug_registers[7] & 0x00002000U) != 0)
    {
        m_state.debug_registers[7] &= ~0x00002000U;
        m_state.debug_registers[6] |= 0x00002000U;
        return RaiseException(1, I386_EXCEPTION_FAULT);
    }

    if (m_state.execution_mode != I386_MODE_REAL && m_state.current_privilege_level != 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u8 special_index = m_instruction.reg;
    u8 general_index = m_instruction.rm;
    bool write_special = opcode == 0x22 || opcode == 0x23 || opcode == 0x26;
    u32 value = GetRegister(general_index, 32);
    bool ok;

    if (opcode == 0x20 || opcode == 0x22)
        ok = MoveControlRegister(special_index, general_index, write_special, value);
    else if (opcode == 0x21 || opcode == 0x23)
        ok = MoveDebugRegister(special_index, general_index, write_special, value);
    else
        ok = MoveTestRegister(special_index, general_index, write_special, value);

    if (!ok)
        return false;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_PUSH_FS_GS()
{
    if (!DecodeAndStart(false, 0, 2, 2))
        return false;

    u8 opcode = m_instruction.opcode2;
    int width = m_instruction.operand_size * 8;
    int segment = opcode == 0xA0 ? I386_SEGMENT_FS : I386_SEGMENT_GS;

    if (!StackPushSized(m_state.segments[segment].selector, width, *m_bus_context, width == 32 ? 16 : 0))
        return false;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_POP_FS_GS()
{
    if (!DecodeAndStart(false, 0, 7, 7))
        return false;

    u8 opcode = m_instruction.opcode2;

    if (m_state.execution_mode == I386_MODE_PROTECTED)
        m_step.clocks = 21;

    int segment = opcode == 0xA1 ? I386_SEGMENT_FS : I386_SEGMENT_GS;
    int old_stack_size = GetStackAddressSize();
    u32 old_stack = GetStackPointer();
    u32 value = 0;

    if (!ReadMemory(I386_SEGMENT_SS, old_stack, 16, *m_bus_context, value, true))
        return false;

    if (!LoadSegment(segment, (u16)value, *m_bus_context))
        return false;

    if (old_stack_size == 32)
        m_state.registers[I386_REG_ESP].value = old_stack + m_instruction.operand_size;
    else
        m_state.registers[I386_REG_ESP].low = (u16)(old_stack + m_instruction.operand_size);

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_Bit_Register()
{
    if (!DecodeOperands(true, 0))
        return false;

    u8 opcode = m_instruction.opcode2;
    u32 clocks = opcode == 0xA3 ? (m_instruction.memory_operand ? 12 : 3) : (m_instruction.memory_operand ? 13 : 6);

    if (!StartExecution(clocks))
        return false;

    int width = m_instruction.operand_size * 8;
    int bit_operation = opcode == 0xA3 ? 0 : opcode == 0xAB ? 1 : opcode == 0xB3 ? 2 : 3;

    if (!OPCodes0F_BitOperation(bit_operation, GetRegister(m_instruction.reg, width)))
        return false;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_DoubleShift()
{
    u8 opcode = m_instruction.opcode2;
    bool immediate_count = opcode == 0xA4 || opcode == 0xAC;

    if (!DecodeAndStart(true, immediate_count ? 1 : 0, 3, 7))
        return false;

    int width = m_instruction.operand_size * 8;
    u32 count = (immediate_count ? m_instruction.immediate : m_state.registers[I386_REG_ECX].byte0) & 0x1F;
    u32 destination = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, destination))
        return false;

    if (count == 0)
    {
        CommitEIP(m_instruction);
        return true;
    }

    u32 source = GetRegister(m_instruction.reg, width);
    u32 mask = GetMask(width);
    u32 value = 0;
    bool right = opcode == 0xAC || opcode == 0xAD;

    if (width == 16)
    {
        bool carry = false;

        if (count <= 16)
        {
            value = right ?
                (destination >> count) | (source << (16 - count)) : (destination << count) | (source >> (16 - count));
            carry = right ? ((destination >> (count - 1)) & 1) != 0 : ((destination >> (16 - count)) & 1) != 0;
        }
        else
        {
            u32 rotate = count - 16;
            value = right ?
                (source >> rotate) | (source << (16 - rotate)) : (source << rotate) | (source >> (16 - rotate));
            carry = right ? (value & 0x8000) != 0 : (value & 1) != 0;
        }

        value &= 0xFFFF;
        m_state.eflags = carry ? m_state.eflags | I386_FLAG_CF : m_state.eflags & ~I386_FLAG_CF;
    }
    else if (!right)
    {
        u64 pair = ((u64)destination << width) | source;
        u64 shifted = pair << count;

        value = (u32)(shifted >> width) & mask;

        u32 position = (u32)(width * 2) - count;
        bool carry = position < 64 && ((pair >> position) & 1) != 0;

        m_state.eflags = carry ? m_state.eflags | I386_FLAG_CF : m_state.eflags & ~I386_FLAG_CF;
    }
    else
    {
        u64 pair = ((u64)source << width) | destination;
        bool carry = ((pair >> (count - 1)) & 1) != 0;

        value = (u32)(pair >> count) & mask;
        m_state.eflags = carry ? m_state.eflags | I386_FLAG_CF : m_state.eflags & ~I386_FLAG_CF;
    }

    SetSZP(value, width);

    if (count == 1)
    {
        m_state.eflags &= ~I386_FLAG_OF;

        bool sign = (value & GetSignBit(width)) != 0;
        bool carry = (m_state.eflags & I386_FLAG_CF) != 0;
        bool next_sign = (value & (GetSignBit(width) >> 1)) != 0;

        if ((!right && sign != carry) || (right && sign != next_sign))
            m_state.eflags |= I386_FLAG_OF;
    }

    if (!WriteRM(m_instruction, width, value, *m_bus_context))
        return false;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_IMUL()
{
    if (!DecodeAndStart(true, 0, 9, 12))
        return false;

    int width = m_instruction.operand_size * 8;
    u32 source = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, source))
        return false;

    m_step.clocks = GetMultiplyClocks(source, width, true, m_instruction.memory_operand) + m_address_clocks;

    s64 product = (s64)SignExtend(GetRegister(m_instruction.reg, width), width) * SignExtend(source, width);
    u32 value = Truncate((u64)product, width);

    SetRegister(m_instruction.reg, width, value);

    bool overflow = product != (s64)SignExtend(value, width);

    m_state.eflags &= ~(I386_FLAG_CF | I386_FLAG_OF);

    if (overflow)
        m_state.eflags |= I386_FLAG_CF | I386_FLAG_OF;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_LoadFarPointer()
{
    if (!DecodeAndStart(true, 0, 7, 7))
        return false;

    u8 opcode = m_instruction.opcode2;
    int width = m_instruction.operand_size * 8;

    if (!m_instruction.memory_operand)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (m_state.execution_mode == I386_MODE_PROTECTED)
        m_step.clocks = (opcode == 0xB2 ? 22 : 25) + m_address_clocks;

    u32 offset = 0;
    u32 selector = 0;
    u32 selector_offset =
        Truncate(m_instruction.effective_offset + m_instruction.operand_size, m_instruction.address_size * 8);

    if (!ReadMemory(m_instruction.segment, m_instruction.effective_offset, width, *m_bus_context, offset))
        return false;

    if (!ReadMemory(m_instruction.segment, selector_offset, 16, *m_bus_context, selector))
        return false;

    int segment = opcode == 0xB2 ? I386_SEGMENT_SS : opcode == 0xB4 ? I386_SEGMENT_FS : I386_SEGMENT_GS;

    if (!LoadSegment(segment, (u16)selector, *m_bus_context))
        return false;

    SetRegister(m_instruction.reg, width, offset);

    if (segment == I386_SEGMENT_SS)
    {
        m_state.interrupt_shadow = I386_SHADOW_MOV_SS;
        m_state.interrupt_shadow_steps = 2;
    }

    CommitEIP(m_instruction);
    return true;
}

template<int width, int source_width, bool sign_extend>
bool I386::OPCodes0F_MOVX()
{
    if (!DecodeAndStart(true, 0, 3, 6))
        return false;

    u32 source = 0;

    if (!ReadRM(m_instruction, source_width, *m_bus_context, source))
        return false;

    u32 value = sign_extend ? (u32)SignExtend(source, source_width) : source;

    SetRegister(m_instruction.reg, width, value);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_Group8()
{
    if (!DecodeOperands(true, 1))
        return false;

    u32 clocks = m_instruction.reg == 4 ? (m_instruction.memory_operand ? 6 : 3) :
        m_instruction.reg >= 5 ? (m_instruction.memory_operand ? 8 : 6) : 0;

    if (!StartExecution(clocks))
        return false;

    if (m_instruction.reg < 4)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (!OPCodes0F_BitOperation(m_instruction.reg - 4, m_instruction.immediate))
        return false;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_BitScan()
{
    if (!DecodeAndStart(true, 0, 10, 10))
        return false;

    u8 opcode = m_instruction.opcode2;
    int width = m_instruction.operand_size * 8;
    u32 source = 0;

    if (!ReadRM(m_instruction, width, *m_bus_context, source))
        return false;

    m_step.clocks = GetBitScanClocks(source, width, opcode == 0xBD) + m_address_clocks;

    if (source == 0)
        m_state.eflags |= I386_FLAG_ZF;
    else
    {
        m_state.eflags &= ~I386_FLAG_ZF;

        int bit = opcode == 0xBC ? 0 : width - 1;

        if (opcode == 0xBC)
        {
            while (((source >> bit) & 1) == 0)
                bit++;
        }
        else
        {
            while (((source >> bit) & 1) == 0)
                bit--;
        }

        SetRegister(m_instruction.reg, width, (u32)bit);
    }

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes0F_SETcc()
{
    if (!DecodeAndStart(true, 0, 4, 5))
        return false;

    u8 opcode = m_instruction.opcode2;

    if (!WriteRM(m_instruction, 8, CheckCondition(opcode & 15) ? 1 : 0, *m_bus_context))
        return false;

    CommitEIP(m_instruction);
    return true;
}
