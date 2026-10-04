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

static const u32 k_tss32_register_offsets[I386_REG_COUNT] = { 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44 };
static const u32 k_tss32_segment_offsets[I386_SEGMENT_COUNT] = { 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C };
static const u32 k_tss16_register_offsets[I386_REG_COUNT] = { 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E, 0x20 };
static const u32 k_tss16_segment_offsets[4] = { 0x22, 0x24, 0x26, 0x28 };
static const int k_tss16_segment_indices[4] = { I386_SEGMENT_ES, I386_SEGMENT_CS, I386_SEGMENT_SS, I386_SEGMENT_DS };

bool I386::ReadTaskState(const Descriptor& descriptor, TaskState& state, GT_Bus_Access_Context& context)
{
    memset(&state, 0, sizeof(state));

    state.tss32 = descriptor.type == 9 || descriptor.type == 11;

    u32 minimum_limit = state.tss32 ? 0x67 : 0x2B;

    if (descriptor.limit < minimum_limit)
        return RaiseException(10, I386_EXCEPTION_FAULT, true, descriptor.selector & 0xFFFC);

    u32 value = 0;

    if (state.tss32)
    {
        if (!ReadLinear(descriptor.base + 0x1C, 32, context, state.cr3, true))
            return false;

        if (!ReadLinear(descriptor.base + 0x20, 32, context, state.eip, true))
            return false;

        if (!ReadLinear(descriptor.base + 0x24, 32, context, state.eflags, true))
            return false;

        for (int i = 0; i < I386_REG_COUNT; i++)
        {
            if (!ReadLinear(descriptor.base + k_tss32_register_offsets[i], 32, context, state.registers[i], true))
                return false;
        }

        for (int i = 0; i < I386_SEGMENT_COUNT; i++)
        {
            if (!ReadLinear(descriptor.base + k_tss32_segment_offsets[i], 16, context, value, true))
                return false;

            state.segments[i] = (u16)value;
        }

        if (!ReadLinear(descriptor.base + 0x60, 16, context, value, true))
            return false;

        state.ldtr = (u16)value;

        if (!ReadLinear(descriptor.base + 0x64, 16, context, value, true))
            return false;

        state.debug_trap = (value & 1) != 0;
    }
    else
    {
        if (!ReadLinear(descriptor.base + 0x0E, 16, context, value, true))
            return false;

        state.eip = (u16)value;

        if (!ReadLinear(descriptor.base + 0x10, 16, context, value, true))
            return false;

        state.eflags = (u16)value;

        for (int i = 0; i < I386_REG_COUNT; i++)
        {
            if (!ReadLinear(descriptor.base + k_tss16_register_offsets[i], 16, context, value, true))
                return false;

            state.registers[i] = 0xFFFF0000U | (u16)value;
        }

        for (int i = 0; i < 4; i++)
        {
            if (!ReadLinear(descriptor.base + k_tss16_segment_offsets[i], 16, context, value, true))
                return false;

            state.segments[k_tss16_segment_indices[i]] = (u16)value;
        }

        if (!ReadLinear(descriptor.base + 0x2A, 16, context, value, true))
            return false;

        state.ldtr = (u16)value;
        state.cr3 = m_state.cr3;
    }

    return true;
}

