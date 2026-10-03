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
#include "memory.h"

void I386::DecodeDescriptor(u16 selector, u32 address, u32 low, u32 high, Descriptor& descriptor) const
{
    memset(&descriptor, 0, sizeof(descriptor));

    descriptor.low = low;
    descriptor.high = high;
    descriptor.address = address;
    descriptor.selector = selector;
    descriptor.base = ((low >> 16) & 0xFFFF) | ((high & 0xFF) << 16) | (high & 0xFF000000U);

    u32 raw_limit = (low & 0xFFFF) | (high & 0x000F0000U);

    descriptor.limit = (high & 0x00800000U) != 0 ? (raw_limit << 12) | 0xFFF : raw_limit;
    descriptor.access = (u8)(high >> 8);
    descriptor.type = descriptor.access & 15;
    descriptor.dpl = (descriptor.access >> 5) & 3;
    descriptor.system = (descriptor.access & 0x10) == 0;
    descriptor.present = (descriptor.access & 0x80) != 0;
    descriptor.attributes = (u16)descriptor.type << I386_SEGMENT_TYPE_SHIFT;

    if (descriptor.present)
        descriptor.attributes |= I386_SEGMENT_PRESENT;

    if (descriptor.system)
        descriptor.attributes |= I386_SEGMENT_SYSTEM;

    if ((high & 0x00400000U) != 0)
        descriptor.attributes |= I386_SEGMENT_DEFAULT_32;

    if ((high & 0x00800000U) != 0)
        descriptor.attributes |= I386_SEGMENT_GRANULAR;

    if (!descriptor.system)
    {
        if ((descriptor.type & 8) != 0)
        {
            descriptor.attributes |= I386_SEGMENT_EXECUTABLE;

            if ((descriptor.type & 2) != 0)
                descriptor.attributes |= I386_SEGMENT_READABLE;

            if ((descriptor.type & 4) != 0)
                descriptor.attributes |= I386_SEGMENT_CONFORMING;
        }
        else
        {
            descriptor.attributes |= I386_SEGMENT_READABLE;

            if ((descriptor.type & 2) != 0)
                descriptor.attributes |= I386_SEGMENT_WRITABLE;

            if ((descriptor.type & 4) != 0)
                descriptor.attributes |= I386_SEGMENT_EXPAND_DOWN;
        }

        if ((descriptor.type & 1) != 0)
            descriptor.attributes |= I386_SEGMENT_ACCESSED;
    }
}

bool I386::ReadGDTDescriptor(u16 selector, Descriptor& descriptor, GT_Bus_Access_Context& context, u8 fault_vector)
{
    u32 offset = (u32)(selector & 0xFFF8);
    u32 error_code = selector & 0xFFFC;

    if ((selector & 4) != 0 || (u64)offset + 7 > m_gdtr.limit)
        return RaiseException(fault_vector, I386_EXCEPTION_FAULT, true, error_code);

    u32 low = 0;
    u32 high = 0;

    if (!ReadLinear(m_gdtr.base + offset, 32, context, low, true))
        return false;

    if (!ReadLinear(m_gdtr.base + offset + 4, 32, context, high, true))
        return false;

    DecodeDescriptor(selector, m_gdtr.base + offset, low, high, descriptor);
    return true;
}

bool I386::ReadDescriptor(u16 selector, Descriptor& descriptor, GT_Bus_Access_Context& context, u8 fault_vector)
{
    const I386_Descriptor_Table* table = &m_gdtr;

    if ((selector & 4) != 0)
    {
        u8 type = (m_ldtr.attributes & I386_SEGMENT_TYPE_MASK) >> I386_SEGMENT_TYPE_SHIFT;

        if ((m_ldtr.selector & 0xFFFC) == 0 || (m_ldtr.attributes & I386_SEGMENT_PRESENT) == 0 ||
            (m_ldtr.attributes & I386_SEGMENT_SYSTEM) == 0 || type != 2)
            return RaiseException(fault_vector, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);
    }

    u32 table_base = (selector & 4) != 0 ? m_ldtr.base : table->base;
    u32 table_limit = (selector & 4) != 0 ? m_ldtr.limit : table->limit;
    u32 offset = (u32)(selector & 0xFFF8);
    u32 error_code = selector & 0xFFFC;

    if ((u64)offset + 7 > table_limit)
        return RaiseException(fault_vector, I386_EXCEPTION_FAULT, true, error_code);

    u32 low = 0;
    u32 high = 0;

    if (!ReadLinear(table_base + offset, 32, context, low, true))
        return false;

    if (!ReadLinear(table_base + offset + 4, 32, context, high, true))
        return false;

    DecodeDescriptor(selector, table_base + offset, low, high, descriptor);
    return true;
}

