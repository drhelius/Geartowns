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

#ifndef I386_RUN_INLINE_H
#define I386_RUN_INLINE_H

#include "../common/profiler.h"

INLINE bool I386::ExecuteOPCode()
{
    if (m_state.repeat.active)
    {
        m_step.clocks = GetStringClocks(true);
        return ContinueRepeat();
    }

    return StartInstruction();
}

INLINE u32 I386::RunCheckedStep()
{
    GT_Bus_Access_Context& context = *m_bus_context;
    StepState& result = m_step;

    result.clocks = 0;
    result.steps = 0;
    result.end_batch = false;
    result.exception = false;
    result.instruction_completed = true;

#if !defined(GT_DISABLE_DISASSEMBLER)
    m_step_call = false;
#endif

    if (m_state.shutdown || m_state.halted)
        return (u32)result.clocks;

#if !defined(GT_DISABLE_DISASSEMBLER)
    if (!m_state.repeat.active)
        DisassembleNextInstruction();
#endif

    m_debug_step = false;
    m_checked_valid = m_trace_enabled;

    if (unlikely(m_checked_valid))
        CaptureCheckedBytes();

    u16 old_task = m_state.task_register.selector;
    bool debug_active = unlikely((m_state.debug_registers[7] & 0xFF) != 0 ||
        (m_state.eflags & (I386_FLAG_TF | I386_FLAG_RF)) != 0);
    I386_State before;

    if (unlikely(m_trace_enabled))
        TraceStep(before);
    else if (debug_active)
        before.eflags = m_state.eflags;

    bool completed = debug_active ? ExecuteOPCodeDebug(before) : ExecuteOPCode();

    if (!completed)
        CompleteFault(old_task);

    if (m_state.interrupt_shadow_steps > 0 && result.instruction_completed)
        CountInterruptShadow();

    result.steps = 1;
    result.end_batch = result.end_batch || context.end_batch || m_state.halted || m_state.shutdown;

    if (unlikely(m_trace_enabled))
        RecordTrace(before);

    return (u32)result.clocks;
}

INLINE void I386::CountInterruptShadow()
{
    m_state.interrupt_shadow_steps--;

    if (m_state.interrupt_shadow_steps == 0)
        m_state.interrupt_shadow = I386_SHADOW_NONE;
}

// A pending INTR only ends the batch where it can be taken
// so CLI sections still run in batches
INLINE bool I386::IsInterruptReady(bool nmi_pending, bool intr_pending) const
{
    return nmi_pending || (intr_pending && CanAcceptMaskableInterrupt());
}

INLINE void I386::DisassembleNextInstruction()
{
    const I386_Segment& code = m_state.segments[I386_SEGMENT_CS];
    u32 eip = m_state.eip;
    u32 linear = code.base + eip;
    I386_Disassembler_Record*& cached = m_disassembler_cache[linear & (k_i386_disassembler_cache_size - 1)];
    I386_Disassembler_Record* record = cached;

    if (likely(IsValidPointer(record) && record->linear == linear && record->eip == eip && record->size > 0 &&
        record->cs == code.selector && record->mode == m_state.execution_mode &&
        record->default32 == ((code.attributes & I386_SEGMENT_DEFAULT_32) != 0)))
    {
        const u8* bytes = GetReadHost(linear, (u32)record->size);

        if (likely(IsValidPointer(bytes)))
        {
            int i = 0;

            while (i < record->size && bytes[i] == record->opcodes[i])
                i++;

            if (likely(i == record->size))
                return;
        }
    }

    cached = Disassemble(code, eip);
}

// A task CALL that faults or traps after entering the new task keeps its frame below the exception's
INLINE bool I386::TrackCall(bool completed, u16 cs, u32 base)
{
#if !defined(GT_DISABLE_DISASSEMBLER)
    bool entered = completed || m_task_call_entered;
    m_task_call_entered = false;

    if (entered)
    {
        int return_size = m_instruction.call_return_size != 0 ? m_instruction.call_return_size :
            m_instruction.operand_size;
        PushCallStack(cs, base, m_instruction.start_eip, Truncate(m_instruction.next_eip, return_size * 8), I386_CALL, 0);
    }
#else
    UNUSED(cs);
    UNUSED(base);
#endif
    return completed;
}

INLINE bool I386::TrackReturn(bool completed)
{
#if !defined(GT_DISABLE_DISASSEMBLER)
    if (completed && !m_disassembler_call_stack.empty())
        PopCallStack();
#endif
    return completed;
}

// Port accesses only run as the first step of a batch, where the context clocks are their access time
// Elsewhere the batch ends before the instruction and the next batch starts with it
INLINE bool I386::DeferIO()
{
    if (!m_batch_mode || m_state.segments[I386_SEGMENT_CS].base + m_state.eip == m_batch_start_pc)
        return false;

    m_step.instruction_completed = false;
    m_bus_context->end_batch = true;
    return true;
}

#endif /* I386_RUN_INLINE_H */