bool I386::SaveTaskState(const I386_Segment& task, u32 return_eip, u32 saved_eflags, GT_Bus_Access_Context& context)
{
    u8 type = (task.attributes & I386_SEGMENT_TYPE_MASK) >> I386_SEGMENT_TYPE_SHIFT;
    bool tss32 = type == 9 || type == 11;
    bool tss16 = type == 1 || type == 3;

    if (!tss32 && !tss16)
        return RaiseException(10, I386_EXCEPTION_FAULT, true, task.selector & 0xFFFC);

    if (tss32)
    {
        if (!WriteLinear(task.base + 0x20, 32, return_eip, context, true))
            return false;

        if (!WriteLinear(task.base + 0x24, 32, saved_eflags, context, true))
            return false;

        for (int i = 0; i < I386_REG_COUNT; i++)
        {
            if (!WriteLinear(task.base + k_tss32_register_offsets[i], 32, m_state.registers[i].value, context, true))
                return false;
        }

        for (int i = 0; i < I386_SEGMENT_COUNT; i++)
        {
            if (!WriteLinear(task.base + k_tss32_segment_offsets[i], 16, m_state.segments[i].selector, context, true))
                return false;
        }

        return true;
    }

    if (!WriteLinear(task.base + 0x0E, 16, return_eip, context, true))
        return false;

    if (!WriteLinear(task.base + 0x10, 16, saved_eflags, context, true))
        return false;

    for (int i = 0; i < I386_REG_COUNT; i++)
    {
        if (!WriteLinear(task.base + k_tss16_register_offsets[i], 16, m_state.registers[i].low, context, true))
            return false;
    }

    for (int i = 0; i < 4; i++)
    {
        u16 selector = m_state.segments[k_tss16_segment_indices[i]].selector;

        if (!WriteLinear(task.base + k_tss16_segment_offsets[i], 16, selector, context, true))
            return false;
    }

    return true;
}