bool I386::ReadDescriptorNoFault(u16 selector, Descriptor& descriptor, GT_Bus_Access_Context& context, bool& valid)
{
    valid = false;

    if ((selector & 0xFFFC) == 0)
        return true;

    u32 table_base = m_gdtr.base;
    u32 table_limit = m_gdtr.limit;

    if ((selector & 4) != 0)
    {
        u8 type = (m_ldtr.attributes & I386_SEGMENT_TYPE_MASK) >> I386_SEGMENT_TYPE_SHIFT;

        if ((m_ldtr.selector & 0xFFFC) == 0 || (m_ldtr.attributes & I386_SEGMENT_PRESENT) == 0 ||
            (m_ldtr.attributes & I386_SEGMENT_SYSTEM) == 0 || type != 2)
            return true;

        table_base = m_ldtr.base;
        table_limit = m_ldtr.limit;
    }

    u32 offset = selector & 0xFFF8;

    if ((u64)offset + 7 > table_limit)
        return true;

    u32 low = 0;
    u32 high = 0;

    if (!ReadLinear(table_base + offset, 32, context, low, true))
        return false;

    if (!ReadLinear(table_base + offset + 4, 32, context, high, true))
        return false;

    DecodeDescriptor(selector, table_base + offset, low, high, descriptor);
    valid = true;
    return true;
}

void I386::LoadDescriptorCache(u16 selector, const Descriptor& descriptor, I386_Segment& segment)
{
    segment.selector = selector;
    segment.base = descriptor.base;
    segment.limit = descriptor.limit;
    segment.attributes = descriptor.attributes;
    segment.dpl = descriptor.dpl;
    UpdateSegmentFastPaths();
}

void I386::ClearSegmentCache(u16 selector, I386_Segment& segment)
{
    memset(&segment, 0, sizeof(segment));
    segment.selector = selector;
    UpdateSegmentFastPaths();
}

bool I386::SetDescriptorAccessed(const Descriptor& descriptor, GT_Bus_Access_Context& context)
{
    if (descriptor.system || (descriptor.type & 1) != 0)
        return true;

    return WriteLinear(descriptor.address + 5, 8, descriptor.access | 1, context, true);
}

bool I386::SetDescriptorType(const Descriptor& descriptor, u8 type, GT_Bus_Access_Context& context)
{
    u8 access = (descriptor.access & 0xF0) | (type & 15);
    return WriteLinear(descriptor.address + 5, 8, access, context, true);
}

bool I386::LoadSegment(int segment, u16 selector, GT_Bus_Access_Context& context)
{
    if (m_execution_mode != I386_MODE_PROTECTED)
        return LoadRealSegment(segment, selector);

    return LoadProtectedSegment(segment, selector, context);
}

bool I386::LoadProtectedSegment(int segment, u16 selector, GT_Bus_Access_Context& context)
{
    if (segment < 0 || segment >= I386_SEGMENT_COUNT || segment == I386_SEGMENT_CS)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    u32 error_code = selector & 0xFFFC;

    if ((selector & 0xFFFC) == 0)
    {
        if (segment == I386_SEGMENT_SS)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        ClearSegmentCache(selector, m_segments[segment]);
        return true;
    }

    Descriptor descriptor;

    if (!ReadDescriptor(selector, descriptor, context))
        return false;

    if (segment == I386_SEGMENT_SS)
    {
        if (descriptor.system || (descriptor.attributes & I386_SEGMENT_EXECUTABLE) != 0 ||
            (descriptor.attributes & I386_SEGMENT_WRITABLE) == 0 || descriptor.dpl != m_current_privilege_level ||
            (selector & 3) != m_current_privilege_level)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, error_code);

        if (!descriptor.present)
            return RaiseException(12, I386_EXCEPTION_FAULT, true, error_code);
    }
    else
    {
        bool executable = (descriptor.attributes & I386_SEGMENT_EXECUTABLE) != 0;
        bool readable = (descriptor.attributes & I386_SEGMENT_READABLE) != 0;
        bool conforming = (descriptor.attributes & I386_SEGMENT_CONFORMING) != 0;
        u8 effective_privilege = MAX(m_current_privilege_level, selector & 3);

        if (descriptor.system || (executable && !readable) || (!conforming && effective_privilege > descriptor.dpl))
            return RaiseException(13, I386_EXCEPTION_FAULT, true, error_code);

        if (!descriptor.present)
            return RaiseException(11, I386_EXCEPTION_FAULT, true, error_code);
    }

    if (!SetDescriptorAccessed(descriptor, context))
        return false;

    LoadDescriptorCache(selector, descriptor, m_segments[segment]);
    return true;
}

