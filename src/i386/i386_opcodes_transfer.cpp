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

bool I386::OPCodes_PUSH_Segment()
{
    if (!DecodeAndStart(false, 0, 2, 2))
        return false;

    u8 opcode = m_instruction.opcode;
    int operand_width = m_instruction.operand_size * 8;
    int segment = opcode == 0x06 ? I386_SEGMENT_ES : opcode == 0x0E ? I386_SEGMENT_CS :
        opcode == 0x16 ? I386_SEGMENT_SS : I386_SEGMENT_DS;

    if (!StackPushSized(m_segments[segment].selector, operand_width, *m_bus_context, operand_width == 32 ? 16 : 0))
        return false;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_POP_Segment()
{
    if (!DecodeAndStart(false, 0, 7, 7))
        return false;

    u8 opcode = m_instruction.opcode;

    if (m_execution_mode == I386_MODE_PROTECTED)
        m_step.clocks = 21;

    int segment = opcode == 0x07 ? I386_SEGMENT_ES : opcode == 0x17 ? I386_SEGMENT_SS : I386_SEGMENT_DS;
    int old_stack_size = GetStackAddressSize();
    u32 old_stack = GetStackPointer();
    u32 value = 0;

    if (!ReadMemory(I386_SEGMENT_SS, old_stack, 16, *m_bus_context, value, true))
        return false;

    if (!LoadSegment(segment, (u16)value, *m_bus_context))
        return false;

    if (old_stack_size == 32)
        m_registers[I386_REG_ESP].value = old_stack + m_instruction.operand_size;
    else
        m_registers[I386_REG_ESP].low = (u16)(old_stack + m_instruction.operand_size);

    if (segment == I386_SEGMENT_SS)
    {
        m_interrupt_shadow = I386_SHADOW_MOV_SS;
        m_interrupt_shadow_steps = 2;
    }

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_PUSHA()
{
    if (!DecodeAndStart(false, 0, 18, 18))
        return false;

    int operand_width = m_instruction.operand_size * 8;
    u32 original_stack = GetStackPointer();
    u32 stack_mask = GetStackAddressSize() == 32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 stack = (original_stack - m_instruction.operand_size * 8) & stack_mask;

    u32 values[8] =
    {
        GetRegister(I386_REG_EDI, operand_width),
        GetRegister(I386_REG_ESI, operand_width),
        GetRegister(I386_REG_EBP, operand_width),
        GetRegister(I386_REG_ESP, operand_width),
        GetRegister(I386_REG_EBX, operand_width),
        GetRegister(I386_REG_EDX, operand_width),
        GetRegister(I386_REG_ECX, operand_width),
        GetRegister(I386_REG_EAX, operand_width)
    };

    for (int i = 0; i < 8; i++)
    {
        u32 offset = (stack + i * m_instruction.operand_size) & stack_mask;

        if (!WriteMemory(I386_SEGMENT_SS, offset, operand_width, values[i], *m_bus_context, true))
            return false;
    }

    SetStackPointer(stack);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_POPA()
{
    static const int k_popa_registers[8] =
    {
        I386_REG_EDI, I386_REG_ESI, I386_REG_EBP, -1, I386_REG_EBX, I386_REG_EDX, I386_REG_ECX, I386_REG_EAX
    };

    if (!DecodeAndStart(false, 0, 24, 24))
        return false;

    int operand_width = m_instruction.operand_size * 8;
    u32 stack = GetStackPointer();
    u32 stack_mask = GetStackAddressSize() == 32 ? 0xFFFFFFFFU : 0xFFFFU;

    for (int i = 0; i < 8; i++)
    {
        u32 value = 0;

        if (!ReadMemory(I386_SEGMENT_SS, stack, operand_width, *m_bus_context, value, true))
            return false;

        if (k_popa_registers[i] >= 0)
            SetRegister(k_popa_registers[i], operand_width, value);
        else if (operand_width == 32 && GetStackAddressSize() == 16)
            m_registers[I386_REG_ESP].high = (u16)(value >> 16);

        stack = (stack + m_instruction.operand_size) & stack_mask;
    }

    SetStackPointer(stack);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_POP_RM()
{
    if (!DecodeAndStart(true, 0, 4, 5))
        return false;

    int operand_width = m_instruction.operand_size * 8;

    if (m_instruction.reg != 0)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    u32 old_stack = GetStackPointer();
    u32 value = 0;

    if (!ReadMemory(I386_SEGMENT_SS, old_stack, operand_width, *m_bus_context, value, true))
        return false;

    u32 stack_mask = GetStackAddressSize() == 32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 new_stack = (old_stack + m_instruction.operand_size) & stack_mask;
    InstructionContext destination = m_instruction;

    // The destination address is calculated with the incremented stack pointer
    SetStackPointer(new_stack);

    if (destination.memory_operand)
        CalculateEffectiveOffset(destination);

    SetStackPointer(old_stack);

    if (!WriteRM(destination, operand_width, value, *m_bus_context))
        return false;

    SetStackPointer(new_stack);

    if (!destination.memory_operand && destination.rm == I386_REG_ESP)
        SetRegister(I386_REG_ESP, operand_width, value);

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_CALL_Far()
{
    if (!DecodeAndStart(false, -2, 17, 17))
        return false;

    int operand_width = m_instruction.operand_size * 8;
    u32 target = m_instruction.immediate;

    if (m_execution_mode == I386_MODE_PROTECTED)
        return ProtectedFarTransfer((u16)m_instruction.immediate2, target, operand_width, true, m_instruction.next_eip,
            *m_bus_context, m_step.clocks, false);

    if ((operand_width == 16 ? (u32)(u16)target : target) > 0xFFFF)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    if (!StackPushSized(m_segments[I386_SEGMENT_CS].selector, operand_width, *m_bus_context))
        return false;

    if (!StackPushSized(m_instruction.next_eip, operand_width, *m_bus_context))
        return false;

    if (!FarTransfer((u16)m_instruction.immediate2, target, operand_width))
        return false;

    m_step.clocks += GetNextInstructionComponents();
    return true;
}

bool I386::OPCodes_PUSHF()
{
    if (!DecodeAndStart(false, 0, 4, 4))
        return false;

    int operand_width = m_instruction.operand_size * 8;

    if (m_execution_mode == I386_MODE_VM86 && GetIOPrivilegeLevel() < 3)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u32 flags = operand_width == 32 ? m_eflags & 0x0000FFFFU : m_eflags;

    if (!StackPushSized(flags, operand_width, *m_bus_context))
        return false;

    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_POPF()
{
    if (!DecodeAndStart(false, 0, 5, 5))
        return false;

    int operand_width = m_instruction.operand_size * 8;

    if (m_execution_mode == I386_MODE_VM86 && GetIOPrivilegeLevel() < 3)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u32 value = 0;

    if (!StackPopSized(value, operand_width, *m_bus_context))
        return false;

    u32 mask = operand_width == 32 ? 0x00014FD5U : 0x00004FD5U;

    if (m_execution_mode == I386_MODE_REAL || m_current_privilege_level == 0)
        mask |= I386_FLAG_IOPL;

    if (m_execution_mode == I386_MODE_PROTECTED && m_current_privilege_level > GetIOPrivilegeLevel())
        mask &= ~I386_FLAG_IF;

    if (operand_width == 16)
        mask &= 0xFFFF;

    m_eflags = (m_eflags & ~mask) | (value & mask) | I386_FLAG_FIXED;
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_ENTER()
{
    if (!DecodeOperands(false, -1))
        return false;

    u32 nesting = m_instruction.immediate2 & 0x1F;

    if (!StartExecution(nesting == 0 ? 10 : nesting == 1 ? 12 : 15 + 4 * (nesting - 1)))
        return false;

    int operand_width = m_instruction.operand_size * 8;
    u32 frame_value = GetRegister(I386_REG_EBP, operand_width);
    int frame_address_width = GetStackAddressSize();
    u32 frame = GetRegister(I386_REG_EBP, frame_address_width);
    u32 stack_mask = frame_address_width == 32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 stack = (GetStackPointer() - m_instruction.operand_size) & stack_mask;

    if (!WriteMemory(I386_SEGMENT_SS, stack, operand_width, frame_value, *m_bus_context, true))
        return false;

    u32 frame_pointer = frame_address_width == 16 && operand_width == 32 ?
        (m_registers[I386_REG_ESP].value & 0xFFFF0000U) | stack : stack;

    for (u32 i = 1; i < nesting; i++)
    {
        u32 value = 0;
        frame = Truncate(frame - m_instruction.operand_size, frame_address_width);

        if (!ReadMemory(I386_SEGMENT_SS, frame, operand_width, *m_bus_context, value, true))
            return false;

        stack = (stack - m_instruction.operand_size) & stack_mask;

        if (!WriteMemory(I386_SEGMENT_SS, stack, operand_width, value, *m_bus_context, true))
            return false;
    }

    if (nesting != 0)
    {
        stack = (stack - m_instruction.operand_size) & stack_mask;

        if (!WriteMemory(I386_SEGMENT_SS, stack, operand_width, frame_pointer, *m_bus_context, true))
            return false;
    }

    u32 final_stack = (stack - (u16)m_instruction.immediate) & stack_mask;

    if ((u16)m_instruction.immediate != 0)
    {
        u32 linear = 0;
        u32 physical = 0;

        if (!LogicalToLinear(I386_SEGMENT_SS, final_stack, 1, true, true, linear))
            return false;

        if (!TranslateLinear(linear, true, *m_bus_context, physical))
            return false;
    }

    SetRegister(I386_REG_EBP, operand_width, frame_pointer);
    SetStackPointer(final_stack);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_LEAVE()
{
    if (!DecodeAndStart(false, 0, 4, 4))
        return false;

    int operand_width = m_instruction.operand_size * 8;
    int stack_address_width = GetStackAddressSize();
    u32 frame = GetRegister(I386_REG_EBP, stack_address_width);
    u32 value = 0;

    if (!ReadMemory(I386_SEGMENT_SS, frame, operand_width, *m_bus_context, value, true))
        return false;

    u32 stack_mask = stack_address_width == 32 ? 0xFFFFFFFFU : 0xFFFFU;

    SetStackPointer((frame + m_instruction.operand_size) & stack_mask);
    SetRegister(I386_REG_EBP, operand_width, value);
    CommitEIP(m_instruction);
    return true;
}

bool I386::OPCodes_RET_Far()
{
    u8 opcode = m_instruction.opcode;

    if (!DecodeAndStart(false, opcode == 0xCA ? 2 : 0, 18, 18))
        return false;

    int operand_width = m_instruction.operand_size * 8;
    u16 release_bytes = opcode == 0xCA ? (u16)m_instruction.immediate : 0;

    if (m_execution_mode == I386_MODE_PROTECTED)
        return ProtectedFarReturn(operand_width, release_bytes, *m_bus_context, m_step.clocks);

    u32 old_stack = GetStackPointer();
    u32 stack_mask = GetStackAddressSize() == 32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 selector_offset = (old_stack + m_instruction.operand_size) & stack_mask;
    u32 target = 0;
    u32 selector = 0;

    if (!ReadMemory(I386_SEGMENT_SS, old_stack, operand_width, *m_bus_context, target, true))
        return false;

    if (!ReadMemory(I386_SEGMENT_SS, selector_offset, operand_width, *m_bus_context, selector, true))
        return false;

    u32 checked_target = operand_width == 16 ? (u16)target : target;

    if (checked_target > 0xFFFF)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u32 adjustment = m_instruction.operand_size * 2 + release_bytes;

    SetStackPointer((old_stack + adjustment) & stack_mask);

    if (!FarTransfer((u16)selector, target, operand_width))
        return false;

    m_step.clocks += GetNextInstructionComponents();
    return true;
}

bool I386::OPCodes_INT()
{
    u8 opcode = m_instruction.opcode;

    if (!DecodeOperands(false, opcode == 0xCD ? 1 : 0))
        return false;

    if (!StartExecution(opcode == 0xCC ? 33 : opcode == 0xCD ? 37 : (m_eflags & I386_FLAG_OF) != 0 ? 35 : 3))
        return false;

    if (opcode == 0xCE && (m_eflags & I386_FLAG_OF) == 0)
    {
        CommitEIP(m_instruction);
        return true;
    }

    u8 vector = opcode == 0xCC ? 3 : opcode == 0xCE ? 4 : (u8)m_instruction.immediate;

    if (opcode == 0xCD && m_execution_mode == I386_MODE_VM86 && GetIOPrivilegeLevel() < 3)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    m_last_exception_vector = vector;
    m_step_exception = {};

    if (!EnterInterrupt(vector, m_instruction.next_eip, *m_bus_context, true, false, 0, false, false, &m_step.clocks,
        &m_step_exception))
        return false;

    m_step.exception = true;
    m_step_exception.exception_after_instruction = false;
    m_step_exception.exception_vector = vector;
    m_step.end_batch = true;
    return true;
}

bool I386::OPCodes_IRET()
{
    if (!DecodeAndStart(false, 0, 22, 22))
        return false;

    int operand_width = m_instruction.operand_size * 8;

    if (m_execution_mode == I386_MODE_PROTECTED)
        return ProtectedInterruptReturn(operand_width, *m_bus_context, m_step.clocks);

    if (m_execution_mode == I386_MODE_VM86 && GetIOPrivilegeLevel() < 3)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u32 old_stack = GetStackPointer();
    u32 stack_mask = GetStackAddressSize() == 32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 selector_offset = (old_stack + m_instruction.operand_size) & stack_mask;
    u32 flags_offset = (old_stack + m_instruction.operand_size * 2) & stack_mask;
    u32 target = 0;
    u32 selector = 0;
    u32 flags = 0;

    if (!ReadMemory(I386_SEGMENT_SS, old_stack, operand_width, *m_bus_context, target, true))
        return false;

    if (!ReadMemory(I386_SEGMENT_SS, selector_offset, operand_width, *m_bus_context, selector, true))
        return false;

    if (!ReadMemory(I386_SEGMENT_SS, flags_offset, operand_width, *m_bus_context, flags, true))
        return false;

    u32 checked_target = operand_width == 16 ? (u16)target : target;

    if (checked_target > 0xFFFF)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    SetStackPointer((old_stack + m_instruction.operand_size * 3) & stack_mask);

    if (!FarTransfer((u16)selector, target, operand_width))
        return false;

    u32 mask = operand_width == 16 ? 0x00007FD5U : 0x00017FD5U;

    m_eflags = (m_eflags & ~mask) | (flags & mask) | I386_FLAG_FIXED;
    m_nmi_blocked = false;
    UpdateExecutionMode();
    return true;
}

bool I386::OPCodes_LOOP()
{
    if (!DecodeAndStart(false, 1, 11, 11))
        return false;

    u8 opcode = m_instruction.opcode;
    int operand_width = m_instruction.operand_size * 8;
    int count_width = m_instruction.address_size * 8;
    u32 count = Truncate(GetRegister(I386_REG_ECX, count_width) - 1, count_width);

    SetRegister(I386_REG_ECX, count_width, count);

    bool taken = count != 0;

    if (opcode == 0xE0)
        taken = taken && (m_eflags & I386_FLAG_ZF) == 0;
    else if (opcode == 0xE1)
        taken = taken && (m_eflags & I386_FLAG_ZF) != 0;

    if (taken)
    {
        if (!BranchTo(m_instruction.next_eip + (s8)(u8)m_instruction.immediate, operand_width))
            return false;
    }
    else
        CommitEIP(m_instruction);

    m_step.clocks += GetNextInstructionComponents();
    return true;
}

bool I386::OPCodes_JCXZ()
{
    if (!DecodeAndStart(false, 1, 5, 5))
        return false;

    int operand_width = m_instruction.operand_size * 8;
    int count_width = m_instruction.address_size * 8;

    if (GetRegister(I386_REG_ECX, count_width) == 0)
    {
        m_step.clocks = 9;

        if (!BranchTo(m_instruction.next_eip + (s8)(u8)m_instruction.immediate, operand_width))
            return false;

        m_step.clocks += GetNextInstructionComponents();
    }
    else
    {
        m_step.clocks = 5;
        CommitEIP(m_instruction);
    }

    return true;
}

bool I386::OPCodes_JMP_Near()
{
    if (!DecodeAndStart(false, m_instruction.operand_size, 7, 7))
        return false;

    int operand_width = m_instruction.operand_size * 8;
    u32 target = m_instruction.next_eip + SignExtend(m_instruction.immediate, operand_width);

    if (!BranchTo(target, operand_width))
        return false;

    m_step.clocks += GetNextInstructionComponents();
    return true;
}

bool I386::OPCodes_JMP_Far()
{
    if (!DecodeAndStart(false, -2, 12, 12))
        return false;

    int operand_width = m_instruction.operand_size * 8;

    if (m_execution_mode == I386_MODE_PROTECTED)
        return ProtectedFarTransfer((u16)m_instruction.immediate2, m_instruction.immediate, operand_width, false,
            m_instruction.next_eip, *m_bus_context, m_step.clocks, false);

    if (!FarTransfer((u16)m_instruction.immediate2, m_instruction.immediate, operand_width))
        return false;

    m_step.clocks += GetNextInstructionComponents();
    return true;
}

bool I386::OPCodes_JMP_Short()
{
    if (!DecodeAndStart(false, 1, 7, 7))
        return false;

    int operand_width = m_instruction.operand_size * 8;

    if (!BranchTo(m_instruction.next_eip + (s8)(u8)m_instruction.immediate, operand_width))
        return false;

    m_step.clocks += GetNextInstructionComponents();
    return true;
}
