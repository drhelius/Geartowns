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

bool I386::ProtectedFarTransfer(u16 selector, u32 offset, int width, bool call, u32 return_eip,
    GT_Bus_Access_Context& context, u64& clocks, bool indirect)
{
    u32 address_clocks = indirect ? m_address_clocks : 0;

    if ((selector & 0xFFFC) == 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    Descriptor descriptor;

    if (!ReadDescriptor(selector, descriptor, context))
        return false;

    if (!descriptor.system && (descriptor.attributes & I386_SEGMENT_EXECUTABLE) != 0)
    {
        clocks = (call ? (indirect ? 38 : 34) : (indirect ? 31 : 27)) + address_clocks;

        bool conforming = (descriptor.attributes & I386_SEGMENT_CONFORMING) != 0;
        bool privilege_allowed = conforming ? descriptor.dpl <= m_current_privilege_level :
            (selector & 3) <= m_current_privilege_level && descriptor.dpl == m_current_privilege_level;

        if (!privilege_allowed)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

        if (!descriptor.present)
            return RaiseException(11, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

        const I386_Segment& stack_segment = m_segments[I386_SEGMENT_SS];
        u32 target = width == 16 ? (u16)offset : offset;
        u32 frame_bytes = 2 * ((u32)width >> 3);

        if (call && !StackHasRoom(stack_segment.limit, stack_segment.attributes, GetStackPointer(), frame_bytes))
            return RaiseException(12, I386_EXCEPTION_FAULT, true, 0);

        if (target > descriptor.limit)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        if (!SetDescriptorAccessed(descriptor, context))
            return false;

        if (call)
        {
            if (!CheckStackFrame(stack_segment.base, stack_segment.attributes, GetStackPointer(), 2, width, false,
                context))
                return false;

            if (!StackPushSized(m_segments[I386_SEGMENT_CS].selector, width, context))
                return false;

            if (!StackPushSized(return_eip, width, context))
                return false;
        }

        LoadDescriptorCache((selector & 0xFFFC) | m_current_privilege_level, descriptor, m_segments[I386_SEGMENT_CS]);

        m_segments[I386_SEGMENT_CS].dpl = m_current_privilege_level;
        m_eip = target;
        m_repeat.active = false;
        clocks += GetNextInstructionComponents();
        return true;
    }

    if (descriptor.system && (descriptor.type == 1 || descriptor.type == 9))
    {
        u8 task_fault = call ? 10 : 13;

        if (descriptor.dpl < m_current_privilege_level || descriptor.dpl < (selector & 3))
            return RaiseException(task_fault, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

        if (!descriptor.present)
            return RaiseException(11, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

#if !defined(GT_DISABLE_DISASSEMBLER)
        u8 old_type = (m_task_register.attributes & I386_SEGMENT_TYPE_MASK) >> I386_SEGMENT_TYPE_SHIFT;
        m_instruction.call_return_size = old_type == 9 || old_type == 11 ? 4 : 2;
#endif

        int switch_type = call ? I386_TASK_SWITCH_CALL : I386_TASK_SWITCH_JMP;
        bool ok = TaskSwitch(selector, descriptor, switch_type, return_eip, context, clocks, false);

        if (indirect)
            clocks += 5 + address_clocks;

        return ok;
    }

    if (descriptor.system && (descriptor.type == 3 || descriptor.type == 11))
        return RaiseException(call ? 10 : 13, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    if (descriptor.system && descriptor.type == 5)
    {
        u8 task_fault = call ? 10 : 13;

        if (descriptor.dpl < m_current_privilege_level || descriptor.dpl < (selector & 3))
            return RaiseException(task_fault, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

        if (!descriptor.present)
            return RaiseException(11, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

        u16 task_selector = (u16)(descriptor.low >> 16);
        Descriptor task;

        if (!ReadGDTDescriptor(task_selector, task, context, task_fault))
            return false;

        if (!task.system || (task.type != 1 && task.type != 9))
            return RaiseException(task_fault, I386_EXCEPTION_FAULT, true, task_selector & 0xFFFC);

        if (!task.present)
            return RaiseException(11, I386_EXCEPTION_FAULT, true, task_selector & 0xFFFC);

#if !defined(GT_DISABLE_DISASSEMBLER)
        u8 old_type = (m_task_register.attributes & I386_SEGMENT_TYPE_MASK) >> I386_SEGMENT_TYPE_SHIFT;
        m_instruction.call_return_size = old_type == 9 || old_type == 11 ? 4 : 2;
#endif

        int switch_type = call ? I386_TASK_SWITCH_CALL : I386_TASK_SWITCH_JMP;
        bool ok = TaskSwitch(task_selector, task, switch_type, return_eip, context, clocks, true);

        if (indirect)
            clocks += 5 + address_clocks;

        return ok;
    }

    bool gate32 = descriptor.system && descriptor.type == 12;
    bool gate16 = descriptor.system && descriptor.type == 4;

    if (!gate32 && !gate16)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    if (descriptor.dpl < m_current_privilege_level || descriptor.dpl < (selector & 3))
        return RaiseException(13, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    if (!descriptor.present)
        return RaiseException(11, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    u16 target_selector = (u16)(descriptor.low >> 16);
    u32 target = (descriptor.low & 0xFFFF) | (gate32 ? descriptor.high & 0xFFFF0000U : 0);

    if ((target_selector & 0xFFFC) == 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    Descriptor code;

    if (!ReadDescriptor(target_selector, code, context))
        return false;

    if (code.system || (code.attributes & I386_SEGMENT_EXECUTABLE) == 0 || code.dpl > m_current_privilege_level)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, target_selector & 0xFFFC);

    bool conforming = (code.attributes & I386_SEGMENT_CONFORMING) != 0;
    bool privilege_change = !conforming && code.dpl < m_current_privilege_level;

    if (!call && privilege_change)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, target_selector & 0xFFFC);

    if (!code.present)
        return RaiseException(11, I386_EXCEPTION_FAULT, true, target_selector & 0xFFFC);

    int gate_width = gate32 ? 32 : 16;
    u32 gate_bytes = (u32)gate_width >> 3;

#if !defined(GT_DISABLE_DISASSEMBLER)
    m_instruction.call_return_size = gate32 ? 4 : 2;
#endif

    if (call && privilege_change)
    {
        u32 old_stack = GetStackPointer();
        u32 old_esp = m_registers[I386_REG_ESP].value;
        u16 old_ss = m_segments[I386_SEGMENT_SS].selector;
        u16 old_cs = m_segments[I386_SEGMENT_CS].selector;
        u32 parameters[31];
        u32 count = descriptor.high & 0x1F;
        u32 new_stack = 0;
        u16 new_ss = 0;

        clocks = (count == 0 ? (indirect ? 90 : 86) : (indirect ? 98 : 94) + 4 * count) + address_clocks;

        if (!ReadPrivilegeStack(code.dpl, new_stack, new_ss, context))
            return false;

        if ((new_ss & 0xFFFC) == 0 || (new_ss & 3) != code.dpl)
            return RaiseException(10, I386_EXCEPTION_FAULT, true, new_ss & 0xFFFC);

        Descriptor stack_descriptor;

        if (!ReadDescriptor(new_ss, stack_descriptor, context, 10))
            return false;

        if (stack_descriptor.system || (stack_descriptor.attributes & I386_SEGMENT_EXECUTABLE) != 0 ||
            (stack_descriptor.attributes & I386_SEGMENT_WRITABLE) == 0 || stack_descriptor.dpl != code.dpl)
            return RaiseException(10, I386_EXCEPTION_FAULT, true, new_ss & 0xFFFC);

        if (!stack_descriptor.present)
            return RaiseException(12, I386_EXCEPTION_FAULT, true, new_ss & 0xFFFC);

        if (!StackHasRoom(stack_descriptor.limit, stack_descriptor.attributes, new_stack, (count + 4) * gate_bytes))
            return RaiseException(12, I386_EXCEPTION_FAULT, true, 0);

        if (target > code.limit)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        if (!SetDescriptorAccessed(code, context))
            return false;

        if (!SetDescriptorAccessed(stack_descriptor, context))
            return false;

        u32 old_stack_mask = GetStackAddressSize() == 32 ? 0xFFFFFFFFU : 0xFFFFU;

        for (u32 i = 0; i < count; i++)
        {
            u32 parameter_offset = (old_stack + i * gate_bytes) & old_stack_mask;

            if (!ReadMemory(I386_SEGMENT_SS, parameter_offset, gate_width, context, parameters[i], true))
                return false;
        }

        if (!CheckStackFrame(stack_descriptor.base, stack_descriptor.attributes, new_stack, count + 4, gate_width, true,
            context))
            return false;

        LoadDescriptorCache((new_ss & 0xFFFC) | code.dpl, stack_descriptor, m_segments[I386_SEGMENT_SS]);

        m_current_privilege_level = code.dpl;
        UpdateUserMode();
        SetStackPointer(new_stack);

        if (!StackPushSized(old_ss, gate_width, context))
            return false;

        if (!StackPushSized(old_esp, gate_width, context))
            return false;

        for (u32 i = count; i > 0; i--)
        {
            if (!StackPushSized(parameters[i - 1], gate_width, context))
                return false;
        }

        if (!StackPushSized(old_cs, gate_width, context))
            return false;

        if (!StackPushSized(return_eip, gate_width, context))
            return false;
    }
    else if (call)
    {
        const I386_Segment& stack_segment = m_segments[I386_SEGMENT_SS];

        clocks = (indirect ? 56 : 52) + address_clocks;

        if (!StackHasRoom(stack_segment.limit, stack_segment.attributes, GetStackPointer(), 2 * gate_bytes))
            return RaiseException(12, I386_EXCEPTION_FAULT, true, 0);

        if (target > code.limit)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        if (!SetDescriptorAccessed(code, context))
            return false;

        if (!CheckStackFrame(stack_segment.base, stack_segment.attributes, GetStackPointer(), 2, gate_width, false,
            context))
            return false;

        if (!StackPushSized(m_segments[I386_SEGMENT_CS].selector, gate_width, context))
            return false;

        if (!StackPushSized(return_eip, gate_width, context))
            return false;
    }
    else
    {
        clocks = (indirect ? 49 : 45) + address_clocks;

        if (target > code.limit)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        if (!SetDescriptorAccessed(code, context))
            return false;
    }

    LoadDescriptorCache((target_selector & 0xFFFC) | m_current_privilege_level, code, m_segments[I386_SEGMENT_CS]);

    m_segments[I386_SEGMENT_CS].dpl = m_current_privilege_level;
    m_eip = gate32 ? target : (u16)target;
    m_repeat.active = false;
    clocks += GetNextInstructionComponents();
    return true;
}

void I386::ValidateDataSegmentsForPrivilege(u8 privilege)
{
    static const int k_data_segments[4] = { I386_SEGMENT_ES, I386_SEGMENT_DS, I386_SEGMENT_FS, I386_SEGMENT_GS };

    for (int i = 0; i < 4; i++)
    {
        I386_Segment& segment = m_segments[k_data_segments[i]];

        if ((segment.selector & 0xFFFC) == 0)
            continue;

        bool executable = (segment.attributes & I386_SEGMENT_EXECUTABLE) != 0;
        bool conforming = (segment.attributes & I386_SEGMENT_CONFORMING) != 0;
        bool readable = (segment.attributes & I386_SEGMENT_READABLE) != 0;
        bool present = (segment.attributes & I386_SEGMENT_PRESENT) != 0;
        bool system = (segment.attributes & I386_SEGMENT_SYSTEM) != 0;
        bool valid = present && !system && (!executable || readable) && (conforming || segment.dpl >= privilege);

        if (!valid)
            ClearSegmentCache(0, segment);
    }
}

bool I386::ProtectedFarReturn(int width, u16 adjustment, GT_Bus_Access_Context& context, u64& clocks)
{
    u32 old_stack = GetStackPointer();
    u32 unit = (u32)width >> 3;
    u32 stack_mask = GetStackAddressSize() == 32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 target = 0;
    u32 selector_value = 0;

    if (!ReadMemory(I386_SEGMENT_SS, old_stack, width, context, target, true))
        return false;

    if (!ReadMemory(I386_SEGMENT_SS, (old_stack + unit) & stack_mask, width, context, selector_value, true))
        return false;

    u16 selector = (u16)selector_value;

    if ((selector & 0xFFFC) == 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    if ((selector & 3) < m_current_privilege_level)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    Descriptor code;

    if (!ReadDescriptor(selector, code, context))
        return false;

    bool executable = !code.system && (code.attributes & I386_SEGMENT_EXECUTABLE) != 0;
    bool conforming = executable && (code.attributes & I386_SEGMENT_CONFORMING) != 0;
    u8 return_privilege = selector & 3;
    bool same_privilege = return_privilege == m_current_privilege_level;

    clocks = same_privilege ? 32 : 68;

    if (!executable || (!conforming && code.dpl != return_privilege) || (conforming && code.dpl > return_privilege))
        return RaiseException(13, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    if (!code.present)
        return RaiseException(11, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    u32 checked_target = width == 16 ? (u16)target : target;

    if (checked_target > code.limit)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    if (same_privilege)
    {
        if (!SetDescriptorAccessed(code, context))
            return false;

        SetStackPointer((old_stack + unit * 2 + adjustment) & stack_mask);
    }
    else
    {
        u32 saved_stack = 0;
        u32 saved_ss_value = 0;
        u32 saved_offset = (old_stack + unit * 2 + adjustment) & stack_mask;

        if (!ReadMemory(I386_SEGMENT_SS, saved_offset, width, context, saved_stack, true))
            return false;

        if (!ReadMemory(I386_SEGMENT_SS, (saved_offset + unit) & stack_mask, width, context, saved_ss_value, true))
            return false;

        u16 saved_ss = (u16)saved_ss_value;

        if ((saved_ss & 0xFFFC) == 0 || (saved_ss & 3) != return_privilege)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, saved_ss & 0xFFFC);

        Descriptor stack_descriptor;

        if (!ReadDescriptor(saved_ss, stack_descriptor, context))
            return false;

        if (stack_descriptor.system || (stack_descriptor.attributes & I386_SEGMENT_EXECUTABLE) != 0 ||
            (stack_descriptor.attributes & I386_SEGMENT_WRITABLE) == 0 || stack_descriptor.dpl != return_privilege)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, saved_ss & 0xFFFC);

        if (!stack_descriptor.present)
            return RaiseException(11, I386_EXCEPTION_FAULT, true, saved_ss & 0xFFFC);

        if (!SetDescriptorAccessed(code, context))
            return false;

        if (!SetDescriptorAccessed(stack_descriptor, context))
            return false;

        LoadDescriptorCache(saved_ss, stack_descriptor, m_segments[I386_SEGMENT_SS]);

        m_current_privilege_level = return_privilege;
        UpdateUserMode();

        if (GetStackAddressSize() == 32)
            m_registers[I386_REG_ESP].value = saved_stack + adjustment;
        else
            m_registers[I386_REG_ESP].low = (u16)(saved_stack + adjustment);

        ValidateDataSegmentsForPrivilege(return_privilege);
    }

    LoadDescriptorCache((selector & 0xFFFC) | return_privilege, code, m_segments[I386_SEGMENT_CS]);

    m_segments[I386_SEGMENT_CS].dpl = return_privilege;
    m_current_privilege_level = return_privilege;
    UpdateUserMode();
    m_eip = checked_target;

    if (same_privilege)
        clocks += GetNextInstructionComponents();

    return true;
}

bool I386::ProtectedInterruptReturn(int width, GT_Bus_Access_Context& context, u64& clocks)
{
    if ((m_eflags & I386_FLAG_NT) != 0)
    {
        if (!TaskReturn(m_instruction.next_eip, context, clocks))
            return false;

        m_nmi_blocked = false;
        return true;
    }

    u32 old_stack = GetStackPointer();
    u32 unit = (u32)width >> 3;
    u32 stack_mask = GetStackAddressSize() == 32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 target = 0;
    u32 selector_value = 0;
    u32 flags = 0;

    if (!ReadMemory(I386_SEGMENT_SS, old_stack, width, context, target, true))
        return false;

    if (!ReadMemory(I386_SEGMENT_SS, (old_stack + unit) & stack_mask, width, context, selector_value, true))
        return false;

    if (!ReadMemory(I386_SEGMENT_SS, (old_stack + unit * 2) & stack_mask, width, context, flags, true))
        return false;

    if (width == 32 && (flags & I386_FLAG_VM) != 0)
    {
        clocks = 60;

        if (m_current_privilege_level != 0)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        u32 values[6];

        for (int i = 0; i < 6; i++)
        {
            u32 offset = (old_stack + unit * (3 + i)) & stack_mask;

            if (!ReadMemory(I386_SEGMENT_SS, offset, 32, context, values[i], true))
                return false;
        }

        m_eip = target;
        m_registers[I386_REG_ESP].value = values[0];

        SetVM86Segment(I386_SEGMENT_CS, (u16)selector_value);
        SetVM86Segment(I386_SEGMENT_SS, (u16)values[1]);
        SetVM86Segment(I386_SEGMENT_ES, (u16)values[2]);
        SetVM86Segment(I386_SEGMENT_DS, (u16)values[3]);
        SetVM86Segment(I386_SEGMENT_FS, (u16)values[4]);
        SetVM86Segment(I386_SEGMENT_GS, (u16)values[5]);

        m_eflags = (flags & 0x0003FFFFU) | I386_FLAG_FIXED;
        m_nmi_blocked = false;
        UpdateExecutionMode();
        return true;
    }

    u16 selector = (u16)selector_value;

    if ((selector & 0xFFFC) == 0 || (selector & 3) < m_current_privilege_level)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    Descriptor code;

    if (!ReadDescriptor(selector, code, context))
        return false;

    u8 return_privilege = selector & 3;
    bool executable = !code.system && (code.attributes & I386_SEGMENT_EXECUTABLE) != 0;
    bool conforming = executable && (code.attributes & I386_SEGMENT_CONFORMING) != 0;

    if (!executable || (!conforming && code.dpl != return_privilege) || (conforming && code.dpl > return_privilege))
        return RaiseException(13, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    if (!code.present)
        return RaiseException(11, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    u32 checked_target = width == 16 ? (u16)target : target;

    if (checked_target > code.limit)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u8 old_privilege = m_current_privilege_level;

    clocks = return_privilege == old_privilege ? 38 : 82;

    if (return_privilege == old_privilege)
    {
        if (!SetDescriptorAccessed(code, context))
            return false;

        SetStackPointer((old_stack + unit * 3) & stack_mask);
    }
    else
    {
        u32 saved_stack = 0;
        u32 saved_ss_value = 0;

        if (!ReadMemory(I386_SEGMENT_SS, (old_stack + unit * 3) & stack_mask, width, context, saved_stack, true))
            return false;

        if (!ReadMemory(I386_SEGMENT_SS, (old_stack + unit * 4) & stack_mask, width, context, saved_ss_value, true))
            return false;

        u16 saved_ss = (u16)saved_ss_value;

        if ((saved_ss & 0xFFFC) == 0 || (saved_ss & 3) != return_privilege)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, saved_ss & 0xFFFC);

        Descriptor stack_descriptor;

        if (!ReadDescriptor(saved_ss, stack_descriptor, context))
            return false;

        if (stack_descriptor.system || (stack_descriptor.attributes & I386_SEGMENT_EXECUTABLE) != 0 ||
            (stack_descriptor.attributes & I386_SEGMENT_WRITABLE) == 0 || stack_descriptor.dpl != return_privilege)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, saved_ss & 0xFFFC);

        if (!stack_descriptor.present)
            return RaiseException(11, I386_EXCEPTION_FAULT, true, saved_ss & 0xFFFC);

        if (!SetDescriptorAccessed(code, context))
            return false;

        if (!SetDescriptorAccessed(stack_descriptor, context))
            return false;

        LoadDescriptorCache(saved_ss, stack_descriptor, m_segments[I386_SEGMENT_SS]);

        if (GetStackAddressSize() == 32)
            m_registers[I386_REG_ESP].value = saved_stack;
        else
            m_registers[I386_REG_ESP].low = (u16)saved_stack;

        ValidateDataSegmentsForPrivilege(return_privilege);
    }

    LoadDescriptorCache((selector & 0xFFFC) | return_privilege, code, m_segments[I386_SEGMENT_CS]);

    m_segments[I386_SEGMENT_CS].dpl = return_privilege;
    m_current_privilege_level = return_privilege;
    UpdateUserMode();

    m_eip = checked_target;

    u32 flag_mask = width == 16 ? 0x00007FD5U : 0x00017FD5U;

    if (old_privilege != 0)
        flag_mask &= ~I386_FLAG_IOPL;

    if (old_privilege > GetIOPrivilegeLevel())
        flag_mask &= ~I386_FLAG_IF;

    m_eflags = (m_eflags & ~flag_mask) | (flags & flag_mask) | I386_FLAG_FIXED;
    m_nmi_blocked = false;
    m_execution_mode = I386_MODE_PROTECTED;

    UpdateUserMode();
    UpdateSegmentFastPaths();
    return true;
}