bool I386::StackHasRoom(u32 limit, u16 attributes, u32 stack, u32 bytes) const
{
    u32 upper_limit = (attributes & I386_SEGMENT_DEFAULT_32) != 0 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 new_stack = (stack - bytes) & upper_limit;
    u64 end = (u64)new_stack + bytes - 1;

    if (end > upper_limit)
        return false;

    if ((attributes & I386_SEGMENT_EXPAND_DOWN) != 0)
        return new_stack > limit;

    return end <= limit;
}

bool I386::CheckStackFrame(u32 base, u16 attributes, u32 stack, u32 items, int width, bool supervisor,
    GT_Bus_Access_Context& context)
{
    if ((m_cr0 & 0x80000000U) == 0)
        return true;

    u32 mask = (attributes & I386_SEGMENT_DEFAULT_32) != 0 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 bytes = (u32)width >> 3;

    for (u32 i = 0; i < items; i++)
    {
        stack = (stack - bytes) & mask;

        if (!CheckLinearAccess(base + stack, bytes, true, context, supervisor))
            return false;
    }

    return true;
}

bool I386::ReadInterruptDescriptor(u8 vector, Descriptor& descriptor, GT_Bus_Access_Context& context)
{
    u32 offset = (u32)vector * 8;
    u32 error_code = offset | 2;

    if ((u64)offset + 7 > m_idtr.limit)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, error_code);

    u32 low = 0;
    u32 high = 0;

    if (!ReadLinear(m_idtr.base + offset, 32, context, low, true))
        return false;

    if (!ReadLinear(m_idtr.base + offset + 4, 32, context, high, true))
        return false;

    DecodeDescriptor((u16)error_code, m_idtr.base + offset, low, high, descriptor);
    return true;
}

bool I386::ReadPrivilegeStack(u8 privilege, u32& stack, u16& selector, GT_Bus_Access_Context& context)
{
    u8 type = (m_task_register.attributes & I386_SEGMENT_TYPE_MASK) >> I386_SEGMENT_TYPE_SHIFT;
    bool tss32 = type == 9 || type == 11;
    bool tss16 = type == 1 || type == 3;

    if ((m_task_register.selector & 0xFFFC) == 0 || (m_task_register.attributes & I386_SEGMENT_PRESENT) == 0 ||
        (m_task_register.attributes & I386_SEGMENT_SYSTEM) == 0 || (!tss32 && !tss16) || privilege > 2)
        return RaiseException(10, I386_EXCEPTION_FAULT, true, m_task_register.selector & 0xFFFC);

    u32 offset = tss32 ? 4 + (u32)privilege * 8 : 2 + (u32)privilege * 4;
    u32 bytes = tss32 ? 6 : 4;

    if ((u64)offset + bytes - 1 > m_task_register.limit)
        return RaiseException(10, I386_EXCEPTION_FAULT, true, m_task_register.selector & 0xFFFC);

    u32 value = 0;

    if (!ReadLinear(m_task_register.base + offset, tss32 ? 32 : 16, context, value, true))
        return false;

    stack = tss32 ? value : (u16)value;

    if (!ReadLinear(m_task_register.base + offset + (tss32 ? 4 : 2), 16, context, value, true))
        return false;

    selector = (u16)value;
    return true;
}