bool I386::LoadTaskSegments(const TaskState& state, GT_Bus_Access_Context& context)
{
    ClearSegmentCache(state.ldtr, m_state.ldtr);

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
        ClearSegmentCache(state.segments[i], m_state.segments[i]);

    UpdateExecutionMode();

    if ((state.ldtr & 0xFFFC) != 0)
    {
        Descriptor ldt;

        if (!ReadGDTDescriptor(state.ldtr, ldt, context, 10))
            return false;

        if (!ldt.system || ldt.type != 2 || !ldt.present)
            return RaiseException(10, I386_EXCEPTION_FAULT, true, state.ldtr & 0xFFFC);

        LoadDescriptorCache(state.ldtr, ldt, m_state.ldtr);
    }

    if ((m_state.eflags & I386_FLAG_VM) != 0)
    {
        for (int i = 0; i < I386_SEGMENT_COUNT; i++)
            SetVM86Segment((I386_Segment_Register)i, state.segments[i]);

        m_state.current_privilege_level = 3;
        m_state.execution_mode = I386_MODE_VM86;

        UpdateUserMode();
        UpdateSegmentFastPaths();
        return true;
    }

    u16 code_selector = state.segments[I386_SEGMENT_CS];

    if ((code_selector & 0xFFFC) == 0)
        return RaiseException(10, I386_EXCEPTION_FAULT, true, 0);

    Descriptor code;

    if (!ReadDescriptor(code_selector, code, context, 10))
        return false;

    u8 privilege = code_selector & 3;
    bool executable = !code.system && (code.attributes & I386_SEGMENT_EXECUTABLE) != 0;
    bool conforming = executable && (code.attributes & I386_SEGMENT_CONFORMING) != 0;

    if (!executable || (!conforming && code.dpl != privilege) || (conforming && code.dpl > privilege))
        return RaiseException(10, I386_EXCEPTION_FAULT, true, code_selector & 0xFFFC);

    if (!code.present)
        return RaiseException(11, I386_EXCEPTION_FAULT, true, code_selector & 0xFFFC);

    if (!SetDescriptorAccessed(code, context))
        return false;

    LoadDescriptorCache((code_selector & 0xFFFC) | privilege, code, m_state.segments[I386_SEGMENT_CS]);

    m_state.segments[I386_SEGMENT_CS].dpl = privilege;

    u16 stack_selector = state.segments[I386_SEGMENT_SS];

    if ((stack_selector & 0xFFFC) == 0 || (stack_selector & 3) != privilege)
        return RaiseException(10, I386_EXCEPTION_FAULT, true, stack_selector & 0xFFFC);

    Descriptor stack;

    if (!ReadDescriptor(stack_selector, stack, context, 10))
        return false;

    if (stack.system || (stack.attributes & I386_SEGMENT_EXECUTABLE) != 0 ||
        (stack.attributes & I386_SEGMENT_WRITABLE) == 0 || stack.dpl != privilege)
        return RaiseException(10, I386_EXCEPTION_FAULT, true, stack_selector & 0xFFFC);

    if (!stack.present)
        return RaiseException(12, I386_EXCEPTION_FAULT, true, stack_selector & 0xFFFC);

    if (!SetDescriptorAccessed(stack, context))
        return false;

    LoadDescriptorCache(stack_selector, stack, m_state.segments[I386_SEGMENT_SS]);

    m_state.current_privilege_level = privilege;
    m_state.execution_mode = I386_MODE_PROTECTED;
    UpdateUserMode();
    UpdateSegmentFastPaths();

    static const int k_data_segments[4] = { I386_SEGMENT_ES, I386_SEGMENT_DS, I386_SEGMENT_FS, I386_SEGMENT_GS };

    for (int i = 0; i < 4; i++)
    {
        int segment = k_data_segments[i];
        u16 selector = state.segments[segment];

        if ((selector & 0xFFFC) == 0)
        {
            ClearSegmentCache(selector, m_state.segments[segment]);
            continue;
        }

        Descriptor descriptor;

        if (!ReadDescriptor(selector, descriptor, context, 10))
            return false;

        bool data_executable = (descriptor.attributes & I386_SEGMENT_EXECUTABLE) != 0;
        bool data_conforming = (descriptor.attributes & I386_SEGMENT_CONFORMING) != 0;
        bool data_readable = (descriptor.attributes & I386_SEGMENT_READABLE) != 0;

        if (descriptor.system || (data_executable && !data_readable) ||
            (!data_conforming && descriptor.dpl < MAX(privilege, selector & 3)))
            return RaiseException(10, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

        if (!descriptor.present)
            return RaiseException(11, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

        if (!SetDescriptorAccessed(descriptor, context))
            return false;

        LoadDescriptorCache(selector, descriptor, m_state.segments[segment]);
    }

    return true;
}

u32 I386::GetTaskSwitchClocks(const Descriptor& descriptor, const TaskState& state, int switch_type,
    bool via_gate) const
{
    u8 old_type = (m_state.task_register.attributes & I386_SEGMENT_TYPE_MASK) >> I386_SEGMENT_TYPE_SHIFT;
    bool old_386 = old_type == 9 || old_type == 11;
    bool old_vm = m_state.execution_mode == I386_MODE_VM86;
    bool new_386 = descriptor.type == 9 || descriptor.type == 11;
    bool new_vm = new_386 && (state.eflags & I386_FLAG_VM) != 0;
    u32 clocks = 0;

    if (switch_type == I386_TASK_SWITCH_JMP)
    {
        if (new_386)
            clocks = new_vm ? (old_386 ? 220 : 218) : (old_386 ? 303 : 301);
        else
            clocks = old_386 ? 276 : 274;

        if (via_gate)
            clocks += 9;
    }
    else if (switch_type == I386_TASK_SWITCH_CALL)
    {
        if (new_386)
            clocks = new_vm ? 217 : (old_386 ? 300 : 298);
        else
            clocks = 273;

        if (via_gate)
            clocks += 9;
    }
    else if (switch_type == I386_TASK_SWITCH_IRET)
    {
        if (new_386)
            clocks = new_vm ? (old_386 ? 224 : 214) : (old_386 ? 275 : 265);
        else
            clocks = old_386 ? 271 : 232;
    }
    else
    {
        if (old_386)
        {
            if (new_386)
                clocks = new_vm ? (old_vm ? 231 : 226) : (old_vm ? 314 : 309);
            else
                clocks = old_vm ? 287 : 282;
        }
        else
            clocks = new_386 ? (new_vm ? 224 : 307) : 280;
    }

    return clocks;
}

bool I386::TaskSwitch(u16 selector, const Descriptor& descriptor, int switch_type, u32 return_eip,
    GT_Bus_Access_Context& context, u64& clocks, bool via_gate, bool has_error_code, u32 error_code, bool fault)
{
    TaskState new_state;

    if (!ReadTaskState(descriptor, new_state, context))
        return false;

    bool nested = switch_type == I386_TASK_SWITCH_CALL || switch_type == I386_TASK_SWITCH_INTERRUPT;

    clocks = GetTaskSwitchClocks(descriptor, new_state, switch_type, via_gate);

    Descriptor old_descriptor;

    if (!ReadGDTDescriptor(m_state.task_register.selector, old_descriptor, context, 10))
        return false;

    if (switch_type == I386_TASK_SWITCH_IRET)
        m_state.eflags &= ~I386_FLAG_NT;

    u32 saved_eflags = m_state.eflags | (fault ? I386_FLAG_RF : 0);

    if (!SaveTaskState(m_state.task_register, return_eip, saved_eflags, context))
        return false;

    // Nested switches store the back link to the old task
    if (nested && !WriteLinear(descriptor.base, 16, m_state.task_register.selector, context, true))
        return false;

    if (switch_type != I386_TASK_SWITCH_IRET && !SetDescriptorType(descriptor, descriptor.type | 2, context))
        return false;

    if (!nested && !SetDescriptorType(old_descriptor, old_descriptor.type & ~2, context))
        return false;

    Descriptor busy_descriptor = descriptor;

    busy_descriptor.type |= 2;
    busy_descriptor.attributes &= ~I386_SEGMENT_TYPE_MASK;
    busy_descriptor.attributes |= (u16)busy_descriptor.type << I386_SEGMENT_TYPE_SHIFT;
    busy_descriptor.attributes |= I386_SEGMENT_ACCESSED;

    LoadDescriptorCache(selector, busy_descriptor, m_state.task_register);

    m_state.cr0 |= 0x08;

    if (new_state.tss32)
    {
        m_state.cr3 = new_state.cr3;
        FlushTLB();
    }

    m_state.debug_registers[7] &= ~0x00000155U;
    UpdateMemoryMode();

    for (int i = 0; i < I386_REG_COUNT; i++)
        m_state.registers[i].value = new_state.registers[i];

    m_state.eip = new_state.eip;
    m_state.eflags = new_state.eflags | I386_FLAG_FIXED;

    if (nested)
        m_state.eflags |= I386_FLAG_NT;

    m_state.repeat.active = false;
    m_state.halted = false;

    bool ok = LoadTaskSegments(new_state, context);

    if (ok && m_state.eip > m_state.segments[I386_SEGMENT_CS].limit)
        ok = RaiseException(switch_type == I386_TASK_SWITCH_CALL ? 10 : 13, I386_EXCEPTION_FAULT, true, 0);

    if (ok && has_error_code)
        ok = StackPushSized(error_code, new_state.tss32 ? 32 : 16, context);

    if (ok && new_state.debug_trap)
    {
        m_state.debug_registers[6] |= 0x00008000U;
        ok = RaiseException(1, I386_EXCEPTION_TRAP);
    }

    // Faults raised once the new task is loaded return to it
    if (!ok && m_exception.pending)
    {
        m_exception.has_return_eip = true;
        m_exception.return_eip = m_state.eip;
    }

    return ok;
}

bool I386::TaskReturn(u32 return_eip, GT_Bus_Access_Context& context, u64& clocks)
{
    u32 backlink = 0;

    if (!ReadLinear(m_state.task_register.base, 16, context, backlink, true))
        return false;

    u16 selector = (u16)backlink;
    Descriptor descriptor;

    if (!ReadGDTDescriptor(selector, descriptor, context, 10))
        return false;

    if (!descriptor.system || (descriptor.type != 3 && descriptor.type != 11))
        return RaiseException(10, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    if (!descriptor.present)
        return RaiseException(11, I386_EXCEPTION_FAULT, true, selector & 0xFFFC);

    return TaskSwitch(selector, descriptor, I386_TASK_SWITCH_IRET, return_eip, context, clocks, false);
}