bool I386::EnterProtectedInterrupt(u8 vector, u32 return_eip, GT_Bus_Access_Context& context,
    bool software, bool has_error_code, u32 error_code, bool fault, u64* clocks, I386_Run_Result* run_result)
{
    Descriptor gate;

    if (!ReadInterruptDescriptor(vector, gate, context))
        return false;

    u32 idt_error = (u32)vector * 8 | 2;

    if (!gate.system || (gate.type != 5 && gate.type != 6 && gate.type != 7 && gate.type != 14 && gate.type != 15))
        return RaiseException(13, I386_EXCEPTION_FAULT, true, idt_error);

    if (software && m_current_privilege_level > gate.dpl)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, idt_error);

    if (!gate.present)
        return RaiseException(11, I386_EXCEPTION_FAULT, true, idt_error);

    if (gate.type == 5)
    {
        u16 task_selector = (u16)(gate.low >> 16);
        Descriptor task;

        if (!ReadGDTDescriptor(task_selector, task, context, 10))
            return false;

        if (!task.system || (task.type != 1 && task.type != 9))
            return RaiseException(10, I386_EXCEPTION_FAULT, true, task_selector & 0xFFFC);

        if (!task.present)
            return RaiseException(11, I386_EXCEPTION_FAULT, true, task_selector & 0xFFFC);

        u64 task_clocks = 0;
        u8 old_type = (m_task_register.attributes & I386_SEGMENT_TYPE_MASK) >> I386_SEGMENT_TYPE_SHIFT;
        bool ok = TaskSwitch(task_selector, task, I386_TASK_SWITCH_INTERRUPT, return_eip, context, task_clocks, true,
            has_error_code, error_code, fault);

        if (IsValidPointer(clocks))
            *clocks = task_clocks;

        if (ok && IsValidPointer(run_result))
            run_result->exception_return_eip = old_type == 9 || old_type == 11 ? return_eip : (u16)return_eip;

        return ok;
    }

    bool gate32 = gate.type == 14 || gate.type == 15;
    u16 target_selector = (u16)(gate.low >> 16);
    u32 target_offset = (gate.low & 0xFFFF) | (gate32 ? gate.high & 0xFFFF0000U : 0);

    if ((target_selector & 0xFFFC) == 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    Descriptor code;

    if (!ReadDescriptor(target_selector, code, context))
        return false;

    if (code.system || (code.attributes & I386_SEGMENT_EXECUTABLE) == 0)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, target_selector & 0xFFFC);

    bool conforming = (code.attributes & I386_SEGMENT_CONFORMING) != 0;

    if (m_execution_mode == I386_MODE_VM86 && (conforming || code.dpl != 0))
        return RaiseException(13, I386_EXCEPTION_FAULT, true, target_selector & 0xFFFC);

    u8 old_privilege = m_execution_mode == I386_MODE_VM86 ? 3 : m_current_privilege_level;

    if (code.dpl > old_privilege)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, target_selector & 0xFFFC);

    u8 new_privilege = conforming ? old_privilege : code.dpl;

    if (!code.present)
        return RaiseException(11, I386_EXCEPTION_FAULT, true, target_selector & 0xFFFC);

    u32 old_flags = m_eflags | (fault ? I386_FLAG_RF : 0);
    u16 old_cs = m_segments[I386_SEGMENT_CS].selector;
    u16 old_ss = m_segments[I386_SEGMENT_SS].selector;
    u32 old_stack = GetStackPointer();
    u32 old_esp = m_registers[I386_REG_ESP].value;
    bool vm86 = m_execution_mode == I386_MODE_VM86;
    int width = gate32 ? 32 : 16;

    if (IsValidPointer(clocks))
        *clocks = vm86 ? 119 : new_privilege < old_privilege ? 99 : 59;

    if (new_privilege < old_privilege)
    {
        u32 new_stack = 0;
        u16 new_ss = 0;

        if (!ReadPrivilegeStack(new_privilege, new_stack, new_ss, context))
            return false;

        if ((new_ss & 0xFFFC) == 0)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        if ((new_ss & 3) != new_privilege)
            return RaiseException(10, I386_EXCEPTION_FAULT, true, new_ss & 0xFFFC);

        Descriptor stack_descriptor;

        if (!ReadDescriptor(new_ss, stack_descriptor, context, 10))
            return false;

        if (stack_descriptor.system || (stack_descriptor.attributes & I386_SEGMENT_EXECUTABLE) != 0 ||
            (stack_descriptor.attributes & I386_SEGMENT_WRITABLE) == 0 || stack_descriptor.dpl != new_privilege)
            return RaiseException(10, I386_EXCEPTION_FAULT, true, new_ss & 0xFFFC);

        if (!stack_descriptor.present)
            return RaiseException(12, I386_EXCEPTION_FAULT, true, new_ss & 0xFFFC);

        u32 frame_items = vm86 ? 9 : 5;

        if (has_error_code)
            frame_items++;

        u32 frame_bytes = frame_items * ((u32)width >> 3);

        if (!StackHasRoom(stack_descriptor.limit, stack_descriptor.attributes, new_stack, frame_bytes))
            return RaiseException(12, I386_EXCEPTION_FAULT, true, 0);

        if (target_offset > code.limit)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        if (!SetDescriptorAccessed(code, context))
            return false;

        if (!SetDescriptorAccessed(stack_descriptor, context))
            return false;

        if (!CheckStackFrame(stack_descriptor.base, stack_descriptor.attributes, new_stack, frame_items, width, true,
            context))
            return false;

        LoadDescriptorCache((new_ss & 0xFFFC) | new_privilege, stack_descriptor, m_segments[I386_SEGMENT_SS]);

        m_current_privilege_level = new_privilege;
        m_eflags &= ~I386_FLAG_VM;
        m_execution_mode = I386_MODE_PROTECTED;
        UpdateUserMode();
        UpdateSegmentFastPaths();
        SetStackPointer(new_stack);

        if (vm86)
        {
            if (!StackPushSized(m_segments[I386_SEGMENT_GS].selector, width, context))
                return false;

            if (!StackPushSized(m_segments[I386_SEGMENT_FS].selector, width, context))
                return false;

            if (!StackPushSized(m_segments[I386_SEGMENT_DS].selector, width, context))
                return false;

            if (!StackPushSized(m_segments[I386_SEGMENT_ES].selector, width, context))
                return false;
        }

        if (!StackPushSized(old_ss, width, context))
            return false;

        if (!StackPushSized(old_esp, width, context))
            return false;
    }
    else
    {
        const I386_Segment& stack_segment = m_segments[I386_SEGMENT_SS];
        u32 frame_items = 3 + (has_error_code ? 1 : 0);
        u32 frame_bytes = frame_items * ((u32)width >> 3);

        if (!StackHasRoom(stack_segment.limit, stack_segment.attributes, old_stack, frame_bytes))
            return RaiseException(12, I386_EXCEPTION_FAULT, true, 0);

        if (target_offset > code.limit)
            return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

        if (!SetDescriptorAccessed(code, context))
            return false;

        if (!CheckStackFrame(stack_segment.base, stack_segment.attributes, old_stack, frame_items, width, false,
            context))
            return false;
    }

    if (!StackPushSized(old_flags, width, context))
        return false;

    if (!StackPushSized(old_cs, width, context))
        return false;

    if (!StackPushSized(return_eip, width, context))
        return false;

    if (has_error_code && !StackPushSized(error_code, width, context))
        return false;

    LoadDescriptorCache((target_selector & 0xFFFC) | new_privilege, code, m_segments[I386_SEGMENT_CS]);

    m_segments[I386_SEGMENT_CS].dpl = new_privilege;
    m_current_privilege_level = new_privilege;
    UpdateUserMode();
    m_eip = gate32 ? target_offset : (u16)target_offset;
    m_eflags &= ~(I386_FLAG_TF | I386_FLAG_NT | I386_FLAG_RF | I386_FLAG_VM);

    if (gate.type == 6 || gate.type == 14)
        m_eflags &= ~I386_FLAG_IF;

    if (vm86)
    {
        ClearSegmentCache(0, m_segments[I386_SEGMENT_ES]);
        ClearSegmentCache(0, m_segments[I386_SEGMENT_DS]);
        ClearSegmentCache(0, m_segments[I386_SEGMENT_FS]);
        ClearSegmentCache(0, m_segments[I386_SEGMENT_GS]);
    }

    m_execution_mode = I386_MODE_PROTECTED;
    m_halted = false;
    UpdateUserMode();
    UpdateSegmentFastPaths();

    if (IsValidPointer(run_result))
        run_result->exception_return_eip = gate32 ? return_eip : (u16)return_eip;

    return true;
}

u8 I386::GetIOPrivilegeLevel() const
{
    return (m_eflags >> 12) & 3;
}

bool I386::CheckIOPermission(u16 port, int width, GT_Bus_Access_Context& context, bool& allowed)
{
    allowed = true;

    if (m_execution_mode == I386_MODE_REAL)
        return true;

    if (m_execution_mode != I386_MODE_VM86 && m_current_privilege_level <= GetIOPrivilegeLevel())
        return true;

    allowed = false;

    u8 type = (m_task_register.attributes & I386_SEGMENT_TYPE_MASK) >> I386_SEGMENT_TYPE_SHIFT;

    if ((m_task_register.selector & 0xFFFC) == 0 || (m_task_register.attributes & I386_SEGMENT_PRESENT) == 0 ||
        (m_task_register.attributes & I386_SEGMENT_SYSTEM) == 0 || (type != 9 && type != 11) ||
        m_task_register.limit < 0x67)
        return true;

    u32 bitmap_offset = 0;

    if (!ReadLinear(m_task_register.base + 0x66, 16, context, bitmap_offset, true))
        return false;

    u32 bytes = (u32)width >> 3;

    for (u32 i = 0; i < bytes; i++)
    {
        u32 current_port = (u32)port + i;
        u32 byte_offset = bitmap_offset + (current_port >> 3);

        if (byte_offset > m_task_register.limit)
            return true;

        u32 permission = 0;

        if (!ReadLinear(m_task_register.base + byte_offset, 8, context, permission, true))
            return false;

        if ((permission & (1U << (current_port & 7))) != 0)
            return true;
    }

    allowed = true;
    return true;
}
